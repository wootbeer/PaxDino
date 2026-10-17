// The Enhancements, Unlocks and Cheats tabs of the menu (F1; F2 opens it on
// Cheats), Screen Filter on the Graphics tab and the music options on the
// Sound tab. Options that change the game itself; the game reads them through
// the hooks in turokrw.us.toml and src/game/extras.cpp.
//
// Unlocks never touch the save: the game asks small functions whether
// something is unlocked, and while an unlock is on those answer yes. Turning
// it off shows the profile's own progress again.
#include "trw_config.h"

#include "recompui/config.h"
#include "ultramodern/config.hpp"
#include "trw_crosshair.h"
#include "trw_music.h"

namespace {
    const std::string enhancements_id = "enhancements";
    const std::string unlocks_id = "unlocks";
    const std::string cheats_id = "cheats";
    const std::string sound_id = "sound"; // recompui::config::sound::id

    bool tabs_added = false;

    const std::vector<recomp::config::ConfigOptionEnumOption> crosshair_style_options = {
        {trw::config::CrosshairStyle::Game, "Game", "Game"},
        {trw::config::CrosshairStyle::Cross, "Cross", "Cross"},
        {trw::config::CrosshairStyle::GapCross, "GapCross", "Open"},
        {trw::config::CrosshairStyle::Dot, "Dot", "Dot"},
        {trw::config::CrosshairStyle::CrossDot, "CrossDot", "Open+Dot"},
        {trw::config::CrosshairStyle::Circle, "Circle", "Ring"},
        {trw::config::CrosshairStyle::CircleDot, "CircleDot", "Ring+Dot"},
        {trw::config::CrosshairStyle::T, "T", "T"},
        {trw::config::CrosshairStyle::Custom, "Custom", "Image"},
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> crosshair_color_options = {
        {trw::config::CrosshairColor::Green, "Green", "Green"},
        {trw::config::CrosshairColor::White, "White", "White"},
        {trw::config::CrosshairColor::Red, "Red", "Red"},
        {trw::config::CrosshairColor::Yellow, "Yellow", "Yellow"},
        {trw::config::CrosshairColor::Cyan, "Cyan", "Cyan"},
        {trw::config::CrosshairColor::Magenta, "Magenta", "Magenta"},
    };

    const std::vector<recomp::config::ConfigOptionEnumOption> weapon_rule_options = {
        {trw::config::WeaponRule::All, "All", "All"},
        {trw::config::WeaponRule::Hammers, "Hammers", "Hammers"},
        {trw::config::WeaponRule::Bullet, "Bullet", "Bullet"},
        {trw::config::WeaponRule::Energy, "Energy", "Energy"},
        {trw::config::WeaponRule::Explosive, "Explosive", "Explosive"},
        {trw::config::WeaponRule::Scorpion, "Scorpion", "Scorpion"},
    };

    const std::string screen_filter_id = "screen_filter";
    const std::string screen_filter_menus_id = "screen_filter_menus_only";
    const std::vector<recomp::config::ConfigOptionEnumOption> screen_filter_options = {
        {trw::config::ScreenFilter::None, "None", "None"},
        {trw::config::ScreenFilter::CRT, "CRT", "CRT"},
        {trw::config::ScreenFilter::Scanlines, "Scanlines", "Scanlines"},
        {trw::config::ScreenFilter::LCD, "LCD", "LCD"},
    };

    const std::string bloom_id = "bloom";
    const std::string field_of_view_id = "field_of_view";
    const std::vector<recomp::config::ConfigOptionEnumOption> bloom_options = {
        {trw::config::Bloom::Off, "Off", "Off"},
        {trw::config::Bloom::Low, "Low", "Low"},
        {trw::config::Bloom::High, "High", "High"},
    };

    struct OptionRef {
        const std::string* tab;
        const char* id;
    };

    // Indexed by trw::config::Option.
    const OptionRef option_refs[] = {
        { &recompui::config::graphics::id, "no_menu_scanlines" }, // Graphics tab, with the filters
        { &unlocks_id, "all_trials" },
        { &unlocks_id, "all_characters" },
        { &unlocks_id, "all_weapons" },
        { &cheats_id, "unlimited_ammo" },
        { &unlocks_id, "extra_modes" },
        { &unlocks_id, "all_skins" },
        { &cheats_id, "infinite_health" },
        { &cheats_id, "moon_jump" },
        { &cheats_id, "rapid_fire" },
        { &cheats_id, "infinite_time" },
        { &enhancements_id, "bot_match_medals" },
        { &sound_id, "custom_music" },
        { &sound_id, "custom_music_keep_song" },
        { &sound_id, "randomize_game_music" },
        { &enhancements_id, "idle_demo" },
        { &cheats_id, "rapid_fire_semi_auto" },
    };
}

static bool graphics_options_added = false;

// Under MS Anti-Aliasing (lib-patches/RecompFrontend/0012).
void trw::config::add_graphics_options_after_msaa(recomp::config::Config& graphics) {
    graphics.add_enum_option(
        bloom_id,
        "Bloom",
        "",
        bloom_options,
        trw::config::Bloom::Low
    );
}

void trw::config::add_graphics_options_after_aspect_ratio(recomp::config::Config& graphics) {
    graphics.add_number_option(
        field_of_view_id,
        "Field of View",
        "How wide the view is in a match, in degrees across a 4:3 picture (wider screens show more at the sides). "
        "<b>90</b> is the game's own.",
        60, 120, 1, 0, false, 90
    );
}

void trw::config::add_graphics_options(recomp::config::Config& graphics) {
    graphics_options_added = true;

    graphics.add_enum_option(
        screen_filter_id,
        "Screen Filter",
        "",
        screen_filter_options,
        trw::config::ScreenFilter::CRT
    );

    graphics.add_bool_option(
        screen_filter_menus_id,
        "Menus Only",
        "",
        true
    );
    graphics.add_option_disable_dependency(screen_filter_menus_id, screen_filter_id, trw::config::ScreenFilter::None);

    // With the filters: their lines clash with the panels' own scanlines.
    // On by default, and turned on in the menu as soon as a filter is picked
    // (turning it off afterwards stays).
    graphics.add_bool_option(
        "no_menu_scanlines",
        "Clear Menu Panels",
        "Removes the scanlines from in-game menus.",
        true
    );
    graphics.add_option_change_callback(screen_filter_id,
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant prev, recomp::config::OptionChangeContext context) {
            if (context == recomp::config::OptionChangeContext::Load || value == prev) {
                return;
            }
            if (std::get<uint32_t>(value) != static_cast<uint32_t>(trw::config::ScreenFilter::None)) {
                recompui::config::get_graphics_config().update_option_value("no_menu_scanlines", true);
            }
        });
}

