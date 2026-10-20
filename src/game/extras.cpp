// Unlocks and cheats from the menu's Enhancements, Unlocks and Cheats
// tabs (src/main/extras_config.cpp) that are applied to the game's memory once
// a frame, rather than through a hook in the recompiled code.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "recomp.h"
#include "trw_config.h"

#include "rt64_screen_filter.h"
#include "trw_controls.h"
#include "trw_crosshair.h"
#include "trw_menu_cursor.h"
#include "trw_music.h"

namespace {
    // The game's Cheats menu settings (func_00420E90), now set from the
    // Cheats tab (the menu itself is left out, turokrw.us.toml).
    constexpr int32_t kCheatFlags = (int32_t)0x80140208;
    struct CheatBit {
        trw::config::Option option;
        uint32_t bit;
    };
    constexpr CheatBit kCheatBits[] = {
        { trw::config::Option::AllCharacters, 0x08000000 },
        { trw::config::Option::AllWeapons, 0x10000000 },
        { trw::config::Option::UnlimitedAmmo, 0x00000008 },
        { trw::config::Option::ExtraModes, 0x20000000 },
    };

    // The options read by trw_unlocked_bit(), refreshed once a frame (the bit
    // test it hooks runs often).
    bool all_trials = false;

    // The player profiles (four of 0x190 bytes) and the unlock bits in them,
    // as offsets into a profile (docs/RAM_NOTES.md).
    constexpr uint32_t kProfiles = 0x800FCB00;
    constexpr uint32_t kProfileSize = 0x190;
    constexpr uint32_t kMenuOptionsAndLevelsStart = 0x7C; // main menu options, then 1 Player
    constexpr uint32_t kMenuOptionsAndLevelsEnd = 0x94;   // Trials, Time Trial and Frag Fest levels

    // The trials map (pointer at 0x800DF2F0) and its copy of the level bits
    // (36 bits from +0xA74), and whether it's being built (trw_trial_map_build).
    constexpr int32_t kTrialMap = (int32_t)0x800DF2F0;
    constexpr uint32_t kTrialMapLevelBits = 0xA74;
    constexpr uint32_t kTrialMapLevelBytes = 5;
    bool trial_map_building = false;

    // The map's cursor: the level it's on (+0xA44) and that level's node
    // widget (+0xA48). Levels not opened in the save are built hidden
    // (without the widget's "shown" flag 0x8): a dot (an image), or a boss's
    // framed picture (a group), so with All Levels the cursor's pulse on one
    // of them didn't show. The node under the cursor is shown while it's
    // there (trw_apply_extras).
    constexpr uint32_t kTrialMapCursorNode = 0xA48;
    constexpr uint16_t kWidgetGroup = 1;
    constexpr uint16_t kWidgetImage = 3;
    constexpr uint32_t kWidgetShown = 0x8;
    uint32_t shown_map = 0;
    uint32_t shown_node = 0;

    // The player objects (docs/RAM_NOTES.md): the first at *0x8013EF84, the
    // rest 0x16E8 apart; the controller port at +0x5D4. The GameShark codes'
    // addresses are for a player at 0x80335F40, so they're offsets from it.
    constexpr int32_t kPlayerPointer = (int32_t)0x8013EF84;
    constexpr uint32_t kPlayerPort = 0x5D4;
    constexpr uint32_t kPlayerHealth = 0x5E6;     // halfword, 0x6400 is 100
    constexpr uint32_t kPlayerJumpSpeed = 0x20;   // float, up
    constexpr uint32_t kPlayerFireDelay = 0x3F0;  // two halfwords: 0 before the first shot, 5 while firing, 2 ready
    constexpr uint16_t kFullHealth = 0x6400;
    constexpr uint32_t kMoonJumpSpeed = 0x43980000; // 304.0f
    constexpr uint16_t kRapidFireDelay = 2;       // ready to fire
    constexpr uint32_t kPlayerHeldWeapon = 0x62E; // halfword, a weapon id
    constexpr uint16_t kFirstPickupWeapon = 0x12; // Cerebral Bore
    constexpr uint16_t kLastPickupWeapon = 0x15;  // Sentry Turret
    constexpr uint16_t kButtonL = 0x0020;
    constexpr uint16_t kButtonZ = 0x2000;

