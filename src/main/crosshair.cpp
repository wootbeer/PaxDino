// Crosshair (Enhancements tab): the game's crosshair replaced by one of a few
// shapes drawn here (in a colour, size and opacity of the player's choice)
// or by a picture of their own.
//
// The game draws its crosshair as HUD sprite 751 (14 x 14, through
// func_002AAB58(image, ?, x, y, ..., x scale at sp+0x10, y scale at sp+0x14),
// hooked in turokrw.us.toml) at its top-left corner, in a 480 x 360 screen
// whose centre is that of the 4:3 picture. With a style other than the game's
// own, that sprite isn't drawn; where it would have been is noted, and at the
// start of the next frame the UI draws the new crosshair there (an overlay
// that takes no input). So it shows exactly when the game's would: one per
// player in split screen, none while paused or between matches.
#include "trw_crosshair.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#include "recomp.h"
#include "recompui/recompui.h"
#include "elements/ui_image.h"
#include "util/file.h"
#include "trw_config.h"

// stb_image's code is in RT64 (render/rt64_texture_cache.cpp).
#include "stb/stb_image.h"

namespace fs = std::filesystem;

namespace {
    using namespace recompui;
    using trw::config::CrosshairColor;
    using trw::config::CrosshairStyle;

    constexpr uint32_t kCrosshairSprite = 751;
    constexpr float kSpriteSize = 14.0f;
    constexpr float kHudWidth = 480.0f;
    constexpr float kHudHeight = 360.0f;
    // The game's crosshair at 100%: 14 of the 360 rows, in the UI's units
    // (1080 to the window's height).
    constexpr float kBaseSizeDp = kSpriteSize * 1080.0f / kHudHeight;
    constexpr int kMaxCrosshairs = 4;
    // Pictures are drawn at most this big, so a large one is shrunk first.
    constexpr int kMaxImageSide = 256;

    struct Mark {
        float x, y;  // the centre, in the 480 x 360 screen
        float scale; // the sprite's
    };

    // Noted by the hook during a frame; shown at the next frame's start (both
    // on the game thread).
    Mark pending[kMaxCrosshairs];
    int pending_count = 0;
    std::atomic<bool> replacing{ false }; // a style other than the game's own

    // The overlay (made on first use, on the game thread).
    ContextId context = ContextId::null();
    Element* root = nullptr;
    Image* images[kMaxCrosshairs] = {};
    bool shown[kMaxCrosshairs] = {};
    int image_serial = 0;      // each new picture gets a new name
    std::string image_src;
    float image_aspect = 1.0f; // width / height
    CrosshairStyle drawn_style = CrosshairStyle::Game;
    CrosshairColor drawn_color = CrosshairColor::Green;
    uint32_t drawn_custom = 0;

    // Choose Image bumps this; the picture is reloaded at the next frame.
    std::atomic<uint32_t> custom_serial{ 1 };

    fs::path custom_image_path() {
        return file::get_app_folder_path() / "custom_crosshair";
    }

    // ------------------------------------------------------------ the shapes

    struct Rgb {
        float r, g, b;
    };

    Rgb color_of(CrosshairColor color) {
        switch (color) {
            case CrosshairColor::White:   return { 1.0f, 1.0f, 1.0f };
            case CrosshairColor::Red:     return { 1.0f, 0.2f, 0.15f };
            case CrosshairColor::Yellow:  return { 1.0f, 0.9f, 0.15f };
            case CrosshairColor::Cyan:    return { 0.2f, 0.95f, 1.0f };
            case CrosshairColor::Magenta: return { 1.0f, 0.25f, 0.95f };
            case CrosshairColor::Green:
            default:                      return { 0.25f, 1.0f, 0.2f };
        }
    }

