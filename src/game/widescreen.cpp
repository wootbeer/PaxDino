// Widescreen (RT64's Expand aspect ratio) fixes for the game's own drawing.
//
// RT64 widens the 3D view by itself. The HUD is 2D: RT64 keeps 2D rectangles
// inside the centred 4:3 area unless the display list anchors them to a
// screen edge. Around each player's HUD (func_0021EED8, called from
// func_0022A36C) the display list gets RT64 extended-GBI commands that widen
// the scissor to the whole window and turn on automatic anchoring
// (lib-patches/rt64), which puts each HUD rectangle at the nearest edge by
// where it sits: the top corners and the bottom row go to the sides, the
// crosshair and messages stay centred.
//
// The front-end menus are a tree of widgets (drawn by func_0040DB84). Left
// alone, RT64 stretches whichever pieces happen to span the screen and
// centres the rest at 4:3, which pulls the menus apart: the background
// stretches but the images filling holes in it don't (black squares), frame
// pieces stretch while the text inside them doesn't, and preview windows
// drift. So each menu is drawn exactly as in 4:3, and its background images
// are drawn a second time, stretched, into the bars either side.
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "recomp.h"
#include "ultramodern/config.hpp"
#include "trw_menu_cursor.h"

namespace {
    // The display list head the game draws through (gDL), and the game's two
    // display list buffers.
    constexpr uint32_t kDlHead = 0x8010A574;
    constexpr uint32_t kDlBufferA = 0x806D2000;
    constexpr uint32_t kDlBufferB = 0x806E9000;

    constexpr uint32_t kRectAspectAuto = 0;    // G_EX_ASPECT_AUTO
    constexpr uint32_t kRectAspectStretch = 1; // G_EX_ASPECT_STRETCH
    constexpr uint32_t kRectAspectAdjust = 2;  // G_EX_ASPECT_ADJUST

    constexpr uint32_t kOriginLeft = 0x0;    // G_EX_ORIGIN_LEFT
    constexpr uint32_t kOriginCenter = 0x200; // G_EX_ORIGIN_CENTER
    constexpr uint32_t kOriginRight = 0x400; // G_EX_ORIGIN_RIGHT

    // The first player view (its size, floats at +0x29C / +0x2A0, is the
    // whole screen with one player) and the game's video mode width.
    constexpr uint32_t kFirstView = 0x8013F008;
    constexpr uint32_t kViModeWidth = 0x8014B508 + 8;
    constexpr uint32_t kMenuWidth = 284; // the menus' video mode width

    bool hud_anchored = false;
    bool weapon_grouped = false; // the weapon's matrix group is pushed
    uint32_t weapon_view = 0;    // views whose weapon was drawn this frame
    bool view_after_hud = false; // a HUD was drawn: the next colour buffer starts a view
    bool hud_split = false;      // a split screen's HUD is being drawn as in 4:3
    bool hud_scissored = false;  // ... under a full-screen scissor
    bool split_stacked = false;  // last frame's second view spanned the screen's width
    uint32_t hud_view = 0;       // HUDs drawn this frame
    uint32_t last_clear_base = 0; // where the display list was at the last depth clear
    uint32_t last_clear_head = 0;
    uint32_t border_start = 0;
    bool menu_stretched = false;
    uint32_t menu_image_start = 0;
    bool menu_image_full = false; // the image being drawn is a background
    int32_t menu_image_top = 0;
    int32_t menu_image_bottom = 0;
    int32_t menu_image_left = 0;
    int32_t menu_image_right = 0;
    uint32_t menu_root = 0; // the stack pointer of the call drawing the whole menu
    bool menu_background_seen = false; // this menu's background image drawn

    float read_float(uint8_t* rdram, uint32_t addr) {
        uint32_t bits = (uint32_t)MEM_W(0, (gpr)(int32_t)addr);
        float f;
        memcpy(&f, &bits, sizeof(f));
        return f;
    }

    // The game's top-level mode (src/main/fps_controls.cpp): 0-4 boot and
    // title, 8 the front end, 20 between modes; anything else is a match.
    constexpr uint32_t kGameMode = 0x801407B8;

    bool in_match(uint8_t* rdram) {
        uint32_t mode = (uint32_t)MEM_W(0, (gpr)(int32_t)kGameMode);
        return !(mode <= 4 || mode == 8 || mode == 20);
    }

    // The front-end menus' screen, 284 pixels wide: in the front end, and in
    // a match for the options opened from the pause menu, drawn like the
    // front end's (the in-match pause list itself is drawn over the match).
    bool menu_screen(uint8_t* rdram) {
        return !in_match(rdram) || (uint32_t)MEM_W(0, (gpr)(int32_t)kViModeWidth) == kMenuWidth;
    }

    // The display list head must point into RDRAM.
    bool head_valid(uint8_t* rdram) {
        uint32_t head = (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead);
        return head >= 0x80000400u && head < 0x807F0000u && (head & 7) == 0;
    }

    // Human players in the match (ports 0-3 of the player objects at
    // *0x8013EF84, 0x16E8 apart, +0x5D0 set while in it), or 0 if unknown.
    // The most seen since the match began: a player's flag can drop for a
    // moment (respawning, it seems), and with one of two players counted,
    // the first player's HUD was anchored to the window's sides for a frame.
    int match_humans = 0;

    int human_players(uint8_t* rdram) {
        uint32_t first = (uint32_t)MEM_W(0, (gpr)(int32_t)0x8013EF84);
        if (!in_match(rdram)) {
            match_humans = 0;
        }
        if (first < 0x80000400u || first >= 0x80800000u - 4 * 0x16E8u || (first & 3) != 0) {
            return match_humans;
        }
        int count = 0;
        for (int port = 0; port < 4; port++) {
            count += MEM_W(0x5D0, (gpr)(int32_t)(first + port * 0x16E8u)) != 0 ? 1 : 0;
        }
        if (in_match(rdram)) {
            match_humans = std::max(match_humans, count);
            return match_humans;
        }
        return count;
    }

    // The width of the last scissor set in this frame's display list, or 0.
    uint32_t last_scissor_width(uint8_t* rdram) {
        uint32_t head = (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead);
        uint32_t base = head >= kDlBufferB ? kDlBufferB : kDlBufferA;
        for (uint32_t a = head; a > base && a + 0x8000 > head; a -= 8) {
            uint32_t w0 = (uint32_t)MEM_W(0, (gpr)(int32_t)(a - 8));
            if ((w0 >> 24) == 0xED) { // G_SETSCISSOR
                uint32_t w1 = (uint32_t)MEM_W(4, (gpr)(int32_t)(a - 8));
                return (((w1 >> 12) & 0xFFF) - ((w0 >> 12) & 0xFFF)) / 4;
            }
        }
        return 0;
    }

    // One player's view filling the screen. Also by the number of human
    // players: the first view's size alone took a split screen for one (its
    // HUDs were anchored to the screen's sides).
    bool single_full_screen_view(uint8_t* rdram) {
        float w = read_float(rdram, kFirstView + 0x29C);
        float h = read_float(rdram, kFirstView + 0x2A0);
        float vi_width = (float)(uint32_t)MEM_W(0, (gpr)(int32_t)kViModeWidth);
        return vi_width > 0.0f && w >= vi_width - 1.0f && h >= w * 0.6f && human_players(rdram) <= 1;
    }

    struct DlWriter {
        uint8_t* rdram;
        gpr head;
        void cmd(uint32_t w0, uint32_t w1) {
            MEM_W(0, head) = (int32_t)w0;
            MEM_W(4, head) = (int32_t)w1;
            head += 8;
        }
    };

    DlWriter dl_at_head(uint8_t* rdram) {
        return DlWriter{ rdram, (gpr)(int32_t)(uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead) };
    }

    void finish(uint8_t* rdram, const DlWriter& dl) {
        MEM_W(0, (gpr)(int32_t)kDlHead) = (int32_t)(uint32_t)dl.head;
    }
}

static void trw_weapon_end(uint8_t* rdram);

// True while the picture is expanded past 4:3.
extern "C" int trw_widescreen_active(void) {
    return ultramodern::renderer::get_graphics_config().ar_option == ultramodern::renderer::AspectRatio::Expand;
}

// The border around each player's view (func_002372B4): its fill rectangles
// at the screen's left and right edges are dropped in Expand, where they'd
// show as lines at the old 4:3 edges.
extern "C" void trw_border_begin(uint8_t* rdram) {
    border_start = head_valid(rdram) ? (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead) : 0;
}

extern "C" void trw_border_end(uint8_t* rdram) {
    uint32_t start = border_start;
    border_start = 0;
    if (start == 0 || !trw_widescreen_active() || !head_valid(rdram)) {
        return;
    }
    uint32_t end = (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead);
    uint32_t width = (uint32_t)MEM_W(0, (gpr)(int32_t)kViModeWidth) * 4; // 10.2
    if (end <= start || end - start > 0x400 || width == 0) {
        return;
    }
    for (uint32_t a = start; a < end; a += 8) {
        gpr cmd = (gpr)(int32_t)a;
        uint32_t w0 = (uint32_t)MEM_W(0, cmd);
        uint32_t w1 = (uint32_t)MEM_W(4, cmd);
        if ((w0 >> 24) != 0xF6) { // G_FILLRECT
            continue;
        }
        uint32_t lrx = (w0 >> 12) & 0xFFF, lry = w0 & 0xFFF;
        uint32_t ulx = (w1 >> 12) & 0xFFF, uly = w1 & 0xFFF;
        bool vertical = (lry - uly) > (lrx - ulx);
        bool at_side = ulx <= 4 * 4 || lrx + 4 * 4 >= width;
        if (vertical && at_side) {
            MEM_W(0, cmd) = 0; // G_SPNOOP
            MEM_W(4, cmd) = 0;
        }
    }
}