void trw::config::add_extra_tabs() {
    tabs_added = true;

    // The music options go on the Sound tab, under Main Volume.
    recomp::config::Config& sound = recompui::config::get_sound_config();
    sound.update_option_description(recompui::config::sound::options::main_volume, "");

    sound.add_bool_option(
        "randomize_game_music",
        "All Music",
        "Play any of the game's tracks in a match.",
        false
    );

    sound.add_bool_option(
        "custom_music",
        "Custom Music",
        "Plays your own music instead of the game's: put .mp3, .flac, .wav or .ogg files in the "
        "<b>Music</b> folder."
        "<br/><br/>"
        "Turning this on or off takes effect from the next song.",
        false
    );

    // A button under Custom Music opening its folder (RecompFrontend patch 0006).
    recompui::config::add_option_button("custom_music", "Open Music Folder", [] { trw::music::open_folder(); });

    sound.add_bool_option(
        "custom_music_keep_song",
        "Keep Song",
        "With Custom Music on, one file plays straight through menus, levels and matches until it ends, "
        "then the next one starts. Off: each new song the game starts gets a new file (within a match "
        "the file plays on until the match is over).",
        false
    );

    sound.add_number_option(
        "custom_music_volume",
        "Custom Music Volume",
        "",
        0, 100, 1, 0, true, 60
    );

    // Keep Song and the volume only matter with Custom Music on.
    std::vector<recomp::config::ConfigValueVariant> music_off = { false };
    sound.add_option_disable_dependency("custom_music_keep_song", "custom_music", music_off);
    sound.add_option_disable_dependency("custom_music_volume", "custom_music", music_off);

    sound.add_bool_option(
        "weapon_reverb",
        "Weapon Reverb",
        "",
        true
    );

    sound.add_number_option(
        "weapon_reverb_amount",
        "Weapon Reverb Amount",
        "",
        0, 100, 1, 0, true, 50
    );

    std::vector<recomp::config::ConfigValueVariant> reverb_off = { false };
    sound.add_option_disable_dependency("weapon_reverb_amount", "weapon_reverb", reverb_off);

    recomp::config::Config& enhancements = recompui::config::create_config_tab("Enhancements", enhancements_id, false);
    enhancements.add_bool_option(
        "idle_demo",
        "Demo",
        "Enable or disable title screen demo match.",
        false
    );

    enhancements.add_bool_option(
        "bot_match_medals",
        "Medals in Solo Matches",
        "Earn medals solo.",
        false
    );

    enhancements.add_enum_option(
        "weapon_rule",
        "Loadout Restrictions",
        "Limit matches to a single weapon type.",
        weapon_rule_options,
        trw::config::WeaponRule::All
    );

    // Pictures of the styles instead of words (src/main/crosshair.cpp).
    enhancements.add_enum_option(
        "crosshair_style",
        "Crosshair",
        trw::crosshair::preview_rml(),
        crosshair_style_options,
        trw::config::CrosshairStyle::Game
    );

    // A button under Crosshair picking the Image (RecompFrontend patch 0006).
    recompui::config::add_option_button("crosshair_style", "Choose Image", [] { trw::crosshair::choose_image(); });
    recompui::config::set_option_disabled_while("weapon_rule", [] { return trw::config::weapon_rule_locked(); });

    enhancements.add_enum_option(
        "crosshair_color",
        "Crosshair Color",
        "",
        crosshair_color_options,
        trw::config::CrosshairColor::Green
    );

    enhancements.add_number_option(
        "crosshair_size",
        "Crosshair Size",
        "",
        50, 200, 5, 0, true, 100
    );

    enhancements.add_number_option(
        "crosshair_opacity",
        "Crosshair Opacity",
        "",
        10, 100, 5, 0, true, 100
    );

    // Greyed out where they do nothing: the colour with the game's own or a
    // picture, the size and opacity with the game's own.
    enhancements.add_option_disable_dependency("crosshair_color", "crosshair_style",
        trw::config::CrosshairStyle::Game, trw::config::CrosshairStyle::Custom);
    enhancements.add_option_disable_dependency("crosshair_size", "crosshair_style", trw::config::CrosshairStyle::Game);
    enhancements.add_option_disable_dependency("crosshair_opacity", "crosshair_style", trw::config::CrosshairStyle::Game);

    recomp::config::Config& unlocks = recompui::config::create_config_tab("Unlocks", unlocks_id, false);
    unlocks.add_bool_option(
        "all_trials",
        "All Levels",
        "Every level of the 1 Player Trials, Time Trial and Frag Fest is open, and so are the extra modes."
        "<br/><br/>"
        "Your progress isn't changed.",
        false
    );

    unlocks.add_bool_option(
        "all_skins",
        "All Skins",
        "Every character's skins can be picked on character select (C-Up), without the medals they need."
        "<br/><br/>"
        "Your progress isn't changed.",
        false
    );

    // These three are the game's own cheats (bits of 0x80140208,
    // src/game/extras.cpp), kept with the unlocks.
    unlocks.add_bool_option(
        "all_characters",
        "All Characters",
        "Every character can be picked."
        "<br/><br/>"
        "Your progress isn't changed.",
        false
    );

    unlocks.add_bool_option(
        "all_weapons",
        "All Weapons",
        "Every weapon can be picked for a loadout.",
        false
    );

    unlocks.add_bool_option(
        "extra_modes",
        "Extra Modes",
        "Adds Time Trial and Frag Fest to the main menu (the next time it opens).",
        false
    );

    // The game's own Cheats menu (Z on the main menu, unlocked by medals) is
    // replaced by this tab and Unlocks.
    recomp::config::Config& cheats = recompui::config::create_config_tab("Cheats", cheats_id, false);
    cheats.add_bool_option(
        "unlimited_ammo",
        "Unlimited Ammo",
        "Weapons never run out of ammo.",
        false
    );

    // Not in the game's Cheats menu: from the community GameShark codes
    // (docs/RAM_NOTES.md), applied by src/game/extras.cpp.
    cheats.add_bool_option(
        "infinite_health",
        "Infinite Health",
        "Your health never drops below 100.",
        false
    );

    cheats.add_bool_option(
        "moon_jump",
        "Moon Jump",
        "Hold <b>L</b> to float up into the air.",
        false
    );

    cheats.add_bool_option(
        "rapid_fire",
        "Rapid Fire",
        "Hold <b>Z</b> (fire) for the weapon to fire as fast as it can.",
        false
    );

    cheats.add_bool_option(
        "rapid_fire_semi_auto",
        "Semi-Auto",
        "Rapid Fire only on each new press of <b>Z</b>: the weapon fires as fast as you can press it. "
        "Holding <b>Z</b> fires at the normal rate, so ammo lasts longer.",
        false
    );
    std::vector<recomp::config::ConfigValueVariant> rapid_fire_off = { false };
    cheats.add_option_disable_dependency("rapid_fire_semi_auto", "rapid_fire", rapid_fire_off);

    cheats.add_bool_option(
        "infinite_time",
        "Infinite Time",
        "The match clock stops counting down, so timed matches only end on their frag or score limit.",
        false
    );

    // F2 opens the menu on this tab (lib-patches/RecompFrontend/0004).
    recompui::config::set_hotkey_tab(cheats_id);
}

