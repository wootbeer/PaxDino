// The command line (include/trw_command_line.h). Settings are the menu's,
// by their ids in the settings files (general.json, graphics.json...), with
// dashes or underscores, plus plainer names for the graphics ones; --help
// lists them from the settings themselves, so new ones are there too.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <variant>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "trw_command_line.h"
#include "recompui/config.h"
#include "json/json.hpp"

namespace {
    using recomp::config::Config;
    using recomp::config::ConfigOption;
    using recomp::config::ConfigOptionType;
    using recomp::config::ConfigValueVariant;

    trw::cli::Options parsed;

    // The settings' tabs, in the menu's order.
    const char* const kConfigIds[] = { "general", "graphics", "sound", "enhancements", "unlocks", "cheats" };

    // Plainer names for the graphics settings recompui names by their fields.
    const std::pair<const char*, const char*> kAliases[] = {
        { "resolution", "res_option" },
        { "downsampling", "ds_option" },
        { "aspect_ratio", "ar_option" },
        { "window_mode", "wm_option" },
        { "framerate", "rr_option" },
        { "framerate_manual", "rr_manual_value" },
        { "msaa", "msaa_option" },
        { "hud_placement", "hr_option" },
        { "graphics_api", "api_option" },
        { "high_precision_framebuffer", "hpfb_option" },
    };

    std::string normalized(std::string name) {
        for (char& c : name) {
            c = c == '-' ? '_' : (char)std::tolower((unsigned char)c);
        }
        for (const auto& [alias, id] : kAliases) {
            if (name == alias) {
                return id;
            }
        }
        return name;
    }

    std::string shown_name(const std::string& id) {
        for (const auto& [alias, option_id] : kAliases) {
            if (id == option_id) {
                std::string name = alias;
                std::replace(name.begin(), name.end(), '_', '-');
                return name;
            }
        }
        std::string name = id;
        std::replace(name.begin(), name.end(), '_', '-');
        return name;
    }

    Config* config_for(const char* id) {
        try {
            return &recompui::config::get_config(id);
        }
        catch (const std::exception&) {
            return nullptr;
        }
    }

    // A setting the command line can set: one in a tab, not one kept for
    // the program's own use (the player assignment, the preset picked).
    const ConfigOption* find_option(const std::string& id, Config*& config) {
        for (const char* config_id : kConfigIds) {
            Config* c = config_for(config_id);
            if (c != nullptr && c->has_option(id)) {
                const ConfigOption& option = c->get_option(id);
                if (option.hidden) {
                    return nullptr;
                }
                config = c;
                return &option;
            }
        }
        return nullptr;
    }

    std::string lower(std::string text) {
        for (char& c : text) {
            c = (char)std::tolower((unsigned char)c);
        }
        return text;
    }

    // A choice as typed: lower case, dashes for spaces.
    std::string typed(const std::string& text) {
        std::string t = lower(text);
        std::replace(t.begin(), t.end(), ' ', '-');
        return t;
    }

    // The words a choice is listed as: the menu's names when they tell the
    // choices apart, else the settings file's keys.
    bool names_unique(const recomp::config::ConfigOptionEnum& choices) {
        for (size_t i = 0; i < choices.options.size(); i++) {
            if (typed(choices.options[i].name).empty()) {
                return false;
            }
            for (size_t j = 0; j < i; j++) {
                if (typed(choices.options[i].name) == typed(choices.options[j].name)) {
                    return false;
                }
            }
        }
        return true;
    }

