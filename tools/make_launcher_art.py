#!/usr/bin/env python3
"""Draws the launcher's background art: a dark, misty jungle at dusk in the
menus' orange and brown, in two layers the launcher scrolls sideways at different
speeds (icons/launcher_back.jpg, icons/launcher_front.png), and a still
shading overlay (icons/launcher_shade.png). See src/main/launcher_art.cpp.

Both images tile horizontally, so every shape is drawn wrapped around the
left and right edges. Everything is generated from a fixed seed; run it again
to get the same images:

    python3 tools/make_launcher_art.py
"""
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

W, H = 2560, 1080   # one tile
SS = 2              # drawn at twice the size, then scaled down (smooth edges)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "icons")


def periodic_noise(rng, n, x, terms=6, base=1, amp_falloff=0.6):
    """A smooth curve over x in [0, n) that wraps at n."""
    y = np.zeros_like(x, dtype=np.float64)
    amp = 1.0
    for i in range(terms):
        k = base * (2 ** i) + rng.randint(0, base * (2 ** i))
        y += amp * np.sin(2 * math.pi * k * x / n + rng.uniform(0, 2 * math.pi))
        amp *= amp_falloff
    return y


def tube(draw, points, r0, r1, fill, offset=0.0):
    """A thick curve through points, its radius going from r0 to r1."""
    n = len(points) - 1
    for i in range(n):
        (ax, ay), (bx, by) = points[i], points[i + 1]
        steps = max(1, int(math.hypot(bx - ax, by - ay) / 2))
        for j in range(steps):
            t = (i + j / steps) / n
            x = ax + (bx - ax) * j / steps + offset
            y = ay + (by - ay) * j / steps
            r = r0 + (r1 - r0) * t
            draw.ellipse((x - r, y - r, x + r, y + r), fill=fill)


def spline(points, steps=12):
    """A smooth (Catmull-Rom) curve through points."""
    pts = [points[0]] + list(points) + [points[-1]]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for j in range(steps):
            t = j / steps
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                                    + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3) for k in range(2)))
    out.append(points[-1])
    return out


def body(draw, points, radii, fill, steps=12):
    """A thick smooth shape along points, radii[i] thick at points[i]."""
    curve = spline(points, steps)
    rad = []
    for i in range(len(points) - 1):
        for j in range(steps):
            rad.append(radii[i] + (radii[i + 1] - radii[i]) * j / steps)
    rad.append(radii[-1])
    for (ax, ay), (bx, by), ra, rb in zip(curve, curve[1:], rad, rad[1:]):
        n = max(1, int(math.hypot(bx - ax, by - ay) / 2))
        for j in range(n):
            t = j / n
            x, y, r = ax + (bx - ax) * t, ay + (by - ay) * t, ra + (rb - ra) * t
            draw.ellipse((x - r, y - r, x + r, y + r), fill=fill)


def wrapped(draw_fn, width):
    """Calls draw_fn(dx) for the shape and its copies one tile left and right."""
    for dx in (-width, 0, width):
        draw_fn(dx)


# ---------------------------------------------------------------- back layer

