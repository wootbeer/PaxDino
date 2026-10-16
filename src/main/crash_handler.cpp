// Crash reporting. On a fatal exception, writes crash_log.txt to the working
// directory (the exe's folder when launched from Explorer; beside the AppImage
// on Linux) and stderr:
// - the exception and its offset in the exe (as Event Viewer reports it)
// - the recompiled game function that faulted, by N64 address
// - for memory faults, the N64 address being accessed
// - a best-effort list of game functions found on the host stack (callers)
// - the faulting game thread's MIPS registers, when a host register still
//   points at its context, and the player list
// - N64 memory, to crash_rdram.bin
// Release builds have no console, so a message box points testers at the log.
#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <csignal>
#include <dlfcn.h>
#include <pthread.h>
#include <ucontext.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#include "recomp.h"
#include "librecomp/sections.h"

namespace {
    struct GameFunc {
        uintptr_t host;
        uint32_t vram;
        uint32_t rom_size;
    };
    std::vector<GameFunc> game_funcs;
    uint8_t* rdram_base = nullptr;

    // Where the reports go: the working directory, or beside the AppImage on
    // Linux (its own folder, the working directory there, is read-only).
    std::string log_path = "crash_log.txt";
    std::string rdram_path = "crash_rdram.bin";

    // The faulting thread's general host registers, set by the handler.
    uintptr_t host_regs[16];
    int host_reg_count = 0;

