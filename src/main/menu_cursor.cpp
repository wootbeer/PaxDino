// The mouse cursor in the game's own menus (include/trw_menu_cursor.h).
//
// The game's menus have no pointer of their own: they move a highlight with
// the stick. Once the mouse moves in them, a cursor is drawn over the
// picture here, on an overlay that takes no input, and the highlight is
// steered onto the item under it (src/main/fps_controls.cpp). The cursor
// moves itself on the UI thread each time the UI draws, so it keeps up with
// the mouse whatever the game's frame rate.
#include "trw_menu_cursor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

#include "recompui/recompui.h"
#include "elements/ui_element.h"
#include "trw_controls.h"

namespace {
    using namespace recompui;
    using Clock = std::chrono::steady_clock;

    // ------------------------------------------------------------ the items

    std::mutex items_mutex;
    trw::menu_cursor::Items last_items;
    Clock::time_point last_items_time;
    uint32_t items_serial = 0;

    // A menu not drawn for this long is gone (a match, a load).
    constexpr auto kItemsTimeout = std::chrono::milliseconds(250);

    // ------------------------------------------------------------ the cursor

    // An arrow pointer, tip at the top left: the outline's corners on a
    // 12 x 19 grid.
    constexpr float kArrow[][2] = {
        { 0.0f, 0.0f }, { 0.0f, 16.0f }, { 4.0f, 12.5f }, { 7.0f, 19.0f },
        { 9.5f, 18.0f }, { 6.5f, 11.5f }, { 11.5f, 11.5f },
    };
    constexpr int kArrowPoints = sizeof(kArrow) / sizeof(kArrow[0]);
    // Inner edges, so it reads as a wire frame: tip to notch, and across it.
    constexpr float kInner[][4] = {
        { 0.0f, 0.0f, 6.5f, 11.5f }, { 4.0f, 12.5f, 6.5f, 11.5f },
    };
    constexpr int kTextureWidth = 64;
    constexpr int kTextureHeight = 96;
    constexpr float kTextureScale = 4.6f; // grid units to texture pixels
    constexpr float kTexturePad = 4.0f;
    // Its height on screen, in the UI's units (1080 to the window's height).
    constexpr float kCursorHeightDp = 42.0f;
    constexpr const char* kCursorSrc = "?/trw/menu_cursor";