bool trw::config::get_option(Option option) {
    if (!tabs_added) {
        return false;
    }
    const OptionRef& ref = option_refs[static_cast<int>(option)];
    // Looked up by id on each read (see game_config.cpp).
    return std::get<bool>(recompui::config::get_config(*ref.tab).get_option_value(ref.id));
}

trw::config::ScreenFilter trw::config::get_screen_filter() {
    if (!graphics_options_added) {
        return ScreenFilter::None;
    }
    // Looked up on each read: the Config& create_graphics_tab() returns moves
    // when later tabs are created.
    return static_cast<ScreenFilter>(std::get<uint32_t>(recompui::config::get_graphics_config().get_option_value(screen_filter_id)));
}

bool trw::config::get_screen_filter_menus_only() {
    if (!graphics_options_added) {
        return false;
    }
    return std::get<bool>(recompui::config::get_graphics_config().get_option_value(screen_filter_menus_id));
}

trw::config::Bloom trw::config::get_bloom() {
    if (!graphics_options_added) {
        return Bloom::Off;
    }
    return static_cast<Bloom>(std::get<uint32_t>(recompui::config::get_graphics_config().get_option_value(bloom_id)));
}

bool trw::config::get_high_framerate() {
    if (!graphics_options_added) {
        return false;
    }
    using ultramodern::renderer::RefreshRate;
    auto& graphics = recompui::config::get_graphics_config();
    auto rate = static_cast<RefreshRate>(std::get<uint32_t>(graphics.get_option_value(recompui::config::graphics::options::rr_option)));
    if (rate == RefreshRate::Manual) {
        return std::get<double>(graphics.get_option_value(recompui::config::graphics::options::rr_manual_value)) >= 60.0;
    }
    return rate == RefreshRate::Display;
}