    // Whether `size` bytes at `addr` can be read without faulting.
    bool readable(uintptr_t addr, size_t size) {
#if defined(_WIN32)
        MEMORY_BASIC_INFORMATION info;
        if (VirtualQuery(reinterpret_cast<const void*>(addr), &info, sizeof(info)) == 0) return false;
        const DWORD ok = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return info.State == MEM_COMMIT && (info.Protect & ok) != 0 && (info.Protect & PAGE_GUARD) == 0 &&
            addr + size <= (uintptr_t)info.BaseAddress + info.RegionSize;
#elif defined(__linux__)
        char buffer[sizeof(recomp_context)];
        struct iovec local { buffer, std::min(size, sizeof(buffer)) };
        struct iovec remote { reinterpret_cast<void*>(addr), local.iov_len };
        return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)local.iov_len;
#else
        return false;
#endif
    }

    // A recompiled function takes (rdram, ctx), so a host register often
    // still holds the game thread's recomp_context. One whose r0 is 0 and
    // whose stack pointer is a KSEG0 address in N64 memory is taken as it.
    const recomp_context* find_guest_context() {
        for (int i = 0; i < host_reg_count; i++) {
            uintptr_t p = host_regs[i];
            if (p == 0 || (p & 7) != 0 || !readable(p, sizeof(recomp_context))) continue;
            const recomp_context* ctx = reinterpret_cast<const recomp_context*>(p);
            uint64_t sp = (uint64_t)ctx->r29;
            if (ctx->r0 == 0 && (sp >> 32) == 0xFFFFFFFFu && (uint32_t)sp >= 0x80000000u && (uint32_t)sp < 0x80800000u) {
                return ctx;
            }
        }
        return nullptr;
    }

    void report_guest_registers(FILE* out) {
        const recomp_context* ctx = find_guest_context();
        if (ctx == nullptr) {
            return;
        }
        const gpr* r = &ctx->r0;
        static const char* names[32] = { "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
            "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra" };
        fprintf(out, "Game registers:\n");
        for (int i = 0; i < 32; i++) {
            fprintf(out, "  %s %08X%s", names[i], (uint32_t)r[i], (i % 4 == 3) ? "\n" : "");
        }
    }

    // Debug builds link incrementally: function pointers point at `jmp rel32`
    // thunks rather than the bodies. Follow one if present.
    uintptr_t resolve_thunk(uintptr_t addr) {
#if defined(_WIN32)
        const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
        if (p[0] == 0xE9) {
            int32_t rel;
            memcpy(&rel, p + 1, sizeof(rel));
            return addr + 5 + rel;
        }
#endif
        return addr;
    }

    // The game function whose host code contains `addr`, if any: the nearest
    // function start at or below it, within a generous body size.
    const GameFunc* find_game_func(uintptr_t addr) {
        auto it = std::upper_bound(game_funcs.begin(), game_funcs.end(), addr,
            [](uintptr_t a, const GameFunc& f) { return a < f.host; });
        if (it == game_funcs.begin()) return nullptr;
        --it;
        // Host code is much larger than MIPS code; allow 64x plus slack.
        if (addr - it->host > (uintptr_t)it->rom_size * 64 + 0x10000) return nullptr;
        return &*it;
    }

    bool in_rdram(uintptr_t addr) {
        return rdram_base != nullptr && addr >= (uintptr_t)rdram_base && addr - (uintptr_t)rdram_base < 0x100000000ull;
    }

    uint32_t n64_address(uintptr_t addr) {
        return (uint32_t)(addr - (uintptr_t)rdram_base) + 0x80000000u;
    }

    // On quit, librecomp frees rdram while the game's threads (never joined)
    // can still be running; their next access faults. While the game runs,
    // the whole 512 MB KSEG0 window is mapped, so a fault inside it can only
    // be that -- exit quietly instead of reporting a crash.
    bool is_shutdown_fault(bool has_access, uintptr_t access) {
        return has_access && in_rdram(access) && (access - (uintptr_t)rdram_base) < 0x20000000ull;
    }

    // A 32-bit guest word (N64 memory is held word-swapped, so a plain load).
    bool guest_word(uint32_t addr, uint32_t* value) {
        if (rdram_base == nullptr || addr < 0x80000000u || addr >= 0x80800000u || (addr & 3) != 0) {
            return false;
        }
        *value = *reinterpret_cast<const uint32_t*>(rdram_base + (addr - 0x80000000u));
        return true;
    }

    // The player list (first player's pointer at 0x8013EF84, the next at
    // +0x16E0, the controller port at +0x5D4): a corrupted link shows here.
    void report_players(FILE* out) {
        uint32_t player = 0;
        if (!guest_word(0x8013EF84, &player)) {
            return;
        }
        fprintf(out, "Players:");
        for (int i = 0; i < 8 && player != 0; i++) {
            uint32_t port = 0, next = 0;
            if (!guest_word(player + 0x5D4, &port) || !guest_word(player + 0x16E0, &next)) {
                fprintf(out, " 0x%08X (bad)", player);
                break;
            }
            fprintf(out, " 0x%08X (port %d)", player, (int32_t)port);
            player = next;
        }
        fprintf(out, "\n");
    }

    void report(FILE* out, const char* what, uintptr_t pc, uintptr_t module_base, bool has_access, uintptr_t access, bool write, const uintptr_t* stack, size_t stack_words) {
        fprintf(out, "PaxDino (Turok Rage Wars Recomp) crashed.\n");
        fprintf(out, "Exception: %s\n", what);
        fprintf(out, "Fault offset: 0x%" PRIxPTR "\n", pc - module_base);
        if (const GameFunc* f = find_game_func(pc)) {
            fprintf(out, "In game function: func_%08X (+0x%" PRIxPTR " host bytes)\n", f->vram, pc - f->host);
        }
        else {
            fprintf(out, "Not in a recompiled game function.\n");
        }
        if (has_access) {
            if (in_rdram(access)) {
                fprintf(out, "%s N64 address 0x%08X\n", write ? "Writing" : "Reading", n64_address(access));
            }
            else {
                fprintf(out, "%s host address 0x%" PRIxPTR " (outside N64 memory)\n", write ? "Writing" : "Reading", access);
            }
        }
        report_guest_registers(out);
        report_players(out);
        fprintf(out, "Game functions on the stack (most recent first, best effort):\n");
        int listed = 0;
        uint32_t last = 0;
        for (size_t i = 0; i < stack_words && listed < 24; i++) {
            const GameFunc* f = find_game_func(stack[i]);
            if (f != nullptr && f->vram != last) {
                fprintf(out, "  func_%08X\n", f->vram);
                last = f->vram;
                listed++;
            }
        }
        fflush(out);
    }

    // The top (highest address) of the current thread's stack, or 0 if unknown.
    uintptr_t stack_top() {
#if defined(_WIN32)
        ULONG_PTR low = 0, high = 0;
        GetCurrentThreadStackLimits(&low, &high);
        return (uintptr_t)high;
#elif defined(__linux__)
        pthread_attr_t attr;
        void* addr = nullptr;
        size_t size = 0;
        if (pthread_getattr_np(pthread_self(), &attr) != 0) return 0;
        pthread_attr_getstack(&attr, &addr, &size);
        pthread_attr_destroy(&attr);
        return (uintptr_t)addr + size;
#else
        return 0;
#endif
    }

    void write_reports(const char* what, uintptr_t pc, uintptr_t module_base, bool has_access, uintptr_t access, bool write, uintptr_t sp) {
        // Scan up to 64 KB of the faulting thread's stack (the handler runs on
        // that thread) for return addresses, never past its top.
        const uintptr_t* stack = reinterpret_cast<const uintptr_t*>(sp);
        uintptr_t top = stack_top();
        size_t words = 0;
        if (top > sp) {
            words = std::min<uintptr_t>(top - sp, 0x10000) / sizeof(uintptr_t);
        }
        report(stderr, what, pc, module_base, has_access, access, write, stack, words);
        if (FILE* f = fopen(log_path.c_str(), "w")) {
            report(f, what, pc, module_base, has_access, access, write, stack, words);
            fclose(f);
        }
        // N64 memory as the game left it (8 MB, each 32-bit word byte-swapped).
        if (rdram_base != nullptr) {
            if (FILE* f = fopen(rdram_path.c_str(), "wb")) {
                fwrite(rdram_base, 1, 0x800000, f);
                fclose(f);
            }
        }
    }