    // The game's mode (0x801407B8): 0-4 the boot screens, 8 the front end,
    // 20 a fade; anything else is a match. The match clock (float, counting
    // down; 0 for no time limit) is at 0x801407F4.
    constexpr int32_t kGameMode = (int32_t)0x801407B8;
    constexpr int32_t kMatchClock = (int32_t)0x801407F4;
    constexpr int32_t kMatchPaused = (int32_t)0x801407D4; // the pause list or its options are up
    float held_clock = 0.0f;

    bool in_match(uint8_t* rdram) {
        uint32_t mode = (uint32_t)MEM_W(0, (gpr)kGameMode);
        return !(mode <= 4 || mode == 8 || mode == 20);
    }

    bool valid_player(uint32_t player) {
        return player >= 0x80000400u && player < 0x80800000u && (player & 3) == 0;
    }

    // Look Spring Override (Controls tab, one per player): look spring off for
    // the players it's on for. The game's own setting is byte +0x7D of a player's
    // settings (0x801402D8 + port * 0x96, which the player object points to
    // at +0x5D8): with it on, the look code (func_002231B0) eases the pitch
    // (+0x724) toward where the stick points, so with the stick centred the
    // view goes back to level. The game's option for it didn't hold for the
    // user. Off, the game's setting is left as it is (put back if it was
    // overridden).
    int16_t saved_look_spring[4] = { -1, -1, -1, -1 }; // a port's own setting while overridden

    void apply_look_spring(uint8_t* rdram) {
        uint32_t first = (uint32_t)MEM_W(0, (gpr)(int32_t)0x8013EF84);
        if (!valid_player(first)) {
            return;
        }
        for (int port = 0; port < 4; port++) {
            bool override = trw::config::get_look_spring_override(port);
            gpr player = (gpr)(int32_t)(first + port * 0x16E8u);
            uint32_t settings = (uint32_t)MEM_W(0x5D8, player);
            // (0x96 apart, so not word-aligned past player 1's.)
            if (MEM_W(0x5D0, player) == 0 || settings < 0x80000400u || settings >= 0x80800000u) {
                continue;
            }
            gpr spring = (gpr)(int32_t)settings;
            if (override) {
                if (saved_look_spring[port] < 0) {
                    saved_look_spring[port] = MEM_BU(0x7D, spring);
                }
                MEM_B(0x7D, spring) = 0;
            }
            else if (saved_look_spring[port] >= 0) {
                // Turned off again: the player's own setting back.
                MEM_B(0x7D, spring) = (int8_t)saved_look_spring[port];
                saved_look_spring[port] = -1;
            }
        }
    }

    // Rapid Fire. A shot puts the fire state at 5 (busy) until the weapon
    // can fire again, then 2 (ready). Rapid Fire sets it to 2 once the shot
    // is out: its ammo has gone down, or a few calls on for a weapon that
    // uses none (or with Unlimited Ammo). Any sooner and the shot never gets
    // out. Never from 0 (before the first shot): the weapon then doesn't fire
    // at all. With Semi-Auto, only for a new press of Z, so holding it fires
    // at the weapon's own rate.
    constexpr int kShotOutCalls = 6;
    int busy_calls[4] = {};
    int busy_ammo[4] = {};
    uint16_t prev_buttons[4] = {};
    bool press_waiting[4] = {};

    int player_ammo(uint8_t* rdram, gpr p) {
        return MEM_BU(0x5F5, p) + MEM_BU(0x5F7, p) + MEM_BU(0x5F9, p); // bullet, energy, explosive
    }

