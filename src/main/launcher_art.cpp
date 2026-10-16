// The launcher's background: a dark jungle at dusk (icons/launcher_*,
// drawn by tools/make_launcher_art.py) in two layers that drift sideways at
// different speeds, with a still shading overlay on top so the menu stays
// easy to read, and a CRT's grit over all of it (scanlines and moving grain,
// made here). The PaxDino logo (icons/launcher_logo.png) takes the place of the
// launcher's title text. The images are built into the executable.
#include "trw_launcher_art.h"

#include <chrono>
#include <cstdint>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "recompui/recompui.h"
#include "elements/ui_image.h"

// stb_image's code is in RT64 (render/rt64_texture_cache.cpp); only the
// declarations are needed here.
#include "stb/stb_image.h"

#include "launcher_back_jpg.h"
#include "launcher_front_png.h"
#include "launcher_logo_png.h"
#include "launcher_shade_png.h"

namespace {
    using namespace recompui;

    // How long a layer takes to move one image width: the far jungle slowly,
    // the near plants a little faster.
    constexpr double back_period_seconds = 420.0;
    constexpr double front_period_seconds = 150.0;

    struct Layer {
        Element* strip = nullptr; // two copies of the image side by side
        double period = 1.0;
    };

    Layer back_layer;
    Layer front_layer;
    Element* grain = nullptr;
    std::chrono::steady_clock::time_point start_time;
    std::mt19937 grain_rng{ 1234 };

    // The grit: dark scanlines (a 1-pixel-wide strip of rows, stretched
    // across) and a field of grain that jumps about every frame.
    constexpr int kScanlineRows = 1080 / 3;      // a line every 3 rows at 1080p
    constexpr float kScanlineDarkness = 0.38f;
    constexpr int kGrainWidth = 960, kGrainHeight = 540; // 2 pixels a grain at 1080p
    constexpr float kGrainStrength = 0.10f;

    std::vector<char> scanline_pixels() {
        std::vector<char> rgba(size_t(kScanlineRows) * 3 * 4, 0);
        for (int y = 0; y < kScanlineRows * 3; y++) {
            // Two lit rows, then a dark one, softened at its edges.
            float shade = (y % 3 == 2) ? kScanlineDarkness : kScanlineDarkness * 0.15f;
            rgba[size_t(y) * 4 + 3] = char(uint8_t(std::lround(shade * 255.0f)));
        }
        return rgba;
    }

    std::vector<char> grain_pixels() {
        std::mt19937 rng{ 99 };
        std::uniform_real_distribution<float> unit{ 0.0f, 1.0f };
        std::vector<char> rgba(size_t(kGrainWidth) * kGrainHeight * 4);
        for (size_t i = 0; i < size_t(kGrainWidth) * kGrainHeight; i++) {
            // Light and dark specks alike, faint.
            float v = unit(rng);
            uint8_t level = v > 0.5f ? 255 : 0;
            float alpha = std::abs(v - 0.5f) * 2.0f;
            alpha = alpha * alpha * kGrainStrength;
            rgba[i * 4 + 0] = char(level);
            rgba[i * 4 + 1] = char(level);
            rgba[i * 4 + 2] = char(level);
            rgba[i * 4 + 3] = char(uint8_t(std::lround(alpha * 255.0f)));
        }
        return rgba;
    }

    void queue_image(const std::string& src, const char* bytes, size_t size) {
        queue_image_from_bytes_file(src, std::vector<char>(bytes, bytes + size));
    }

