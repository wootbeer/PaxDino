// Netplay's shared settings (include/trw_config.h): the options that change
// what the game does are the host's for everyone in the session, so the two
// games stay the same; the field of view and the aspect ratio too (the field
// of view is in the game's camera, widescreen adds to the game's display
// lists). Settings that change the game's memory but aren't worth the host
// choosing are fixed for the session (here and in extras_config.cpp): Clear Menu Panels
// on, Custom Music and Weapon Reverb off, the game's own crosshair. Picture
// and sound options (the screen filters, bloom, the volumes...) stay each
// player's own, and so do the controls: the bindings, mouse sensitivity and
// invert apply before the input is sent, and Look Spring Override goes per
// player (src/main/netplay.cpp, each its own on its port).
#include <algorithm>
#include <atomic>
#include <iterator>
#include <string>

#include "trw_config.h"
#include "recompui/config.h"
#include "ultramodern/config.hpp"

namespace {
    using trw::config::Option;

    // The options shared, with their names on the tabs.
    struct SharedOption {
        Option option;
        const char* name;
    };
    constexpr SharedOption kShared[] = {
        { Option::AllTrials, "All Levels" },
        { Option::AllCharacters, "All Characters" },
        { Option::AllWeapons, "All Weapons" },
        { Option::ExtraModes, "Extra Modes" },
        { Option::AllSkins, "All Skins" },
        { Option::UnlimitedAmmo, "Unlimited Ammo" },
        { Option::InfiniteHealth, "Infinite Health" },
        { Option::MoonJump, "Moon Jump" },
        { Option::RapidFire, "Rapid Fire" },
        { Option::SemiAuto, "Semi-Auto" },
        { Option::InfiniteTime, "Infinite Time" },
        { Option::RandomizeMusic, "All Music" },
        { Option::IdleDemo, "Demo" },
    };

    constexpr const char* kWeaponRules[] = { "All", "Hammers", "Bullet", "Energy", "Explosive", "Scorpion" };

    // Off for the session: a file playing in place of the game's song sets
    // the game's music player's volume, in the game's memory, as the file
    // loads (in real time). Medals in Solo Matches: there are none online.
    constexpr Option kOffOnline[] = { Option::CustomMusic, Option::KeepSong, Option::BotMatchMedals };

    trw::config::SharedSettings shared{};
    std::atomic<bool> in_use{ false };
}

trw::config::SharedSettings trw::config::get_shared_settings() {
    SharedSettings settings{};
    for (const SharedOption& entry : kShared) {
        if (get_option(entry.option)) {
            settings.options |= 1u << static_cast<int>(entry.option);
        }
    }
    settings.weapon_rule = static_cast<uint32_t>(get_weapon_rule());
    settings.field_of_view = get_field_of_view();
    // This PC's own choice, from the settings (the runtime's may be the
    // session's override).
    settings.widescreen = std::get<uint32_t>(recompui::config::get_graphics_config().get_option_value(
        recompui::config::graphics::options::ar_option)) == static_cast<uint32_t>(ultramodern::renderer::AspectRatio::Expand) ? 1 : 0;
    for (int port = 0; port < 4; port++) {
        if (get_look_spring_override(port)) {
            settings.look_spring |= 1u << port;
        }
    }
    return settings;
}

void trw::config::use_shared_settings(const SharedSettings& settings) {
    shared = settings;
    // Options not shared keep this PC's own (get_option reads them while
    // in_use is false, so take them first).
    for (int option = 0; option < 32; option++) {
        bool is_shared = false;
        for (const SharedOption& entry : kShared) {
            is_shared |= static_cast<int>(entry.option) == option;
        }
        bool off_online = false;
        for (Option off : kOffOnline) {
            off_online |= static_cast<int>(off) == option;
        }
        if (!is_shared && !off_online && option <= static_cast<int>(Option::SemiAuto) && get_option(static_cast<Option>(option))) {
            shared.options |= 1u << option;
        }
        else if (!is_shared) {
            shared.options &= ~(1u << option);
        }
    }
    in_use = true;
    // Widescreen or not, the host's: the widescreen fixes add to the game's
    // display lists, which moves the game's own pointer into them.
    ultramodern::renderer::set_aspect_ratio_override(shared.widescreen != 0 ? ultramodern::renderer::AspectRatio::Expand
                                                                            : ultramodern::renderer::AspectRatio::Original);
}

const trw::config::SharedSettings* trw::config::shared_settings_in_use() {
    return in_use ? &shared : nullptr;
}

std::string trw::config::describe_shared_settings(const SharedSettings& settings) {
    std::string text;
    for (const SharedOption& entry : kShared) {
        if ((settings.options >> static_cast<int>(entry.option)) & 1) {
            text += entry.name;
            text += "\n";
        }
    }
    if (settings.weapon_rule != 0 && settings.weapon_rule < std::size(kWeaponRules)) {
        text += "Loadout Restrictions: ";
        text += kWeaponRules[settings.weapon_rule];
        text += "\n";
    }
    text += settings.widescreen != 0 ? "Aspect Ratio: Widescreen\n" : "Aspect Ratio: Original (4:3)\n";
    if (settings.load_profiles != 0) {
        text += "Load Profiles: On\n";
    }
    if (settings.field_of_view != 90.0f) {
        text += "Field of View: " + std::to_string((int)(settings.field_of_view + 0.5f)) + "\n";
    }
    return text;
}