def back_layer():
    rng = random.Random(1999)
    w, h = W * SS, H * SS
    ys = np.linspace(0.0, 1.0, h)[:, None]
    xs = np.arange(w)[None, :]

    # Sky: near black above, a dim teal-green haze around the horizon.
    top = np.array([9, 5, 3], dtype=np.float64)
    horizon = np.array([82, 36, 12], dtype=np.float64)
    bottom = np.array([11, 6, 3], dtype=np.float64)
    t1 = np.clip(ys / 0.58, 0, 1) ** 1.6
    t2 = np.clip((ys - 0.58) / 0.42, 0, 1)
    sky = top * (1 - t1) + horizon * t1
    sky = np.where(ys > 0.58, horizon * (1 - t2) + bottom * t2, sky)
    img = np.broadcast_to(sky[:, None, :] if sky.ndim == 2 else sky, (h, 1, 3)).repeat(w, axis=1).astype(np.float64)

    # A faint moon glow, wrapped around the edges.
    mx, my = 0.68 * w, 0.22 * h
    dx = np.abs(xs - mx)
    dx = np.minimum(dx, w - dx)
    d = np.sqrt((dx / (0.30 * h)) ** 2 + ((ys * h - my) / (0.30 * h)) ** 2)
    glow = np.exp(-d ** 2 * 1.4)[..., None]
    img += glow * np.array([58, 22, 4])
    moon = np.exp(-((dx / (0.018 * h)) ** 2 + ((ys * h - my) / (0.018 * h)) ** 2) ** 2)[..., None]
    img += moon * np.array([120, 52, 14])

    base = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB")
    draw = ImageDraw.Draw(base)
    x_line = np.arange(0, w + 1, 4, dtype=np.float64)

    def ridge(y_base, amplitude, color, canopy, seed_terms, bumpiness):
        prof = periodic_noise(rng, w, x_line, terms=seed_terms, base=2)
        ys_r = y_base * h - amplitude * h * prof
        pts = [(float(x), float(y)) for x, y in zip(x_line, ys_r)]
        pts += [(w, h), (0, h)]
        draw.polygon(pts, fill=color)
        # Treetops: overlapping round crowns along the ridge line.
        for _ in range(canopy):
            x = rng.uniform(0, w)
            i = int(x / 4) % len(ys_r)
            y = ys_r[i]
            r = rng.uniform(0.6, 1.0) * bumpiness * h
            wrapped(lambda o: draw.ellipse((x + o - r, y - r * 0.8, x + o + r, y + r * 1.2), fill=color), w)

    def pterosaur(x, y, s, color):
        """A pteranodon gliding left: long bent wings, beak and swept-back crest."""
        def at(o):
            P = lambda a, b: (x + o + a * s, y + b * s)
            # Wings, each bent at the wrist.
            draw.polygon([P(-8, -2), P(-30, -18), P(-62, -22), P(-110, -10), P(-72, -6), P(-40, 2), P(-10, 6)], fill=color)
            draw.polygon([P(8, -2), P(30, -18), P(62, -22), P(110, -10), P(72, -6), P(40, 2), P(10, 6)], fill=color)
            # Body, then the head pointing left with its beak and crest.
            draw.ellipse((x + o - 14 * s, y - 6 * s, x + o + 16 * s, y + 6 * s), fill=color)
            draw.polygon([P(-10, -4), P(-46, -2), P(-12, 2)], fill=color)   # beak
            draw.polygon([P(-12, -4), P(-2, -16), P(-4, -3)], fill=color)    # crest
        wrapped(at, w)

    far = (44, 25, 14)
    mid = (24, 13, 7)
    near = (11, 6, 3)
    ridge(0.60, 0.035, far, 260, 5, 0.020)
    # A few pterosaurs gliding far off, the farther ones smaller and fainter.
    for px, py, size, colour in ((0.10, 0.36, 0.8, (18, 9, 4)), (0.14, 0.32, 0.6, (22, 11, 5)),
                                 (0.47, 0.20, 1.2, (12, 6, 3)), (0.44, 0.14, 0.55, (20, 10, 5)),
                                 (0.88, 0.29, 0.85, (16, 8, 4))):
        pterosaur(px * w, py * h, size * SS, colour)
    base = base.filter(ImageFilter.GaussianBlur(1.2 * SS))  # far things are soft
    draw = ImageDraw.Draw(base)
    ridge(0.70, 0.045, mid, 200, 5, 0.032)
    ridge(0.83, 0.05, near, 160, 4, 0.045)

    # Low fog drifting between the ridges.
    arr = np.asarray(base).astype(np.float64)
    fog = np.zeros((h, w))
    for y_c, thick, strength in ((0.64, 0.035, 0.55), (0.76, 0.045, 0.45), (0.90, 0.06, 0.35)):
        band = np.exp(-(((ys - y_c) / thick) ** 2))
        wob = (periodic_noise(rng, w, np.arange(w, dtype=np.float64), terms=3, base=2) * 0.5 + 0.5)[None, :]
        fog += band * (0.4 + 0.6 * wob) * strength
    arr = arr * (1 - 0.5 * fog[..., None]) + np.array([104, 52, 22]) * 0.5 * fog[..., None]

    # A little grain so the dark gradient doesn't band.
    noise = np.random.default_rng(7).normal(0, 0.8, arr.shape)
    arr = np.clip(arr + noise, 0, 255).astype(np.uint8)
    out = Image.fromarray(arr, "RGB").resize((W, H), Image.LANCZOS)
    return out


# --------------------------------------------------------------- front layer

