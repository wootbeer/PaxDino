#ifndef __TRW_CONFIG_H__
#define __TRW_CONFIG_H__

#include <cstdint>

#include "librecomp/config.hpp"

// Game-specific options, shown on the General tab (src/main/game_config.cpp).
namespace trw::config {
    // What's plugged into a controller's accessory slot. Like on the N64, one
    // controller holds one pak at a time.
    enum class Accessory : uint32_t {
        ControllerPak, // saves (not wired up yet: PROGRESS.md)
        RumblePak,
        None,
    };

    // Adds this game's options to the General tab. Call between
    // recompui::config::create_general_tab() and recompui::config::finalize().
    void add_general_options(recomp::config::Config& general);

    Accessory get_accessory(int port);
    bool get_local_multiplayer();
    // Local Multiplayer's players, kept across launches (src/main/game_config.cpp).
    void save_player_assignment();
    // True once restored (or there's nothing to restore).
    bool restore_player_assignment();
    bool get_invert_mouse_y();
    // Mouse in Menus (General): the cursor in the game's menus
    // (include/trw_menu_cursor.h).
    bool get_menu_cursor();
    // Look Spring Override (Controls tab), for the player on `port` (0-3):
    // off, the game's own look spring setting; on, no look spring, and mouse
    // look's pitch kept (src/game/extras.cpp, src/main/fps_controls.cpp).
    bool get_look_spring_override(int port);

    // The Enhancements, Unlocks and Cheats tabs (src/main/extras_config.cpp).
    // Call after the built-in tabs, before recompui::config::finalize().
    void add_extra_tabs();

    // The on/off options of those tabs. The numbers are what hooks in
    // turokrw.us.toml would pass to trw_option(); keep them in step.
    enum class Option : int {
        NoMenuScanlines = 0,
        AllTrials = 1,
        AllCharacters = 2,
        AllWeapons = 3,
        UnlimitedAmmo = 4,
        ExtraModes = 5,
        AllSkins = 6,
        InfiniteHealth = 7,
        MoonJump = 8,
        RapidFire = 9,
        InfiniteTime = 10,
        BotMatchMedals = 11,
        CustomMusic = 12,
        KeepSong = 13,
        RandomizeMusic = 14,
        IdleDemo = 15,
        SemiAuto = 16,
    };
    bool get_option(Option option);

    // Adds Screen Filter to the Graphics tab (src/main/extras_config.cpp).
    // Call right after recompui::config::create_graphics_tab().
    void add_graphics_options(recomp::config::Config& graphics);
    // Bloom, under MS Anti-Aliasing: pass to
    // recompui::config::set_graphics_options_after_msaa() before
    // create_graphics_tab().
    void add_graphics_options_after_msaa(recomp::config::Config& graphics);
    // Field of View, under Aspect Ratio: pass to
    // recompui::config::set_graphics_options_after_aspect_ratio() before
    // create_graphics_tab().
    void add_graphics_options_after_aspect_ratio(recomp::config::Config& graphics);
    // Graphics tab: whether the Framerate is above the original (Display, or
    // Manual at 60 or more): matches then run at 60 frames a second.
    bool get_high_framerate();
    // Graphics tab: the field of view in matches, in degrees across a 4:3
    // picture; 90 is the game's own.
    float get_field_of_view();

    // Graphics tab: the filter over the final picture (RT64::ScreenFilter,
    // lib-patches/rt64/0005).
    enum class ScreenFilter : uint32_t {
        None = 0,
        CRT = 1,
        Scanlines = 2,
        LCD = 3,
    };
    ScreenFilter get_screen_filter();
    // Graphics tab: the filter only in the front end's menus, not in matches.
    bool get_screen_filter_menus_only();

    // Graphics tab: the glow on bright coloured things (RT64::Bloom,
    // lib-patches/rt64/0006).
    enum class Bloom : uint32_t {
        Off = 0,
        Low = 1,
        High = 2,
    };
    Bloom get_bloom();

    // Enhancements tab: the Custom Music volume, 0 to 1.
    float get_custom_music_volume();

    // Sound tab: Weapon Reverb (src/game/weapon_reverb.cpp), and its amount,
    // 0 to 1.
    bool get_weapon_reverb();
    float get_weapon_reverb_amount();

    // Enhancements tab: Crosshair (src/main/crosshair.cpp).
    enum class CrosshairStyle : uint32_t {
        Game,      // the game's own
        Cross,
        GapCross,
        Dot,
        CrossDot,
        Circle,
        CircleDot,
        T,
        Custom,    // the picture picked with Choose Image
    };
    enum class CrosshairColor : uint32_t {
        Green,
        White,
        Red,
        Yellow,
        Cyan,
        Magenta,
    };
    struct CrosshairSettings {
        CrosshairStyle style;
        CrosshairColor color;
        float size;    // 0.5 to 2, of the game's crosshair
        float opacity; // 0.1 to 1
    };
    CrosshairSettings get_crosshair();

    // Enhancements tab: Weapons (src/game/weapon_rules.cpp).
    enum class WeaponRule : uint32_t {
        All,
        Hammers,
        Bullet,
        Energy,
        Explosive,
        Scorpion,
    };
    WeaponRule get_weapon_rule();
    // True during a multiplayer match (src/game/weapon_rules.cpp): the option
    // is greyed out then, as the match keeps the rule it started with.
    bool weapon_rule_locked();
}

// For the hooks in the recompiled code: 1 while the option is on.
extern "C" int trw_option(int option);

#endif