// Before a player's HUD is drawn: with one player, its pieces near the top
// and bottom of the screen are anchored to the window's sides; with two
// above each other, each HUD is drawn as in 4:3. Side by side and with
// three or four players, the HUDs are left to RT64.
extern "C" void trw_pause_hold_wheels(void);

// Whether the last finished match frame drew a HUD: not during the
// fly-through at the start of a level (src/main/fps_controls.cpp skips it),
// nor while a level loads (cleared outside matches). The controls only
// write to the player (mouse look, the wheel) while it's set.
static std::atomic<bool> hud_this_frame{ false };
static std::atomic<bool> hud_last_frame{ false };

extern "C" int trw_match_hud_drawn(void) {
    return hud_last_frame.load();
}

extern "C" void trw_hud_begin(uint8_t* rdram, gpr hud_player) {
    hud_this_frame.store(true);
    view_after_hud = true;
    trw_pause_hold_wheels();
    trw_weapon_end(rdram);
    hud_anchored = false;
    hud_split = false;
    hud_scissored = false;
    uint32_t view = hud_view++;
    // The HUD's own player (its s0, 0x16E8 apart from the first) gives its
    // view when it can: counting the HUDs drawn since the frame began is
    // thrown by a frame start missed or seen twice.
    {
        uint32_t first = (uint32_t)MEM_W(0, (gpr)(int32_t)0x8013EF84);
        uint32_t player = (uint32_t)hud_player;
        if (first >= 0x80000400u && player >= first && player < first + 4 * 0x16E8u && (player - first) % 0x16E8u == 0) {
            view = (player - first) / 0x16E8u;
        }
    }
    if (!trw_widescreen_active() || !in_match(rdram) || !head_valid(rdram)) {
        return;
    }
    int humans = human_players(rdram);
    if (humans == 2 && view == 1) {
        // The second view's own scissor: the whole width above each other,
        // half of it side by side.
        uint32_t vi_width = (uint32_t)MEM_W(0, (gpr)(int32_t)kViModeWidth);
        split_stacked = last_scissor_width(rdram) + 4 >= vi_width;
    }
    if (humans >= 2) {
        if (humans != 2 || !split_stacked) {
            return;
        }
        // Split screen: each HUD as in 4:3 (rectangles held to the 4:3
        // middle). RT64 widens a pass (from one colour buffer to the next)
        // only where its scissors add up to the screen's shape: the second
        // view's did, the first's (its view, the top half) didn't, so its
        // HUD was stretched with the pass whatever the rect aspect. The
        // first HUD is drawn under a full-screen scissor, which widens its
        // pass like the second's. Anchoring each HUD to its view instead
        // shook the second one with frame interpolation.
        DlWriter dl = dl_at_head(rdram);
        dl.cmd(0xE0525464, 0x10000064);           // gEXEnable
        dl.cmd(0x64000034, 0);                    // gEXSetAutoAnchor(off)
        dl.cmd(0x64000033, kRectAspectAdjust);    // gEXSetRectAspect
        if (view == 0) {
            uint32_t width = (uint32_t)read_float(rdram, kFirstView + 0x29C);
            uint32_t height = (uint32_t)read_float(rdram, kFirstView + 0x2A0);
            dl.cmd(0x64000017, 0);                // gEXPushScissor
            dl.cmd(0xED000000, ((width - 1) * 4) << 12 | ((height - 1) * 4)); // G_SETSCISSOR
            hud_scissored = true;
        }
        finish(rdram, dl);
        hud_split = true;
        return;
    }
    if (!single_full_screen_view(rdram)) {
        return;
    }
    uint32_t height = (uint32_t)read_float(rdram, kFirstView + 0x2A0);
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable (G_SPNOOP + RT64 magic, opcode 0x64)
    dl.cmd(0x64000017, 0);          // gEXPushScissor
    // gEXSetScissor(G_SC_NON_INTERLACE, LEFT, RIGHT, 0, 0, -1, height - 1):
    // the whole window, ending where the game's own full-screen scissor ends
    // (a pixel short of the right and bottom edges). Not taller: RT64 writes
    // back as many rows as were drawn to, and rows past the end of the last
    // framebuffer overwrote the game's heap (allocator crashes in
    // func_002559D4). Not wider: RT64 widens the view's 3D only where it
    // spans everything drawn in the pass, and the weapon (drawn just before)
    // was clipped to 4:3.
    dl.cmd(0x64000005, (kOriginLeft << 2) | (kOriginRight << 14));
    dl.cmd(0, (((uint32_t)-4 & 0xFFFF) << 16) | ((height - 1) * 4));
    dl.cmd(0x64000034, 1);          // gEXSetAutoAnchor(on) (lib-patches/rt64)
    finish(rdram, dl);
    hud_anchored = true;
}

// Around the weapon wheel (func_002183E8, drawn inside the HUD as 48x48
// tiles of each icon and its frame) and the team select after it
// (func_00218F08): anchoring off, so they stay centred. As the wheel turns,
// its tiles crossing into the top quarter were sent to the screen's sides,
// and the icons came apart; so was the team select's top team.
extern "C" void trw_hud_anchor(uint8_t* rdram, int on) {
    if (!hud_anchored || !head_valid(rdram)) {
        return;
    }
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064);     // gEXEnable
    dl.cmd(0x64000034, on ? 1u : 0u);   // gEXSetAutoAnchor
    finish(rdram, dl);
}

// The name of a kill by the skulls (the HUD routine, after it sets the
// name's colour at 0x0021FEB0; `entry` is the player plus 4 x the entry,
// its fade at +0x12F4, 255 down to 0). The text routine (func_002A8820)
// mixes the prim colour (the name's) and the env colour (here 1,1,1) by a
// second texture whose tile has a line width of 0; RT64 skips such a tile,
// so the mix came out at the dark env colour, translucent. The game's other
// faded texts set both colours to the same grey, so the mix doesn't matter
// and the grey fades from white to black; the name gets the same: both
// colours its own scaled by the fade. The colour (r, g, b, then the env's
// three) is at 0x80135798 (func_002AAA6C).
extern "C" void trw_kill_name_colour(uint8_t* rdram, gpr entry) {
    const gpr colour = (gpr)(int32_t)0x80135798;
    uint32_t fade = std::min<uint32_t>((uint32_t)MEM_W(0x12F4, entry), 255);
    for (int i = 0; i < 3; i++) {
        uint32_t c = std::min<uint32_t>((uint32_t)MEM_W(i * 4, colour), 255) * fade / 255;
        MEM_W(i * 4, colour) = (int32_t)c;
        MEM_W(0xC + i * 4, colour) = (int32_t)c;
    }
}

// After the HUD.
extern "C" void trw_hud_end(uint8_t* rdram) {
    if (hud_split) {
        hud_split = false;
        if (head_valid(rdram)) {
            DlWriter dl = dl_at_head(rdram);
            dl.cmd(0xE0525464, 0x10000064);       // gEXEnable
            dl.cmd(0x64000033, kRectAspectAuto);  // gEXSetRectAspect
            if (hud_scissored) {
                dl.cmd(0x64000018, 0);            // gEXPopScissor
            }
            finish(rdram, dl);
        }
        return;
    }
    if (!hud_anchored) {
        return;
    }
    hud_anchored = false;
    if (!head_valid(rdram)) {
        return;
    }
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x64000034, 0);          // gEXSetAutoAnchor(off)
    dl.cmd(0x64000018, 0);          // gEXPopScissor
    finish(rdram, dl);
}

namespace {
    // Menu widgets: +0x0 parent, +0x4 next sibling, +0x8 first child (in a
    // container), halfword +0xE the type (3 an image),
    // halfwords +0x14 / +0x16 the position, +0x18 / +0x1A the size.
    constexpr uint32_t kWidgetImage = 3;

    // Half the width of the framebuffer the menus draw to (284 pixels).
    constexpr int32_t kMenuHalfWidth = 142;
    constexpr uint32_t kMenuHeight = 222;

    // Where the bars end, in quarter pixels from the centre: 3 pixels inside
    // the 4:3 area, under the image drawn there, as RT64 puts that image's
    // edges a few pixels off the centre-anchored ones (a gap showed as dark
    // lines).
    constexpr int32_t kBarOverlap = 3;
    constexpr int32_t kBarInnerEdge = (kMenuHalfWidth - kBarOverlap) * 4;

    // Black at 77% over the menus' backgrounds in the bars.
    constexpr uint32_t kBarShade = 0x000000C4;


    bool valid_widget(uint32_t addr) {
        return addr >= 0x80000400u && addr < 0x807FFF00u && (addr & 3) == 0;
    }

    bool full_screen_image(uint8_t* rdram, uint32_t widget) {
        gpr w = (gpr)(int32_t)widget;
        return MEM_HU(w, 0xE) == kWidgetImage && MEM_HU(w, 0x18) >= 280 && MEM_HU(w, 0x1A) >= 160;
    }

    struct Box {
        int32_t x0, y0, x1, y1;
    };
    Box widget_box(uint8_t* rdram, uint32_t widget) {
        gpr w = (gpr)(int32_t)widget;
        int32_t x = (int16_t)MEM_HU(w, 0x14);
        int32_t y = (int16_t)MEM_HU(w, 0x16);
        return Box{ x, y, x + MEM_HU(w, 0x18), y + MEM_HU(w, 0x1A) };
    }
}

static void set_rect_aspect(uint8_t* rdram, uint32_t aspect) {
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x64000033, aspect);     // gEXSetRectAspect
    finish(rdram, dl);
}