def front_layer():
    rng = random.Random(65)
    w, h = W * SS, H * SS
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    ink = (4, 2, 1, 255)

    def leaf(cx, cy, angle, length, width, o):
        """A long pointed leaf from (cx, cy) along angle."""
        pts_l, pts_r = [], []
        for i in range(13):
            t = i / 12
            px = cx + math.cos(angle) * length * t
            py = cy + math.sin(angle) * length * t + (length * 0.25) * t * t  # droops
            wd = width * math.sin(math.pi * t) * (1 - 0.3 * t)
            nx, ny = -math.sin(angle), math.cos(angle)
            pts_l.append((px + nx * wd + o, py + ny * wd))
            pts_r.append((px - nx * wd + o, py - ny * wd))
        draw.polygon(pts_l + pts_r[::-1], fill=ink)

    def frond(cx, cy, angle, length, o):
        """A palm frond: a drooping spine with narrow leaflets."""
        spine = []
        for i in range(25):
            t = i / 24
            spine.append((cx + math.cos(angle) * length * t,
                          cy + math.sin(angle) * length * t + length * 0.45 * t * t))
        for (ax, ay), (bx, by) in zip(spine, spine[1:]):
            draw.line((ax + o, ay, bx + o, by), fill=ink, width=int(5 * SS))
        for i in range(2, 24):
            ax, ay = spine[i]
            bx, by = spine[i + 1]
            a = math.atan2(by - ay, bx - ax)
            ln = length * 0.28 * math.sin(math.pi * i / 24) + 6 * SS
            for side in (-1, 1):
                la = a + side * 1.1 + 0.35
                leaf(ax, ay, la, ln, 4 * SS, o)

    def palm(x, ground, height, lean):
        def at(o):
            trunk = []
            for i in range(31):
                t = i / 30
                trunk.append((x + lean * height * t * t, ground - height * t))
            tube(draw, trunk, 22 * SS, 11 * SS, ink, o)
            for i in range(2, 30, 2):  # rings on the trunk
                ax, ay = trunk[i]
                r = (22 - 11 * i / 30) * SS
                draw.ellipse((ax + o - r * 1.25, ay - r * 0.35, ax + o + r * 1.25, ay + r * 0.35), fill=ink)
            tx, ty = trunk[-1]
            for k in range(9):
                a = -math.pi / 2 + (k - 4) * 0.42 + rng.uniform(-0.1, 0.1)
                frond(tx, ty, a, height * rng.uniform(0.32, 0.42), o)
        wrapped(at, w)

    def fern(x, ground, size):
        def at(o):
            for k in range(11):
                a = -math.pi / 2 + (k - 5) * 0.22 + rng.uniform(-0.05, 0.05)
                ln = size * rng.uniform(0.7, 1.0)
                stem = []
                for i in range(17):
                    t = i / 16
                    stem.append((x + math.cos(a) * ln * t + math.copysign(1, a + math.pi / 2) * ln * 0.35 * t * t,
                                 ground + math.sin(a) * ln * t + ln * 0.45 * t * t))
                for (ax, ay), (bx, by) in zip(stem, stem[1:]):
                    draw.line((ax + o, ay, bx + o, by), fill=ink, width=int(3 * SS))
                for i in range(1, 16):
                    ax, ay = stem[i]
                    bx, by = stem[i + 1]
                    sa = math.atan2(by - ay, bx - ax)
                    pl = size * 0.11 * (1 - i / 17) + 3 * SS
                    for side in (-1, 1):
                        leaf(ax, ay, sa + side * 1.2, pl, 3 * SS, o)
        wrapped(at, w)

    def vine(x, length, sway):
        def at(o):
            pts = []
            for i in range(41):
                t = i / 40
                pts.append((x + sway * math.sin(t * 3.1) * length * 0.08, length * t))
            for (ax, ay), (bx, by) in zip(pts, pts[1:]):
                draw.line((ax + o, ay, bx + o, by), fill=ink, width=int(4 * SS))
            for i in range(3, 40, 3):
                ax, ay = pts[i]
                side = 1 if (i // 3) % 2 else -1
                leaf(ax, ay, math.pi / 2 + side * 0.9, 26 * SS, 7 * SS, o)
        wrapped(at, w)

    def grass(x, ground, n):
        def at(o):
            for _ in range(n):
                bx = x + rng.uniform(-60, 60) * SS
                ln = rng.uniform(40, 120) * SS
                a = -math.pi / 2 + rng.uniform(-0.5, 0.5)
                leaf(bx, ground, a, ln, 4 * SS, o)
        wrapped(at, w)

    def raptor(x, ground, s):
        """A raptor stalking left: head low, tail held straight out, mouth shut."""
        def at(o):
            X = lambda v: x + o + v * s
            Y = lambda v: ground - v * s
            P = lambda a, b: (X(a), Y(b))

            def leg(hx, knee_dx, ankle_dx, r):
                body(draw, [P(hx, 175), P(hx + knee_dx, 110), P(hx + ankle_dx, 48), P(hx + ankle_dx + 12, 8)],
                     [r * s, r * 0.55 * s, r * 0.3 * s, r * 0.25 * s], ink, steps=8)
                fx = hx + ankle_dx + 12
                # Toes forward, the sickle claw raised.
                draw.polygon([P(fx + 6, 12), P(fx - 46, 4), P(fx - 50, 0), P(fx + 10, 0)], fill=ink)
                draw.polygon([P(fx - 10, 10), P(fx - 22, 30), P(fx - 32, 26), P(fx - 22, 10)], fill=ink)

            leg(130, 30, -10, 26)   # far leg
            # Tail tip to neck.
            body(draw, [P(560, 222), P(440, 210), P(320, 198), P(210, 190), P(140, 182),
                        P(60, 168), P(10, 170), P(-25, 192), P(-45, 222)],
                 [3 * s, 8 * s, 14 * s, 24 * s, 38 * s, 34 * s, 20 * s, 15 * s, 13 * s], ink)
            # Head: a long narrow skull, mouth closed.
            draw.polygon([P(-30, 238), P(-62, 250), P(-112, 243), P(-146, 228), P(-150, 218),
                          P(-138, 210), P(-95, 206), P(-48, 208)], fill=ink)
            # Arms, held forward, with claws.
            body(draw, [P(30, 160), P(0, 132), P(-28, 124)], [9 * s, 6 * s, 4 * s], ink, steps=6)
            for cy in (128, 120):
                draw.polygon([P(-26, cy), P(-44, cy - 8), P(-30, cy - 2)], fill=ink)
            leg(165, 35, -5, 30)    # near leg
        wrapped(at, w)

    ground = h + 10 * SS
    raptor(0.44 * w, h + 10 * SS, 0.7 * SS)  # small, half in the undergrowth
    # Undergrowth all along the bottom.
    for i in range(18):
        grass((i + rng.random()) * w / 18, ground, 26)
    for x, s in ((0.06, 260), (0.23, 200), (0.41, 300), (0.58, 220), (0.77, 280), (0.93, 210)):
        fern(x * w, ground, s * SS)
    # A few palms, mostly near the edges of a tile's view.
    palm(0.12 * w, ground, 0.68 * h, 0.10)
    palm(0.62 * w, ground, 0.80 * h, -0.10)
    # Vines hanging from the top.
    for x, ln in ((0.30, 220), (0.34, 140), (0.48, 180), (0.84, 260), (0.88, 160)):
        vine(x * w, ln * SS, rng.choice((-1, 1)))
    # A canopy edge along the top.
    for _ in range(70):
        x = rng.uniform(0, w)
        leaf(x, -20 * SS, math.pi / 2 + rng.uniform(-0.8, 0.8), rng.uniform(60, 140) * SS, 10 * SS, 0)
        if x < 200 * SS:
            leaf(x + w, -20 * SS, math.pi / 2, 100 * SS, 10 * SS, 0)

    # Slightly out of focus, as the nearest things would be.
    layer = layer.filter(ImageFilter.GaussianBlur(1.5 * SS))
    return layer.resize((W, H), Image.LANCZOS)


# ------------------------------------------------------------------- shading

def shade_layer():
    """A still overlay: darker edges, and a soft shadow behind the menu in the
    middle so its text stays easy to read. Stretched to the window."""
    w, h = 320, 180
    ys = np.linspace(-1, 1, h)[:, None]
    xs = np.linspace(-1, 1, w)[None, :]
    edge = np.clip((xs ** 2 * 0.55 + ys ** 2 * 0.9) - 0.15, 0, 1) ** 1.2
    centre = np.exp(-((xs / 0.42) ** 2 + (ys / 0.75) ** 2)) * 0.55
    alpha = np.clip(edge * 0.85 + centre, 0, 0.9)
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[..., 3] = (alpha * 255).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


if __name__ == "__main__":
    back_layer().save(os.path.join(OUT, "launcher_back.jpg"), quality=92, subsampling=0)
    front_layer().save(os.path.join(OUT, "launcher_front.png"), optimize=True)
    shade_layer().save(os.path.join(OUT, "launcher_shade.png"), optimize=True)
    print("wrote icons/launcher_back.jpg, launcher_front.png and launcher_shade.png")