    void apply_rapid_fire(uint8_t* rdram, gpr p, int port, uint16_t buttons, bool semi_auto) {
        uint16_t state = MEM_HU(kPlayerFireDelay, p);
        bool busy = state != 0 && state != kRapidFireDelay;
        // The weapons picked up in a level (Cerebral Bore, PFM Mine, Iron
        // Claw, Sentry Turret) go after one use; cut short, they could be
        // used again and again.
        uint16_t weapon = MEM_HU(kPlayerHeldWeapon, p);
        if (weapon >= kFirstPickupWeapon && weapon <= kLastPickupWeapon) {
            busy = false;
        }
        bool held = (buttons & kButtonZ) != 0;
        if (held && !(prev_buttons[port] & kButtonZ)) {
            // A press while the weapon is ready fires as it is.
            press_waiting[port] = busy;
        }
        if (!held) {
            press_waiting[port] = false;
        }
        prev_buttons[port] = buttons;

        if (!busy) {
            busy_calls[port] = 0;
            return;
        }
        int ammo = player_ammo(rdram, p);
        if (busy_calls[port]++ == 0) {
            busy_ammo[port] = ammo;
        }
        bool shot_out = ammo < busy_ammo[port] || busy_calls[port] > kShotOutCalls;
        if (!held || !shot_out || (semi_auto && !press_waiting[port])) {
            return;
        }
        press_waiting[port] = false;
        busy_calls[port] = 0;
        MEM_H(kPlayerFireDelay, p) = (int16_t)kRapidFireDelay;
        MEM_H(kPlayerFireDelay + 2, p) = (int16_t)kRapidFireDelay;
    }

    // The cheats that change the players, for each player on a controller
    // port (the bots have none).
    void apply_player_cheats(uint8_t* rdram) {
        bool health = trw::config::get_option(trw::config::Option::InfiniteHealth);
        bool moon_jump = trw::config::get_option(trw::config::Option::MoonJump);
        bool rapid_fire = trw::config::get_option(trw::config::Option::RapidFire);
        bool semi_auto = trw::config::get_option(trw::config::Option::SemiAuto);
        if (!health && !moon_jump && !rapid_fire) {
            return;
        }
        // The human players: ports 0-3 of the players in a row (0x16E8
        // apart). The list through +0x16E0 holds only the first.
        uint32_t first = (uint32_t)MEM_W(0, (gpr)kPlayerPointer);
        for (int i = 0; i < 4 && valid_player(first); i++) {
            gpr p = (gpr)(int32_t)(first + i * 0x16E8u);
            int32_t port = MEM_W(kPlayerPort, p);
            if (port == i && MEM_W(0x5D0, p) != 0) {
                uint16_t buttons = trw::controls::buttons_held(port);
                if (health && MEM_HU(kPlayerHealth, p) < kFullHealth) {
                    MEM_H(kPlayerHealth, p) = (int16_t)kFullHealth;
                }
                if (moon_jump && (buttons & kButtonL)) {
                    MEM_W(kPlayerJumpSpeed, p) = (int32_t)kMoonJumpSpeed;
                }
                if (rapid_fire) {
                    apply_rapid_fire(rdram, p, port, buttons, semi_auto);
                }
            }
        }
    }

    // Infinite Time: the clock is held where it was when the option was
    // turned on or the match started.
    void apply_infinite_time(uint8_t* rdram) {
        if (!trw::config::get_option(trw::config::Option::InfiniteTime)) {
            held_clock = 0.0f;
            return;
        }
        gpr clock_addr = (gpr)kMatchClock;
        uint32_t bits = (uint32_t)MEM_W(0, clock_addr);
        float clock;
        std::memcpy(&clock, &bits, sizeof(clock));
        if (!(clock > 0.0f)) {
            held_clock = 0.0f; // no time limit, or not set up yet
            return;
        }
        if (held_clock == 0.0f || clock > held_clock) {
            held_clock = clock; // a new match
            return;
        }
        if (clock < held_clock) {
            std::memcpy(&bits, &held_clock, sizeof(bits));
            MEM_W(0, clock_addr) = (int32_t)bits;
        }
    }
}