// gEXSetScissor(G_SC_NON_INTERLACE, left origin, right origin, ulx, uly, lrx,
// lry), x in quarter pixels from each origin, y in pixels.
static void set_scissor(DlWriter& dl, uint32_t left_origin, uint32_t right_origin, int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    dl.cmd(0x64000005, (left_origin << 2) | (right_origin << 14));
    dl.cmd((((uint32_t)ulx & 0xFFFF) << 16) | ((uint32_t)(uly * 4) & 0xFFFF), (((uint32_t)lrx & 0xFFFF) << 16) | ((uint32_t)(lry * 4) & 0xFFFF));
}

// RT64's frame interpolation (Refresh Rate above the game's 30 fps). By
// default (AUTO components) RT64 stops interpolating a matrix's position on
// any frame where its movement jumps tenfold over the last frame, so things
// moving unevenly flip between smoothed and snapped frames: jitter. The
// decomposed components below ("decomposed normal", as Banjo-Kazooie:
// Recompiled tags its objects) always interpolate position, rotation, scale,
// skew and perspective; vertices and texcoords skipped, look-at automatic.
constexpr uint32_t kIdAuto = 0xFFFFFFFF; // G_EX_ID_AUTO

static uint32_t matrix_group_params(bool push, bool order_auto) {
    const uint32_t interpolate = 1, autos = 2;
    return (push ? 1u : 0u) | (1u << 2) /* decompose */ |
        (interpolate << 3) | (interpolate << 5) | (interpolate << 7) | (interpolate << 9) | (interpolate << 11) |
        (interpolate << 15) /* tiles */ | ((order_auto ? 1u : 0u) << 17) | (autos << 24);
}

// At the start of a match frame: give the base of RT64's model matrix-group
// stack (reset every frame) these components, with automatic matching, for
// everything drawn (players, projectiles, the level, the fly-through camera).
static void set_frame_base_group(uint8_t* rdram) {
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x6400000C, kIdAuto);    // gEXMatrixGroup, no push: replaces the base entry
    dl.cmd(matrix_group_params(false, true), 0);
    finish(rdram, dl);
}

// At the start of a split screen's second (third, fourth) view: its own
// base group, so RT64 matches its things with the same view's last frame,
// not the first view's (they're drawn once per view, and auto matching
// paired them across views: the second player's view jittered).
constexpr uint32_t kViewGroupId = 0x7EA80000;

static void set_view_base_group(uint8_t* rdram, uint32_t view) {
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x6400000C, kViewGroupId + view); // gEXMatrixGroup, no push: replaces the base entry
    dl.cmd(matrix_group_params(false, true), 0);
    finish(rdram, dl);
}

// The weapon, relative to the camera. The game draws the first-person
// weapon as a model in the world, just in front of the camera: its matrices
// place it in the world, and the projection (the view's, loaded before the
// world) carries the camera. RT64 interpolates the two apart, the weapon's
// matrices on their own and the camera with the projection, so on a fast
// turn the weapon, moved along a straight line between frames while the
// camera turns along an arc, came loose from the view and shook. So the
// weapon is drawn with a projection of its own, the view's split into the
// camera and a plain perspective: the camera goes into copies of the
// weapon's matrices (and its lights' directions, which are in the same
// space), and the perspective is loaded for the weapon. What RT64 then
// interpolates is the weapon's position relative to the view (its sway).
// The picture of each real frame stays the same.
namespace {
    struct Mat4 {
        float m[4][4];
    };

    constexpr uint32_t kDlBuffersEnd = kDlBufferB + 0x17000;

