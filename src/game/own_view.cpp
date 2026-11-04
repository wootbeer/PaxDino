// Netplay: each PC shows only its own player's view of a split-screen
// match, full screen. The game draws every player's view each frame, each
// in its part of the screen with its own projection; just before a display
// list goes to the renderer (src/main/main.cpp), the other players' views
// are jumped over and this player's is drawn over the whole screen: the
// game's full-screen viewport, its scissor opened up, its projections
// rescaled to the screen's shape (copies in spare room at the end of the
// display list buffer), and its HUD's rectangles moved to the same place on
// the full screen. Only the display list changes, never the game's own
// memory, so the two PCs' games stay the same.
//
// A frame's views, as the game builds them: at the top level of the display
// list, a projection load, then a viewport whose rectangle is part of the
// screen; after the last, a projection and the full-screen viewport for what
// goes over the whole screen (the match clock, on the line between the views,
// moved to the top).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <iterator>
#include <mutex>
#include <vector>

#include "recomp.h"
#include "trw_config.h"

namespace {
    constexpr uint32_t kDlBufferA = 0x806D2000;
    constexpr uint32_t kDlBufferB = 0x806E9000;
    constexpr uint32_t kDlBufferSize = 0x17000;
    constexpr uint32_t kScratchSize = 0x400; // the copied projections, at the buffer's end
    constexpr int kMaxCommands = 200000;
    constexpr double kPi = 3.14159265358979;

    struct Viewport {
        uint32_t cmd;      // the G_MOVEMEM loading it
        uint32_t address;  // its Vp
        uint32_t proj_cmd; // the projection load just before it (0 if none)
        int depth;         // display list calls deep
        float sx, sy, tx, ty; // scale and translation, pixels
    };

    // Where the last frame's view went: the view's rectangle (x0, y0, w by h)
    // and the screen's top left (fx0, fy0) and scale (sx, sy), for the
    // crosshair drawn over the picture (src/main/crosshair.cpp).
    struct Placement {
        bool valid = false;
        float x0, y0, w, h, fx0, fy0, sx, sy;
    };
    std::mutex placement_mutex;
    Placement placement;

    // Frames that look split but are left as they are, for the log (a few
    // per session).
    int unhandled_logged = 0;
    constexpr int kUnhandledLogMax = 12;

    template <typename... Args>
    void log_unhandled(const char* fmt, Args... args) {
        if (unhandled_logged++ >= kUnhandledLogMax) {
            return;
        }
        std::fprintf(stderr, "[NET] own view: ");
        std::fprintf(stderr, fmt, args...);
        std::fprintf(stderr, "\n");
        std::fflush(stderr);
    }

    struct Command {
        uint32_t address;
        uint32_t w0, w1;
        int depth;
    };

    uint32_t k0(uint32_t address) {
        return (address & 0x1FFFFFFFu) | 0x80000000u;
    }

    uint32_t word(uint8_t* rdram, uint32_t address) {
        return (uint32_t)MEM_W(0, (gpr)(int32_t)k0(address));
    }

    // The frame's commands in the order the renderer meets them.
    std::vector<Command> walk(uint8_t* rdram, uint32_t root) {
        std::vector<Command> commands;
        std::vector<uint32_t> stack;
        uint32_t segments[16] = {};
        uint32_t address = k0(root);
        for (int n = 0; n < kMaxCommands; n++) {
            uint32_t w0 = word(rdram, address), w1 = word(rdram, address + 4);
            uint32_t op = w0 >> 24;
            commands.push_back({ address, w0, w1, (int)stack.size() });
            if (op == 0xDB && ((w0 >> 16) & 0xFF) == 6) { // G_MOVEWORD, a segment
                segments[(w0 & 0xFFFF) / 4 & 15] = w1;
            }
            if (op == 0xDE) { // G_DL
                uint32_t target = w1;
                if ((target >> 24) < 16 && target < 0x80000000u) {
                    target = segments[target >> 24] + (target & 0xFFFFFF);
                }
                if (((w0 >> 16) & 0xFF) == 0) {
                    if (stack.size() >= 18) {
                        return {};
                    }
                    stack.push_back(address + 8);
                }
                address = k0(target);
                continue;
            }
            if (op == 0xDF) { // G_ENDDL
                if (stack.empty()) {
                    return commands;
                }
                address = stack.back();
                stack.pop_back();
                continue;
            }
            address += 8;
        }
        return {};
    }