    float segment_distance(float px, float py, float ax, float ay, float bx, float by) {
        float dx = bx - ax, dy = by - ay;
        float t = std::clamp(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
        float qx = ax + t * dx - px, qy = ay + t * dy - py;
        return std::sqrt(qx * qx + qy * qy);
    }

    // How far (x, y), from -1 to 1 across the picture, is outside the shape's
    // strokes (negative inside).
    float shape_distance(CrosshairStyle style, float x, float y) {
        constexpr float half_stroke = 0.075f;
        constexpr float arm = 0.9f;  // where the arms end
        constexpr float gap = 0.32f; // the open cross's gap
        constexpr float ring = 0.62f;
        constexpr float dot = 0.15f;
        float d = 1e9f;
        auto line = [&](float ax, float ay, float bx, float by) {
            d = std::min(d, segment_distance(x, y, ax, ay, bx, by) - half_stroke);
        };
        auto open_arms = [&](bool top) {
            line(-arm, 0, -gap, 0);
            line(gap, 0, arm, 0);
            line(0, gap, 0, arm);
            if (top) {
                line(0, -arm, 0, -gap);
            }
        };
        float radius = std::sqrt(x * x + y * y);
        switch (style) {
            case CrosshairStyle::Cross:
                line(-arm, 0, arm, 0);
                line(0, -arm, 0, arm);
                break;
            case CrosshairStyle::GapCross:
                open_arms(true);
                break;
            case CrosshairStyle::Dot:
                d = radius - dot * 1.4f;
                break;
            case CrosshairStyle::CrossDot:
                open_arms(true);
                d = std::min(d, radius - dot);
                break;
            case CrosshairStyle::Circle:
                d = std::abs(radius - ring) - half_stroke;
                break;
            case CrosshairStyle::CircleDot:
                d = std::min(std::abs(radius - ring) - half_stroke, radius - dot);
                break;
            case CrosshairStyle::T:
                open_arms(false);
                d = std::min(d, radius - dot);
                break;
            default:
                break;
        }
        return d;
    }

    // The shape in the colour with a dark outline, so it shows on light and
    // dark scenes alike.
    std::vector<char> draw_shape(CrosshairStyle style, CrosshairColor color, int side) {
        constexpr float outline = 0.07f;
        constexpr float outline_alpha = 0.75f;
        Rgb rgb = color_of(color);
        float pixel = 2.0f / side;
        std::vector<char> out(size_t(side) * side * 4);
        for (int py = 0; py < side; py++) {
            for (int px = 0; px < side; px++) {
                float x = (px + 0.5f) * pixel - 1.0f;
                float y = (py + 0.5f) * pixel - 1.0f;
                float d = shape_distance(style, x, y);
                float fill = std::clamp(0.5f - d / pixel, 0.0f, 1.0f);
                float edge = std::clamp(0.5f - (d - outline) / pixel, 0.0f, 1.0f);
                float rim = (edge - fill) * outline_alpha;
                float alpha = fill + rim;
                float k = alpha > 0.0f ? fill / alpha : 0.0f; // the rim is black
                uint8_t* p = reinterpret_cast<uint8_t*>(&out[(size_t(py) * side + px) * 4]);
                p[0] = uint8_t(std::lround(rgb.r * k * 255.0f));
                p[1] = uint8_t(std::lround(rgb.g * k * 255.0f));
                p[2] = uint8_t(std::lround(rgb.b * k * 255.0f));
                p[3] = uint8_t(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
            }
        }
        return out;
    }

    // ------------------------------------------------------ custom pictures

    struct Picture {
        std::vector<char> rgba;
        int width = 0, height = 0;
    };

    bool decode_picture(const std::vector<char>& bytes, Picture& out) {
        int width = 0, height = 0;
        stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), int(bytes.size()), &width, &height, nullptr, 4);
        if (pixels == nullptr || width <= 0 || height <= 0) {
            if (pixels != nullptr) {
                stbi_image_free(pixels);
            }
            return false;
        }
        // Shrunk to at most kMaxImageSide on its longer side (averaging the
        // source pixels under each one).
        int side = std::max(width, height);
        int out_w = width, out_h = height;
        if (side > kMaxImageSide) {
            out_w = std::max(1, width * kMaxImageSide / side);
            out_h = std::max(1, height * kMaxImageSide / side);
        }
        out.width = out_w;
        out.height = out_h;
        out.rgba.assign(size_t(out_w) * out_h * 4, 0);
        for (int oy = 0; oy < out_h; oy++) {
            int y0 = oy * height / out_h, y1 = std::max(y0 + 1, (oy + 1) * height / out_h);
            for (int ox = 0; ox < out_w; ox++) {
                int x0 = ox * width / out_w, x1 = std::max(x0 + 1, (ox + 1) * width / out_w);
                // Colours weighted by alpha, so see-through pixels don't darken the edges.
                double sum[4] = {};
                for (int sy = y0; sy < y1; sy++) {
                    for (int sx = x0; sx < x1; sx++) {
                        const stbi_uc* s = pixels + (size_t(sy) * width + sx) * 4;
                        double a = s[3];
                        sum[0] += s[0] * a;
                        sum[1] += s[1] * a;
                        sum[2] += s[2] * a;
                        sum[3] += a;
                    }
                }
                double count = double(x1 - x0) * (y1 - y0);
                uint8_t* p = reinterpret_cast<uint8_t*>(&out.rgba[(size_t(oy) * out_w + ox) * 4]);
                for (int c = 0; c < 3; c++) {
                    p[c] = uint8_t(sum[3] > 0 ? std::lround(sum[c] / sum[3]) : 0);
                }
                p[3] = uint8_t(std::lround(sum[3] / count));
            }
        }
        stbi_image_free(pixels);
        return true;
    }