    Mat4 mat_mul(const Mat4& a, const Mat4& b) {
        Mat4 r;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
            }
        }
        return r;
    }

    // An Mtx: the 16 integer halves, row by row, then the 16 fractions.
    Mat4 read_mtx(uint8_t* rdram, uint32_t addr) {
        gpr a = (gpr)(int32_t)addr;
        Mat4 r;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                int32_t v = (int32_t)(((uint32_t)(uint16_t)MEM_H(i * 8 + j * 2, a) << 16) | (uint16_t)MEM_HU(32 + i * 8 + j * 2, a));
                r.m[i][j] = (float)v / 65536.0f;
            }
        }
        return r;
    }

    void write_mtx(uint8_t* rdram, uint32_t addr, const Mat4& m) {
        gpr a = (gpr)(int32_t)addr;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float f = std::clamp(m.m[i][j] * 65536.0f, -2147483648.0f, 2147483520.0f);
                int32_t v = (int32_t)lrintf(f);
                MEM_H(i * 8 + j * 2, a) = (int16_t)(v >> 16);
                MEM_H(32 + i * 8 + j * 2, a) = (int16_t)(v & 0xFFFF);
            }
        }
    }

    // Splits a projection with the camera in it into the camera (view) and a
    // perspective of the form guPerspective makes (w = z):
    //   a 0 0 0
    //   0 b 0 0
    //   0 0 c 1
    //   0 0 d 0
    // with view * proj == vp. False when vp has no camera in it.
    bool split_view_proj(const Mat4& vp, Mat4& view, Mat4& proj) {
        const auto& m = vp.m;
        if (std::fabs(m[3][3]) < 1e-6f) {
            return false;
        }
        float a = std::sqrt(m[0][0] * m[0][0] + m[1][0] * m[1][0] + m[2][0] * m[2][0]);
        float b = std::sqrt(m[0][1] * m[0][1] + m[1][1] * m[1][1] + m[2][1] * m[2][1]);
        int k = 0;
        for (int i = 1; i < 3; i++) {
            if (std::fabs(m[i][3]) > std::fabs(m[k][3])) {
                k = i;
            }
        }
        if (a < 1e-6f || b < 1e-6f || std::fabs(m[k][3]) < 1e-3f) {
            return false;
        }
        float c = m[k][2] / m[k][3];
        float d = m[3][2] - c * m[3][3];
        if (std::fabs(d) < 1e-6f) {
            return false;
        }
        proj = Mat4{ { { a, 0, 0, 0 }, { 0, b, 0, 0 }, { 0, 0, c, 1 }, { 0, 0, d, 0 } } };
        for (int i = 0; i < 4; i++) {
            view.m[i][0] = m[i][0] / a;
            view.m[i][1] = m[i][1] / b;
            view.m[i][2] = m[i][3];
            view.m[i][3] = (m[i][2] - c * m[i][3]) / d;
        }
        return true;
    }

    bool in_frame_dl(uint32_t addr) {
        return addr >= kDlBufferA && addr < kDlBuffersEnd;
    }

    // The projection loaded last before `end` at the top level of the frame's
    // display list, or 0.
    uint32_t last_projection(uint8_t* rdram, uint32_t end) {
        uint32_t base = end >= kDlBufferB ? kDlBufferB : kDlBufferA;
        uint32_t found = 0;
        for (uint32_t a = base; a < end; a += 8) {
            uint32_t w0 = (uint32_t)MEM_W(0, (gpr)(int32_t)a);
            uint32_t w1 = (uint32_t)MEM_W(4, (gpr)(int32_t)a);
            if ((w0 >> 24) == 0xDA && (((w0 & 0xFF) ^ 1) & 4)) { // G_MTX, projection
                found = ((((w0 & 0xFF) ^ 1) & 2) && w1 >= 0x80000000u) ? w1 : 0; // loaded from a known address
            }
        }
        return found;
    }

    // State between the weapon's start and end.
    struct WeaponView {
        uint32_t proj_cmd;   // our load of the plain perspective, 0 if none
        uint32_t view_proj;  // the view's projection
        uint32_t start;      // the weapon's first command
        Mat4 view;
    };
    WeaponView weapon_camera{};

    // What the weapon's commands refer to, found by walking them.
    struct SegmentUse {
        uint32_t cmd;         // the G_MOVEWORD setting the segment, in the frame's list
        uint32_t base;
        uint32_t lo, hi;      // the matrices' span (offsets)
        bool other;           // also used for something else
    };
    struct WeaponWalk {
        uint8_t* rdram;
        uint32_t seg_base[16];
        int seg_use[16];      // index into uses, -1 none (set outside the frame's list or unset)
        SegmentUse uses[4];
        int use_count = 0;
        struct Ref {
            int use;          // -1: a direct address, the command repointed
            uint32_t cmd, addr;
        };
        Ref mtx[48];
        int mtx_count = 0;
        Ref lights[16];       // light (and look-at) loads, always repointed
        uint32_t light_size[16];
        int light_count = 0;
        bool modelview_loaded = false;
        bool ok = true;
        bool done = false;            // reached the end of the weapon
        bool projection_after = false; // the game loads a projection right after it

        bool resolve(uint32_t addr, uint32_t& out) const {
            if (addr >= 0x80000000u) {
                out = addr;
                return true;
            }
            uint32_t seg = (addr >> 24) & 0xF;
            if (seg != 0 && seg_base[seg] == 0xFFFFFFFFu) {
                return false;
            }
            out = 0x80000000u | ((seg == 0 ? 0 : seg_base[seg]) + (addr & 0xFFFFFF));
            return true;
        }

        // Something other than a matrix through a segment: if the segment
        // also has matrices, which get copied, it can't be moved to the copy.
        void check_other_use(uint32_t w1) {
            if (w1 < 0x80000000u && seg_use[(w1 >> 24) & 0xF] >= 0) {
                uses[seg_use[(w1 >> 24) & 0xF]].other = true;
            }
        }

        void walk(uint32_t addr, uint32_t end, int depth) {
            for (int n = 0; n < 4096 && ok && !done; n++) {
                // Past what's been written of the frame's list: the end.
                if (in_frame_dl(addr) && addr >= end) {
                    done = true;
                    return;
                }
                if (addr < 0x80000400u || addr >= 0x807FFFF8u) {
                    ok = false;
                    return;
                }
                uint32_t cmd = addr;
                uint32_t w0 = (uint32_t)MEM_W(0, (gpr)(int32_t)addr);
                uint32_t w1 = (uint32_t)MEM_W(4, (gpr)(int32_t)addr);
                addr += 8;
                switch (w0 >> 24) {
                case 0xDB: // G_MOVEWORD
                    if (((w0 >> 16) & 0xFF) == 6) { // G_MW_SEGMENT
                        uint32_t seg = ((w0 & 0xFFFF) / 4) & 0xF;
                        seg_base[seg] = w1 & 0xFFFFFF;
                        seg_use[seg] = -1;
                        if (in_frame_dl(cmd) && use_count < 4) {
                            uses[use_count] = SegmentUse{ cmd, w1 & 0xFFFFFF, 0xFFFFFFFFu, 0, false };
                            seg_use[seg] = use_count++;
                        }
                    }
                    break;
                case 0xDA: { // G_MTX
                    uint32_t params = (w0 & 0xFF) ^ 1;
                    if (params & 4) { // the game's next projection (the HUD's): the weapon's done
                        projection_after = true;
                        done = true;
                        return;
                    }
                    if (!(params & 2)) { // multiplied onto a loaded one: fine as it is
                        break;
                    }
                    if (mtx_count >= 48) {
                        ok = false;
                        return;
                    }
                    int use = w1 < 0x80000000u ? seg_use[(w1 >> 24) & 0xF] : -1;
                    uint32_t source;
                    if (!resolve(w1, source) || (use < 0 && !in_frame_dl(cmd))) {
                        ok = false;
                        return;
                    }
                    if (use >= 0) {
                        uint32_t off = w1 & 0xFFFFFF;
                        uses[use].lo = std::min(uses[use].lo, off);
                        uses[use].hi = std::max(uses[use].hi, off + 64);
                    }
                    mtx[mtx_count++] = Ref{ use, cmd, use >= 0 ? (w1 & 0xFFFFFF) : source };
                    modelview_loaded = true;
                    break;
                }
                case 0xDC: { // G_MOVEMEM
                    check_other_use(w1);
                    if ((w0 & 0xFF) == 0x0A) { // G_MV_LIGHT: lights and look-at, directions to turn
                        uint32_t source;
                        if (light_count >= 16 || !in_frame_dl(cmd) || !resolve(w1, source)) {
                            ok = false;
                            return;
                        }
                        light_size[light_count] = (((w0 >> 19) & 0x1F) + 1) * 8;
                        lights[light_count++] = Ref{ -1, cmd, source };
                    }
                    break;
                }
                case 0x01: // G_VTX
                    check_other_use(w1);
                    if (!modelview_loaded) { // drawn with a matrix from before the weapon
                        ok = false;
                        return;
                    }
                    break;
                case 0xFD: // G_SETTIMG
                    check_other_use(w1);
                    break;
                case 0xDE: { // G_DL
                    check_other_use(w1);
                    uint32_t target;
                    if (depth >= 8 || !resolve(w1, target)) {
                        ok = false;
                        return;
                    }
                    walk(target, end, depth + 1);
                    if (((w0 >> 16) & 0xFF) == 1) { // branch: no return
                        return;
                    }
                    break;
                }
                case 0xDF: // G_ENDDL
                    if (depth > 0) {
                        return;
                    }
                    break;
                }
            }
            if (depth > 0 && !done) {
                ok = false; // ran on without an end
            }
        }
    };

    // Copies the weapon's matrices with the camera put in, and its lights
    // turned to match, into data at the display list's head, and points
    // the weapon's commands at them. False (nothing changed) when the
    // weapon's commands aren't the expected kind.
    bool weapon_to_camera_space(uint8_t* rdram, uint32_t start, uint32_t end, const Mat4& view, bool& projection_after) {
        WeaponWalk w{};
        w.rdram = rdram;
        for (int i = 0; i < 16; i++) {
            w.seg_base[i] = 0xFFFFFFFFu;
            w.seg_use[i] = -1;
        }
        w.walk(start, end, 0);
        if (!w.ok || w.mtx_count == 0) {
            return false;
        }
        projection_after = w.projection_after;
        // Lay out the data: each segment's span of matrices, then the
        // directly addressed matrices, then the lights.
        uint32_t size = 0;
        uint32_t use_data[4];
        for (int i = 0; i < w.use_count; i++) {
            if (w.uses[i].lo == 0xFFFFFFFFu) {
                continue;
            }
            if (w.uses[i].other || w.uses[i].hi - w.uses[i].lo > 0x2000) {
                return false;
            }
            use_data[i] = size;
            size += (w.uses[i].hi - w.uses[i].lo + 7) & ~7u;
        }
        uint32_t mtx_data = size;
        for (int i = 0; i < w.mtx_count; i++) {
            if (w.mtx[i].use < 0) {
                size += 64;
            }
        }
        uint32_t light_data = size;
        for (int i = 0; i < w.light_count; i++) {
            size += (w.light_size[i] + 7) & ~7u;
        }
        uint32_t head = (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead);
        uint32_t data = head + 8;
        if (data + size + 0x100 > ((head >= kDlBufferB) ? kDlBuffersEnd : kDlBufferB)) {
            return false;
        }

        // gSPBranchList over the data.
        MEM_W(0, (gpr)(int32_t)head) = (int32_t)0xDE010000u;
        MEM_W(4, (gpr)(int32_t)head) = (int32_t)(data + size);
        for (int i = 0; i < w.use_count; i++) {
            const SegmentUse& u = w.uses[i];
            if (u.lo == 0xFFFFFFFFu) {
                continue;
            }
            for (uint32_t off = u.lo; off < u.hi; off += 4) {
                MEM_W(0, (gpr)(int32_t)(data + use_data[i] + off - u.lo)) = MEM_W(0, (gpr)(int32_t)(0x80000000u | (u.base + off)));
            }
            // The segment now starts at the copy, back to the original after the weapon.
            MEM_W(4, (gpr)(int32_t)u.cmd) = (int32_t)((data + use_data[i] - u.lo) & 0xFFFFFF);
        }
        uint32_t next_mtx = data + mtx_data;
        for (int i = 0; i < w.mtx_count; i++) {
            const WeaponWalk::Ref& r = w.mtx[i];
            if (r.use >= 0) {
                const SegmentUse& u = w.uses[r.use];
                uint32_t copy = data + use_data[r.use] + r.addr - u.lo;
                write_mtx(rdram, copy, mat_mul(read_mtx(rdram, 0x80000000u | (u.base + r.addr)), view));
            }
            else {
                write_mtx(rdram, next_mtx, mat_mul(read_mtx(rdram, r.addr), view));
                MEM_W(4, (gpr)(int32_t)r.cmd) = (int32_t)next_mtx;
                next_mtx += 64;
            }
        }
        // Light directions: signed bytes at +8 of each 16-byte light (and look-at).
        float turn[3][3];
        for (int i = 0; i < 3; i++) {
            float len = std::sqrt(view.m[i][0] * view.m[i][0] + view.m[i][1] * view.m[i][1] + view.m[i][2] * view.m[i][2]);
            for (int j = 0; j < 3; j++) {
                turn[i][j] = len > 1e-6f ? view.m[i][j] / len : (i == j ? 1.0f : 0.0f);
            }
        }
        uint32_t next_light = data + light_data;
        for (int i = 0; i < w.light_count; i++) {
            uint32_t src = w.lights[i].addr, dst = next_light, n = w.light_size[i];
            for (uint32_t b = 0; b < n; b++) {
                MEM_B(b, (gpr)(int32_t)dst) = MEM_B(b, (gpr)(int32_t)src);
            }
            for (uint32_t l = 0; l + 16 <= n; l += 16) {
                float d[3];
                for (int j = 0; j < 3; j++) {
                    d[j] = (float)(int8_t)MEM_B(l + 8 + j, (gpr)(int32_t)src);
                }
                for (int j = 0; j < 3; j++) {
                    float v = d[0] * turn[0][j] + d[1] * turn[1][j] + d[2] * turn[2][j];
                    MEM_B(l + 8 + j, (gpr)(int32_t)dst) = (int8_t)std::clamp(lrintf(v), -127l, 127l);
                }
            }
            MEM_W(4, (gpr)(int32_t)w.lights[i].cmd) = (int32_t)dst;
            next_light += (n + 7) & ~7u;
        }

        // After the weapon: the segments as they were, and the view's projection.
        gpr h = (gpr)(int32_t)(data + size);
        for (int i = 0; i < w.use_count; i++) {
            if (w.uses[i].lo == 0xFFFFFFFFu) {
                continue;
            }
            MEM_W(0, h) = MEM_W(0, (gpr)(int32_t)w.uses[i].cmd);
            MEM_W(4, h) = (int32_t)w.uses[i].base;
            h += 8;
        }
        MEM_W(0, (gpr)(int32_t)kDlHead) = (int32_t)(uint32_t)h;
        return true;
    }
}

// The first-person weapon. In a match each view draws its weapon after a
// second depth clear (func_00291B00), then its HUD. RT64 matches each
// frame's matrices to the last frame's by guessing unless they're tagged,
// and the weapon's (a skinned model, its bones in a matrix palette) jumped
// around at high frame rates. They're put in a group with a fixed id per
// view (G_EX_ORDER_LINEAR).
constexpr uint32_t kWeaponGroupId = 0x7EA90000;