// From the skin change on character select (func_00420214): the number of
// skins a character has, from the profile byte at 0x800FCB57 + offset (+0x57
// plus the character's id). With All Skins on it's the full number: what the
// game sets there by 50 medals (func_00425BC0).
extern "C" uint32_t trw_skin_count(uint8_t* rdram, gpr offset, gpr count) {
    (void)rdram;
    // By character id, from profile +0x57.
    static constexpr uint8_t kFullSkins[] = {
        3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, // ids 0-10
        0, 0, 0, 0, 0, 0,                // ids 11-16
        3, 3,                            // ids 17, 18
    };
    if (!trw::config::get_option(trw::config::Option::AllSkins)) {
        return (uint32_t)count;
    }
    uint32_t byte = 0x800FCB57u + (uint32_t)offset;
    if (byte < kProfiles || byte >= kProfiles + 4 * kProfileSize) {
        return (uint32_t)count;
    }
    uint32_t index = (byte - kProfiles) % kProfileSize - 0x57;
    if (index >= sizeof(kFullSkins)) {
        return (uint32_t)count;
    }
    return kFullSkins[index] > (uint32_t)count ? kFullSkins[index] : (uint32_t)count;
}

namespace {
    bool map_node(uint8_t* rdram, uint32_t node) {
        if (node < 0x80000400u || node >= 0x80800000u || (node & 3) != 0) {
            return false;
        }
        uint16_t type = MEM_HU(0xE, (gpr)(int32_t)node);
        return type == kWidgetGroup || type == kWidgetImage;
    }

    // Shows the hidden node under the trials map's cursor, hiding the last
    // one again once the cursor has left it.
    void show_trial_cursor(uint8_t* rdram, bool front_end) {
        uint32_t map = front_end && all_trials ? (uint32_t)MEM_W(0, (gpr)kTrialMap) : 0;
        uint32_t node = map != 0 ? (uint32_t)MEM_W(kTrialMapCursorNode, (gpr)(int32_t)map) : 0;
        if (shown_node != 0 && shown_node != node) {
            if (front_end && map == shown_map && map_node(rdram, shown_node)) {
                gpr w = (gpr)(int32_t)shown_node;
                MEM_W(0x10, w) = (int32_t)((uint32_t)MEM_W(0x10, w) & ~kWidgetShown);
            }
            shown_node = 0;
        }
        if (node == 0 || node == shown_node || !map_node(rdram, node)) {
            return;
        }
        gpr w = (gpr)(int32_t)node;
        uint32_t flags = (uint32_t)MEM_W(0x10, w);
        if ((flags & kWidgetShown) == 0) {
            MEM_W(0x10, w) = (int32_t)(flags | kWidgetShown);
            shown_map = map;
            shown_node = node;
        }
    }
}

// Around the trials map's build (func_004271E8): 1 at its start, 0 at its end.
extern "C" void trw_trial_map_build(int building) {
    trial_map_building = building != 0;
}

void trw_apply_weapon_rules(uint8_t* rdram, bool in_match); // src/game/weapon_rules.cpp
void trw_set_weapon_reverb(bool on, float level); // src/game/weapon_reverb.cpp

// 60 frames a second in matches, with the Graphics tab's Framerate above
// Original (at Original the game runs as it always did). Each frame,
// func_00292818 averages the retraces the last frames took (three of the
// four counters below), never fewer than 2 (4 in one case), into
// 0x801408A8, and makes the frame step from it (0x800CD648, how far each
// frame moves things on: retraces x 15 / 60). func_00292220 then has the
// frame's buffer wait that many retraces before the next starts (its setup's
// +0x124), so a match ran at 30 frames a second at most. With the higher
// rate the wait is 1 retrace and the step comes from the same average with
// a floor of 1: at 60 frames a second the step is 0.25, and a PC that can't
// keep up takes more retraces, and the step follows, as on the console.
namespace {
    bool fps60_active = false;
}