    std::vector<char> read_file(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return {};
        }
        return std::vector<char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    // ------------------------------------------------------------ the overlay

    void make_overlay() {
        context = create_context();
        context.open();
        context.set_captures_input(false);
        context.set_captures_mouse(false);
        root = context.get_root_element();
        root->set_width(100.0f, Unit::Percent);
        root->set_height(100.0f, Unit::Percent);
        // Made once and kept: a new picture only changes their source.
        for (int i = 0; i < kMaxCrosshairs; i++) {
            images[i] = context.create_element<Image>(root, "");
            images[i]->set_position(Position::Absolute);
            images[i]->set_translate_2D(-50.0f, -50.0f, Unit::Percent);
            images[i]->set_display(Display::None);
            shown[i] = false;
        }
        context.close();
        show_context(context, "");
    }

    // A new picture for the crosshairs (made while the context is open).
    void set_picture(const std::vector<char>& rgba, int width, int height) {
        image_src = "?/trw/crosshair/" + std::to_string(++image_serial);
        queue_image_from_bytes_rgba32(image_src, rgba, uint32_t(width), uint32_t(height));
        image_aspect = float(width) / float(height);
        for (int i = 0; i < kMaxCrosshairs; i++) {
            images[i]->set_src(image_src);
        }
    }

    // The picture for the settings: false if there's none to show (a custom
    // image that's missing or can't be read).
    bool update_picture(const trw::config::CrosshairSettings& settings) {
        uint32_t custom = custom_serial.load();
        bool same = settings.style == drawn_style && !image_src.empty() &&
            (settings.style == CrosshairStyle::Custom ? custom == drawn_custom : settings.color == drawn_color);
        if (same) {
            return true;
        }
        if (settings.style == CrosshairStyle::Custom) {
            // A picture that can't be read is tried again only once a new one is chosen.
            static uint32_t failed_custom = 0;
            if (failed_custom == custom) {
                return false;
            }
            Picture picture;
            if (!decode_picture(read_file(custom_image_path()), picture)) {
                failed_custom = custom;
                return false;
            }
            set_picture(picture.rgba, picture.width, picture.height);
            drawn_custom = custom;
        }
        else {
            constexpr int side = 128;
            set_picture(draw_shape(settings.style, settings.color, side), side, side);
            drawn_color = settings.color;
        }
        drawn_style = settings.style;
        return true;
    }

    void hide_all() {
        for (int i = 0; i < kMaxCrosshairs; i++) {
            if (images[i] != nullptr && shown[i]) {
                images[i]->set_display(Display::None);
                shown[i] = false;
            }
        }
    }
}