    int16_t half(uint8_t* rdram, uint32_t address) {
        return MEM_H(0, (gpr)(int32_t)address);
    }

    int32_t fixed_at(uint8_t* rdram, uint32_t m, int i) {
        return (int32_t)(((uint32_t)(uint16_t)half(rdram, m + i * 2) << 16) | (uint16_t)half(rdram, m + 32 + i * 2));
    }

    // A projection's copy, its x and y columns scaled.
    void scale_projection(uint8_t* rdram, uint32_t from, uint32_t to, float fx, float fy) {
        for (int i = 0; i < 16; i++) {
            int32_t value = fixed_at(rdram, from, i);
            int column = i % 4;
            if (column == 0) {
                value = (int32_t)((double)value * fx);
            }
            else if (column == 1) {
                value = (int32_t)((double)value * fy);
            }
            MEM_H(0, (gpr)(int32_t)(to + i * 2)) = (int16_t)(value >> 16);
            MEM_H(0, (gpr)(int32_t)(to + 32 + i * 2)) = (int16_t)(value & 0xFFFF);
        }
    }

    // The camera's projection (func_00234FCC) for a view `wide` times the
    // screen's shape (2 for one of two stacked views), remade as one player's
    // would be: the same picture as playing alone. The camera picks its
    // vertical angle as atan(h / aspect), h fixed (zoom aside), so a wide
    // view gets a narrow angle; the Field of View setting then widens it in
    // tangents (src/game/extras.cpp). Other projections (the 47.5 degree
    // one, square) are left as they are, stretched with the view.
    void single_player_projection(uint8_t* rdram, uint32_t from, uint32_t to, float wide) {
        constexpr double kScreenAspect = 4.0 / 3.0;
        // Its x and y scales: the lengths of its x and y columns (the
        // world's comes with the camera's turn in it), against w's.
        auto length = [&](int column) {
            double sum = 0.0;
            for (int row = 0; row < 3; row++) {
                double v = fixed_at(rdram, from, row * 4 + column) / 65536.0;
                sum += v * v;
            }
            return std::sqrt(sum);
        };
        double w = length(3);
        double x = w > 0.0 ? length(0) / w : 0.0, y = w > 0.0 ? length(1) / w : 0.0;
        double kx = 1.0, ky = 1.0;
        double aspect = x > 0.0 ? y / x : 0.0;
        if (y > 0.0 && std::abs(aspect - kScreenAspect * wide) < 0.05 * kScreenAspect * wide) {
            double setting = std::tan(trw::config::get_field_of_view() * 0.5 * kPi / 180.0);
            double angle = 2.0 * std::atan(1.0 / y / setting); // the game's, before the setting
            double alone = std::atan(std::tan(angle) * wide);
            double y_alone = 1.0 / (std::tan(alone * 0.5) * setting);
            ky = y_alone / y;
            kx = ky * wide;
        }
        scale_projection(rdram, from, to, (float)kx, (float)ky);
    }

    // Where a point in the view goes on the screen: `to` plus its distance
    // from `from` times (kx, ky).
    struct Map {
        float from_x, from_y, to_x, to_y, kx, ky;
        int32_t x(uint32_t v) const { return (int32_t)((to_x + ((float)v / 4.0f - from_x) * kx) * 4.0f); }
        int32_t y(uint32_t v) const { return (int32_t)((to_y + ((float)v / 4.0f - from_y) * ky) * 4.0f); }
    };