    bool inside_arrow(float x, float y) {
        bool inside = false;
        for (int i = 0, j = kArrowPoints - 1; i < kArrowPoints; j = i++) {
            float xi = kArrow[i][0], yi = kArrow[i][1], xj = kArrow[j][0], yj = kArrow[j][1];
            if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) {
                inside = !inside;
            }
        }
        return inside;
    }

    float segment_distance(float x, float y, float ax, float ay, float bx, float by) {
        float dx = bx - ax, dy = by - ay;
        float t = std::clamp(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
        float ex = ax + t * dx - x, ey = ay + t * dy - y;
        return std::sqrt(ex * ex + ey * ey);
    }

    float edge_distance(float x, float y) {
        float best = 1e9f;
        for (int i = 0, j = kArrowPoints - 1; i < kArrowPoints; j = i++) {
            best = std::min(best, segment_distance(x, y, kArrow[j][0], kArrow[j][1], kArrow[i][0], kArrow[i][1]));
        }
        return best;
    }

    float inner_distance(float x, float y) {
        float best = 1e9f;
        for (const auto& e : kInner) {
            best = std::min(best, segment_distance(x, y, e[0], e[1], e[2], e[3]));
        }
        return best;
    }

    // A value in [0, 1] for a pixel (and a seed), the same every time.
    float noise(int x, int y, int seed) {
        uint32_t n = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 1442695041u;
        n = (n ^ (n >> 13)) * 1274126177u;
        return (float)((n ^ (n >> 16)) & 0xFFFF) / 65535.0f;
    }

    // A green wire-frame arrow (the outline and two inner edges) with a dark
    // rim, over a see-through dark fill, so it shows on the gold text and on
    // the dark rock alike; the green is roughed up a little (70-100%
    // brightness by a coarse noise). Antialiased, 4 x 4 samples a pixel.
    std::vector<char> draw_arrow() {
        constexpr float kWire = 0.30f, kInnerWire = 0.17f, kRim = 0.62f; // grid units
        struct Paint { float r, g, b, a; };
        constexpr Paint kGreen{ 110.0f, 255.0f, 96.0f, 1.0f };
        constexpr Paint kDark{ 6.0f, 14.0f, 4.0f, 0.9f };
        constexpr Paint kFill{ 8.0f, 20.0f, 6.0f, 0.42f };
        std::vector<char> rgba(kTextureWidth * kTextureHeight * 4, 0);
        for (int py = 0; py < kTextureHeight; py++) {
            for (int px = 0; px < kTextureWidth; px++) {
                float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
                for (int sy = 0; sy < 4; sy++) {
                    for (int sx = 0; sx < 4; sx++) {
                        float gx = (px + (sx + 0.5f) / 4.0f - kTexturePad) / kTextureScale;
                        float gy = (py + (sy + 0.5f) / 4.0f - kTexturePad) / kTextureScale;
                        bool in = inside_arrow(gx, gy);
                        float edge = edge_distance(gx, gy);
                        float inner = in ? inner_distance(gx, gy) : 1e9f;
                        const Paint* paint = nullptr;
                        if (edge <= kWire || inner <= kInnerWire) {
                            paint = &kGreen;
                        }
                        else if (edge <= kRim || inner <= kInnerWire + 0.30f) {
                            paint = &kDark;
                        }
                        else if (in) {
                            paint = &kFill;
                        }
                        if (paint != nullptr) {
                            r += paint->r * paint->a / 16.0f;
                            g += paint->g * paint->a / 16.0f;
                            b += paint->b * paint->a / 16.0f;
                            a += paint->a / 16.0f;
                        }
                    }
                }
                if (a <= 0.0f) {
                    continue;
                }
                r /= a;
                g /= a;
                b /= a;
                if (g > 120.0f) {
                    float grit = 0.72f + 0.28f * (0.5f * noise(px / 2, py / 2, 1) + 0.5f * noise(px, py, 2));
                    r *= grit;
                    g *= grit;
                    b *= grit;
                }
                char* out = &rgba[(py * kTextureWidth + px) * 4];
                out[0] = (char)(uint8_t)std::lround(r);
                out[1] = (char)(uint8_t)std::lround(g);
                out[2] = (char)(uint8_t)std::lround(b);
                out[3] = (char)(uint8_t)std::lround(std::min(a, 1.0f) * 255.0f);
            }
        }
        return rgba;
    }

    // The cursor: an image that puts itself where the mouse is each time
    // the UI updates.
    class PointerImage : public Element {
    protected:
        std::string_view get_type_name() override { return "MenuCursor"; }
        void process_event(const Event& e) override {
            if (e.type != EventType::Update) {
                return;
            }
            float x, y;
            bool show = trw::controls::menu_cursor(x, y);
            if (show) {
                set_left(x * 100.0f, Unit::Percent);
                set_top(y * 100.0f, Unit::Percent);
            }
            if (show != shown) {
                set_display(show ? Display::Block : Display::None);
                shown = show;
            }
            queue_update();
        }
    public:
        PointerImage(ResourceId rid, Element* parent) : Element(rid, parent, Events(EventType::Update), "img") {
            set_src(kCursorSrc);
            set_position(Position::Absolute);
            set_height(kCursorHeightDp, Unit::Dp);
            set_width(kCursorHeightDp * kTextureWidth / kTextureHeight, Unit::Dp);
            // The tip is the hotspot: the arrow starts kTexturePad pixels in.
            float pad = kTexturePad / kTextureHeight * 100.0f;
            set_translate_2D(-pad * kTextureHeight / kTextureWidth, -pad, Unit::Percent);
            set_display(Display::None);
            queue_update();
        }
    private:
        bool shown = false;
    };

    ContextId context = ContextId::null();
}

// Some screens are drawn as more than one menu (the match settings over
// the level select's): of the menus drawn in the same frame (moments apart),
// the one with the most items is kept, and the serial goes up once a frame.
void trw::menu_cursor::publish(const Items& items) {
    constexpr auto kSameFrame = std::chrono::milliseconds(8);
    std::lock_guard lock(items_mutex);
    Clock::time_point now = Clock::now();
    if (now - last_items_time < kSameFrame) {
        if (items.pointable > last_items.pointable) {
            uint32_t serial = last_items.serial;
            last_items = items;
            last_items.serial = serial;
        }
        return; // (the frame's time stays that of its first menu)
    }
    last_items = items;
    last_items.serial = ++items_serial;
    last_items_time = now;
}

bool trw::menu_cursor::current(Items& out) {
    std::lock_guard lock(items_mutex);
    if (last_items.count == 0 || Clock::now() - last_items_time > kItemsTimeout) {
        return false;
    }
    out = last_items;
    return true;
}

void trw::menu_cursor::frame_start() {
    if (context != ContextId::null()) {
        return;
    }
    queue_image_from_bytes_rgba32(kCursorSrc, draw_arrow(), kTextureWidth, kTextureHeight);
    context = create_context();
    context.open();
    context.set_captures_input(false);
    context.set_captures_mouse(false);
    Element* root = context.get_root_element();
    root->set_width(100.0f, Unit::Percent);
    root->set_height(100.0f, Unit::Percent);
    context.create_element<PointerImage>(root);
    context.close();
    show_context(context, "");
}