static void trw_weapon_begin(uint8_t* rdram) {
    trw_weapon_end(rdram); // the last view's, if its HUD didn't end it
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x6400000C, kWeaponGroupId + weapon_view);
    dl.cmd(matrix_group_params(true, false), 0);
    // The perspective without the camera, in the list behind a branch.
    weapon_camera.proj_cmd = 0;
    weapon_camera.view_proj = last_projection(rdram, (uint32_t)dl.head);
    Mat4 proj;
    if (weapon_camera.view_proj != 0 && split_view_proj(read_mtx(rdram, weapon_camera.view_proj), weapon_camera.view, proj)) {
        uint32_t data = (uint32_t)dl.head + 8;
        dl.cmd(0xDE010000, data + 64); // gSPBranchList
        write_mtx(rdram, data, proj);
        dl.head += 64;
        weapon_camera.proj_cmd = (uint32_t)dl.head;
        dl.cmd(0xDA380007, data);      // gSPMatrix(G_MTX_PROJECTION | G_MTX_LOAD)
    }
    finish(rdram, dl);
    weapon_camera.start = (uint32_t)dl.head;
    weapon_grouped = true;
    weapon_view++;
}

// Before a view's HUD.
static void trw_weapon_end(uint8_t* rdram) {
    if (!weapon_grouped) {
        return;
    }
    weapon_grouped = false;
    if (!head_valid(rdram)) {
        return;
    }
    if (weapon_camera.proj_cmd != 0) {
        uint32_t end = (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead);
        bool projection_after = false;
        if (end > weapon_camera.start && weapon_to_camera_space(rdram, weapon_camera.start, end, weapon_camera.view, projection_after)) {
            if (!projection_after) {
                DlWriter dl = dl_at_head(rdram);
                dl.cmd(0xDA380007, weapon_camera.view_proj); // the view's projection again
                finish(rdram, dl);
            }
        }
        else {
            // Left as the game drew it: drop our perspective.
            MEM_W(0, (gpr)(int32_t)weapon_camera.proj_cmd) = (int32_t)0xE7000000u; // gDPPipeSync
            MEM_W(4, (gpr)(int32_t)weapon_camera.proj_cmd) = 0;
        }
        weapon_camera.proj_cmd = 0;
    }
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x6400000D, 0x001);      // gEXPopMatrixGroup(model)
    finish(rdram, dl);
}

// Clears the colour buffer just set (width x height) to black.
static void clear_colour_buffer(uint8_t* rdram, uint32_t width, uint32_t height, bool scissor_inset) {
    uint32_t right = width - 1, bottom = height - 1;
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x64000019, 0);          // gEXPushOtherMode
    dl.cmd(0x64000025, 0);          // gEXPushFillColor
    dl.cmd(0x64000017, 0);          // gEXPushScissor
    // gDPSetScissor: in a match ending a pixel short right and bottom, as the
    // game's own do: RT64 widens a pass's 3D only where it spans the union of
    // its scissors, and a wider one kept the match's 3D at 4:3. The menus'
    // passes are judged against the whole framebuffer (a short one there put
    // the trial select's 4:3 picture out of place). The fill becomes a clear
    // of the whole framebuffer either way.
    uint32_t sx = scissor_inset ? right : width, sy = scissor_inset ? bottom : height;
    dl.cmd(0xED000000, (sx * 4 << 12) | (sy * 4));
    dl.cmd(0xE3000A01, 0x00300000); // gDPSetCycleType(G_CYC_FILL)
    dl.cmd(0xF7000000, 0x00010001); // gDPSetFillColor(black)
    dl.cmd(0xF6000000 | (right * 4 << 12) | (bottom * 4), 0); // gDPFillRectangle over it all
    dl.cmd(0x64000018, 0);          // gEXPopScissor
    dl.cmd(0x64000026, 0);          // gEXPopFillColor
    dl.cmd(0x6400001A, 0);          // gEXPopOtherMode
    finish(rdram, dl);
}

// After the game clears the depth buffer and sets the frame's colour buffer
// (func_00291B00; first call each frame). The menus never clear the colour
// buffer, relying on the background to cover it, but in Expand part of every
// frame is drawn by nothing (beside and around the 4:3 menu) and kept
// whatever that framebuffer last held; RT64 keeps three of them, so those
// areas flickered. In the front end with Expand the colour buffer is cleared
// to black here, before anything else is drawn.
extern "C" void trw_apply_extras(uint8_t* rdram);

// Text drawn before a match frame's colour buffer is set: a trial's
// objective ("PROVING GROUNDS", "acquire 2 frags", ...) during the level's
// fly-through. The game draws it first, into whichever colour buffer the last
// frame left set (on the N64 the finished frame, about to be shown), then
// fills the new buffer and draws the level, which in RT64 leaves the text
// nowhere to be seen. It's moved to the end of the frame instead: copied from
// the display list here (from its scissor, so it keeps its own setup, up to
// the frame's own setup: its next display list call or colour image) and
// NOPed, then put back just before the frame's end (trw_frame_end).
static std::vector<uint32_t> early_text;

static void take_early_text(uint8_t* rdram, uint32_t base, uint32_t head) {
    early_text.clear();
    if (head <= base || head - base > 0x10000) {
        return;
    }
    uint32_t from = 0;
    uint32_t to = 0;
    bool has_text = false;
    for (uint32_t a = base; a < head && to == 0; a += 8) {
        uint32_t op = (uint32_t)MEM_W(0, (gpr)(int32_t)a) >> 24;
        if (from == 0) {
            if (op == 0xED) { // G_SETSCISSOR
                from = a;
            }
        }
        else if (op == 0xE4) { // G_TEXRECT
            has_text = true;
        }
        else if (op == 0xDE || op == 0xFF || op == 0xFE) { // G_DL, G_SETCIMG, G_SETZIMG
            to = a;
        }
    }
    if (!has_text || to == 0) {
        return;
    }
    for (uint32_t a = from; a < to; a += 8) {
        early_text.push_back((uint32_t)MEM_W(0, (gpr)(int32_t)a));
        early_text.push_back((uint32_t)MEM_W(4, (gpr)(int32_t)a));
        MEM_W(0, (gpr)(int32_t)a) = 0; // G_SPNOOP
        MEM_W(4, (gpr)(int32_t)a) = 0;
    }
}

// Before the game ends a match frame's display list (func_00288DF8).
extern "C" void trw_frame_end(uint8_t* rdram) {
    if (early_text.empty() || !head_valid(rdram)) {
        early_text.clear();
        return;
    }
    uint32_t head = (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead);
    uint32_t base = head >= kDlBufferB ? kDlBufferB : kDlBufferA;
    if (head + early_text.size() * 4 + 8 <= base + (kDlBufferB - kDlBufferA)) {
        for (size_t i = 0; i < early_text.size(); i++) {
            MEM_W(0, (gpr)(int32_t)(head + i * 4)) = (int32_t)early_text[i];
        }
        MEM_W(0, (gpr)(int32_t)kDlHead) = (int32_t)(head + early_text.size() * 4);
    }
    early_text.clear();
}

extern "C" void trw_frame_buffers_set(uint8_t* rdram) {
    trw_apply_extras(rdram); // src/game/extras.cpp
    if (!head_valid(rdram)) {
        return;
    }
    // The frame's first call: the display list is in the other buffer than
    // at the last call, or has started over in the same one.
    uint32_t head = (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead);
    uint32_t base = head >= kDlBufferB ? kDlBufferB : kDlBufferA;
    bool frame_start = base != last_clear_base || head <= last_clear_head;
    last_clear_base = base;
    last_clear_head = head;
    if (frame_start && in_match(rdram)) {
        take_early_text(rdram, base, head);
    }
    if (frame_start) {
        // RT64's frame interpolation (the Display refresh rate) is off in the
        // front end: its frames made between two of the level select's,
        // whose level preview is a 3D picture drawn into the menu, came out
        // without most of the menu (a flash as the preview changed). The
        // menus gain little from it. In a match (paused too) it's left to
        // RT64 (UINT16_MAX: the rate it works out).
        DlWriter dl = dl_at_head(rdram);
        dl.cmd(0xE0525464, 0x10000064);                   // gEXEnable
        dl.cmd(0x64000009, in_match(rdram) ? 0xFFFFu : 0u); // gEXSetRefreshRate
        finish(rdram, dl);
    }
    if (!in_match(rdram)) {
        hud_this_frame.store(false);
        hud_last_frame.store(false);
        match_humans = 0;
    }
    if (!menu_screen(rdram)) {
        if (frame_start) {
            hud_last_frame.store(hud_this_frame.exchange(false));
            weapon_grouped = false;
            weapon_view = 0;
            hud_view = 0;
            view_after_hud = false;
            weapon_camera.proj_cmd = 0;
            set_frame_base_group(rdram);
            // In split screen the views and their borders leave the last
            // rows undrawn, which kept stale pixels (old HUD icons) in Expand.
            uint32_t width = (uint32_t)MEM_W(0, (gpr)(int32_t)kViModeWidth);
            if (trw_widescreen_active() && width >= 160 && width <= 640) {
                clear_colour_buffer(rdram, width, width * 3 / 4, true);
            }
        }
        else if (view_after_hud) {
            // A split screen's next view starts (its world, then its weapon
            // after another clear): its things matched among themselves.
            view_after_hud = false;
            set_view_base_group(rdram, weapon_view);
        }
        else {
            trw_weapon_begin(rdram);
        }
        return;
    }
    if (trw_widescreen_active() && frame_start) {
        clear_colour_buffer(rdram, kMenuHalfWidth * 2, kMenuHeight, false);
    }
}