extern "C" uint32_t trw_frame_step(uint8_t* rdram, uint32_t step_bits) {
    if (!fps60_active) {
        return step_bits;
    }
    // The game's average (func_00292818), without its floor.
    uint32_t c[4] = {
        (uint32_t)MEM_W(0, (gpr)(int32_t)0x80140C34), (uint32_t)MEM_W(0, (gpr)(int32_t)0x80140CA4),
        (uint32_t)MEM_W(0, (gpr)(int32_t)0x801199F8), (uint32_t)MEM_W(0, (gpr)(int32_t)0x801408AC),
    };
    uint32_t sum = c[0] + c[1] + c[2] + c[3];
    uint32_t most = std::max(std::max(c[0], c[1]), std::max(c[2], c[3]));
    uint32_t retraces = std::max<uint32_t>((sum - most + 2) / 3, 1);
    MEM_W(0, (gpr)(int32_t)0x801408A8) = 1; // the wait
    uint32_t rate_bits = (uint32_t)MEM_W(0, (gpr)(int32_t)0x80140C3C); // 60.0, retraces a second
    float rate;
    std::memcpy(&rate, &rate_bits, sizeof(rate));
    float step = (float)(retraces * 15) / rate;
    std::memcpy(&step_bits, &step, sizeof(step));
    return step_bits;
}

// Game frames to one of the game's own (30 a second): 2 while matches run at
// 60, for timers the game counts in frames (turokrw.us.toml).
extern "C" int trw_frames_scale(uint8_t*) {
    return fps60_active ? 2 : 1;
}

// From the frame start (trw_frame_buffers_set, src/game/widescreen.cpp).
extern "C" void trw_apply_extras(uint8_t* rdram) {
    fps60_active = trw::config::get_high_framerate() && in_match(rdram);
    all_trials = trw::config::get_option(trw::config::Option::AllTrials);
    // Menus Only: the filter in the front end (title, menus, selects) and the
    // options opened from the pause list (drawn full-screen like the front
    // end's, in its 284-pixel video mode), not in play or on the pause list
    // over it.
    bool pause_options = MEM_W(0, (gpr)kMatchPaused) != 0 && (uint32_t)MEM_W(0, (gpr)(int32_t)(0x8014B508 + 8)) == 284;
    bool filter_off = trw::config::get_screen_filter_menus_only() && in_match(rdram) && !pause_options;
    RT64::setScreenFilter(filter_off ? RT64::ScreenFilter::None : static_cast<RT64::ScreenFilter>(trw::config::get_screen_filter()));
    // Bloom only in play: the menus' gold text (the front end's and the pause
    // list's options) is bright and saturated enough to glow.
    bool playing = in_match(rdram) && MEM_W(0, (gpr)kMatchPaused) == 0;
    RT64::setBloom(playing ? static_cast<RT64::Bloom>(trw::config::get_bloom()) : RT64::Bloom::Off);
    trw_set_weapon_reverb(trw::config::get_weapon_reverb(), trw::config::get_weapon_reverb_amount());
    trw::music::set_options(trw::config::get_option(trw::config::Option::CustomMusic),
        trw::config::get_option(trw::config::Option::KeepSong), trw::config::get_custom_music_volume());

    gpr flags_addr = (gpr)kCheatFlags;
    uint32_t flags = (uint32_t)MEM_W(0, flags_addr);
    uint32_t wanted = flags;
    for (const CheatBit& cheat : kCheatBits) {
        if (trw::config::get_option(cheat.option)) {
            wanted |= cheat.bit;
        }
        else {
            wanted &= ~cheat.bit;
        }
    }
    if (wanted != flags) {
        MEM_W(0, flags_addr) = (int32_t)wanted;
    }

    trw::music::set_randomize(trw::config::get_option(trw::config::Option::RandomizeMusic));
    trw::music::set_game_state(in_match(rdram), MEM_W(0, (gpr)kMatchPaused) != 0);
    show_trial_cursor(rdram, (uint32_t)MEM_W(0, (gpr)kGameMode) == 8);
    trw::crosshair::frame_start(in_match(rdram));
    trw::menu_cursor::frame_start();
    trw_apply_weapon_rules(rdram, in_match(rdram));
    if (in_match(rdram)) {
        apply_look_spring(rdram);
        apply_player_cheats(rdram);
        apply_infinite_time(rdram);
    }
    else {
        held_clock = 0.0f;
    }
}

