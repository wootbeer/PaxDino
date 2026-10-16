// Weapon Reverb (Sound tab): a little more of the game's own reverb on
// gunshots, projectiles, impacts and explosions, so they carry in the mix.
// Voices, footsteps, pickups, weapon switches and menu sounds stay as they
// are.
//
// The game's synth is set up with a reverb (alSynNew's config: fxType 6, a
// custom room of its own at 0x800CB634), and each sound has a reverb amount,
// 0-127 (alSndpSetFXMix), but the game gives its sounds 0 to 3, so next to
// nothing reaches the reverb. A sound starts in func_00257794, which sets
// that amount (at 0x002578B4: its own plus a global one, at most 127); this
// adds to it.
//
// Which sounds: the sound effects are numbered in blocks of 100 (decimal),
// one per weapon (800 Tek Crossbow, 1000 Mag 60, 1100 Assault Rifle, 1200
// Grenade Launcher, 1300 Scorpion, 1500 Plasma Rifle, 1600 Shotgun, 1800
// Cerebral Bore, 1900 Minigun, 2000 Warhammer, 2200 Boomerang, 2300 Napalm
// Gel, 2400 Emaciator, 2500 Inflator, 2600 Freeze Gun, 2800 Chest Burster,
// 2900 Flare Gun, 3100 Sentry Turret, 3300 explosions), then the weapons'
// select sounds (3000s), the Raptor Claw's and Mite Bite's (3310s, maybe
// growls), the weapon switch (3400), pickups (3500s), the menus (3700s) and
// footsteps (5000s). A request to play one comes through func_0025DDF4
// (effect, position) or func_0025DE60 (the same and a volume; animations'
// sound events, the shots among them), and the sound starts within it. Any
// sound requested where effects and projectiles are made (a muzzle flash, a
// rocket, an energy bolt, a bullet's impact; some of their numbers are below
// 800, among the player's own sounds) gets the reverb too; none requested by
// the players' movement code does. func_0025DED4, which plays the menus' and
// other sounds of the player's own, is left alone.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "recomp.h"

namespace {
    std::atomic<bool> enabled{ false };
    std::atomic<int32_t> amount{ 0 }; // added to a sound's reverb amount (0-127)

    bool world_site = false;  // the next request is from the effects' and projectiles' code
    bool local_site = false;  // the next request is from the players' movement code
    bool request_boosted = false; // the request being made is one of the sounds above

    bool weapon_effect(int32_t effect) {
        return (effect >= 800 && effect < 3000) || (effect >= 3100 && effect < 3310);
    }
}

// The most the slider adds to a sound's reverb amount: 30% of the full 127
// (38).
constexpr float kMaxAmount = 0.3f * 127.0f;

// Each frame, from the Sound tab (src/game/extras.cpp): on or off, and the
// amount, 0 to 1.
void trw_set_weapon_reverb(bool on, float level) {
    enabled.store(on);
    amount.store((int32_t)std::lround(std::clamp(level, 0.0f, 1.0f) * kMaxAmount));
}

// Before the effects' and projectiles' code asks for a sound
// (turokrw.us.toml: the calls at 0x0027D1AC, 0x0027D86C, 0x00281954,
// 0x00282010, 0x00283E90).
extern "C" void trw_sfx_world_site(void) {
    world_site = true;
}

// Before the players' movement code asks for a sound (footsteps, landing, and
// effect 2555, among the Inflator's numbers; turokrw.us.toml: the calls at
// 0x0021BC58, 0x0021C250).
extern "C" void trw_sfx_local_site(void) {
    local_site = true;
}

// At the start of func_0025DDF4 / func_0025DE60: a sound effect is asked for.
extern "C" void trw_sfx_request_begin(int32_t effect) {
    request_boosted = !local_site && (world_site || weapon_effect((int16_t)effect));
    world_site = false;
    local_site = false;
}

// At their return.
extern "C" void trw_sfx_request_end(void) {
    request_boosted = false;
}

// A sound starts (func_00257794, before its alSndpSetFXMix): its reverb amount.
extern "C" int32_t trw_sfx_fx_mix(int32_t fx_mix) {
    if (!request_boosted || !enabled.load()) {
        return fx_mix;
    }
    return std::min<int32_t>(fx_mix + amount.load(), 127);
}