void trw::crosshair::frame_start(bool in_match) {
    // This runs more than once a frame (at each colour buffer the game
    // sets), so the crosshairs noted are kept until a few calls in a row
    // have gone by without any.
    constexpr int kEmptyCallsToHide = 6;
    static Mark marks[kMaxCrosshairs];
    static int count = 0;
    static int empty_calls = 0;
    if (pending_count > 0) {
        std::copy(pending, pending + pending_count, marks);
        count = pending_count;
        empty_calls = 0;
    }
    else if (++empty_calls >= kEmptyCallsToHide) {
        count = 0;
    }
    pending_count = 0;
    if (!in_match) {
        count = 0;
    }

    trw::config::CrosshairSettings settings = trw::config::get_crosshair();
    replacing.store(settings.style != CrosshairStyle::Game);
    if (context == ContextId::null()) {
        if (settings.style == CrosshairStyle::Game) {
            return;
        }
        make_overlay();
    }
    // Hidden only when the UI shuts down (it hides every context first): its
    // elements are about to go, so nothing more is done with them.
    if (!is_context_shown(context)) {
        return;
    }

    // Asked before the context is opened: these take the UI's own lock,
    // which the UI thread holds while it opens the context to draw it.
    bool menu_open = is_context_capturing_input(); // the menu (F1) over the game
    int window_w = 0, window_h = 0;
    get_window_size(window_w, window_h);

    context.open();
    if (settings.style == CrosshairStyle::Game || count == 0 || menu_open || window_w <= 0 || window_h <= 0 ||
        !update_picture(settings)) {
        hide_all();
        context.close();
        return;
    }
    // The 480 x 360 screen in the window: the 4:3 picture, centred.
    float scale = std::min(window_w / kHudWidth, window_h / kHudHeight);
    float left = (window_w - kHudWidth * scale) * 0.5f;
    float top = (window_h - kHudHeight * scale) * 0.5f;
    for (int i = 0; i < kMaxCrosshairs; i++) {
        Image* image = images[i];
        if (i >= count) {
            if (shown[i]) {
                image->set_display(Display::None);
                shown[i] = false;
            }
            continue;
        }
        float size = kBaseSizeDp * settings.size * marks[i].scale;
        float width = image_aspect >= 1.0f ? size : size * image_aspect;
        float height = image_aspect >= 1.0f ? size / image_aspect : size;
        image->set_left((left + marks[i].x * scale) * 100.0f / window_w, Unit::Percent);
        image->set_top((top + marks[i].y * scale) * 100.0f / window_h, Unit::Percent);
        image->set_width(width, Unit::Dp);
        image->set_height(height, Unit::Dp);
        image->set_opacity(settings.opacity);
        if (!shown[i]) {
            image->set_display(Display::Block);
            shown[i] = true;
        }
    }
    context.close();
}

namespace {
    // A picture frame with a hill and a sun, for the Image style.
    std::vector<char> draw_picture_glyph(int side) {
        std::vector<char> out(size_t(side) * side * 4, 0);
        float pixel = 2.0f / side;
        for (int py = 0; py < side; py++) {
            for (int px = 0; px < side; px++) {
                float x = (px + 0.5f) * pixel - 1.0f;
                float y = (py + 0.5f) * pixel - 1.0f;
                float frame = std::max(std::abs(x), std::abs(y) * 1.25f);
                bool border = frame > 0.72f && frame < 0.86f;
                bool inside = frame <= 0.72f;
                bool hill = inside && y > 0.45f - 0.6f * std::max(0.0f, 0.5f - std::abs(x + 0.15f)) * 1.8f;
                bool sun = inside && (x - 0.35f) * (x - 0.35f) + (y + 0.25f) * (y + 0.25f) < 0.03f;
                uint8_t* p = reinterpret_cast<uint8_t*>(&out[(size_t(py) * side + px) * 4]);
                if (border || hill || sun) {
                    p[0] = p[1] = p[2] = 235;
                    p[3] = 255;
                }
            }
        }
        return out;
    }

