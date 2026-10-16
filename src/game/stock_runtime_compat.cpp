// Stock-runtime compatibility shims for functions N64Recomp's own built-in
// `ignored_funcs` list (N64Recomp/src/symbol_lists.cpp) expects some
// runtime to provide under a `_recomp` suffix, but that stock
// N64ModernRuntime doesn't implement (confirmed by grepping its whole
// source; found this way in the BattleTanx recomp, STATUS.md round 23 there).
//
// Each implementation is a reading of real libultra behavior (documented,
// or read directly from this ROM's own bytes at the relevant address).
// They run in the shipping game; the ones with lower confidence are
// flagged individually below.

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "recomp.h"

// --- COP0 Status register read -------------------------------------------
// __osGetSR() just does `mfc0 $v0, C0_SR` on real hardware. Nothing in this
// runtime emulates COP0 state (the game's own SR helpers are stubbed in
// turokrw.us.toml for the same reason), so there's no real value to return.
// librecomp's own equivalent for the sibling function __osSetFpcCsr_recomp
// (lib/N64ModernRuntime/librecomp/src/ultra_translation.cpp) does the same
// thing for the same reason.
extern "C" void __osGetSR_recomp(uint8_t*, recomp_context* ctx) {
    ctx->r2 = 0;
}

// --- libultra thread scheduler internals ---------------------------------
// Real prototypes (confirmed against this ROM's own call sites -- e.g.
// __osEnqueueThread and __osDequeueThread are each called with the queue
// head in $a0 and the thread pointer in $a1, matching libultra's public
// prototype ordering): `__osEnqueueThread(OSThread **queue, OSThread *t)`,
// `__osDequeueThread(OSThread **queue, OSThread *t)`,
// `OSThread *__osPopThread(OSThread **queue)`, `void __osDispatchThread(void)`.
// ultramodern already implements the same priority-queue-of-OSThread
// operations these need (ultramodern/include/ultramodern/ultramodern.hpp)
// for its own osStartThread/osCreateThread/etc., operating directly on the
// guest OSThread queue linked list in rdram -- not ultramodern's own
// private state -- so calling straight through to those should be
// behaviorally equivalent regardless of which queue (a message queue's
// blocked-thread list, the run queue, etc.) is being manipulated.
extern "C" void __osEnqueueThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::thread_queue_insert(rdram, (PTR(PTR(OSThread)))(int32_t)ctx->r4, (PTR(OSThread))(int32_t)ctx->r5);
}

extern "C" void __osDequeueThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::thread_queue_remove(rdram, (PTR(PTR(OSThread)))(int32_t)ctx->r4, (PTR(OSThread))(int32_t)ctx->r5);
}

extern "C" void __osPopThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = ultramodern::thread_queue_pop(rdram, (PTR(PTR(OSThread)))(int32_t)ctx->r4);
}

// __osDispatchThread's real job (pick the next ready thread off the run
// queue and context-switch into it) is done very differently here than on
// real hardware: real hardware does it with a raw register/COP0 context
// restore, while ultramodern runs each N64 thread as a real host thread
// and switches via semaphore signaling. run_next_thread_and_wait is
// ultramodern's own equivalent entry point for "give up the CPU and let
// the scheduler pick who runs next."
extern "C" void __osDispatchThread_recomp(uint8_t* rdram, recomp_context*) {
    ultramodern::run_next_thread_and_wait(rdram);
}

// --- SI access serialization ----------------------------------------------
// __osSiCreateAccessQueue(void) takes no arguments on real hardware or in
// this ROM's own (never-recompiled, but still readable) bytes at its own
// address -- it creates a 1-slot OSMesgQueue used as a mutex around SI bus
// access, then immediately posts one message so the "mutex" starts
// available. In this ROM (0x002B9AF0) it sets __osSiAccessQueueEnabled
// (0x800D3050) to 1, then osCreateMesgQueue(&0x801467E8, &0x801467E0, 1)
// and osSendMesg(&0x801467E8, NULL, OS_MESG_NOBLOCK) -- textbook libultra.
// Replicated here against the same addresses so anything that later blocks
// on that queue (expecting the mutex-primed message) doesn't deadlock.
extern "C" void __osSiCreateAccessQueue_recomp(uint8_t* rdram, recomp_context*) {
    constexpr int32_t si_access_queue_enabled = 0x800D3050;
    constexpr int32_t si_access_queue = 0x801467E8;
    constexpr int32_t si_access_queue_msgs = 0x801467E0;
    MEM_W(0, si_access_queue_enabled) = 1;
    osCreateMesgQueue(rdram, si_access_queue, si_access_queue_msgs, 1);
    osSendMesg(rdram, si_access_queue, 0, 0);
}

// --- Timer / VI internals --------------------------------------------------
// Both are called with no arguments from this ROM's VI manager thread code
// (vimgr_text_0188), matching their real void-void libultra prototypes.
// ultramodern manages both VI framebuffer swaps (via the renderer context,
// tied to the real osViSwapBuffer/osViGetCurrentFramebuffer exports) and
// timer expiry (ultramodern::init_timers, backing the real
// osSetTimer/osStopTimer exports) through its own separate mechanism
// already, so these ROM-internal calls -- which just update the N64-side
// bookkeeping structures those subsystems would otherwise touch on real
// hardware -- are made no-ops here rather than risk them fighting with
// ultramodern's own tracking of the same state. The game runs correctly
// with them as no-ops; if VI timing or timer-driven gameplay logic ever
// misbehaves, check whether the ROM reads that bookkeeping directly.
extern "C" void __osTimerInterrupt_recomp(uint8_t*, recomp_context*) {
}

extern "C" void __osViSwapContext_recomp(uint8_t*, recomp_context*) {
}

// --- Turok: Rage Wars ------------------------------------------------------
// The Controller Pak library in the game's second code block (0x004460F0-
// 0x00448E34) talks to the SI directly until its osPfs* entry points are
// identified and replaced (PROGRESS.md). Until then it sees no pak: the bus
// "lock" is a no-op and every raw SI DMA fails.
extern "C" void __osSiGetAccess_recomp(uint8_t*, recomp_context*) {
}

extern "C" void __osSiRelAccess_recomp(uint8_t*, recomp_context*) {
}

extern "C" void __osSiRawStartDma_recomp(uint8_t*, recomp_context* ctx) {
    ctx->r2 = (uint32_t)-1;
}

// Only reached from the game's TLB paging code (0x0023C064) and TLB-miss
// handler, which don't run here: the second code block is always resident
// (src/game/tlb_views.cpp) and no exception is ever raised.
extern "C" void osUnmapTLB_recomp(uint8_t*, recomp_context*) {
}

extern "C" void osGetCurrFaultedThread_recomp(uint8_t*, recomp_context* ctx) {
    ctx->r2 = 0;
}