    // The value as the option takes it, or monostate if it isn't one.
    ConfigValueVariant parse_value(const ConfigOption& option, const std::string& text) {
        switch (option.type) {
            case ConfigOptionType::Bool: {
                std::string t = lower(text);
                if (t == "1" || t == "on" || t == "true" || t == "yes") {
                    return true;
                }
                if (t == "0" || t == "off" || t == "false" || t == "no") {
                    return false;
                }
                return std::monostate{};
            }
            case ConfigOptionType::Enum: {
                const auto& choices = std::get<recomp::config::ConfigOptionEnum>(option.variant);
                std::string t = typed(text);
                for (const auto& choice : choices.options) {
                    if (t == typed(choice.key) || (names_unique(choices) && t == typed(choice.name))) {
                        return choice.value;
                    }
                }
                return std::monostate{};
            }
            case ConfigOptionType::Number: {
                const auto& number = std::get<recomp::config::ConfigOptionNumber>(option.variant);
                std::string t = text;
                if (!t.empty() && t.back() == '%') {
                    t.pop_back();
                }
                char* end = nullptr;
                double value = std::strtod(t.c_str(), &end);
                if (t.empty() || end == nullptr || *end != '\0' || value < number.min || value > number.max) {
                    return std::monostate{};
                }
                return value;
            }
            case ConfigOptionType::String:
                return text;
            default:
                return std::monostate{};
        }
    }

    // A value as the settings file has it (Config::get_storage_json's rules;
    // Downsampling, which recompui writes as its number).
    nlohmann::json value_json(const ConfigOption& option, const ConfigValueVariant& value) {
        if (option.id == "ds_option" && std::holds_alternative<uint32_t>(value)) {
            return nlohmann::json(std::get<uint32_t>(value));
        }
        switch (option.type) {
            case ConfigOptionType::Enum: {
                const auto& choices = std::get<recomp::config::ConfigOptionEnum>(option.variant);
                auto found = choices.find_option_from_value(std::get<uint32_t>(value));
                return found != choices.options.end() ? nlohmann::json(found->key) : nlohmann::json();
            }
            case ConfigOptionType::Number:
                return std::get<double>(value);
            case ConfigOptionType::String:
                return std::get<std::string>(value);
            case ConfigOptionType::Bool:
                return std::get<bool>(value);
            default:
                return nlohmann::json();
        }
    }

    std::string values_text(const ConfigOption& option) {
        switch (option.type) {
            case ConfigOptionType::Bool:
                return "0|1";
            case ConfigOptionType::Enum: {
                const auto& choices = std::get<recomp::config::ConfigOptionEnum>(option.variant);
                bool names = names_unique(choices);
                std::string text;
                for (const auto& choice : choices.options) {
                    text += (text.empty() ? "" : "|") + typed(names ? choice.name : choice.key);
                }
                return text;
            }
            case ConfigOptionType::Number: {
                const auto& number = std::get<recomp::config::ConfigOptionNumber>(option.variant);
                char text[64];
                std::snprintf(text, sizeof(text), "%g-%g", number.min, number.max);
                return text;
            }
            case ConfigOptionType::String:
                return "TEXT";
            default:
                return "";
        }
    }
}

bool trw::cli::parse(int argc, char** argv, std::string& error) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&](std::string& into) {
            if (i + 1 >= argc) {
                error = arg + " needs a value";
                return false;
            }
            into = argv[++i];
            return true;
        };
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            parsed.help = true;
        }
        else if (arg == "--boot") {
            parsed.boot = true;
        }
        else if (arg == "--netplay") {
            parsed.netplay = true;
        }
        else if (arg == "--host") {
            parsed.host = true;
        }
        else if (arg == "--join") {
            if (!next(parsed.join)) {
                return false;
            }
        }
        else if (arg == "--rom") {
            if (!next(parsed.rom)) {
                return false;
            }
        }
        else if (arg.rfind("--", 0) == 0 && arg.size() > 2) {
            // A setting: --name value, --name=value, or --name alone (on).
            std::string name = arg.substr(2), value;
            size_t eq = name.find('=');
            if (eq != std::string::npos) {
                value = name.substr(eq + 1);
                name.resize(eq);
            }
            else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
                value = argv[++i];
            }
            else {
                value = "1";
            }
            parsed.settings.emplace_back(name, value);
        }
        else {
            error = "unknown argument: " + arg;
            return false;
        }
    }
    return true;
}