float trw::config::get_field_of_view() {
    if (!graphics_options_added) {
        return 90.0f;
    }
    return static_cast<float>(std::get<double>(recompui::config::get_graphics_config().get_option_value(field_of_view_id)));
}

bool trw::config::get_weapon_reverb() {
    if (!tabs_added) {
        return false;
    }
    return std::get<bool>(recompui::config::get_config(sound_id).get_option_value("weapon_reverb"));
}

float trw::config::get_weapon_reverb_amount() {
    if (!tabs_added) {
        return 0.0f;
    }
    return static_cast<float>(std::get<double>(recompui::config::get_config(sound_id).get_option_value("weapon_reverb_amount")) / 100.0);
}

float trw::config::get_custom_music_volume() {
    if (!tabs_added) {
        return 0.6f;
    }
    return static_cast<float>(std::get<double>(recompui::config::get_config(sound_id).get_option_value("custom_music_volume")) / 100.0);
}

trw::config::WeaponRule trw::config::get_weapon_rule() {
    if (!tabs_added) {
        return WeaponRule::All;
    }
    return static_cast<WeaponRule>(std::get<uint32_t>(recompui::config::get_config(enhancements_id).get_option_value("weapon_rule")));
}

trw::config::CrosshairSettings trw::config::get_crosshair() {
    if (!tabs_added) {
        return { CrosshairStyle::Game, CrosshairColor::Green, 1.0f, 1.0f };
    }
    const recomp::config::Config& enhancements = recompui::config::get_config(enhancements_id);
    return {
        static_cast<CrosshairStyle>(std::get<uint32_t>(enhancements.get_option_value("crosshair_style"))),
        static_cast<CrosshairColor>(std::get<uint32_t>(enhancements.get_option_value("crosshair_color"))),
        static_cast<float>(std::get<double>(enhancements.get_option_value("crosshair_size")) / 100.0),
        static_cast<float>(std::get<double>(enhancements.get_option_value("crosshair_opacity")) / 100.0),
    };
}

extern "C" int trw_option(int option) {
    return trw::config::get_option(static_cast<trw::config::Option>(option)) ? 1 : 0;
}