#if defined(_WIN32)
    const char* exception_name(DWORD code) {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "access violation";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
        case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
        case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
        default: return "unhandled exception";
        }
    }

    // MSVC C++ exceptions (code 0xE06D7363) carry their type information:
    // ExceptionInformation[1] is the thrown object, [2] its ThrowInfo, [3]
    // the image base its RVAs are relative to. Gives the type name and, for
    // std::exception and subclasses, what().
    constexpr DWORD msvc_cpp_exception = 0xE06D7363;
    struct CatchableTypeX64 {
        uint32_t properties;
        int32_t type;           // RVA of the TypeDescriptor
        int32_t mdisp, pdisp, vdisp;
        int32_t size;
        int32_t copy;
    };

    void describe_cpp_exception(const EXCEPTION_RECORD* rec, char* out, size_t out_size) {
        out[0] = 0;
        if (rec->ExceptionCode != msvc_cpp_exception || rec->NumberParameters < 4) {
            return;
        }
        const uint8_t* object = reinterpret_cast<const uint8_t*>(rec->ExceptionInformation[1]);
        const int32_t* throw_info = reinterpret_cast<const int32_t*>(rec->ExceptionInformation[2]);
        uintptr_t base = (uintptr_t)rec->ExceptionInformation[3];
        if (object == nullptr || throw_info == nullptr || base == 0) {
            return;
        }
        const int32_t* types = reinterpret_cast<const int32_t*>(base + throw_info[3]);
        int count = types[0];
        const char* first_name = nullptr;
        const char* message = nullptr;
        for (int i = 0; i < count && i < 16; i++) {
            const CatchableTypeX64* ct = reinterpret_cast<const CatchableTypeX64*>(base + types[1 + i]);
            const char* name = reinterpret_cast<const char*>(base + ct->type + 2 * sizeof(void*));
            if (first_name == nullptr) {
                first_name = name;
            }
            if (strcmp(name, ".?AVexception@std@@") == 0) {
                message = reinterpret_cast<const std::exception*>(object + ct->mdisp)->what();
            }
        }
        snprintf(out, out_size, "C++ exception %s: %s", first_name ? first_name : "(unknown type)", message ? message : "(no message)");
    }

    LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* info) {
        static volatile LONG entered = 0;
        if (InterlockedExchange(&entered, 1) != 0) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        const EXCEPTION_RECORD* rec = info->ExceptionRecord;
        {
            const CONTEXT* c = info->ContextRecord;
            const DWORD64 regs[16] = { c->Rax, c->Rcx, c->Rdx, c->Rbx, c->Rsp, c->Rbp, c->Rsi, c->Rdi, c->R8, c->R9, c->R10, c->R11, c->R12, c->R13, c->R14, c->R15 };
            for (int i = 0; i < 16; i++) host_regs[i] = (uintptr_t)regs[i];
            host_reg_count = 16;
        }
        uintptr_t pc = (uintptr_t)rec->ExceptionAddress;
        HMODULE module = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)pc, &module);
        bool has_access = (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || rec->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) && rec->NumberParameters >= 2;
        if (is_shutdown_fault(has_access, has_access ? (uintptr_t)rec->ExceptionInformation[1] : 0)) {
            TerminateProcess(GetCurrentProcess(), 0);
        }
        char module_name[MAX_PATH] = "";
        if (module != nullptr) {
            GetModuleFileNameA(module, module_name, sizeof(module_name));
        }
        char cpp_what[512];
        describe_cpp_exception(rec, cpp_what, sizeof(cpp_what));
        char what[1024];
        snprintf(what, sizeof(what), "%s (code 0x%08lX, in %s)%s%s", exception_name(rec->ExceptionCode),
            (unsigned long)rec->ExceptionCode, module_name[0] ? module_name : "unknown module",
            cpp_what[0] ? "\n" : "", cpp_what);
        write_reports(what, pc, (uintptr_t)module, has_access,
            has_access ? (uintptr_t)rec->ExceptionInformation[1] : 0,
            has_access && rec->ExceptionInformation[0] == 1, (uintptr_t)info->ContextRecord->Rsp);
        MessageBoxA(nullptr,
            "The game crashed. Details were saved to crash_log.txt in the game's folder -- "
            "please include that file when reporting the problem.",
            "PaxDino: Rage Wars Recomp", MB_OK | MB_ICONERROR);
        return EXCEPTION_CONTINUE_SEARCH;
    }
