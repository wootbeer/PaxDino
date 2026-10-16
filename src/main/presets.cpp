// The launcher's Preset entry, under Settings: "Preset: Modern" or
// "Preset: Classic", switching on a click. A preset sets only the options
// listed below (each in its own tab's settings file, saved as the menu's
// Apply would); every other setting stays as the player left it. Which one
// was picked last is kept in general.json ("preset", hidden). Modern is the
// default: the options of our own default to its values, and on the first
// launch (no graphics.json yet) the whole list is applied, since the
// frontend's graphics defaults are its own.
#include "trw_presets.h"

#include <cstdint>
#include <filesystem>
#include <string>

#include "librecomp/config.hpp"
#include "librecomp/game.hpp"
#include "recompui/config.h"
#include "recompui/recompui.h"
#include "base/ui_launcher.h"
#include "trw_config.h"

namespace {
    bool first_launch = false;
    recompui::GameOption* preset_option = nullptr;

    enum class Preset : uint32_t {
        Classic = 0,
        Modern = 1,
    };

    // Downsampling Quality's values (the frontend's DownsamplingOption).
    constexpr uint32_t kDownsampleOff = 0;
    constexpr uint32_t kDownsample4x = 4;

    Preset current_preset() {
        return static_cast<Preset>(std::get<uint32_t>(recompui::config::get_general_config().get_option_value("preset")));
    }

    std::string title_for(Preset preset) {
        return preset == Preset::Modern ? "Preset: Modern" : "Preset: Classic";
    }

    // 4x anti-aliasing, or the most the graphics card allows.
    uint32_t modern_msaa(recomp::config::Config& graphics) {
        using AA = ultramodern::renderer::Antialiasing;
        size_t index = graphics.get_config_schema().options_by_id.at(recompui::config::graphics::options::msaa_option);
        for (AA aa : { AA::MSAA4X, AA::MSAA2X }) {
            if (!graphics.get_enum_option_disabled(index, static_cast<uint32_t>(aa)) && !graphics.is_config_option_disabled(index)) {
                return static_cast<uint32_t>(aa);
            }
        }
        return static_cast<uint32_t>(AA::None);
    }

    void apply(Preset preset) {
        namespace gfx = recompui::config::graphics::options;
        using namespace ultramodern::renderer;
        const bool modern = preset == Preset::Modern;
        auto e = [](auto value) { return recomp::config::ConfigValueVariant{ static_cast<uint32_t>(value) }; };

        recomp::config::Config& graphics = recompui::config::get_graphics_config();
        graphics.update_option_value(gfx::res_option, e(modern ? Resolution::Original2x : Resolution::Original));
        graphics.update_option_value(gfx::ds_option, e(modern ? kDownsample4x : kDownsampleOff));
        graphics.update_option_value(gfx::ar_option, e(modern ? AspectRatio::Expand : AspectRatio::Original));
        graphics.update_option_value(gfx::rr_option, e(modern ? RefreshRate::Display : RefreshRate::Original));
        graphics.update_option_value(gfx::msaa_option, e(modern ? modern_msaa(graphics) : static_cast<uint32_t>(Antialiasing::None)));
        graphics.update_option_value("bloom", e(modern ? trw::config::Bloom::Low : trw::config::Bloom::Off));
        graphics.update_option_value(gfx::hr_option, e(modern ? HUDRatioMode::Full : HUDRatioMode::Original));
        graphics.update_option_value("screen_filter", e(modern ? trw::config::ScreenFilter::CRT : trw::config::ScreenFilter::None));
        graphics.update_option_value("screen_filter_menus_only", modern);
        // After the filter: choosing one turns the panels on (extras_config.cpp).
        graphics.update_option_value("no_menu_scanlines", modern);
        graphics.save_config();

        recomp::config::Config& general = recompui::config::get_general_config();
        general.update_option_value("menu_cursor", modern);
        general.update_option_value("look_spring_override", modern);
        for (int player = 2; player <= 4; player++) {
            general.update_option_value("look_spring_override_p" + std::to_string(player), modern);
        }
        general.update_option_value("preset", e(preset));
        general.save_config();

        recomp::config::Config& sound = recompui::config::get_sound_config();
        sound.update_option_value("weapon_reverb", modern);
        if (modern) {
            sound.update_option_value("weapon_reverb_amount", 50.0);
        }
        sound.save_config();

        recomp::config::Config& enhancements = recompui::config::get_config("enhancements");
        enhancements.update_option_value("idle_demo", !modern);
        enhancements.save_config();
    }
}

void trw::presets::note_first_launch() {
    std::error_code ec;
    first_launch = !std::filesystem::exists(recomp::get_config_path() / (recompui::config::graphics::id + ".json"), ec);
}

void trw::presets::apply_first_launch() {
    if (first_launch) {
        apply(Preset::Modern);
    }
}

void trw::presets::add_launcher_option(recompui::GameOptionsMenu* options) {
    preset_option = options->add_option(title_for(current_preset()), [] {
        Preset next = current_preset() == Preset::Modern ? Preset::Classic : Preset::Modern;
        apply(next);
        preset_option->set_title(title_for(next));
    });
    // Smaller than the entries above it.
    preset_option->get_label()->set_font_size(28.0f);
    preset_option->set_padding_top(4.0f);
    preset_option->set_padding_bottom(12.0f);
}