// Before the screen fade (func_00293008, called from func_00292220 with its
// colour, RGBA bytes, at sp+0x18). The game's fade covers the 4:3 area only;
// in the front end the bars beside it get a rectangle of the same colour, so
// they fade with the rest. (Stretching the game's own fade doesn't work: it
// doubles the picture.)
extern "C" void trw_fade_begin(uint8_t* rdram, gpr sp) {
    if (!trw_widescreen_active() || !menu_screen(rdram) || !head_valid(rdram)) {
        return;
    }
    uint32_t color = 0;
    for (int i = 0; i < 4; i++) {
        color = (color << 8) | (uint8_t)MEM_BU(0x18 + i, sp);
    }
    if ((color & 0xFF) == 0) {
        return;
    }
    uint32_t right = kMenuHalfWidth * 2, bottom = kMenuHeight;
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x64000019, 0);          // gEXPushOtherMode
    dl.cmd(0x6400001B, 0);          // gEXPushCombine
    dl.cmd(0x64000027, 0);          // gEXPushPrimColor
    dl.cmd(0x64000017, 0);          // gEXPushScissor
    dl.cmd(0x64000033, kRectAspectStretch);
    dl.cmd(0xE3000A01, 0x00000000); // gDPSetCycleType(G_CYC_1CYCLE)
    dl.cmd(0xE200001C, 0x00504240); // gDPSetRenderMode(G_RM_XLU_SURF, G_RM_XLU_SURF2)
    dl.cmd(0xFCFFFFFF, 0xFFFDF6FB); // gDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE)
    dl.cmd(0xFA000000, color);      // gDPSetPrimColor
    // The framebuffer's rows only: RT64 judges whether a pass is 4:3 by the
    // scissors drawn with, and taller ones double the picture.
    set_scissor(dl, kOriginLeft, kOriginCenter, 0, 0, -kMenuHalfWidth * 4, kMenuHeight - 1);
    dl.cmd(0xF6000000 | ((right * 4) << 12) | (bottom * 4), 0); // gDPFillRectangle over the framebuffer
    set_scissor(dl, kOriginCenter, kOriginRight, kBarInnerEdge, 0, -1, kMenuHeight - 1);
    dl.cmd(0xF6000000 | ((right * 4) << 12) | (bottom * 4), 0);
    dl.cmd(0x64000018, 0);          // gEXPopScissor
    dl.cmd(0x64000028, 0);          // gEXPopPrimColor
    dl.cmd(0x6400001C, 0);          // gEXPopCombine
    dl.cmd(0x6400001A, 0);          // gEXPopOtherMode
    dl.cmd(0x64000033, kRectAspectAuto);
    finish(rdram, dl);
}

// Before a widget is drawn (`sp` the stack pointer on entry, which tells the
// calls apart: func_0040DB84 is recursive). A widget without a parent is a
// whole menu.
// The title ("press start"): a menu of just a full-screen image and one
// text. The keyboard and mouse controls send Start there
// (src/main/fps_controls.cpp), the only button it takes.
static std::atomic<bool> title_screen{ false };

extern "C" int trw_on_title_screen(void) {
    return title_screen.load();
}

static bool is_title_menu(uint8_t* rdram, uint32_t root) {
    uint32_t image = (uint32_t)MEM_W(8, (gpr)(int32_t)root);
    if (!valid_widget(image) || MEM_HU(0xE, (gpr)(int32_t)image) != kWidgetImage || MEM_H(0x18, (gpr)(int32_t)image) < kMenuHalfWidth * 2) {
        return false;
    }
    uint32_t text = (uint32_t)MEM_W(4, (gpr)(int32_t)image);
    return valid_widget(text) && MEM_HU(0xE, (gpr)(int32_t)text) == 4 && MEM_W(4, (gpr)(int32_t)text) == 0;
}

// The options opened from the pause list leave only through their Resume
// Game item (neither B nor Start does). Escape there moves the cursor to it
// and selects it (src/main/fps_controls.cpp), steered by where it is from
// the highlighted item: 0 on it, 1 below it, -1 above it, 2 not showing (a
// sub-screen such as the sound options is open, or another menu).
// The items: a list widget (flag 0x8 while showing) of items (flag 0x40 on
// the highlighted one, the y position at +0x16), each holding a widget with
// its text (type 4, the string number at +0x34; Resume Game is 781).
static std::atomic<int> resume_direction{ 2 };

extern "C" int trw_pause_resume_direction(void) {
    return resume_direction.load();
}

static uint32_t find_text(uint8_t* rdram, uint32_t root, uint32_t string_id) {
    uint32_t stack[16];
    int depth = 0;
    uint32_t w = root;
    for (int n = 0; n < 1024 && valid_widget(w); n++) {
        gpr g = (gpr)(int32_t)w;
        if (MEM_HU(0xE, g) == 4 && (uint32_t)MEM_W(0x34, g) == string_id) {
            return w;
        }
        uint32_t child = (uint32_t)MEM_W(8, g);
        if (valid_widget(child) && (uint32_t)MEM_W(0, (gpr)(int32_t)child) == w && depth < 16) {
            stack[depth++] = w;
            w = child;
            continue;
        }
        // The next sibling, or the parent's next sibling (not the root's:
        // those are other menus).
        if (depth == 0) {
            break;
        }
        w = (uint32_t)MEM_W(4, g);
        while (!valid_widget(w) && depth > 1) {
            w = (uint32_t)MEM_W(4, (gpr)(int32_t)stack[--depth]);
        }
        if (!valid_widget(w)) {
            break;
        }
    }
    return 0;
}

static int resume_item_direction(uint8_t* rdram, uint32_t root) {
    constexpr uint32_t kResumeGame = 781;
    uint32_t text = find_text(rdram, root, kResumeGame);
    if (text == 0) {
        return 2;
    }
    uint32_t holder = (uint32_t)MEM_W(0, (gpr)(int32_t)text);
    uint32_t item = valid_widget(holder) ? (uint32_t)MEM_W(0, (gpr)(int32_t)holder) : 0;
    uint32_t list = valid_widget(item) ? (uint32_t)MEM_W(0, (gpr)(int32_t)item) : 0;
    if (!valid_widget(list) || (MEM_W(0x10, (gpr)(int32_t)list) & 0x8) == 0) {
        return 2;
    }
    uint32_t child = (uint32_t)MEM_W(8, (gpr)(int32_t)list);
    for (int i = 0; i < 32 && valid_widget(child); i++) {
        gpr c = (gpr)(int32_t)child;
        if (MEM_W(0x10, c) & 0x40) {
            if (child == item) {
                return 0;
            }
            int16_t y = (int16_t)MEM_HU(0x16, c);
            int16_t resume_y = (int16_t)MEM_HU(0x16, (gpr)(int32_t)item);
            return y < resume_y ? -1 : 1;
        }
        child = (uint32_t)MEM_W(4, c);
    }
    return 2;
}

// The pause list (a group holding a dark box, type 6, then its items, type
// 9, each holding its text, type 4) marks the highlighted item only with a
// faint bar behind its white text. The other items' text is faded (the top
// byte of a widget's +0x10 is its alpha), so the highlighted one stands out.
static void fade_pause_items(uint8_t* rdram, uint32_t root) {
    constexpr uint16_t kGroup = 1, kBox = 6, kItem = 9, kText = 4;
    constexpr uint32_t kHighlighted = 0x40;
    constexpr uint32_t kFadedAlpha = 0x60;
    uint32_t group = (uint32_t)MEM_W(8, (gpr)(int32_t)root);
    for (int g = 0; g < 16 && valid_widget(group); g++, group = (uint32_t)MEM_W(4, (gpr)(int32_t)group)) {
        uint32_t box = (uint32_t)MEM_W(8, (gpr)(int32_t)group);
        if (MEM_HU(0xE, (gpr)(int32_t)group) != kGroup || !valid_widget(box) || MEM_HU(0xE, (gpr)(int32_t)box) != kBox) {
            continue;
        }
        uint32_t item = (uint32_t)MEM_W(4, (gpr)(int32_t)box);
        for (int i = 0; i < 16 && valid_widget(item); i++, item = (uint32_t)MEM_W(4, (gpr)(int32_t)item)) {
            if (MEM_HU(0xE, (gpr)(int32_t)item) != kItem) {
                continue;
            }
            bool highlighted = ((uint32_t)MEM_W(0x10, (gpr)(int32_t)item) & kHighlighted) != 0;
            uint32_t text = (uint32_t)MEM_W(8, (gpr)(int32_t)item);
            for (int t = 0; t < 4 && valid_widget(text); t++, text = (uint32_t)MEM_W(4, (gpr)(int32_t)text)) {
                if (MEM_HU(0xE, (gpr)(int32_t)text) == kText) {
                    MEM_BU(0x10, (gpr)(int32_t)text) = (uint8_t)(highlighted ? 0xFF : kFadedAlpha);
                }
            }
        }
    }
}

// The items the mouse cursor can point at (include/trw_menu_cursor.h): in
// each list or grid of a menu, the widget the highlight is on has flag 0x40
// and is an item (type 9 or the game's own); the others it can move to are
// its siblings that are items too (an options list mixes plain items,
// switches and sliders: types 0xB61, 0xB63, 0xB64). The cursor points only
// at those shown (0x8) and not greyed out (0x100), but the highlight can be
// on a hidden one (the character select's locked cells), so they're kept
// for steering. Positions are relative to the parent. Of the lists in a menu,
// the one with the most items on screen is taken (some menus keep a
// one-item list off to the side).
namespace {
    constexpr uint32_t kWidgetShown = 0x8;
    constexpr uint32_t kWidgetHighlighted = 0x40;
    constexpr uint32_t kWidgetGreyed = 0x100;
    // Types up to this are the menus' parts (1 a group, 2 a menu, 3 an
    // image, 4 text, 6 a box, 8 a bar); items are 9 and the game's own types
    // (0xB5F up). A group can be marked too (the character select's skin
    // counter), but the stick doesn't move it.
    constexpr uint16_t kWidgetLastPart = 8;

