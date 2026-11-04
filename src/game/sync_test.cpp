// Sync test (a step toward netplay): with PAXDINO_SYNCTEST set to a file
// path, each game frame's RDRAM is hashed in 64 KB chunks and written there,
// and the frame step is fixed (two retraces, as at 30 frames a second); the
// runtime runs in its deterministic mode (src/main/main.cpp), so the PC's
// speed doesn't change what the game does. Two runs given the same
// input should write the same file; tools/sync_compare.py finds the first
// frame and the chunks where they differ.
//
// File: per frame, the frame number (u32), the game mode (u32), the
// retraces so far (u64, the runtime's deterministic count), then kChunks u64
// hashes.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "recomp.h"
#include "ultramodern/ultramodern.hpp"

namespace {
    constexpr uint32_t kRdramSize = 0x800000;
    constexpr uint32_t kChunkSize = 0x10000;
    constexpr uint32_t kChunks = kRdramSize / kChunkSize;

    FILE* sync_file = nullptr;
    bool checked = false;
    uint32_t frame = 0;
    uint8_t* trw_sync_rdram = nullptr; // set at the first frame

    bool active() {
        if (!checked) {
            checked = true;
            const char* path = std::getenv("PAXDINO_SYNCTEST");
            if (path != nullptr && path[0] != '\0') {
                sync_file = std::fopen(path, "wb");
                if (sync_file == nullptr) {
                    std::fprintf(stderr, "[TRW] sync test: can't write %s\n", path);
                }
            }
        }
        return sync_file != nullptr;
    }

    // The runtime keeps a host pointer in each OSThread (its context, at
    // +0x18 past the field the game's threads have here), which differs from
    // run to run; it's left out. These are the game's nine threads.
    constexpr uint32_t kThreadContexts[] = {
        0x800FD310, 0x800FD740, 0x800FE5D0, 0x800FF1D8, 0x80104268,
        0x80105718, 0x801195B0, 0x801197E0, 0x801409F8,
    };

    bool is_thread_context(uint32_t addr) {
        for (uint32_t context : kThreadContexts) {
            if (addr == context) {
                return true;
            }
        }
        return false;
    }

    // Left out too: what the renderer writes back into RDRAM on its own time
    // (the three framebuffers from 0x80500000 and the depth buffer after
    // them, and what it draws at 0x80700000 before the game's first frame),
    // and the display list buffers (0x806D2000 and 0x806E9000), which carry
    // each player's own picture options (widescreen and the like). None of it
    // is the game's own state.
    constexpr uint32_t kSkipped[][2] = {
        { 0x80500000, 0x80660000 },
        { 0x806D2000, 0x80760000 },
    };

    bool is_skipped(uint32_t addr) {
        for (const auto& range : kSkipped) {
            if (addr >= range[0] && addr < range[1]) {
                return true;
            }
        }
        return false;
    }

    uint64_t hash_chunk(const uint8_t* p, uint32_t base) {
        const uint64_t* w = reinterpret_cast<const uint64_t*>(p);
        uint64_t h = 0x9E3779B97F4A7C15ull;
        if (is_skipped(base) && is_skipped(base + kChunkSize - 1)) {
            return h;
        }
        for (uint32_t i = 0; i < kChunkSize / 8; i++) {
            uint64_t word = is_thread_context(base + i * 8) || is_skipped(base + i * 8) ? 0 : w[i];
            h = (h ^ word) * 0x100000001B3ull;
            h ^= h >> 29;
        }
        return h;
    }
}

// RDRAM's hash, the threads' host pointers left out, once the RSP tasks and
// display lists still running are done (netplay's check that both games are
// the same, src/main/netplay.cpp).
extern "C" uint64_t trw_sync_rdram_hash(void) {
    if (trw_sync_rdram == nullptr) {
        return 0; // before the first frame
    }
    ultramodern::wait_for_deterministic_tasks();
    uint64_t h = 0;
    for (uint32_t i = 0; i < kChunks; i++) {
        h = (h ^ hash_chunk(trw_sync_rdram + i * kChunkSize, 0x80000000u + i * kChunkSize)) * 0x100000001B3ull;
    }
    return h;
}

// Where the game is, for netplay to tell a small desync from games that are
// somewhere else entirely: the game's mode (*0x801407B8: front end, a match,
// loading...) and the arena (*0x8014BD9C), a byte each.
extern "C" uint32_t trw_sync_place(void) {
    if (trw_sync_rdram == nullptr) {
        return 0;
    }
    auto word = [](uint32_t addr) { return *reinterpret_cast<const uint32_t*>(trw_sync_rdram + (addr - 0x80000000u)); };
    return (word(0x801407B8) & 0xFF) | ((word(0x8014BD9C) & 0xFF) << 8);
}

extern "C" int trw_sync_test_active(void) {
    return active() ? 1 : 0;
}

// The frame start (trw_frame_buffers_set, src/game/widescreen.cpp).
extern "C" void trw_sync_frame(uint8_t* rdram) {
    trw_sync_rdram = rdram;
    if (!active()) {
        return;
    }
    // The RSP tasks and display lists still running write RDRAM from other
    // threads; hashed while they run, their buffers would differ by timing
    // alone. (The game sees them finish only at its next idle point anyway.)
    ultramodern::wait_for_deterministic_tasks();
    uint32_t header[2] = { frame++, (uint32_t)MEM_W(0, (gpr)(int32_t)0x801407B8) };
    uint64_t retraces = ultramodern::deterministic_vi_count();
    std::fwrite(header, sizeof(header), 1, sync_file);
    std::fwrite(&retraces, sizeof(retraces), 1, sync_file);
    uint64_t hashes[kChunks];
    for (uint32_t i = 0; i < kChunks; i++) {
        hashes[i] = hash_chunk(rdram + i * kChunkSize, 0x80000000u + i * kChunkSize);
    }
    std::fwrite(hashes, sizeof(hashes), 1, sync_file);
    if ((frame & 63) == 0) {
        std::fflush(sync_file);
    }
    // PAXDINO_SYNCDUMP=N,M,...: the whole of RDRAM at those frames, to the
    // sync file's path + ".<frame>", to see which words differ.
    static const char* dumps = std::getenv("PAXDINO_SYNCDUMP");
    if (dumps != nullptr) {
        for (const char* p = dumps; *p != '\0';) {
            char* end = nullptr;
            unsigned long n = std::strtoul(p, &end, 10);
            if (end == p) {
                break;
            }
            if (n + 1 == frame) {
                char name[1024];
                std::snprintf(name, sizeof(name), "%s.%lu", std::getenv("PAXDINO_SYNCTEST"), n);
                if (FILE* f = std::fopen(name, "wb")) {
                    std::fwrite(rdram, 1, kRdramSize, f);
                    std::fclose(f);
                }
            }
            p = *end == ',' ? end + 1 : end;
        }
    }
}