    // The UI only reads PNG (and DDS) files, so the JPEG is decoded here.
    void queue_jpeg(const std::string& src, const char* bytes, size_t size) {
        int width = 0, height = 0;
        stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes), static_cast<int>(size), &width, &height, nullptr, 4);
        if (pixels == nullptr) {
            return;
        }
        std::vector<char> rgba(reinterpret_cast<const char*>(pixels), reinterpret_cast<const char*>(pixels) + size_t(width) * size_t(height) * 4);
        stbi_image_free(pixels);
        queue_image_from_bytes_rgba32(src, rgba, uint32_t(width), uint32_t(height));
    }

    // A full-height strip of two copies of the image; moving it left by half
    // its width shows the same picture again, so it can loop.
    Layer make_layer(ContextId context, Element* parent, const std::string& src, double period) {
        Element* strip = context.create_element<Element>(parent, 0, "div", false);
        strip->set_position(Position::Absolute);
        strip->set_top(0);
        strip->set_left(0);
        strip->set_height(100.0f, Unit::Percent);
        strip->set_width_auto();
        strip->set_display(Display::Flex);
        strip->set_flex_direction(FlexDirection::Row);
        for (int i = 0; i < 2; i++) {
            Image* image = context.create_element<Image>(strip, src);
            image->set_height(100.0f, Unit::Percent);
            image->set_width_auto();
            image->set_flex_shrink(0.0f);
        }
        return Layer{ strip, period };
    }

    void move_layer(const Layer& layer, double seconds) {
        if (layer.strip == nullptr) {
            return;
        }
        double phase = std::fmod(seconds / layer.period, 1.0);
        layer.strip->set_translate_2D(static_cast<float>(-50.0 * phase), 0.0f, Unit::Percent);
    }
}

void trw::launcher_art::init(recompui::LauncherMenu* menu) {
    queue_jpeg("?/trw/launcher_back", launcher_back_jpg, launcher_back_jpg_size);
    queue_image("?/trw/launcher_front", launcher_front_png, launcher_front_png_size);
    queue_image("?/trw/launcher_shade", launcher_shade_png, launcher_shade_png_size);

    ContextId context = get_current_context();
    Element* background = menu->get_background_wrapper();
    background->set_overflow(Overflow::Hidden);
    back_layer = make_layer(context, background, "?/trw/launcher_back", back_period_seconds);
    front_layer = make_layer(context, background, "?/trw/launcher_front", front_period_seconds);

    Image* shade = context.create_element<Image>(background, "?/trw/launcher_shade");
    shade->set_position(Position::Absolute);
    shade->set_top(0);
    shade->set_left(0);
    shade->set_width(100.0f, Unit::Percent);
    shade->set_height(100.0f, Unit::Percent);

    // The grit, over the background only (the logo and the menu come after
    // the background in the launcher, so they stay clean).
    queue_image_from_bytes_rgba32("?/trw/launcher_scanlines", scanline_pixels(), 1, kScanlineRows * 3);
    queue_image_from_bytes_rgba32("?/trw/launcher_grain", grain_pixels(), kGrainWidth, kGrainHeight);
    Image* scanlines = context.create_element<Image>(background, "?/trw/launcher_scanlines");
    scanlines->set_position(Position::Absolute);
    scanlines->set_top(0);
    scanlines->set_left(0);
    scanlines->set_width(100.0f, Unit::Percent);
    scanlines->set_height(100.0f, Unit::Percent);
    // Larger than the window, so it can be moved about without its edges showing.
    grain = context.create_element<Image>(background, "?/trw/launcher_grain");
    grain->set_position(Position::Absolute);
    grain->set_top(-10.0f, Unit::Percent);
    grain->set_left(-10.0f, Unit::Percent);
    grain->set_width(120.0f, Unit::Percent);
    grain->set_height(120.0f, Unit::Percent);

    // The logo instead of the title text.
    queue_image("?/trw/launcher_logo", launcher_logo_png, launcher_logo_png_size);
    menu->remove_default_title();
    Image* logo = context.create_element<Image>(menu, "?/trw/launcher_logo");
    logo->set_position(Position::Absolute);
    logo->set_top(20.0f, Unit::Percent);
    logo->set_left(50.0f, Unit::Percent);
    logo->set_translate_2D(-50.0f, -50.0f, Unit::Percent);
    logo->set_height(30.0f, Unit::Percent);
    logo->set_width_auto();

    start_time = std::chrono::steady_clock::now();
}

void trw::launcher_art::update() {
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
    move_layer(back_layer, seconds);
    move_layer(front_layer, seconds);
    if (grain != nullptr) {
        std::uniform_real_distribution<float> offset{ -8.0f, 8.0f };
        grain->set_translate_2D(offset(grain_rng), offset(grain_rng), Unit::Percent);
    }
}