    struct ItemSearch {
        trw::menu_cursor::Items best;
        int visited = 0;
        bool shared = false; // several players choose in this menu at once
        int marked_lists = 0;
    };

    void list_items(uint8_t* rdram, uint32_t parent, int32_t px, int32_t py, uint32_t highlighted, ItemSearch& search) {
        trw::menu_cursor::Items found;
        found.width = search.best.width;
        found.height = search.best.height;
        int marked_others = 0;
        uint32_t child = (uint32_t)MEM_W(8, (gpr)(int32_t)parent);
        for (int i = 0; i < 64 && valid_widget(child) && found.count < trw::menu_cursor::kMaxItems; i++) {
            gpr c = (gpr)(int32_t)child;
            uint32_t flags = (uint32_t)MEM_W(0x10, c);
            int32_t x = px + (int16_t)MEM_HU(0x14, c);
            int32_t y = py + (int16_t)MEM_HU(0x16, c);
            uint16_t w = MEM_HU(0x18, c), h = MEM_HU(0x1A, c);
            // (Not a full-screen one: each front-end menu has one marked, type
            // 2005, that isn't an item to point at.)
            bool on_screen = w > 0 && h > 0 && x >= 0 && y >= 0 && x + w <= found.width && y + h <= found.height &&
                !(w >= found.width && h >= found.height * 0.75f);
            if (MEM_HU(0xE, c) > kWidgetLastPart && on_screen) {
                if (child == highlighted) {
                    found.highlighted = found.count;
                }
                else if (flags & kWidgetHighlighted) {
                    marked_others++;
                }
                bool pointable = (flags & kWidgetShown) && !(flags & kWidgetGreyed);
                found.pointable += pointable ? 1 : 0;
                uint16_t type = MEM_HU(0xE, c);
                bool steps = type == trw::menu_cursor::kValueItem || type == trw::menu_cursor::kSliderItem;
                found.items[found.count++] = { child, type, pointable, steps, (float)x, (float)y, (float)(x + w), (float)(y + h) };
            }
            child = (uint32_t)MEM_W(4, c);
        }
        // The loadout's column: items (type 9) each over a bar (type 8), the
        // weapon slots, whose weapon Right steps, and OK last.
        bool slots = found.count >= 2;
        for (int i = 0; i < found.count && slots; i++) {
            uint32_t first = (uint32_t)MEM_W(8, (gpr)(int32_t)found.items[i].widget);
            slots = found.items[i].type == 9 && valid_widget(first) && MEM_HU(0xE, (gpr)(int32_t)first) == 8;
        }
        for (int i = 0; slots && i < found.count - 1; i++) {
            found.items[i].steps = true;
        }
        // Several players choosing at once (in split screen the character
        // select marks each player's cell in one grid, the loadout has a
        // column each): which mark is whose isn't known, so the menu isn't
        // taken, and a click picks as A does.
        if (marked_others > 0) {
            search.shared = true;
            return;
        }
        if (found.highlighted >= 0 && found.pointable >= 3 && ++search.marked_lists > 1) {
            search.shared = true;
        }
        if (found.highlighted >= 0 && found.pointable > search.best.pointable) {
            search.best = found;
        }
    }

    // `w` and its later siblings, at the parent's position (px, py).
    void search_items(uint8_t* rdram, uint32_t w, uint32_t parent, int32_t px, int32_t py, int depth, ItemSearch& search) {
        for (int i = 0; i < 64 && valid_widget(w) && search.visited < 2048; i++, search.visited++) {
            gpr g = (gpr)(int32_t)w;
            uint32_t flags = (uint32_t)MEM_W(0x10, g);
            // (A hidden item can be highlighted: the character select's
            // locked cells.)
            if ((flags & kWidgetHighlighted) && parent != 0 && MEM_HU(0xE, g) > kWidgetLastPart) {
                list_items(rdram, parent, px, py, w, search);
            }
            if (flags & kWidgetShown) {
                uint32_t child = (uint32_t)MEM_W(8, g);
                if (depth < 16 && valid_widget(child) && (uint32_t)MEM_W(0, (gpr)(int32_t)child) == w) {
                    search_items(rdram, child, w, px + (int16_t)MEM_HU(0x14, g), py + (int16_t)MEM_HU(0x16, g), depth + 1, search);
                }
            }
            if (depth == 0) {
                return; // the root's siblings are other menus
            }
            w = (uint32_t)MEM_W(4, g);
        }
    }

    // The multiplayer level select: a skull per game mode across the top and
    // its levels as rings below, none of them marked; the cursor is drawn
    // apart. Its place is in the screen's state: the level name's text
    // (string 751) shows a buffer in it (+0x38), with the column (the mode)
    // 0x40 after the buffer and the row 0x44 after (0 the skull, 1 on the
    // levels down from it, 14 pixels apart; the first column's branch to the
    // left counts on from 16 at its top, downward).
    bool publish_level_select(uint8_t* rdram, uint32_t root, const trw::menu_cursor::Items& frame) {
        constexpr uint32_t kLevelName = 751;
        uint32_t text = find_text(rdram, root, kLevelName);
        if (text == 0) {
            return false;
        }
        uint32_t state = (uint32_t)MEM_W(0x38, (gpr)(int32_t)text);
        if (!valid_widget(state)) {
            return false;
        }
        int32_t col = MEM_W(0x40, (gpr)(int32_t)state);
        int32_t row = MEM_W(0x44, (gpr)(int32_t)state);
        trw::menu_cursor::Items found = frame;
        int32_t skull_x[8];
        int skulls = 0;
        uint32_t child = (uint32_t)MEM_W(8, (gpr)(int32_t)root);
        for (int i = 0; i < 128 && valid_widget(child) && found.count < trw::menu_cursor::kMaxItems; i++) {
            gpr c = (gpr)(int32_t)child;
            uint16_t w = MEM_HU(0x18, c), h = MEM_HU(0x1A, c);
            bool skull = w == 21 && h == 31, ring = w == 16 && h == 12;
            if (MEM_HU(0xE, c) == 1 && (skull || ring) && ((uint32_t)MEM_W(0x10, c) & kWidgetShown)) {
                int32_t x = (int16_t)MEM_HU(0x14, c), y = (int16_t)MEM_HU(0x16, c);
                if (skull && skulls < 8) {
                    skull_x[skulls++] = x;
                }
                found.items[found.count++] = { child, 1, true, false, (float)x, (float)y, (float)(x + w), (float)(y + h) };
                found.pointable++;
            }
            child = (uint32_t)MEM_W(4, c);
        }
        std::sort(skull_x, skull_x + skulls);
        if (col < 0 || col >= skulls || row < 0 || row > 32) {
            return false;
        }
        // Where the cursor is.
        int32_t want_x, want_y, want_w;
        if (row == 0) {
            want_x = skull_x[col];
            want_y = -1;
            want_w = 21;
        }
        else if (col == 0 && row >= 11) {
            want_x = skull_x[0] - 15;
            want_y = 114 + (16 - row) * 14;
            want_w = 16;
        }
        else {
            want_x = skull_x[col] + 3;
            want_y = 58 + (row - 1) * 14;
            want_w = 16;
        }
        for (int i = 0; i < found.count; i++) {
            const trw::menu_cursor::Item& item = found.items[i];
            bool x_ok = std::abs((int32_t)item.x0 - want_x) <= 2 && (int32_t)(item.x1 - item.x0) == want_w;
            bool y_ok = want_y < 0 || std::abs((int32_t)item.y0 - want_y) <= 2;
            if (x_ok && y_ok) {
                found.highlighted = i;
                break;
            }
        }
        if (found.highlighted < 0) {
            return false;
        }
        trw::menu_cursor::publish(found);
        return true;
    }

    void publish_menu_items(uint8_t* rdram, uint32_t root) {
        ItemSearch search;
        // The menus draw at 284 x 222; the pause list over a match at the
        // match's resolution.
        uint32_t vi_width = (uint32_t)MEM_W(0, (gpr)(int32_t)kViModeWidth);
        if (vi_width >= 200 && vi_width <= 640) {
            search.best.width = (float)vi_width;
            search.best.height = vi_width == 284 ? 222.0f : vi_width * 0.75f;
        }
        if (publish_level_select(rdram, root, search.best)) {
            return;
        }
        search_items(rdram, root, 0, 0, 0, 0, search);
        if (search.best.count > 0 && !search.shared) {
            trw::menu_cursor::publish(search.best);
        }
    }
}

extern "C" void trw_menu_widget_begin(uint8_t* rdram, gpr widget, gpr sp) {
    if (!valid_widget((uint32_t)widget) || MEM_W(0, widget) != 0) {
        return;
    }
    publish_menu_items(rdram, (uint32_t)widget);
    title_screen.store(is_title_menu(rdram, (uint32_t)widget));
    resume_direction.store(MEM_W(0, (gpr)(int32_t)0x801407D4) != 0 ? resume_item_direction(rdram, (uint32_t)widget) : 2);
    if (MEM_W(0, (gpr)(int32_t)0x801407D4) != 0) {
        fade_pause_items(rdram, (uint32_t)widget);
    }
    if (!trw_widescreen_active() || !menu_screen(rdram) || !head_valid(rdram)) {
        return;
    }
    set_rect_aspect(rdram, kRectAspectAdjust);
    // Passes that draw only part of the screen this frame (the level preview)
    // keep to 4:3 too (lib-patches/rt64).
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x64000035, 1);          // gEXSetFramebufferAspect(on)
    finish(rdram, dl);
    menu_root = (uint32_t)sp;
    menu_background_seen = false;
}