const trw::cli::Options& trw::cli::options() {
    return parsed;
}

bool trw::cli::apply_settings(std::string& error) {
    struct Override {
        Config* config;
        const ConfigOption* option;
        ConfigValueVariant value;
    };
    std::map<std::string, Override> overrides; // the last one given wins
    for (const auto& [name, text] : parsed.settings) {
        Config* config = nullptr;
        const ConfigOption* option = find_option(normalized(name), config);
        if (option == nullptr) {
            error = "unknown setting: --" + name;
            return false;
        }
        ConfigValueVariant value = parse_value(*option, text);
        if (std::holds_alternative<std::monostate>(value)) {
            error = "--" + name + " can't be " + text + " (it takes " + values_text(*option) + ")";
            return false;
        }
        overrides[option->id] = Override{ config, option, value };
    }
    bool graphics = false;
    for (auto& [id, o] : overrides) {
        // What the file has, to write back instead of this run's value.
        nlohmann::json file = o.config->get_json_config();
        nlohmann::json original = file.contains(id) ? file[id] : value_json(*o.option, o.config->get_option_value(id));
        o.config->set_option_value(id, o.value);
        if (o.config->requires_confirmation) {
            o.config->apply_option_value(id);
        }
        const ConfigOption* option = o.option;
        ConfigValueVariant run_value = o.value;
        o.config->on_json_serialize_option(id, [option, run_value, original](const ConfigValueVariant& value) {
            return value == run_value ? original : value_json(*option, value);
        });
        graphics |= o.config == &recompui::config::get_graphics_config();
        fprintf(stderr, "[TRW] setting for this run: %s = %s\n", id.c_str(), value_json(*o.option, o.value).dump().c_str());
    }
    if (graphics) {
        recompui::config::graphics::apply_config();
    }
    return true;
}

std::string trw::cli::help_text(const std::string& title) {
    std::string text = title + "\n\n"
        "  --boot            play at once, without the launcher\n"
        "  --rom FILE        use FILE as the game's ROM (kept, like Load ROM)\n"
        "  --netplay         open the Netplay screen\n"
        "  --host            open the Netplay screen and host\n"
        "  --join CODE       open the Netplay screen and join CODE (a join code or address)\n"
        "  --help            this list\n"
        "\n"
        "Any setting, for this run only (not saved): --NAME VALUE or --NAME=VALUE\n";
    for (const char* config_id : kConfigIds) {
        Config* config = config_for(config_id);
        if (config == nullptr) {
            continue;
        }
        std::string section;
        for (const ConfigOption& option : config->get_config_schema().options) {
            if (option.hidden || option.type == ConfigOptionType::None) {
                continue;
            }
            section += "  --" + shown_name(option.id) + " " + values_text(option) +
                (option.name.empty() ? "" : "   (" + option.name + ")") + "\n";
        }
        if (!section.empty()) {
            std::string tab = config_id;
            tab[0] = (char)std::toupper((unsigned char)tab[0]);
            text += tab + ":\n" + section;
        }
    }
    text += "\nExample: PaxDino --boot --resolution auto --msaa 4x --unlimited-ammo 1\n"
        "Choices can also be given as the settings file has them.\n";
    return text;
}

void trw::cli::tell(const std::string& title, const std::string& text) {
    fprintf(stderr, "%s\n", text.c_str());
    fflush(stderr);
#ifdef _WIN32
    // A release build has no console of its own: the terminal it was started
    // from, if any, else a message box.
    if (GetConsoleWindow() != nullptr) {
        return; // a build with its own console: stderr is it
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        if (FILE* console = std::fopen("CONOUT$", "w")) {
            std::fprintf(console, "\n%s\n", text.c_str());
            std::fclose(console);
            return;
        }
    }
    MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
#else
    (void)title;
    fprintf(stdout, "%s\n", text.c_str());
    fflush(stdout);
#endif
}