// Before a menu image is drawn (func_0040DB84). The panels' scanlines are a
// stack of 8-pixel strips of one image (number 0xBB, at the image widget's
// +0x2C) drawn over them.
// Field of View (Graphics tab). The camera (func_00234FCC) makes its
// perspective with guPerspectiveF (0x002B6ECC) from a vertical angle, 74.4
// degrees normally (90 across at 4:3) and narrower when zoomed. The setting,
// across a 4:3 picture, scales it in tangents, so zooming keeps working.
extern "C" void trw_camera_begin(uint8_t*) {
    trw::controls::camera_begin();
}

extern "C" uint32_t trw_field_of_view(uint8_t*, uint32_t fovy_bits) {
    float setting = trw::config::get_field_of_view();
    if (setting == 90.0f) {
        return fovy_bits;
    }
    constexpr float deg = 3.14159265f / 180.0f;
    float fovy;
    std::memcpy(&fovy, &fovy_bits, sizeof(fovy));
    float scale = std::tan(setting * 0.5f * deg); // tan(45 degrees) is 1
    fovy = 2.0f * std::atan(std::tan(fovy * 0.5f * deg) * scale) / deg;
    std::memcpy(&fovy_bits, &fovy, sizeof(fovy));
    return fovy_bits;
}

extern "C" int trw_menu_image_hidden(uint8_t* rdram, gpr widget) {
    constexpr uint32_t kScanlineImage = 0xBB;
    if (!trw::config::get_option(trw::config::Option::NoMenuScanlines)) {
        return 0;
    }
    return (uint32_t)MEM_W(0x2C, widget) == kScanlineImage ? 1 : 0;
}

// At the start of the bit test (func_002655F0(bits, n): byte bits + n / 8,
// bit n % 8), which the game uses to ask whether something is unlocked. 1 to
// answer "set" while All Levels is on, for a profile's level bits (and the
// main menu's extra modes) and the trials map's copy of them; the profile
// itself isn't changed. While the map is built from the profile it gets the
// real bits, so it's lit as the save has it and the cursor stands out.
// (Skins, +0x58, aren't read through it.) Medal bits are left alone.
extern "C" int trw_unlocked_bit(uint8_t* rdram, gpr bits, gpr n) {
    if (!all_trials || trial_map_building) {
        return 0;
    }
    int32_t index = (int32_t)n;
    if (index < 0) {
        return 0;
    }
    uint32_t byte = (uint32_t)bits + (uint32_t)(index >> 3);
    if (byte >= kProfiles && byte < kProfiles + 4 * kProfileSize) {
        uint32_t offset = (byte - kProfiles) % kProfileSize;
        return offset >= kMenuOptionsAndLevelsStart && offset < kMenuOptionsAndLevelsEnd ? 1 : 0;
    }
    uint32_t map = (uint32_t)MEM_W(0, (gpr)kTrialMap);
    if (map != 0 && byte - (map + kTrialMapLevelBits) < kTrialMapLevelBytes) {
        return 1;
    }
    return 0;
}
