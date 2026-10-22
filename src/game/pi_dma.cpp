// Cartridge DMA for the game's own DMA thread.
//
// Turok: Rage Wars doesn't load through osPiStartDma and the PI manager. Its
// DMA thread (0x0025651C) takes requests from a queue, calls
// osPiRawStartDma(OS_READ, rom_offset, dram, size) directly, and waits for
// the PI interrupt message it registered with
// osSetEventMesg(OS_EVENT_PI, ...) (at 0x0025626C). The runtime only has a
// stub for osPiRawStartDma and doesn't track OS_EVENT_PI, so both are done
// here: the DMA happens at once, then the PI event message is posted, at
// once too.
#include <atomic>
#include <cstdio>

#include "ultramodern/ultramodern.hpp"
#include "librecomp/game.hpp"
#include "librecomp/helpers.hpp"
#include "recomp.h"

namespace {
    std::atomic<int32_t> pi_event_mq{ 0 };
    std::atomic<uint32_t> pi_event_msg{ 0 };
}

// Called from the [[patches.hook]] in turokrw.us.toml.
extern "C" void trw_set_pi_event(uint8_t*, uint64_t mq, uint32_t msg) {
    pi_event_msg = msg;
    pi_event_mq = (int32_t)mq;
}

// osPiRawStartDma(s32 direction, u32 devAddr, void* dramAddr, u32 size)
extern "C" void trw_osPiRawStartDma(uint8_t* rdram, recomp_context* ctx) {
    uint32_t direction = (uint32_t)ctx->r4;
    // libultra ORs in osRomBase (0xB0000000) and converts to physical.
    uint32_t physical_addr = ((uint32_t)ctx->r5 | 0xB0000000u) & 0x1FFFFFFFu;
    gpr dram_addr = ctx->r6;
    uint32_t size = (uint32_t)ctx->r7;

    if (direction != 0) {
        fprintf(stderr, "[TRW PI] Unexpected PI write to 0x%08X (0x%X bytes)\n", physical_addr, size);
    }
    else {
        recomp::do_rom_read(rdram, dram_addr, physical_addr, size);
    }

    // Posted at once, from this (the DMA) thread, as a game thread would. As
    // an external message it came later, through the runtime's own thread:
    // the DMA thread blocked waiting for it, lower threads ran meanwhile, and
    // quitting a match could free buffers whose requests were already queued
    // behind it; their DMA then wrote over memory given to something else
    // (issue 7: crashes after quitting and restarting matches).
    int32_t mq = pi_event_mq;
    if (mq != 0) {
        osSendMesg(rdram, mq, pi_event_msg, OS_MESG_NOBLOCK);
    }
    ctx->r2 = 0;
}