    struct Preview {
        CrosshairStyle style;
        const char* name;
    };
    constexpr Preview kPreviews[] = {
        { CrosshairStyle::Game, "Game" },
        { CrosshairStyle::Cross, "Cross" },
        { CrosshairStyle::GapCross, "Open" },
        { CrosshairStyle::Dot, "Dot" },
        { CrosshairStyle::CrossDot, "Open+Dot" },
        { CrosshairStyle::Circle, "Ring" },
        { CrosshairStyle::CircleDot, "Ring+Dot" },
        { CrosshairStyle::T, "T" },
        { CrosshairStyle::Custom, "Image" },
    };
}

const char* trw::crosshair::preview_rml() {
    static const std::string rml = [] {
        std::string out;
        for (const Preview& preview : kPreviews) {
            out += "<div style=\"display: inline-block; width: 96dp; margin: 0dp 8dp 20dp 0dp; text-align: center;\">"
                   "<img src=\"?/trw/crosshair_preview/" + std::to_string(int(preview.style)) +
                   "\" style=\"width: 64dp; height: 64dp;\"/><br/>" + preview.name + "</div>";
        }
        return out;
    }();
    return rml.c_str();
}

void trw::crosshair::queue_previews() {
    constexpr int side = 96;
    for (const Preview& preview : kPreviews) {
        std::vector<char> pixels;
        if (preview.style == CrosshairStyle::Custom) {
            pixels = draw_picture_glyph(side);
        }
        else if (preview.style == CrosshairStyle::Game) {
            // Like the game's own: a green cross.
            pixels = draw_shape(CrosshairStyle::Cross, CrosshairColor::Green, side);
        }
        else {
            pixels = draw_shape(preview.style, CrosshairColor::White, side);
        }
        queue_image_from_bytes_rgba32("?/trw/crosshair_preview/" + std::to_string(int(preview.style)), pixels, side, side);
    }
}

void trw::crosshair::choose_image() {
    file::open_file_dialog([](bool success, const fs::path& path) {
        if (!success) {
            return;
        }
        std::vector<char> bytes = read_file(path);
        Picture picture;
        if (bytes.empty() || !decode_picture(bytes, picture)) {
            open_info_prompt("Crosshair", "That file couldn't be read as a picture. Use a PNG, JPG, BMP or TGA file.", "OK", [] {});
            return;
        }
        std::error_code ec;
        fs::create_directories(custom_image_path().parent_path(), ec);
        std::ofstream out(custom_image_path(), std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), std::streamsize(bytes.size()));
        out.close();
        custom_serial.fetch_add(1);
        open_info_prompt("Crosshair", "Picture saved. Set <b>Crosshair</b> to <b>Image</b> to use it.", "OK", [] {});
    });
}

// From the HUD sprite call (func_002AAB58): 1 to leave the game's crosshair
// out (its place is noted for the new one).
extern "C" int trw_crosshair_sprite(uint8_t* rdram, gpr image, gpr x, gpr y, gpr sp) {
    if ((uint32_t)image != kCrosshairSprite || !replacing.load()) {
        return 0;
    }
    if (pending_count < kMaxCrosshairs) {
        uint32_t sx_bits = (uint32_t)MEM_W(0x10, sp);
        uint32_t sy_bits = (uint32_t)MEM_W(0x14, sp);
        float sx, sy;
        memcpy(&sx, &sx_bits, sizeof(sx));
        memcpy(&sy, &sy_bits, sizeof(sy));
        if (!(sx > 0.0f && sx < 8.0f)) sx = 1.0f;
        if (!(sy > 0.0f && sy < 8.0f)) sy = 1.0f;
        pending[pending_count++] = Mark{
            (float)(int16_t)x + kSpriteSize * 0.5f * sx,
            (float)(int16_t)y + kSpriteSize * 0.5f * sy,
            sx,
        };
    }
    return 1;
}
