// Game-specific options on the General tab: which pak each controller holds,
// and local multiplayer. See include/trw_config.h.
#include "trw_config.h"

#include <string>
#include <vector>

#include "recompinput/players.h"
#include "recompui/config.h"
#include "recompui/recompui.h"

namespace {
    using Accessory = trw::config::Accessory;

    const std::string p1_accessory_id = "p1_accessory";
    const std::string other_accessory_id = "other_accessory";
    const std::string local_multiplayer_id = "local_multiplayer";
    const std::string invert_mouse_y_id = "invert_mouse_y";
    const std::string menu_cursor_id = "menu_cursor";
    const std::string look_spring_override_id = "look_spring_override";
    const std::string player_assignment_id = "player_assignment";

    const std::vector<recomp::config::ConfigOptionEnumOption> accessory_options = {
        {Accessory::ControllerPak, "ControllerPak", "Controller Pak"},
        {Accessory::RumblePak, "RumblePak", "Rumble Pak"},
        {Accessory::None, "None", "None"},
    };

    // Looked up by id on each read: the Config& create_general_tab() returns
    // lives in a container that moves when later tabs are created, so a
    // pointer kept from it dangles.
    bool options_added = false;
}

void trw::config::add_general_options(recomp::config::Config& general) {
    options_added = true;

    general.add_enum_option(
        p1_accessory_id,
        "Player 1 Accessory",
        "The pak plugged into player 1's controller.",
        accessory_options,
        Accessory::ControllerPak
    );

    general.add_enum_option(
        other_accessory_id,
        "Players 2-4 Accessory",
        "The pak plugged into the other players' controllers in local multiplayer. "
        "Each player's Controller Pak is a separate .mpk file.",
        accessory_options,
        Accessory::RumblePak
    );

    general.add_bool_option(
        local_multiplayer_id,
        "Local Multiplayer",
        "Lets up to 4 players use their own controllers."
        "<br/><br/>"
        "<b>Restart the game after changing this.</b> Then open the Controls tab and use "
        "<b>Assign players</b> to choose each player's controller or keyboard.",
        false
    );

    // Turning it on or off only takes effect at the next launch (main.cpp
    // reads it once): say so as it's changed. Not when it's put back to what
    // the game started with.
    general.add_option_change_callback(local_multiplayer_id,
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant prev, recomp::config::OptionChangeContext context) {
            if (context == recomp::config::OptionChangeContext::Load || value == prev) {
                return;
            }
            bool on = std::get<bool>(value);
            if (on == !recompinput::players::is_single_player_mode()) {
                return;
            }
            recompui::open_info_prompt(
                "Restart Required",
                on ? "Close and reopen the game to turn on Local Multiplayer. "
                     "Then open the Controls tab and use Assign players to choose each player's controller or keyboard."
                   : "Close and reopen the game to turn off Local Multiplayer.",
                "OK",
                []() {},
                recompui::ButtonStyle::Primary
            );
        });

    general.add_bool_option(
        invert_mouse_y_id,
        "Invert Mouse Y",
        "Moving the mouse forward looks down instead of up.",
        false
    );

    general.add_bool_option(
        menu_cursor_id,
        "Mouse in Menus",
        "",
        true
    );

    // Per player, each on its own player's page of the Controls tab (players
    // 2-4 with Local Multiplayer on); player 1's keeps the id the single
    // option had.
    general.add_bool_option(look_spring_override_id, "Look Spring Override", "", true);
    for (int port = 1; port < 4; port++) {
        general.add_bool_option(look_spring_override_id + "_p" + std::to_string(port + 1),
            "Look Spring Override", "", true);
        general.add_option_hidden_dependency(look_spring_override_id + "_p" + std::to_string(port + 1), local_multiplayer_id, false);
    }

    // The players last assigned with Local Multiplayer on, restored at the
    // next launch (trw::config::restore_player_assignment).
    general.add_string_option(player_assignment_id, "Player Assignment", "", "", true);

    // The launcher's Preset entry (src/main/presets.cpp): the one picked last.
    general.add_enum_option(
        "preset",
        "Preset",
        "",
        std::vector<recomp::config::ConfigOptionEnumOption>{ { 0u, "Classic", "Classic" }, { 1u, "Modern", "Modern" } },
        1u,
        true
    );

    general.update_option_description(recompui::config::general::options::mouse_sensitivity,
        "How fast the mouse turns and aims. <b>Zero turns mouse aiming off</b> and frees the cursor."
        "<br/><br/>"
        "The mouse locks to the window while the game runs; press <b>F1</b> for this menu.");

    // Shown on the Controls tab, above the mappings, instead
    // (lib-patches/RecompFrontend/0007).
    namespace general_tab = recompui::config::general;
    recompui::config::show_option_on_controls_tab(general_tab::id, menu_cursor_id);
    recompui::config::show_option_on_controls_tab(general_tab::id, general_tab::options::mouse_sensitivity);
    recompui::config::show_option_on_controls_tab(general_tab::id, general_tab::options::joystick_deadzone);
    recompui::config::show_option_on_controls_tab(general_tab::id, invert_mouse_y_id);
    recompui::config::show_option_on_controls_tab(general_tab::id, look_spring_override_id, 0);
    for (int port = 1; port < 4; port++) {
        recompui::config::show_option_on_controls_tab(general_tab::id, look_spring_override_id + "_p" + std::to_string(port + 1), port);
    }
    recompui::config::show_option_on_controls_tab(general_tab::id, general_tab::options::background_input_mode);
}