// After it.
extern "C" void trw_menu_widget_end(uint8_t* rdram, gpr sp) {
    if (menu_root == 0 || (uint32_t)sp != menu_root) {
        return;
    }
    menu_root = 0;
    if (head_valid(rdram)) {
        set_rect_aspect(rdram, kRectAspectAuto);
    }
}


// Before an image widget in the menus is drawn. A menu's background (its
// first full-screen image) is drawn as in 4:3, so its holes stay where the
// game fills them (the hole patches, the level preview), with a stretched
// copy underneath that shows in the bars beside the 4:3 area. The copy is
// point-sampled: filtering at the stretched scale blends the background's
// texels into the (unused) colour of a hole beside them. In the bars nothing
// fills the holes, so under each copy go more, shifted sideways, whose rock
// shows through them. The game's own draw is clipped away (an empty scissor)
// and all of it is drawn afterwards.
// Later full-screen images are overlays (the trial select's blinking panel
// highlight), which would blink in the bars too. The images filling holes
// aren't copied either: the bars show the same rock there whatever fills the
// hole. On the level select the hole is the preview: its "...." picture,
// then that picture fading out over a new level's preview, then nothing;
// copied, the bar beside it changed each time (a dark box while it faded).
extern "C" void trw_menu_image_begin(uint8_t* rdram, gpr widget) {
    if (!trw_widescreen_active() || !menu_screen(rdram) || !head_valid(rdram) || menu_root == 0 ||
        !valid_widget((uint32_t)widget) || !full_screen_image(rdram, (uint32_t)widget) || menu_background_seen) {
        return;
    }
    menu_background_seen = true;
    // The bars span the image's rows only: RT64 judges whether a pass is 4:3
    // by the scissors drawn with.
    Box box = widget_box(rdram, (uint32_t)widget);
    menu_image_top = std::max(box.y0, 0);
    menu_image_bottom = std::max(box.y1, menu_image_top);
    menu_image_left = std::clamp(box.x0, 0, kMenuHalfWidth * 2);
    menu_image_right = std::clamp(box.x1, menu_image_left, kMenuHalfWidth * 2);
    DlWriter dl = dl_at_head(rdram);
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    dl.cmd(0x64000017, 0);          // gEXPushScissor
    set_scissor(dl, kOriginLeft, kOriginLeft, 0, 0, 1, 1); // a quarter pixel: nothing
    dl.cmd(0x64000033, kRectAspectStretch);
    dl.cmd(0xE3001201, 0x0000);     // gDPSetTextureFilter(G_TF_POINT)
    finish(rdram, dl);
    menu_image_start = (uint32_t)dl.head;
    menu_image_full = true;
    menu_stretched = true;
}

// After it: the copy for the right bar, then the image in 4:3.
extern "C" void trw_menu_image_end(uint8_t* rdram) {
    if (!menu_stretched) {
        return;
    }
    menu_stretched = false;
    if (!head_valid(rdram)) {
        return;
    }
    uint32_t start = menu_image_start;
    uint32_t end = (uint32_t)MEM_W(0, (gpr)(int32_t)kDlHead);
    uint32_t size = end - start;
    bool copy = end > start && size < 0x4000;
    DlWriter dl = dl_at_head(rdram);
    // Commands [from, to) again, their texture rectangles moved right by
    // `shift` pixels (left if negative), clipped at the screen's left edge.
    bool clip_to_bar = false;
    auto repeat_range = [&](uint32_t from, uint32_t to, int32_t shift) {
        for (uint32_t a = from; a < to; a += 8) {
            uint32_t w0 = (uint32_t)MEM_W(0, (gpr)(int32_t)a);
            uint32_t w1 = (uint32_t)MEM_W(4, (gpr)(int32_t)a);
            bool texrect = (w0 >> 24) == 0xE4 && a + 24 <= to &&
                ((uint32_t)MEM_W(8, (gpr)(int32_t)a) >> 24) == 0xE1 && ((uint32_t)MEM_W(16, (gpr)(int32_t)a) >> 24) == 0xF1;
            if ((w0 >> 24) == 0xED && clip_to_bar) { // G_SETSCISSOR: the bar's stays
                continue;
            }
            if (shift == 0 || !texrect) {
                dl.cmd(w0, w1);
                continue;
            }
            // G_TEXRECT, G_RDPHALF_1 (s, t in s10.5), G_RDPHALF_2 (dsdx, dtdy in s5.10).
            int32_t ulx = (int32_t)((w1 >> 12) & 0xFFF) + shift * 4;
            int32_t lrx = (int32_t)((w0 >> 12) & 0xFFF) + shift * 4;
            uint32_t st = (uint32_t)MEM_W(12, (gpr)(int32_t)a);
            uint32_t d = (uint32_t)MEM_W(20, (gpr)(int32_t)a);
            a += 16;
            if (lrx <= 0 || ulx > 0xFFF || lrx > 0xFFF) {
                continue;
            }
            int32_t s0 = (int16_t)(st >> 16);
            if (ulx < 0) {
                int32_t dsdx = (int16_t)(d >> 16);
                s0 += (-ulx) * dsdx / 128; // quarter pixels x s5.10 -> s10.5
                ulx = 0;
            }
            dl.cmd((w0 & ~(0xFFFu << 12)) | ((uint32_t)lrx << 12), (w1 & ~(0xFFFu << 12)) | ((uint32_t)ulx << 12));
            dl.cmd((uint32_t)MEM_W(8, (gpr)(int32_t)(a - 16)), ((uint32_t)(s0 & 0xFFFF) << 16) | (st & 0xFFFF));
            dl.cmd((uint32_t)MEM_W(16, (gpr)(int32_t)(a - 16)), d);
        }
    };
    auto repeat = [&](int32_t shift) { repeat_range(start, end, shift); };
    // Black at kBarShade over columns [x0, x1) of the image's rows.
    auto shade = [&](int32_t x0, int32_t x1) {
        dl.cmd(0x64000019, 0);          // gEXPushOtherMode
        dl.cmd(0x6400001B, 0);          // gEXPushCombine
        dl.cmd(0x64000027, 0);          // gEXPushPrimColor
        dl.cmd(0xE3000A01, 0x00000000); // gDPSetCycleType(G_CYC_1CYCLE)
        dl.cmd(0xE200001C, 0x00504240); // gDPSetRenderMode(G_RM_XLU_SURF, G_RM_XLU_SURF2)
        dl.cmd(0xFCFFFFFF, 0xFFFDF6FB); // gDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE)
        dl.cmd(0xFA000000, kBarShade);  // gDPSetPrimColor
        dl.cmd(0xF6000000 | ((uint32_t)x1 * 4 << 12) | ((uint32_t)menu_image_bottom * 4), // gDPFillRectangle
            ((uint32_t)x0 * 4 << 12) | ((uint32_t)menu_image_top * 4));
        dl.cmd(0x64000028, 0);          // gEXPopPrimColor
        dl.cmd(0x6400001C, 0);          // gEXPopCombine
        dl.cmd(0x6400001A, 0);          // gEXPopOtherMode
    };
    // One bar: for a background, copies shifted sideways first (their rock
    // shows through its holes), then the image. Under the rest, copies stepped
    // inward by the width of the rock at the background's outer edge
    // (`edge_step`, + for the left bar, - for the right), so a hole near
    // that edge gets the edge's rock (what fills the hole isn't copied, see
    // trw_menu_image_begin); then copies by a third of the width each way.
    auto bar = [&](uint32_t left_origin, uint32_t right_origin, int32_t ulx, int32_t lrx, int32_t edge_step) {
        set_scissor(dl, left_origin, right_origin, ulx, menu_image_top, lrx, menu_image_bottom);
        clip_to_bar = true;
        if (menu_image_full) {
            for (int step = 3; step >= 1; step--) {
                repeat(edge_step * step);
            }
            int32_t shift = kMenuHalfWidth * 2 / 3;
            repeat(shift);
            repeat(-shift);
        }
        repeat(0);
        {
            // The copies repeat plainly beside the 4:3 picture: darkened over
            // the image's box (an image filling a hole gets its own), so the
            // bars stay in the background (still under the bar's scissor).
            shade(menu_image_left, menu_image_right);
        }
        clip_to_bar = false;
    };
    dl.cmd(0xE0525464, 0x10000064); // gEXEnable
    if (copy) {
        constexpr int32_t kEdgeRock = 17; // menu pixels of plain rock at the background's sides
        bar(kOriginLeft, kOriginCenter, 0, -kBarInnerEdge, kEdgeRock);
        // The right bar. It stops a quarter pixel short of the edge, where the
        // game's own scissor stops: RT64 judges what spans the screen by the
        // scissors drawn with, and a wider one would leave nothing spanning it.
        bar(kOriginCenter, kOriginRight, kBarInnerEdge, -1, -kEdgeRock);
    }
    dl.cmd(0x64000018, 0);          // gEXPopScissor
    dl.cmd(0xE3001201, 0x2000);     // gDPSetTextureFilter(G_TF_BILERP), the menus' filter
    dl.cmd(0x64000033, kRectAspectAdjust);
    if (copy) {
        repeat(0);
        // The background's outermost columns (rock beside the panels' metal
        // on most screens) are shaded like the bars, which reach under them,
        // so no undimmed line shows between the two.
        if (menu_image_full) {
            shade(menu_image_left, std::min(menu_image_left + kBarOverlap, menu_image_right));
            shade(std::max(menu_image_right - kBarOverlap, menu_image_left), menu_image_right);
        }
    }
    dl.cmd(0x64000033, menu_root != 0 ? kRectAspectAdjust : kRectAspectAuto);
    finish(rdram, dl);
}
