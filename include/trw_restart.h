#ifndef __TRW_RESTART_H__
#define __TRW_RESTART_H__

// The menu's Restart button (src/main/restart.cpp).
namespace trw::restart {
    // Gives the frontend a restart callback. Call before recomp::start with
    // main's arguments.
    void install(int argc, char** argv);
    // If Restart was chosen, starts the game again. Call right before the
    // process ends; it does nothing after the first call.
    void relaunch_if_requested();
}

#endif
