// The game's TLB-mapped address ranges (docs/ROM_LAYOUT.md).
//
// Turok: Rage Wars doesn't run its code from kseg0 like most N64 games. Its
// entry point sets TLB entry 31 so that 0x00200000-0x003FFFFF maps onto
// physical 0-2 MB, and all of its code is linked there (the first MB of ROM
// becomes 0x00200400-0x002FFFFF). A second block of code and read-only data,
// ROM 0x164000-0x1B5A50, is linked at 0x00400000 and mapped with TLB entry 0:
// with the Expansion Pak the game loads it all and maps it as one 1 MB page;
// on a 4 MB console its own TLB-miss handler pages it in 4 KB at a time.
// Data is reached through kseg0 as usual.
//
// The recompiled code needs two things from these ranges:
//
// - Function lookups. Calls through function pointers use the TLB addresses
//   (0x002xxxxx / 0x004xxxxx), so every function is registered there.
//   librecomp's own init registers the first MB at its load address,
//   0x80000400, which the code never uses but which does no harm.
//
// - Data reads. Strings, tables and constants in the 0x00400000 block are
//   read through their TLB addresses. Generated code turns an address into
//   rdram + (address - 0xFFFFFFFF80000000), so 0x00400000 lands 0x80400000
//   bytes into the runtime's 4 GB rdram reservation, which is otherwise
//   inaccessible. Those pages are made readable and writable and filled from
//   the ROM. They are copies, not aliases of physical memory: nothing the game
//   writes to physical memory shows up here. That matches what the game
//   relies on: on a 4 MB console it re-reads the 0x00400000 block from ROM
//   whenever a page is evicted, so it can't depend on writes there, and no
//   code reads or writes data through the 0x00200000 view outside the
//   exception code and osInitialize, which aren't run (STATUS.md round 1).
//   If that ever turns out wrong, the fix is to back rdram with shared memory
//   and map these ranges as real aliases.
//
// - ROM-backed virtual memory regions. The game also maps data straight from
//   ROM into TLB address space and lets its TLB-miss handler page it in: the
//   audio banks, through 0x0023C76C (trw_VmMapRomRegion), which asks a
//   server thread for a region and gets back its virtual base. No TLB miss
//   ever happens here, so trw_VmMapRomRegion is replaced: each region gets
//   the address the game's server would give it, made accessible and filled
//   from ROM at once. The game can't depend on writing to these either
//   (evicted pages are re-read from ROM). Regions are never freed: the only
//   callers are the two audio-bank loads at startup.
#include <cstdio>
#include <algorithm>
#include <cstdint>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

#include "librecomp/game.hpp"
#include "librecomp/helpers.hpp"
#include "recomp.h"
#include "trw_controls.h"

extern "C" void trw_crash_note_rdram(uint8_t* rdram); // src/main/crash_handler.cpp

extern "C" void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);

namespace {
    struct TlbView {
        uint32_t vaddr;      // where the code sees it
        uint32_t size;       // size of the view
        uint32_t rom;        // ROM image of the view's start, or 0
        uint32_t rom_vaddr;  // where that ROM image starts within the view
        uint32_t rom_size;
        const char* what;
    };

    constexpr TlbView views[] = {
        // TLB entry 31: physical 0-2 MB; ROM 0x1000 is physical 0x400.
        { 0x00200000, 0x200000, 0x001000, 0x00200400, 0x100000, "main code (TLB entry 31)" },
        // TLB entry 0: the second code block.
        { 0x00400000, 0x100000, 0x164000, 0x00400000, 0x051A50, "second code block (TLB entry 0)" },
    };

    // Offset of a 32-bit guest address from rdram, as the generated code computes it.
    constexpr uint64_t rdram_offset(uint32_t vaddr) {
        return (uint64_t)(int64_t)(int32_t)vaddr - 0xFFFFFFFF80000000ULL;
    }

    bool make_accessible(uint8_t* rdram, uint32_t vaddr, uint32_t size) {
        uint8_t* host = rdram + rdram_offset(vaddr);
#ifdef _WIN32
        // librecomp reserves and commits the whole 4 GB as PAGE_NOACCESS.
        DWORD old_protect = 0;
        return VirtualProtect(host, size, PAGE_READWRITE, &old_protect) != 0;
#else
        return mprotect(host, size, PROT_READ | PROT_WRITE) == 0;
#endif
    }
}

namespace trw {
    // FPU register mode. The N64's boot code sets Status.FR, so the odd FPU
    // registers are registers of their own, and this game's GCC-compiled
    // code does single-precision math in them (f1, f21, ...). The runtime
    // starts every context in FR=0 mode, where the generated code's odd
    // register loads and its arithmetic disagree (round 3), so every thread,
    // and the entry thread here, is switched to FR=1.
    void set_fpu_mode(recomp_context* ctx) {
        ctx->f_odd = &ctx->f1.u32l;
        ctx->mips3_float_mode = true;
    }

    void on_thread_create(uint8_t*, recomp_context* ctx) {
        set_fpu_mode(ctx);
    }

    void init_tlb_views(uint8_t* rdram, recomp_context* ctx) {
        set_fpu_mode(ctx);
        trw::controls::rdram = rdram;
        trw_crash_note_rdram(rdram);
        for (const TlbView& view : views) {
            if (!make_accessible(rdram, view.vaddr, view.size)) {
                fprintf(stderr, "Couldn't set up the %s view at 0x%08X\n", view.what, view.vaddr);
                continue;
            }
            recomp::do_rom_read(rdram, (gpr)(int32_t)view.rom_vaddr, 0x10000000u + view.rom, view.rom_size);
            load_overlays(view.rom, (int32_t)view.rom_vaddr, view.rom_size);
        }
    }
}

namespace {
    std::mutex vm_mutex;
    // The VM server hands out pages from 0x800 (0x00800000) up, each region
    // starting on an even page (a TLB entry maps an even/odd pair). The ROM
    // data is linked for those addresses: the first audio bank holds
    // pointers like 0x00800014, so regions must come in the same order.
    uint32_t vm_next_page = 0x800;
}

// trw_VmMapRomRegion(u32 size, unused, u32 rom_address) -> virtual base
extern "C" void trw_VmMapRomRegion(uint8_t* rdram, recomp_context* ctx) {
    uint32_t size = (uint32_t)ctx->r4;
    uint32_t rom = (uint32_t)ctx->r6 & 0x0FFFFFFF;
    uint32_t rom_size = (uint32_t)recomp::get_rom().size();

    std::lock_guard lock{ vm_mutex };
    uint32_t pages = (size + 0xFFF) >> 12;
    uint32_t base_page = std::max<uint32_t>(0x800, (vm_next_page + 1) & ~1u);
    uint32_t base = base_page << 12;
    uint32_t bytes = pages << 12;
    if (pages == 0 || rom >= rom_size || base + bytes >= 0x7F000000 || !make_accessible(rdram, base, bytes)) {
        fprintf(stderr, "[TRW VM] Couldn't map ROM 0x%08X (0x%X bytes)\n", rom, size);
        ctx->r2 = 0;
        return;
    }
    uint32_t copy = std::min(bytes, rom_size - rom) & ~1u;
    recomp::do_rom_read(rdram, (gpr)(int32_t)base, 0x10000000u + rom, copy);
    vm_next_page = base_page + pages;

    fprintf(stderr, "[TRW VM] ROM 0x%08X (0x%X bytes) mapped at 0x%08X\n", rom, size, base);
    ctx->r2 = (gpr)(int32_t)base;
}