bool trw::config::get_invert_mouse_y() {
    if (!options_added) {
        return false;
    }
    return std::get<bool>(recompui::config::get_general_config().get_option_value(invert_mouse_y_id));
}

bool trw::config::get_look_spring_override(int port) {
    if (!options_added || port < 0 || port > 3) {
        return false;
    }
    std::string id = port == 0 ? look_spring_override_id : look_spring_override_id + "_p" + std::to_string(port + 1);
    return std::get<bool>(recompui::config::get_general_config().get_option_value(id));
}

bool trw::config::get_menu_cursor() {
    if (!options_added) {
        return true;
    }
    return std::get<bool>(recompui::config::get_general_config().get_option_value(menu_cursor_id));
}

trw::config::Accessory trw::config::get_accessory(int port) {
    if (!options_added) {
        return Accessory::None;
    }
    const std::string& id = port == 0 ? p1_accessory_id : other_accessory_id;
    return static_cast<Accessory>(std::get<uint32_t>(recompui::config::get_general_config().get_option_value(id)));
}

// Local Multiplayer's player assignment, kept across launches: saved when
// players are assigned in the menu, and put back while the launcher is up
// once every saved controller is connected (until then, or if the player
// assigns them again, nothing is changed).
void trw::config::save_player_assignment() {
    std::string text;
    for (const std::string& entry : recompinput::playerassignment::describe()) {
        text += (text.empty() ? "" : ";") + entry;
    }
    recomp::config::Config& general = recompui::config::get_general_config();
    general.update_option_value(player_assignment_id, text);
    general.save_config();
}

bool trw::config::restore_player_assignment() {
    if (!options_added || recompinput::players::get_number_of_assigned_players() > 0 ||
        recompinput::playerassignment::is_active()) {
        return true;
    }
    std::string text = std::get<std::string>(recompui::config::get_general_config().get_option_value(player_assignment_id));
    if (text.empty()) {
        return true;
    }
    std::vector<std::string> entries;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(';', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        entries.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return recompinput::playerassignment::restore(entries);
}

bool trw::config::get_local_multiplayer() {
    if (!options_added) {
        return false;
    }
    return std::get<bool>(recompui::config::get_general_config().get_option_value(local_multiplayer_id));
}