#elif defined(__linux__)
    void signal_handler(int sig, siginfo_t* info, void* uctx_void) {
        ucontext_t* uctx = static_cast<ucontext_t*>(uctx_void);
#if defined(__x86_64__)
        uintptr_t pc = (uintptr_t)uctx->uc_mcontext.gregs[REG_RIP];
        uintptr_t sp = (uintptr_t)uctx->uc_mcontext.gregs[REG_RSP];
#else
        uintptr_t pc = 0, sp = (uintptr_t)&uctx;
#endif
#if defined(__x86_64__)
        {
            const int regs[16] = { REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI, REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15 };
            for (int i = 0; i < 16; i++) host_regs[i] = (uintptr_t)uctx->uc_mcontext.gregs[regs[i]];
            host_reg_count = 16;
        }
#endif
        const char* what = sig == SIGSEGV ? "segmentation fault" : sig == SIGBUS ? "bus error" : sig == SIGILL ? "illegal instruction" : "floating point exception";
        if (is_shutdown_fault(sig == SIGSEGV || sig == SIGBUS, (uintptr_t)info->si_addr)) {
            _exit(0);
        }
        Dl_info dl{};
        uintptr_t module_base = dladdr((void*)pc, &dl) != 0 ? (uintptr_t)dl.dli_fbase : 0;
        write_reports(what, pc, module_base, sig == SIGSEGV || sig == SIGBUS, (uintptr_t)info->si_addr, false, sp);
        signal(sig, SIG_DFL);
        raise(sig);
    }
#endif
}

// Records where N64 memory starts, to translate faulting addresses (called
// once at boot, src/game/tlb_views.cpp).
extern "C" void trw_crash_note_rdram(uint8_t* rdram) {
    rdram_base = rdram;
}

void trw_install_crash_handler(const SectionTableEntry* sections, size_t num_sections) {
    for (size_t s = 0; s < num_sections; s++) {
        for (size_t i = 0; i < sections[s].num_funcs; i++) {
            const FuncEntry& f = sections[s].funcs[i];
            game_funcs.push_back({ resolve_thunk((uintptr_t)f.func), sections[s].ram_addr + f.offset, f.rom_size });
        }
    }
    std::sort(game_funcs.begin(), game_funcs.end(), [](const GameFunc& a, const GameFunc& b) { return a.host < b.host; });

    if (const char* appimage = std::getenv("APPIMAGE")) {
        std::string dir = appimage;
        dir = dir.substr(0, dir.find_last_of('/') + 1);
        log_path = dir + "crash_log.txt";
        rdram_path = dir + "crash_rdram.bin";
    }

#if defined(_WIN32)
    SetUnhandledExceptionFilter(unhandled_filter);
#elif defined(__linux__)
    struct sigaction sa {};
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (int sig : { SIGSEGV, SIGBUS, SIGILL, SIGFPE }) {
        sigaction(sig, &sa, nullptr);
    }
#endif
}