    // A G_TEXRECT's or G_FILLRECT's rectangle (10.2 fixed point), moved by
    // `map`. Stretched with it (a texture's steps, in the G_RDPHALF_2 after
    // a G_TEXRECT, made smaller to match), or only its centre moved, its size
    // kept.
    void move_rect(uint8_t* rdram, const Command& c, const Map& map, bool keep_size) {
        uint32_t lrx = (c.w0 >> 12) & 0xFFF, lry = c.w0 & 0xFFF;
        uint32_t ulx = (c.w1 >> 12) & 0xFFF, uly = c.w1 & 0xFFF;
        auto clamp12 = [](int32_t v) { return (uint32_t)(v < 0 ? 0 : v > 0xFFF ? 0xFFF : v); };
        int32_t nulx, nuly, nlrx, nlry;
        if (keep_size) {
            int32_t half_w = ((int32_t)lrx - (int32_t)ulx) / 2, half_h = ((int32_t)lry - (int32_t)uly) / 2;
            nulx = map.x(ulx + half_w) - half_w;
            nuly = map.y(uly + half_h) - half_h;
            nlrx = nulx + ((int32_t)lrx - (int32_t)ulx);
            nlry = nuly + ((int32_t)lry - (int32_t)uly);
        }
        else {
            nulx = map.x(ulx);
            nuly = map.y(uly);
            nlrx = map.x(lrx);
            nlry = map.y(lry);
            uint32_t op = c.w0 >> 24;
            uint32_t steps = c.address + 16;
            if ((op == 0xE4 || op == 0xE5) && word(rdram, c.address + 8) >> 24 == 0xE1 && word(rdram, steps) >> 24 == 0xF1) {
                // On whole pixels, as one player's are: the texture starts at
                // its top left, so a rectangle starting part way into a pixel
                // loses part of its top row. (Edges are rounded alike, so a
                // sprite's pieces still meet.)
                auto whole = [](int32_t v) { return ((v + 2) >> 2) << 2; };
                nulx = whole(nulx);
                nuly = whole(nuly);
                nlrx = whole(nlrx);
                nlry = whole(nlry);
                // G_TEXRECTFLIP's s runs down the screen, t across.
                float ks = op == 0xE4 ? map.kx : map.ky, kt = op == 0xE4 ? map.ky : map.kx;
                uint32_t both = word(rdram, steps + 4);
                int32_t dsdx = (int16_t)(both >> 16), dtdy = (int16_t)(both & 0xFFFF);
                dsdx = (int32_t)std::lround(dsdx / ks);
                dtdy = (int32_t)std::lround(dtdy / kt);
                MEM_W(4, (gpr)(int32_t)k0(steps)) = (int32_t)(((uint32_t)(uint16_t)dsdx << 16) | (uint16_t)dtdy);
            }
        }
        uint32_t w0 = (c.w0 & 0xFF000000u) | (clamp12(nlrx) << 12) | clamp12(nlry);
        uint32_t w1 = (c.w1 & 0xFF000000u) | (clamp12(nulx) << 12) | clamp12(nuly);
        MEM_W(0, (gpr)(int32_t)c.address) = (int32_t)w0;
        MEM_W(4, (gpr)(int32_t)c.address) = (int32_t)w1;
    }

    // The game's HUD sprites (func_002AAB58), as they're added to the frame:
    // where their commands start and end, and whether drawn as wide as high.
    // In split screen the game draws its HUD's sprites squashed to the
    // view's shape (a stacked view's at about half height: y scale times
    // 0.496), but not the crosshair or the weapons' sights.
    struct Sprite {
        uint32_t start, end;
        bool plain;
    };
    std::mutex sprite_mutex;
    std::vector<Sprite> sprites;
    constexpr uint32_t kDlHead = 0x8010A574; // where the game adds its next command
}

void trw_own_view_sprite(uint8_t* rdram, float x_scale, float y_scale) {
    std::lock_guard lock{ sprite_mutex };
    if (sprites.size() >= 4096) {
        sprites.clear();
    }
    sprites.push_back({ k0((uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead)), 0, x_scale == y_scale });
}

extern "C" void trw_own_view_sprite_end(uint8_t* rdram) {
    std::lock_guard lock{ sprite_mutex };
    if (!sprites.empty() && sprites.back().end == 0) {
        sprites.back().end = k0((uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead));
    }
}

