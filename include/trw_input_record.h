#ifndef __TRW_INPUT_RECORD_H__
#define __TRW_INPUT_RECORD_H__

#include <cstdint>

// Input record and replay (src/main/input_record.cpp): PAXDINO_INPUTREC and
// PAXDINO_INPUTPLAY.
namespace trw::inputrec {
    // What else a controller read decided for the game, from the host's
    // state (the mouse, mostly), that a replay has to apply the same way.
    struct Extras {
        uint32_t flags = 0;
        float yaw = 0.0f, pitch = 0.0f; // mouse look's turn, radians
    };
    // One controller port's read, as it goes to the game (and over the
    // network in netplay).
    struct PortRead {
        int32_t response = 0; // the controller answered
        uint32_t buttons = 0;
        float x = 0.0f, y = 0.0f;
        Extras extras{};
    };
    constexpr uint32_t kMouseAims = 1u << 0;  // the mouse aims this port's player
    constexpr uint32_t kPicking = 1u << 1;    // a weapon pick (wheel) in progress
    constexpr uint32_t kSkipBoot = 1u << 2;   // a press skips the boot screen

    // A recording is being played in place of the live input.
    bool replaying();
    // The next recorded controller read, for `port`; returns whether the
    // controller answered.
    bool replay(int port, uint16_t* buttons, float* x, float* y, Extras* extras);
    // One controller read as the game got it.
    void record(int port, bool response, uint16_t buttons, float x, float y, const Extras& extras);
}

#endif