// From the renderer's send (src/main/main.cpp): `view` is this PC's
// player's (0 the first). Does nothing unless the frame has split views.
void trw_own_view(uint8_t* rdram, uint32_t root, int view) {
    {
        std::lock_guard lock{ placement_mutex };
        placement.valid = false;
    }
    uint32_t buffer = k0(root) >= kDlBufferB ? kDlBufferB : kDlBufferA;
    if (k0(root) < kDlBufferA || k0(root) >= kDlBufferB + kDlBufferSize) {
        return;
    }
    // This frame's sprites, in the order they were added.
    std::vector<Sprite> frame_sprites;
    {
        std::lock_guard lock{ sprite_mutex };
        auto in_frame = [&](const Sprite& s) { return s.start >= buffer && s.start < buffer + kDlBufferSize; };
        std::copy_if(sprites.begin(), sprites.end(), std::back_inserter(frame_sprites), in_frame);
        sprites.erase(std::remove_if(sprites.begin(), sprites.end(), in_frame), sprites.end());
    }
    std::sort(frame_sprites.begin(), frame_sprites.end(), [](const Sprite& a, const Sprite& b) { return a.start < b.start; });
    // Whether a command belongs to a sprite drawn as wide as high (the
    // crosshair, sights). Not all the HUD's rectangles come through the
    // sprite call: the numbers can follow the crosshair's.
    auto plain_sprite = [&](uint32_t address) {
        auto after = std::upper_bound(frame_sprites.begin(), frame_sprites.end(), address,
            [](uint32_t a, const Sprite& s) { return a < s.start; });
        if (after == frame_sprites.begin()) {
            return false;
        }
        const Sprite& sprite = *std::prev(after);
        return sprite.plain && address < sprite.end;
    };

    std::vector<Command> commands = walk(rdram, root);
    if (commands.empty()) {
        log_unhandled("display list too long or too deep to follow");
        return;
    }

    // The viewports, and the frame's full-screen one (the first).
    std::vector<Viewport> viewports;
    uint32_t last_proj = 0;
    uint32_t highest = 0; // the end of what the frame uses of its buffer
    for (const Command& c : commands) {
        uint32_t op = c.w0 >> 24;
        if (c.address >= buffer && c.address < buffer + kDlBufferSize && c.address + 8 > highest) {
            highest = c.address + 8;
        }
        if (op == 0xDA && (c.w0 & 4)) { // G_MTX, projection
            last_proj = c.address;
        }
        if (op == 0xDC && (c.w0 & 0xFF) == 8) { // G_MOVEMEM, the viewport
            uint32_t vp = k0(c.w1);
            viewports.push_back({ c.address, vp, last_proj, c.depth, half(rdram, vp) / 4.0f, half(rdram, vp + 2) / 4.0f,
                half(rdram, vp + 8) / 4.0f, half(rdram, vp + 10) / 4.0f });
            last_proj = 0;
        }
    }
    if (viewports.empty()) {
        return;
    }
    const Viewport full = viewports[0];
    // The split views: top-level viewports smaller than the screen, each
    // after its projection; the full-screen one after them ends the last.
    std::vector<size_t> views;
    size_t overlay = 0;
    for (size_t i = 1; i < viewports.size(); i++) {
        const Viewport& v = viewports[i];
        bool split = v.sx < full.sx - 1.0f || v.sy < full.sy - 1.0f;
        if (split && v.depth == 0 && v.proj_cmd != 0) {
            views.push_back(i);
        }
        else if (!split && !views.empty() && v.depth == 0 && v.proj_cmd != 0) {
            overlay = i;
            break;
        }
    }
    if (views.size() < 2 || overlay == 0 || view < 0 || view >= (int)views.size()) {
        // Split viewports seen, but not as one player's view after another.
        int split = 0;
        for (size_t i = 1; i < viewports.size(); i++) {
            split += viewports[i].sx < full.sx - 1.0f || viewports[i].sy < full.sy - 1.0f ? 1 : 0;
        }
        if (split >= 2) {
            std::string list;
            for (size_t i = 1; i < viewports.size() && i < 9; i++) {
                const Viewport& v = viewports[i];
                char buf[64];
                std::snprintf(buf, sizeof(buf), " %gx%g at %g,%g depth %d%s", v.sx * 2, v.sy * 2, v.tx - v.sx, v.ty - v.sy, v.depth,
                    v.proj_cmd != 0 ? "" : " (no projection)");
                list += buf;
            }
            log_unhandled("%d views, overlay %s; viewports:%s", (int)views.size(), overlay != 0 ? "found" : "not found", list.c_str());
        }
        return;
    }
    auto start_of = [&](size_t k) { return viewports[k < views.size() ? views[k] : overlay].proj_cmd; };

    // Room for the remade projections (64 bytes each): the buffer's spare
    // end, or, when the game has filled it (Frag Tag's two views do), runs of
    // eight of the other views' own commands, which are jumped over (not
    // their first, where the jump goes, nor data they point at: only the
    // commands' own words, in a row at the top level).
    std::vector<uint32_t> slots;
    uint32_t scratch = buffer + kDlBufferSize - kScratchSize;
    if (highest <= scratch) {
        for (uint32_t a = scratch; a + 64 <= buffer + kDlBufferSize; a += 64) {
            slots.push_back(a);
        }
    }
    else {
        for (size_t k = 0; k < views.size(); k++) {
            if ((int)k == view) {
                continue;
            }
            uint32_t from = start_of(k), to = start_of(k + 1);
            bool inside = false;
            uint32_t run = 0, run_length = 0;
            for (const Command& c : commands) {
                if (c.address == from) {
                    inside = true;
                    run_length = 0;
                    continue;
                }
                if (c.address == to) {
                    break;
                }
                if (!inside) {
                    continue;
                }
                bool top_level = c.depth == 0 && c.address >= buffer && c.address < buffer + kDlBufferSize;
                if (top_level && run_length > 0 && c.address == run + run_length * 8) {
                    run_length++;
                }
                else if (top_level) {
                    run = c.address;
                    run_length = 1;
                }
                else {
                    run_length = 0;
                }
                if (run_length == 8) {
                    slots.push_back(run);
                    run_length = 0;
                }
            }
        }
    }

    // This player's view: from its projection load to the next view's.
    const Viewport& mine = viewports[views[view]];
    uint32_t begin = start_of((size_t)view), end = start_of((size_t)view + 1);
    float fx = mine.sx / full.sx, fy = mine.sy / full.sy;
    float x0 = mine.tx - mine.sx, y0 = mine.ty - mine.sy;
    float fx0 = full.tx - full.sx, fy0 = full.ty - full.sy;
    float sx = full.sx / mine.sx, sy = full.sy / mine.sy;
    {
        std::lock_guard lock{ placement_mutex };
        placement = { true, x0, y0, 2.0f * mine.sx, 2.0f * mine.sy, fx0, fy0, sx, sy };
    }
    size_t needed = 0;
    {
        bool in_mine = false;
        for (const Command& c : commands) {
            in_mine = c.address == begin ? true : c.address == end ? false : in_mine;
            needed += in_mine && (c.w0 >> 24) == 0xDA && (c.w0 & 4) && (c.w0 & 2) ? 1 : 0;
        }
    }
    if (slots.size() < needed) {
        log_unhandled("no room for the view's projections (%d needed, %d found)", (int)needed, (int)slots.size());
        std::lock_guard lock{ placement_mutex };
        placement.valid = false;
        return;
    }
    // A fill along one of the view's edges, a few lines thick, most of the
    // edge long (the views' borders).
    auto border_line = [&](const Command& c) {
        float ulx = (float)((c.w1 >> 12) & 0xFFF) / 4.0f, uly = (float)(c.w1 & 0xFFF) / 4.0f;
        float lrx = (float)((c.w0 >> 12) & 0xFFF) / 4.0f, lry = (float)(c.w0 & 0xFFF) / 4.0f;
        float w = 2.0f * mine.sx, h = 2.0f * mine.sy;
        bool across = lrx - ulx >= w * 0.9f && lry - uly <= 4.0f && (uly <= y0 + 4.0f || lry >= y0 + h - 4.0f);
        bool down = lry - uly >= h * 0.9f && lrx - ulx <= 4.0f && (ulx <= x0 + 4.0f || lrx >= x0 + w - 4.0f);
        return across || down;
    };
    bool inside = false;
    size_t next_slot = 0;
    for (const Command& c : commands) {
        if (c.address == begin) {
            inside = true;
        }
        else if (c.address == end) {
            inside = false;
        }
        if (!inside) {
            continue;
        }
        uint32_t op = c.w0 >> 24;
        if (op == 0xDA && (c.w0 & 4) && (c.w0 & 2)) { // a projection loaded
            uint32_t copy = slots[next_slot++];
            single_player_projection(rdram, k0(c.w1), copy, fx / fy);
            MEM_W(4, (gpr)(int32_t)c.address) = (int32_t)copy;
        }
        else if (op == 0xDC && (c.w0 & 0xFF) == 8) { // the viewport: the full screen's
            MEM_W(4, (gpr)(int32_t)c.address) = (int32_t)full.address;
        }
        else if (op == 0xED) { // G_SETSCISSOR: the view's own opened to the screen
            uint32_t uly = c.w0 & 0xFFF, lry = c.w1 & 0xFFF;
            uint32_t ulx = (c.w0 >> 12) & 0xFFF, lrx = (c.w1 >> 12) & 0xFFF;
            bool view_rect = (float)uly / 4.0f >= y0 - 1.0f && (float)lry / 4.0f <= y0 + 2.0f * mine.sy + 1.0f &&
                (float)ulx / 4.0f >= x0 - 1.0f && (float)lrx / 4.0f <= x0 + 2.0f * mine.sx + 1.0f;
            if (view_rect) {
                uint32_t w0 = (c.w0 & 0xFF000000u) | ((uint32_t)(fx0 * 4.0f) << 12) | (uint32_t)(fy0 * 4.0f);
                uint32_t w1 = (c.w1 & 0xFF000000u) | ((uint32_t)((fx0 + 2.0f * full.sx - 1.0f) * 4.0f) << 12) |
                    (uint32_t)((fy0 + 2.0f * full.sy - 1.0f) * 4.0f);
                MEM_W(0, (gpr)(int32_t)c.address) = (int32_t)w0;
                MEM_W(4, (gpr)(int32_t)c.address) = (int32_t)w1;
            }
        }
        else if (op == 0xF6 && border_line(c)) {
            // The thin line the game draws along the view's edges, to part it
            // from the others: not on a screen of its own.
            MEM_W(0, (gpr)(int32_t)c.address) = 0; // G_SPNOOP
            MEM_W(4, (gpr)(int32_t)c.address) = 0;
        }
        else if (op == 0xE4 || op == 0xE5 || op == 0xF6) { // the HUD's rectangles
            // Stretched with the view, as they were squashed to it; the
            // crosshair, which wasn't, kept its size and its distance from
            // the middle.
            if (plain_sprite(c.address)) {
                move_rect(rdram, c, { mine.tx, mine.ty, full.tx, full.ty, 1.0f, 1.0f }, true);
            }
            else {
                move_rect(rdram, c, { x0, y0, fx0, fy0, sx, sy }, false);
            }
        }
    }

    // What's drawn over the whole screen after the views (the match clock)
    // sits where the views meet, in the middle (on the line between stacked
    // views, at the corner of four), squashed as the views' HUDs are: moved
    // to the top and stretched as one player's would be, its digits 20 lines
    // down. Not while paused: the pause list is drawn there too, around the
    // middle of the screen.
    bool paused = MEM_W(0, (gpr)(int32_t)0x801407D4) != 0;
    float seam = 0.0f; // the top of the lower views
    for (size_t k = 0; k < views.size(); k++) {
        const Viewport& v = viewports[views[k]];
        if (v.ty - v.sy > fy0 + 1.0f && (seam == 0.0f || v.ty - v.sy < seam)) {
            seam = v.ty - v.sy;
        }
    }
    std::vector<uint32_t> clock_rects; // moved, so left alone below
    if (!paused && seam != 0.0f) {
        bool after = false;
        for (const Command& c : commands) {
            after |= c.address == viewports[overlay].proj_cmd;
            uint32_t op = c.w0 >> 24;
            if (!after || (op != 0xE4 && op != 0xE5 && op != 0xF6)) {
                continue;
            }
            float top = (float)(c.w1 & 0xFFF) / 4.0f, bottom = (float)(c.w0 & 0xFFF) / 4.0f;
            float left = (float)((c.w1 >> 12) & 0xFFF) / 4.0f, right = (float)((c.w0 >> 12) & 0xFFF) / 4.0f;
            float centre_y = (top + bottom) / 2.0f, centre_x = (left + right) / 2.0f;
            if (centre_y < seam - 12.0f || centre_y > seam + 12.0f || centre_x < full.tx - 48.0f || centre_x > full.tx + 48.0f) {
                continue;
            }
            move_rect(rdram, c, { full.tx, seam, full.tx, fy0 + 27.0f, sx, sy }, false);
            clock_rects.push_back(c.address);
        }
    }

    // The rest drawn over the whole screen that lies within one player's
    // view (Frag Tag's monkey face, flashed in each while the monkey is
    // picked) is that view's: moved as its HUD is if it's this player's, left
    // out if it's another's. Not while paused (the pause list, in the middle,
    // can lie within a view).
    if (!paused) {
        bool after = false;
        for (const Command& c : commands) {
            after |= c.address == viewports[overlay].proj_cmd;
            uint32_t op = c.w0 >> 24;
            if (!after || (op != 0xE4 && op != 0xE5 && op != 0xF6) ||
                std::find(clock_rects.begin(), clock_rects.end(), c.address) != clock_rects.end()) {
                continue;
            }
            float top = (float)(c.w1 & 0xFFF) / 4.0f, bottom = (float)(c.w0 & 0xFFF) / 4.0f;
            float left = (float)((c.w1 >> 12) & 0xFFF) / 4.0f, right = (float)((c.w0 >> 12) & 0xFFF) / 4.0f;
            int within = -1;
            for (size_t k = 0; k < views.size() && within < 0; k++) {
                const Viewport& v = viewports[views[k]];
                float vx0 = v.tx - v.sx, vy0 = v.ty - v.sy;
                if (left >= vx0 - 0.5f && right <= vx0 + 2.0f * v.sx + 0.5f && top >= vy0 - 0.5f && bottom <= vy0 + 2.0f * v.sy + 0.5f) {
                    within = (int)k;
                }
            }
            if (within < 0) {
                continue;
            }
            if (within != view) {
                MEM_W(0, (gpr)(int32_t)c.address) = 0; // G_SPNOOP (a texture rectangle's halves after it do nothing alone)
                MEM_W(4, (gpr)(int32_t)c.address) = 0;
            }
            else if (plain_sprite(c.address)) {
                move_rect(rdram, c, { mine.tx, mine.ty, full.tx, full.ty, 1.0f, 1.0f }, true);
            }
            else {
                move_rect(rdram, c, { x0, y0, fx0, fy0, sx, sy }, false);
            }
        }
    }

    // The other views: jumped over, to the one after.
    for (size_t k = 0; k < views.size(); k++) {
        if ((int)k == view) {
            continue;
        }
        uint32_t from = start_of(k), to = start_of(k + 1);
        MEM_W(0, (gpr)(int32_t)from) = (int32_t)0xDE010000u; // G_DL, branch
        MEM_W(4, (gpr)(int32_t)from) = (int32_t)to;
    }
}

// A HUD piece's centre (x, y, in the 480 x 360 screen), moved as this
// player's HUD is: to the same place on the full screen. 0 if it's in another
// player's view; left as it is if the last frame had no split views.
int trw_own_view_place(float* x, float* y) {
    std::lock_guard lock{ placement_mutex };
    if (!placement.valid) {
        return 1;
    }
    const Placement& p = placement;
    if (*x < p.x0 || *x > p.x0 + p.w || *y < p.y0 || *y > p.y0 + p.h) {
        return 0;
    }
    *x = p.fx0 + (*x - p.x0) * p.sx;
    *y = p.fy0 + (*y - p.y0) * p.sy;
    return 1;
}
