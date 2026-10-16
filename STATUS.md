# Status

The working log: each investigation and fix, newest first, as "rounds".
PROGRESS.md has the short version.

# Log

## 2026-10-16, round 97: clean-up for the public release

- Removed: the TRW_AUTOSTART and TRW_NO_AUDIO_CAP testing switches, the F8
  frame dump and tools/texdump.py (which read its dumps), the unused MIPS
  patch pipeline (patches/, PatchesLib), src/main/scheduler_workaround.cpp
  (carried over from BattleTanx, never hooked), include/macro.inc (a splat
  template), the online play study, and CMake leftovers (macOS, mods
  embedding, an empty-src placeholder, forked-runtime detection).
  patches/ui_funcs.h and the two headers it includes came back: RecompFrontend
  includes it from the game's patches/ folder (ui_api_events.cpp), which the
  user's Windows build caught; the sandbox build found a copy one folder up.
- Long comments trimmed; notes about the user and the sandbox out of the
  code. PROGRESS.md rewritten to the current state; RAM_NOTES.md has the
  1.1 Frag Tag fix.
- BUILDING.md and README point at the public repo, wootbeer/PaxDino.
- Checked: no personal information in tracked files or commit messages;
  our sources compile clean with -Wall -Wextra.

## 2026-10-16, round 96: Linux / Steam Deck AppImage

tools/package-linux.sh (after the user's Battlezone port): a Release build
packaged as an AppImage with linuxdeploy, in a .tar.gz with the README and
license notices; the system's GTK stack is left out. Inside an AppImage the
game's own folder is read-only (AppRun runs it from there, for assets/), so
the Music folder and crash_log.txt / crash_rdram.bin go beside the AppImage
($APPIMAGE), and Add to Steam records the AppImage rather than the mounted
program. Settings and saves: ~/.config/trwrecomp. README: Linux and Steam
Deck builds marked experimental. Checked: the AppImage built here and ran
(launcher, v0.1.0), making Music/ beside it.

## 2026-10-16, round 95: README and CHANGELOG for the first beta

README rewritten in the style of the user's other ports (features, game
files with SHA-1s, download, controls, issues, credits). CHANGELOG.md added:
0.1.0 Beta, listed against the original N64 game (Added: not in the
original; Changed: original behaviour changed; Fixed: the original's bugs).
Version string 0.1.0 (launcher). The mouse sensitivity description said Esc
opens the menu; it's F1.

## 2026-10-16, round 94: launcher Preset: Classic / Modern

User: a small entry under Settings on the launcher switching between two
presets, each setting only its own list of options (the rest kept), Modern
the default. src/main/presets.cpp: "Preset: Modern" / "Preset: Classic"
(smaller than the other entries); a click switches and saves each tab's
settings file (graphics as with Apply). The last one picked is kept in
general.json ("preset", hidden).

- Modern: Original 2x, Downsampling 4x, Expand, Framerate Display, MSAA 4x
  (or the most the card allows), Bloom Low, HUD Expand, CRT with Menus
  Only, Clear Menu Panels on, Mouse in Menus on, Look Spring Override on
  (all four players), Weapon Reverb on at 50%, Demo off.
- Classic: Original, Downsampling off, Original aspect, Framerate
  Original, MSAA off, Bloom off, HUD Original, no filter (Menus Only off),
  Clear Menu Panels off, Mouse in Menus off, Look Spring Override off,
  Weapon Reverb off (its amount kept), Demo on.
- Defaults: our own options now default to Modern's values (Bloom, Screen
  Filter, Menus Only, Look Spring Override, Weapon Reverb, Demo). The
  frontend's graphics defaults are its own, so on the first launch (no
  graphics.json yet) Modern is applied as a whole; existing settings are
  untouched until the entry is clicked.
- Checked: a first launch wrote all of Modern; the entry switched to
  Classic and back, writing each list (window mode etc. kept); the title
  survives a restart. MSAA came out None here (the sandbox's renderer
  doesn't support it, so the option is disabled).

## 2026-10-15, round 93: Local Multiplayer asks for a restart

User: a pop-up when Local Multiplayer is switched, so the game gets
restarted. It's read once at launch (main.cpp), so changing it in the
menu now opens a "Restart Required" prompt with OK (on: also pointing to
Assign players on the Controls tab). Switching it back to what the game
started with shows nothing. The prompt's text is plain (no tags). Checked:
on shows the prompt, OK closes it, back off shows none.

## 2026-10-15, round 92: split screen doorway "seams" not found

Round 79's note (rooms through doorways drawn at 4:3 in widescreen split
screen) was looked into: with RT64 logging every perspective projection it
didn't widen (rt64_projection_processor.cpp, the AUTO aspect test), five
levels' 2-player matches logged none in a match. The old screenshot reads
as level geometry as well. Not reported by the user; left unless one shows
up. Seen: a thin vertical line across a view on some levels (likely
cracks between polygons, not a 4:3 edge).

## 2026-10-15, round 91: right click is the weapon's second function

User: right click as secondary fire; in the game that's B (the weapon's
second function). Right click in a match pressed L; it presses B now, as
R already did. In the menus it's still back.

## 2026-10-15, round 90: player 2's mouse look; split screen HUD by its player

- User: with the keyboard and mouse on player 2, the mouse didn't look. The
  player on a port was found through the list at +0x16E0, which in a match
  holds only the first player (checked: its link is 0 in a 2-player
  match), so port 2 found none. The players are in a row from
  *0x8013EF84, 0x16E8 apart, port n the n-th; that's used now (the list
  kept as a fallback). It also gave player 2 the weapon keys, the wheel's
  hold under the pause menu, and Look Spring Override's mouse handling, and
  the cheats (Infinite Health, Moon Jump, Rapid Fire) reach every human
  player instead of player 1 only. Checked here: port 2's lookup finds
  player 2 (0x80693C88; the list found none). User: player 2's mouse look
  works.
- User: player 1's HUD still jumped to the edges now and then (round 89
  didn't fix it). Each HUD's view now comes from its own player (the HUD
  routine's s0, 0x16E8 apart from the first) rather than from counting the
  HUDs since the frame began. Not reproduced here; the cause isn't known.
  User: no jump seen in a session after it (it was rare). If it comes
  back: the stretch to the sides suggests player 1's pass not being widened
  for a frame (its full-screen scissor trick), to be fixed in RT64.

## 2026-10-15, round 89: split screen: player 1's HUD jumping to the edges

User: in split screen, now and then (rarely, standing still) player 1's HUD
jumped to the window's edges and straight back. The HUD code tells a split
screen by the human players in the match (the ports' player objects with
+0x5D0 set); with one counted and the first view as tall as the screen's
(its stored size stays the full screen's in split screen), player 1's HUD
took the one-player anchoring for that frame. The flag looks to drop for a
moment as a player respawns (not caught here: 8 minutes of a still 2-player
match counted 2 throughout). The count is now the most seen since the
match began, cleared outside matches. Checked: the stacked HUDs and a
one-player match's as before.

## 2026-10-15, round 87: Q as the Raptor

User: as the Raptor, Q (next weapon) froze the aim a moment, moving still
working. Q (and the wheel and number keys) pick a weapon by holding A
until the weapon wheel opens, with the mouse's look dropped meanwhile; the
Raptor's and Mite's own attacks (weapons 16 and 17) have no wheel, so it
waited the full 12 reads for nothing. Now those keys do nothing while
weapon 16 or 17 is held (+0x62E). Not checked here (no Raptor match in
the sandbox).

## 2026-10-15, round 86: Monkey Tag's counter with the other counters (Expand)

User: in Monkey Tag the monkey counter (the second row of counters, at the
top right, about 27% down the screen) stayed by the 4:3 picture's edge
while the skull counter above it went to the window's edge. The HUD's
anchoring (lib-patches/rt64/0003) only took the top quarter to the sides.
lib-patches/rt64/0007: from the top quarter to a third, pieces well to a
side (the outer 30% each way) go to that side too; those nearer the middle
stay centred. Split screen draws each HUD as in 4:3 (round 79), so no
change there. Not checked here (no Monkey Tag match in the sandbox).

## 2026-10-14, round 85: Graphics tab order, Clear Menu Panels with the filters, pause options

- Clear Menu Panels moves from Enhancements to the Graphics tab, under
  Screen Filter and Menus Only, on by default (its saved value in
  enhancements.json is left behind). Choosing a filter (the applied Screen
  Filter changing to one) turns it on: the filters' lines clashed with the
  panels' scanlines (user). First done each frame from the game, which only
  saw it after Apply and with the game running (the user saw nothing
  change in the menu); now a change callback on Screen Filter sets it in
  the menu as the filter is clicked (update_option_value, before Apply).
  Turning it off afterwards stays. Checked: clicking Scanlines with the
  panels off showed them on at once.
- Bloom sits under MS Anti-Aliasing (lib-patches/RecompFrontend/0012: a
  callback create_graphics_tab() calls right after that option).
- Menus Only shows the filter on the options opened from the pause list
  too (drawn full-screen like the front end's, in its 284-pixel mode), not
  on the pause list over the match. Checked: scanlines on those options.
- Right click on those options goes back as Escape does, through their
  Resume Game item (a B there does nothing: they're only left that way);
  B in their sub-screens as before. Checked: a right click there returned
  to the match. A left click on Resolution stepped it (it already did with
  the cursor showing; with the cursor hidden after keys, a click is A on
  the highlighted item, as elsewhere).

## 2026-10-14, round 84: per-player override fixes, controller profiles, filter in menus only

- Look Spring Override for players 2-4 did nothing (user): their settings
  blocks are 0x96 apart, so not word-aligned past player 1's, and the
  check meant for player objects (word-aligned) skipped them. Checked with
  two ports in the sandbox (player 2 mirrored from the keyboard): with only
  player 2's override on, player 2's view held (-0.84 to -0.87, byte 0) and
  player 1's went back to level.
- Each player's override shows only on that player's page of the Controls
  tab (lib-patches/RecompFrontend/0011: `show_option_on_controls_tab` takes
  a player; player 0's page in single-player mode), all four named Look
  Spring Override. Before, every player's page showed all four.
- Saved controller profiles (each player's) kept the library's old layout
  (user: player 2's sticks swapped back). Once, after the controls config
  loads (trw::controls::update_controller_profiles; a marker file,
  controller_layout_v2, beside it), every controller profile's bindings
  for moving, looking, fire and jump that still start with the old default
  get the new default; ones set by hand stay.
- Screen Filter: Menus Only (Graphics tab, under it; greyed out with no
  filter): the filter in the front end only (title, menus, selects), not in
  a match (its pause menu included).
- Seen while testing, not new: in the sandbox, CRT now crashes the game in
  func_0028E9DC while a level loads (reading 0x00000003), with or without
  Menus Only and without the bloom patch too; Scanlines and LCD don't.
  Likely the sandbox's CPU renderer: CRT is the heaviest filter. Not seen
  on the user's machine so far.

## 2026-10-14, round 83: Look Spring Override per player; dual-stick controller defaults

- Look Spring Override is one option per player (players 2-4 on the
  Controls tab only with Local Multiplayer on; player 1 keeps the first
  option's id, so its saved value stays). Each player's own look spring
  byte (+0x7D of its settings) is overridden by its option, and the mouse's
  pitch kept by the option of the player the mouse is assigned to.
- Controllers' default bindings (src/main/fps_controls.cpp,
  set_controller_defaults): the left stick moves and strafes (the C
  buttons), the right stick turns and looks (the N64 stick), and the right
  trigger fires (Z) with the left one jumping (R), like a modern shooter. We hadn't set controller defaults before, so RecompFrontend's
  own applied (left stick the N64 stick). The digital C buttons (Y, B,
  right stick click, right shoulder) and every other button are as before.
  Defaults only fill a profile's bindings when it's made or reset, so a
  saved profile keeps its own until reset to defaults.

## 2026-10-14, round 82: Look Spring Override (Controls tab)

User: the view went back to level mid-match, with the mouse (single player
too) and with player 2's controller; the game's look spring option, turned
off, didn't stop it. The game's own behaviour (reproduced in builds from
before the day's changes), so a toggle: Look Spring Override, off by
default (the game as it is).

What the game does:
- A player's settings are at 0x801402D8 + port * 0x96 (the player object
  points to them at +0x5D8; the name, "PLAYER1", at +0x84). Byte +0x7D is
  look spring: on, the look input (+0x6A0) is the stick's position, back to
  0 when it's centred; off, it's kept where the stick left it.
- The look code (func_002231B0 standing, walking and so on, func_00224078,
  func_002251B8) eases the pitch (+0x724) toward the angle the look input
  points at (func_00274860: value += (target - value) * rate * dt). Mouse
  look (src/main/fps_controls.cpp) sets the pitch and not the input, so the
  pitch was eased back: to level with look spring on, to where the stick
  last left it with it off.

With the override on:
- src/game/extras.cpp sets +0x7D to 0 for every player in a match (look
  spring off for controllers too), and puts each port's own value back when
  the override is turned off.
- Hooks before the three easings (turokrw.us.toml) give the mouse's player
  a target of its current pitch plus the game target's change since the
  last frame, so the mouse's angle stays and the look keys (or a stick)
  still move the view from there.

Checked (sandbox, values read from the player): override off, the view
eased back to level after a look key and after a mouse-style pitch change
(as before); on, both held (-0.78, -0.30), and a look key moved on from the
mouse's angle.

## 2026-10-14, round 81: Weapon Reverb (Sound tab)

Sound tab: Weapon Reverb (off by default) and Weapon Reverb Amount (0-100,
50 by default; 100 adds 38 of 127, what 30 of the full range added at
first, which the user found too much): a little more of the game's own reverb on gunshots,
projectiles, impacts and explosions so they carry in the mix; voices,
footsteps, pickups, weapon switches and menu sounds as they were
(src/game/weapon_reverb.cpp).

What the game does (found with sandbox-only logging hooks):
- The synth is made (func_002B36A0, alSynNew; config built in
  func_00257300) with fxType 6, a custom reverb of the game's own (params at
  0x800CB634: 6 sections over 4800 samples, about 0.2 s at 22050 Hz, quiet
  outputs). Each sound's reverb amount (0-127) is set as it starts in
  func_00257794 (alSndpSetFXMix at 0x002578B4: its own plus a global one at
  +0x2B9C, 0 here); the game's own amounts are 0 to 3 (to 7), so next to
  nothing reached the reverb.
- Sound effects are asked for through func_0025DDF4 (effect, position) and
  func_0025DE60 (also a volume; animations' sound events through a table of
  handlers, func_002672A4 for one), and start within the call;
  func_0025DED4 plays the menus' sounds and others of the player's own. The
  effects are numbered in blocks of 100 (decimal): one per weapon (800 Tek
  Crossbow ... 2900 Flare Gun, 3100 Sentry Turret; each weapon's fire,
  reload and so on), 3000s the weapons' select sounds, 3300 explosions
  (3302), 3310s the Raptor Claw's and Mite Bite's, 3400 the weapon switch,
  3500s pickups, 3700s menus, 5000s footsteps. Found by stepping player 1
  through every weapon (All Weapons, Unlimited Ammo) and firing each, and
  matches of each Loadout Restriction.
- The effects' and projectiles' own calls (a spawned effect's sound in
  func_00280014: muzzle flashes, rockets, energy bolts, explosions; flight
  and hits in func_00281BF4 and func_00283DAC; impacts in func_0027CC50 and
  func_0027D3FC) use numbers from the weapons' blocks and some below 800.
  The players' movement code (func_0021B468, func_0021BFBC) plays
  footsteps, landings and 2555 (among the Inflator's numbers).

So: a request through func_0025DDF4/func_0025DE60 from the effects' and
projectiles' calls, or with an effect in 800-2999 or 3100-3309, and not
from the movement code, gets the amount (0-127 from the slider) added to
its sounds' reverb amounts. Checked with logging (Explosive, Energy,
Hammers, Bullet): shots, launches, rockets, explosions and hammer swings
went from 0-7 to 38-45 at 30; switches, pickups, footsteps, 2555 and the
sounds started outside those calls (ambience, voices) unchanged. Not
checked: how it sounds (the sandbox's recordings carry too much of the
level's other sound to measure a short, quiet reverb tail).

## 2026-10-14, round 80: Bloom (Graphics tab)

Graphics tab, under Screen Filter: Bloom, Off (the default), Low, High. A
glow around bright, strongly coloured things (lava, neon, energy shots),
written for this port in RT64's final pass (lib-patches/rt64/0006, the
same place as the screen filters, round 57):

- What glows is picked by colour, as the N64 picture has nothing brighter
  than white: the game's lava is no brighter than its sunlit sand (both
  around half of white, sampled on Fire Temple) and only purer (lava 0.9+
  saturated, sand and wood 0.8 and less; text and stone near grey). So only
  colours past 0.84 saturation glow (fully at 0.94), with some brightness,
  at their hue's full brightness so dim lava still glows (shared/rt64_bloom.h).
- Compute passes before the swap chain's pass (VIRenderer::prepareBloom,
  shaders/BloomCS.hlsl): the glowing parts shrunk to 240 lines (each
  output texel judges up to 6 x 6 input pixels on their own), blurred
  (Gaussian, sigma 3), halved to 120 lines and blurred again; fixed sizes,
  so it looks the same at any resolution.
- The VI pass (shaders/VideoInterfacePS.hlsl) adds the two levels' average,
  less 80% of what glows at the pixel itself, so a wide patch of lava gets
  a glow at its edges and only a little inside, not a wash of light over
  it; capped at 0.6 and blended as screen, on top of any screen filter.
  Low adds it at 0.6, High at 1.2.
- Off: no passes; 1 x 1 textures stay bound for the VI pass.
- Only in play (src/game/extras.cpp): off in the front end and while a
  match is paused (0x801407D4), where the menus' gold text glowed (user).
- Checked (sandbox, Vulkan): Fire Temple's lava (with the colour key run
  offline on a captured frame too), The Pit's neon strips (a soft halo on
  Low, wider on High), grey and sand levels and the pause menu unchanged.
  The HUD skull's red eyes get a faint glow on High: the HUD is in the same
  picture. Not checked: D3D12 (Windows' default; the shaders compile
  with the same dxc).

## 2026-10-14, round 79: split screen: player 2's jitter; both HUDs as in 4:3

User: in widescreen, player 2's HUD jittered (player 1's didn't); player 2's
objects and textures jittered with the Display refresh rate, in both aspect
ratios.

- Objects: everything drawn in a match frame is in one RT64 matrix group
  with automatic matching (round 28), and in split screen each view draws
  the level and its things again, so RT64 paired the second view's matrices
  with the first view's and interpolated between the two. The second view's
  start (its colour buffer, the first after a HUD) took the weapon's handling
  for it; it now replaces the base group with one of its own (id 0x7EA80000 +
  view), so its things are matched with its own last frame.
- HUD: round 77's anchoring to each view (lib-patches/rt64/0006, now
  removed) was what shook player 2's HUD. Each HUD is now drawn as in 4:3
  with two players above each other: anchoring off, gEXSetRectAspect(Adjust).
  RT64 widens a pass only where its scissors add up to the screen's shape;
  player 2's pass did, player 1's (its view, the top half) didn't, so
  player 1's HUD was stretched whatever the rect aspect: it's drawn under a
  full-screen scissor (pushed and popped around it), which widens its pass.
  Side by side (player 2's view scissor half the width, from the frame
  before) and with three or four players, the HUDs are left alone.
- Checked: both HUDs at the same 4:3 places, both views' 3D still widened,
  one player's HUD unchanged. Not checked here: the jitter itself (the
  sandbox's display is 60 Hz), the side-by-side layout.
- Seen while testing, not new: in widescreen split screen, rooms seen
  through a doorway are sometimes drawn at 4:3 inside the widened view (a
  visible seam at the doorway's edges).

## 2026-10-14, round 78: local multiplayer with the keyboard and a controller

User: keyboard assigned to player 1, a controller to player 2; on the
loadout both drove player 2's column and player 1's didn't move.

In RecompFrontend's input, each player has a controller profile and a
keyboard profile (an index pair, all starting 0), and get_n64_input adds
both. The keyboard is read the same for every player, so a player's
keyboard profile is all that keeps the keys apart. Committing the
assignment set only the assigned device's profile: player 2 (controller)
kept keyboard profile 0, the single-player keyboard profile, so the
keyboard drove player 2 too; and the keyboard player got a new
per-player keyboard profile with no bindings, so player 1 got only what
src/main/fps_controls.cpp adds for the mouse's player in menus (WASD,
Enter, E), which is why the character select seemed to work.
lib-patches/RecompFrontend/0010: a controller player's keyboard profile is
none (-1); the keyboard player uses the single-player keyboard profile
(its bindings and the game's defaults). Controllers are read per player,
so the keyboard player's controller profile is left as it was (the menus
read player 1's for controller navigation).

Not tested here (no second real device in the sandbox; the earlier
split-screen tests faked a connected port, which skips the assignment).

## 2026-10-14, round 77: split screen: HUD anchoring per view; the menu cursor with two players

Reproduced in the sandbox by reporting two controllers connected (a local
change only), both driven by the keyboard.

- HUD: the first view's size (0x8013F008 +0x29C / +0x2A0) reads 480 x 360 in
  a two-player split screen too, so the HUD's edge anchoring (meant for one
  player) ran there and judged the screen's quarters: player 1's skulls went
  to the right edge, player 2's health and the lower half of a targeting
  bracket to the left. Now:
  - The players are counted (ports 0-3 of the player objects, +0x5D0).
  - One player: anchoring by the screen, as before.
  - Two above each other: by each player's view. Player 2's last scissor
    before its HUD is its view (0,178)-(479,357); player 1's is the whole
    screen, so its view is the rows above player 2's (kept from the frame
    before). gEXSetAutoAnchor takes the view's top and bottom rows (bits
    8-19, 20-31; lib-patches/rt64/0006); RT64's quarters are of them. The
    widened scissor spans the view's rows only.
  - Side by side, three or four players: off.
  - RT64's DisplayList::p1(0, 32) can't mask 32 bits (1 << 32), so the
    parameter is read as the whole word.
  Checked: both views' health at the left edge, skulls at the right,
  crosshairs centred; one player unchanged.
- Menu cursor: in split screen the character select marks each player's
  cell in one grid and the loadout has a column per player, so the cursor
  steered the mouse player toward whichever mark it found. A list with more
  than one marked item, or a menu with more than one marked list of 3+
  items, now isn't taken: no hover, a click picks as A (as before the
  cursor). Checked: the two-player grid doesn't move with the mouse; one
  player's menus still follow it.

## 2026-10-14, round 76: the level select's flash: frame interpolation

The flash on the level select happens with the Display refresh rate only
(user, both aspect ratios): RT64's interpolated frames between two of the
level select's, whose preview is a 3D picture drawn into the menu, came
out without most of the menu. At each frame's start (trw_frame_buffers_set)
the display list now gets gEXSetRefreshRate: 0 in the front end (no
interpolated frames), UINT16_MAX in a match (RT64 works out the rate, as
before; RT64 keeps the last rate set, so it's set every frame). Checked:
menus and a match run with Display set. Not checked here: the flash's
absence (the sandbox's display is 60 Hz, so nothing was interpolated).

## 2026-10-14, round 75: the level select's bars, again

The box beside the preview still changed in the user's video (it only
changed texture after round 74): the images filling the background's holes
were copied into the bars, so the bar beside the preview followed whatever
filled the hole ("....", it fading, nothing). Now only a menu's background
(its first full-screen image) is copied into the bars; the hole patches
aren't (nor redrawn over the background's copy), and the bars show the
background's edge rock there, the same whatever fills the hole. Checked by
recording a level change, and the main menu and character select bars.

Not found: a full-screen flash in the user's video (one frame without the
skulls, the panels' insides showing bright rock), only in widescreen. The
sandbox's recordings don't show it.

## 2026-10-14, round 74: the level select's bar box; bots and the Warhammer

- Level select, widescreen: as a new level's preview fades in, the game
  draws the "...." picture over it, fading (its alpha, the top byte of
  +0x10, below 0xFF), after the background. As a hole patch it got its own
  copies in the bars, faint, under the bar's full shade: a dark box beside
  the preview. A fading patch isn't copied now. The bars also get copies of
  the background stepped inward by the rock at its outer edge (17 menu
  pixels, three steps), under the rest, so holes near the edge show that
  rock. Checked by recording the sandbox's screen through a level change
  (screenshots were too slow to catch it).
- Loadout Restrictions: a bot whose AI wants the Warhammer while it has
  ammo for the rule's guns gets the stand-in instead (a gun-using
  character kept swinging it in a Scorpion match). The Raptor Claw and Mite
  Bite characters fight only with those and keep them. Checked: a bot's
  wanted Warhammer became the Scorpion Launcher.

## 2026-10-14, round 73: the team select's look; picked-up weapons under Loadout Restrictions

- The team select's top team came apart in widescreen: the HUD's automatic
  anchoring sent its tiles (in the top quarter) to the screen's sides, as
  the weapon wheel's once did. The team select is drawn right after the
  wheel (`func_00218F08`, given the player + 0xCCC, next to the team fields
  at +0xCC8 / +0xD3C), so anchoring now stays off over both calls
  (0x0021EFBC-0x0021EFD4). Checked: all four teams whole and centred.
- Loadout Restrictions: the weapons picked up in a level (Cerebral Bore,
  PFM Mine, Iron Claw, Sentry Turret) are usable whatever the rule, like
  melee. They're never handed out: the loadout keeps them as the game has
  them, a picked-up one keeping its wheel slot first. Checked: a Cerebral
  Bore given in an Energy match stayed in its slot. Toggles for them are
  for later.
- Launcher: the menu is as wide as the logo's drawing (its highlight
  spanned half the window).

## 2026-10-14, round 72: the menu cursor on the level select and the match settings

- Match settings: a marked list after all, but drawn as a second menu over
  the level select's, whose full-screen item (type 2005) then won as the
  last list published. Now the full-screen item isn't taken, and of the
  menus drawn in one frame (within 8 ms) the one with the most items is
  kept (the serial goes up once a frame). Tested: hover follows the rows,
  a click on Kill Friend flips Yes/No.
- Level select: the skulls (game modes, 21 x 31) and rings (levels,
  16 x 12) aren't marked and the cursor is drawn apart. Its place is in the
  screen's state, found through the level name's text widget (string 751):
  its +0x38 points at the name's buffer, with the column 0x40 after it and
  the row 0x44 after (row 0 the skull; 1-10 down the column, 14 pixels
  apart from y 58; the first column's branch to the left, x 15 left of its
  skull, counts 16 at y 114 down to 11). The skulls and rings are published
  as the items, the one at the cursor highlighted, and the usual steering
  moves it. Tested: skull 1, column 0 row 5 (The Pit), the branch's row 16
  (Amphitheater), column 1 row 3 (Matrix), and a click on Matrix went on to
  the match settings. Up to 48 items a menu now.

## 2026-10-14, round 71: the team select with the mouse; loadout clicks; Shift-click

- The team select at the start of a team match (Capture the Flag, the
  fourth mode skull) is a wheel of four teams: the stick points at one, A
  picks it (A with the stick centred does nothing). The mouse turned the
  view and a click fired, so it couldn't be used. It's open while the
  player's `+0x11B4` is set (`+0x11B8` too; both 0 in a deathmatch) and no
  team is chosen (`+0xCC8` -1; `+0xD3C` is the team pointed at). The mouse
  now moves a virtual stick there (as in the weapon wheel with E held; a
  controller's stick still works until the mouse moves), and a click is A,
  held at least 250 ms (a quick one only marked the team). Tested: pointing
  right lit the right team, a click chose it, and mouse look came back.
- Loadout: its column is items (type 9) each over a bar (type 8), the
  weapon slots, then OK. A click on a slot steps its weapon (Right; A does
  nothing there), and on OK is A. Tested: a click changed a slot from the
  Tek Crossbow to the Boomerang.
- Shift and a click steps a setting or a slot back (Left). Shift is bound to
  L, so it no longer hides the cursor, and its L is dropped in the menus
  while the cursor shows. Tested: Blood Color Red, click Green, Shift-click
  Red.
- The level select (the mode skulls and the level tree) and the match
  settings aren't lists of marked items (their selection is kept elsewhere),
  so the cursor can't point there yet; a click selects as before (checked:
  a click on the tree went on to the match settings). Every front-end menu
  also has a full-screen item (type 2005, flag 0x40) that becomes the "list"
  where there's no other: it only makes a click select, as without one.

## 2026-10-14, round 70: the menu cursor on the character select; clicking settings

- Stuck on the character select: the locked cells are hidden items, but the
  game's highlight can still move onto them (up from the right-hand
  character went to the locked cell above it). They weren't in the item
  list, so with the highlight on one the steering lost it and stopped. The
  list now keeps hidden siblings too (the highlight is looked for on hidden
  items as well), and only shown, not greyed-out ones can be pointed at.
  Steering marks a direction that didn't move the highlight as blocked from
  there and tries the best one left (the marks clear when it moves).
  Tested: bottom, left, right, Adon (through the locked cell), left,
  bottom, right, each reached.
- A click on a setting with a value (types `0xB64`, cycled, and `0xB63`, a
  slider: Blood Color, 2 Player Layout, Resolution, Brightness) pushes
  Right instead of pressing A, stepping the value on (Blood Color: Red,
  Green, Off).

## 2026-10-14, round 69: the menu cursor's look and aim

- The cursor is a green wire-frame arrow (the outline and two inner edges),
  a dark rim and a see-through dark fill, so it shows on the gold text and
  the dark rock; the green's brightness varies a little for grit
  (src/main/menu_cursor.cpp, drawn at start-up, 64 x 96).
- Options (from the main menu and from the pause list) mixes item types:
  plain items, switches and sliders (`0xB61`, `0xB63`, `0xB64`, ...). Only
  the highlighted item's type was taken, so most rows couldn't be pointed
  at. Any item-type sibling counts now (all 7 rows on the main Options).
- Gaps: inside the box around a menu's items (3 menu pixels of margin) the
  nearest item is the one pointed at; the character select's pictures are
  smaller than their cells, and list rows have gaps between them. Clicks
  outside that box still pick the highlighted item.

## 2026-10-14, round 68: Loadout Restrictions crash; renames

- A crash starting a Scorpion match (user's PC; the player had the Grenade
  Launcher in their loadout): access violation in `func_00255E78` (unlinking
  a node from a linked list whose next pointer was garbage), during the
  level's object setup (`func_0044C100` <- `func_002869F8` <-
  `func_00234FCC`). Not the loadout: in the sandbox the same setup (Grenades
  in the explosive slot, Scorpion) starts and everyone holds the Scorpion
  Launcher. The pickup rewrite was the suspect, and two things it did were
  untested: the special weapons' pickups (0x84B-0x84F, other objects than
  ammo crates) became crates, and a crate could become the other type of the
  kind's pair when the level had only one of them. Now only ammo crates are
  changed, and only to types the level has. Not confirmed on the user's
  level (the sandbox levels tried have neither case).
- The level's type list (`*(0x80119DC8 + 0x94)`) has an entry per placed
  object, not per type (347 on the first level, 124 of type 7), so repeated
  types in it are normal.
- Starting ammo: a player (re)spawning in a multiplayer match gets 50
  bullet ammo and no energy or explosive (`func_0021AF6C`; 100/100/50
  otherwise), whatever the loadout, so restricted matches started with
  nothing to fire. Hooked after those writes (0x0021B08C): Energy gives 100
  energy, Explosive and Scorpion 10 explosive, players and bots. Tested at a
  match's start (Energy: 50/100/0 for both); respawns go through the same
  function but weren't seen.
- Weapons is now **Loadout Restrictions** ("Limit matches to a single weapon
  type."). Mouse in Menus moved to the top of the Controls tab, without a
  description.
- Sandbox only: with the saved graphics settings (Expand, 2x, CRT) the game
  now crashes at boot every time (`func_0028E9DC` reading address 3, on the
  thread from `func_0028E880`), with or without the menu cursor's overlay,
  and boots with the default graphics settings. The same settings booted
  earlier in the day; left for now.

## 2026-10-13, round 67: a mouse cursor in the game's menus

Controls, **Mouse in Menus** (the first option; on by default). Moving the mouse in the
front end's menus or a paused match shows a cursor (drawn on a UI overlay
that takes no input, src/main/menu_cursor.cpp); pointing at an item moves
the game's highlight onto it and a left click picks it; a click in a gap
between items does nothing. Right click goes back and the wheel moves up and down as
before. Keys or a controller hide the cursor; controllers are unchanged.
Off, clicks pick the highlighted item as before. Clicks skip the boot
screens, the title and the fly-through either way.

- The items (src/game/widescreen.cpp, from the menu hook): in each list or
  grid of a menu the highlighted widget has flag `0x40` (shown `0x8`) and is
  an item (type 9, or the game's own types from `0xB5F`; groups can be
  marked too, such as the character select's skin counter, but the stick
  doesn't move them). Its siblings of the same type, shown and not greyed
  out (`0x100`), are where the highlight can go. Positions are relative to
  the parent; the front end draws at 284 x 222, the pause list at 480 x 360
  (the VI width at `0x8014B510`). The list with the most items on screen is
  taken (the character select keeps a one-item list at x 300).
- Steering (src/main/fps_controls.cpp): the stick is pushed toward the item
  one step at a time, by the items' positions, so lists and grids work alike
  and greyed-out items are skipped by the game. A push lasts while the menu
  is drawn twice, and the next waits until it's drawn three times more: the
  pause list wants the stick back in the middle a while (shorter gaps
  moved it every other push). A push that doesn't move the highlight tries
  the other direction; four in a row end it.
- The cursor moves by the mouse's own motion, only in the menus; mouse look
  reads its motion separately, only in play, so neither carries over.
  Mouse buttons held when play and the menus change over are ignored until
  let go (a click on Resume doesn't fire; firing while pausing doesn't pick
  Resume). A click is held as A for at least 100 ms (a quick click could be
  missed between controller reads).

Tested: main menu (hover past the greyed-out item, click Multiplayer),
player count, character select (the grid), pause list (Resume to Quit),
mouse look unchanged by motion while paused. Not covered: parts of screens
not in such a list (the loadout's slots and OK, the trials map, level
select): a click outside the list's items picks the highlighted item, as
before, so the wheel and a click still get through them.

## 2026-10-13, round 66: Weapons: pickups and the HUD's ammo counters

**Pickups.** Round 65's hook on `func_002ABB0C` was on the wrong system: the
`0x13xx` types are a different item table, and the multiplayer ammo crates
never reached it. What the levels place:

- The level's object types: a list at `*(0x80119DC8 + 0x94)` (count at
  `+0x4`, halfword types from `+0x8`). Placed objects (pool from
  `0x8012B540`, stride `0x1E0`; a pickup's first byte is 3) keep their index
  into it at `+0x4` (`+0xE4` when the first byte is 1; `func_0024E904`), and
  their position at `+0x8`/`+0xC`/`+0x10`.
- Ammo crates: `0xBC4`/`0xBC5` energy +100, `0xBC6`/`0xBC7` bullet +50,
  `0xBC8`/`0xBC9` explosive +5 (table at `0x800CDED0`, `0x14` per entry:
  message, type, sound, kind at `+0xC`, amount at `+0xE`; handler
  `0x002ACA48`). The special weapons' pickups are `0x84B`-`0x84F` (table at
  `0x800CDE70`).
- A pickup's model is chosen when it spawns: changing `+0x4` afterwards
  changes what it gives, not how it looks. `func_0028B1C4(world, obj, type)`
  changes both.

So while a multiplayer level loads (game mode `0x801407B8` past boot and
not the front end, 8), the list is rewritten once: ammo crates become the
allowed kind (keeping which of the pair each was) and special weapons'
pickups the allowed kind's crate, only if the level already has that kind
(so its model is loaded). Hammers leaves the pickups as they are. Tested
(Explosive, Gauntlet): every pickup in the list `0xBC8`; not yet seen
collected or compared on screen.

**HUD.** `func_0021EA30` draws the three ammo counters (sprites 3050-3052),
each in a block gated by its flags and the held weapon; hooked at
`0x0021EC04`/`0x0021ECEC`/`0x0021EDD4` to skip the counters no allowed
weapon uses (`trw_ammo_counter_shown`): Bullet, Energy and Explosive show
their own, Scorpion explosive, Hammers none. Tested (Gauntlet): at the
match start All shows health and the three counters, Energy health and the
energy counter only.

**Set per match.** The rule is read once, as the level loads (game mode
past boot and not the front end), and kept until the front end, so changing
the option mid-match does nothing until the next match. The option is
also greyed out then: lib-patches/RecompFrontend/0009 adds
`config::set_option_disabled_while(option_id, check)`, run by the options
page on the UI thread each frame; Weapons' check reads an atomic the game
thread sets (`trw::config::weapon_rule_locked`). Tested: greyed out in a
match, a click on another rule does nothing. Not tested: enabled again back
at the menus (the same front-end check that resets the rule).

## 2026-10-13, round 65: Weapons (gun-game restrictions)

Enhancements, **Weapons**: All, Hammers, Bullet, Energy, Explosive or
Scorpion, for every player and bot in multiplayer matches (trial type
`0x80140215` = 0). src/game/weapon_rules.cpp. Found this round:

- Player objects: `*0x8013EF84 + port * 0x16E8`, ports 0-3 players, 4-7
  bots (`+0x5D0` set in the match). `+0x770` (halfword) the weapon wanted,
  `+0x62E` the one held; `func_0022FD8C` / `func_0022FEE4` /
  `func_002325DC` switch to the wanted one (hooked: a weapon not allowed
  becomes the class's stand-in). Players' loadout: (have, wheel slot) byte
  pairs per weapon from `+0x602`; the wheel (`+0x938`) is rebuilt from it.
  Bots don't use it; their AI sets `+0x770`.
- Ammo: `+0x5F4` bullet, `+0x5F6` energy, `+0x5F8` explosive (halfwords).
  Weapon definitions `*(0x800CB1FC + id * 4)`, ammo use at `+0x20`
  (kind, cost). Bullet: Mag 60, Shotgun, Assault Rifle, Mini Gun. Energy:
  Boomerang, Tek Crossbow, Plasma Rifle, Freeze Gun, Flaregun, Emaciator,
  Inflator, Chest Burster. Explosive: Grenade Launcher, Scorpion Launcher,
  Napalm Gel. Cerebral Bore, PFM Mine, Iron Claw and Sentry Turret have
  ammo kinds 3-6 of their own.
- Pickups: `func_002ABB0C(player, type entry)` switches on the type at
  `+0x4`: `0x13A0` bullet +50, `0x139F` energy +50, `0x1398`-`0x139E`
  explosive +10, `0x13C1` health, `0x1389`/`0x138A` weapons. Hooked so ammo
  and weapon pickups give the allowed kind (health in Hammers).

Each frame the players' loadouts keep melee (Warhammer, Raptor Claw, Mite
Bite: always allowed) and only the allowed class (Energy has 8 and the wheel
8 slots with the hammer, so Chest Burster is left out), and anyone wanting a
weapon not allowed gets the stand-in. Tested (bot match, Gauntlet): Energy
(loadout and wheel all energy, the player and the bot on the Tek Crossbow)
and Hammers (both on the Warhammer, no ammo on the HUD). Not tested: a
pickup being collected under a rule; the pickups keep their own models.

Also: Keep Song and Custom Music Volume are greyed out while Custom Music is
off; a slider greyed out when its page opens showed as enabled
(lib-patches/RecompFrontend/0008).

## 2026-10-13, round 64: menu clean-up

- Tabs: Mods moved to the end (created last). Mouse Sensitivity, Joystick
  Deadzone, Invert Mouse Y and Background Input Mode are shown on the
  Controls tab above the mappings (still stored in general.json):
  lib-patches/RecompFrontend/0007 adds `config::show_option_on_controls_tab`
  (the options page skips them; the controls page draws them and saves their
  config when the tab closes). The same patch drops "Go get some!".
- Sound tab: All Music (was Randomize Game Music), Custom Music (with Open
  Music Folder), Keep Song and Custom Music Volume, under Main Volume; now in
  sound.json, so they start off again once.
- Unlocks: All Characters, All Weapons and Extra Modes moved from Cheats.
- Crosshair: its description is a picture of each style
  (`trw::crosshair::preview_rml`, images queued at launcher init). Colour is
  greyed out for Game and Image, size and opacity for Game
  (`add_option_disable_dependency`).
- Text: Demo, Medals in Solo Matches, All Music renamed; descriptions
  shortened or removed as asked (Main Volume via `update_option_description`).
- Fix (user report): "Attempted to destroy a UI resource that doesn't exist"
  on quitting with a custom crosshair. The overlay destroyed and remade its
  images on each new picture, from the game thread; now the images are made
  once and only their source changes, and nothing is done once the UI has
  hidden the overlay (it hides every context before destroying them). An
  unreadable custom picture is no longer re-read every frame.

## 2026-10-13, round 63: crosshair, new icon and launcher logo

- **Crosshair** (Enhancements): Game, Cross, Open, Dot, Open+Dot, Ring,
  Ring+Dot, T or Image, with Colour (6), Size (50-200% of the game's) and
  Opacity (10-100%). The game's crosshair is HUD sprite 751 (14 x 14, in a
  480 x 360 screen, scale at sp+0x10/+0x14) drawn by `func_002AAB58`; the hook
  skips it and notes where it would be, and src/main/crosshair.cpp draws the
  chosen one there in an input-transparent UI overlay. So it follows the
  game's own: none while paused, between matches or with F1 open; one per
  view in split screen. `trw_apply_extras` runs at each colour buffer, more
  than once a frame, so the noted marks are kept until six calls go by
  without any. Choose Image (a button under Crosshair) copies a PNG, JPG,
  BMP or TGA to `custom_crosshair` in the app folder (shrunk to 256 px when
  loaded). Tested: Open+Dot white 100%, Ring+Dot red 200% at 50%, Image.
- **App icon**: the new PaxDino crosshair icon (icons/app.png, app.ico with
  16-256 px, app.res rebuilt with llvm-rc).
- **Launcher**: the PaxDino logo (icons/launcher_logo.png, the user's PNG
  cropped) replaces the title text; the background gets CRT grit
  (scanlines and grain moved every frame) under the logo and menu only.

## 2026-10-13, round 62: music options, idle demo, menu fixes, online study

- **Keep Song** (Enhancements, under Custom Music): one file plays through
  every song change until it ends. Without it, a file started in a match now
  plays on until the match is over: the pause menu's options ask for the
  menu song (52) and stop the player, and both are ignored within the
  file's match (matches are counted by `set_game_state`; mode not 0-4, 8 or
  20). The shuffle bag still never repeats a file before all have played.
  Tested: 550 Hz test file unbroken from the match start through pause,
  Options, Resume Game and on.
- **Open Music Folder** button under Custom Music
  (lib-patches/RecompFrontend/0006, `config::add_option_button`).
  Explorer on Windows (ShellExecute), xdg-open elsewhere.
- **Randomize Game Music**: `func_0025E274(song)` (the song request) hooked;
  a match's song (56-59) becomes any of 50, 56-60, never the last one.
  Compiled; not heard in the sandbox yet.
- **Demo When Idle** (on by default): the title's update (`func_002A1FD0`)
  starts the attract demo once its idle timer (`0x800CD940`) passes a limit,
  only while a1 is 1; off makes it 0 at `0x002A205C`. Tested: the title
  stayed put for over two minutes. (Modes: 3 Expansion Pak screen,
  4 Acclaim logo, 8 title and front end, 13 a match; the next mode is at
  `0x801407BC`, set by `func_002937F0`.)
- **Trials map cursor with All Levels**: levels not opened in the save are
  built hidden (widget flag `0x8` off): a dot (image `0xC5`) or a boss's
  framed picture. The cursor's pulse ran on the hidden widget. The map's
  cursor level is at `+0xA44`, its node at `+0xA48`; that node is shown while
  the cursor is on it and hidden again after (`show_trial_cursor`). Tested
  on Fire Temple, Earth Temple, Hopeless (dots) and Warehouse (boss).
- **Pause list highlight**: the pause list (a group: box, type 6, then items,
  type 9, flag `0x40` on the highlighted one) only showed a faint bar. The
  other items' text is faded to alpha `0x60` (a widget's alpha is the top
  byte of `+0x10`). The options screen is untouched.
- **Widescreen menus**: the stretched background copies in the side bars
  are shaded black at 77% (each copy over its own box, so images filling
  holes are shaded too), and so are the background's outer 3 columns inside
  the 4:3 area, where undimmed rock showed as a line beside the panels'
  metal. Bars reach the window edges at any width (checked at 21:9).
- An online play study (lockstep input; cheats off and the host's options
  for everyone; about 3-4 months).
- Sandbox only: the software Vulkan driver (llvmpipe) sometimes crashes on
  its own thread at the title; not a game bug.

## 2026-10-12, round 61: Custom Music

A user's request: their own music files in place of the game's songs.
Enhancements, Custom Music (and its volume): .mp3, .flac, .wav or .ogg files
in the Music folder beside the executable (made at startup) replace each
song the game starts, picked at random from a shuffled list, the next one
following when a file ends (src/main/custom_music.cpp; decoders in
third_party/audio_decoders, public domain).

The game plays its songs on one music object (0x80107DE4: sequence player
+0x14, requested song +0x28, state +0x18: 0x40 start, 0x80 loaded, 0x10
playing, 0x20 fading out, 0x100 song not found). Song 0 is "no song": its
lookup fails and is retried, so the hook is on the loaded path
(0x0025D588), not on the request. n_alSeqpSetVol on that player is hooked:
the player gets 0, and the volume asked for, as a share of the one at the
song's start, scales the file (fades, pause ducking). n_alSeqpStop on it
fades the file out. Files are read on a thread of their own; the audio
thread decodes 1024-frame chunks, resamples linearly to the game's rate and
mixes into the queued buffers (src/main/main.cpp), only those actually
queued.

Tested with SDL's disk audio driver and a tone per format (MP3 44.1 kHz,
FLAC 48 kHz mono, WAV 22.05 kHz, OGG 44.1 kHz): each plays at its pitch,
one follows another, the menu song (52) and a match's (58) each get a new
file, and with the option on at volume 0 the output is silent (the game's
song is muted). The sandbox runs slower than real time, so its output has
gaps; within them the tones are exact.

## 2026-10-12, round 60: the launcher's jungle background

The launcher shows a dark jungle at dusk in the menus' orange and brown
(src/main/launcher_art.cpp): a back layer (sky glow, misty ridges,
pterosaurs) and a front layer of black palms, ferns, vines and a small
raptor in the undergrowth, each two copies wide and moved left at its own
speed (one image width in 420 s and 150 s), with a still shading overlay
that darkens the edges and the middle behind the menu. The images are drawn
by tools/make_launcher_art.py (seeded, tileable; icons/launcher_*) and
built into the executable with file_to_c. RT64's loader only takes PNG
files, so the back layer (a JPEG, 140 KB against 1.9 MB as PNG) is decoded
with stb_image and handed over as RGBA; image names start with "?/" so
RmlUi doesn't treat them as paths.

## 2026-10-12, round 59: medals in bot matches

Medals are handed out at the end of a match by `func_004245C0`, which first
asks `func_00426454`: it needs a medal-capable match (bytes `0x8014022C` /
`0x8014022D`) and two or more human slots (`0x801402D8`, 0x96 apart: `+0x78`
in the match, `+0x91` 1 for a bot) whose per-port count at `0x800FD140`
(raised in `func_0021A9A4`, reset each match) is at
least 76, so an idle controller doesn't count. Enhancements, Medals in Bot
Matches: one such player is enough (hook at `0x004264D0`). Each medal's
own task still applies. Checked with gdb on a bot match ended early: the
check returns 0 with the option off or the count under 76, 1 with the option
on and the count at 100.

## 2026-10-12, round 58: more cheats

Cheats tab: Infinite Health, Moon Jump (hold L), Rapid Fire (hold Z),
Infinite Time, from the community GameShark codes turned into offsets from
the player pointer (docs/RAM_NOTES.md; the codes' player is at 0x80335F40,
found from the ammo bytes). src/game/extras.cpp applies them each frame in a
match to the players on a controller port (the buttons come from the input
code, trw::controls::buttons_held); Infinite Time holds the match clock
(0x801407F4). Tested: health held at 100, the clock stays, L lifts the
player. Unlimited ammo is the game's own Unlimited Ammo cheat.

## 2026-10-12, round 57: screen filters

Graphics tab (first Enhancements), Screen Filter: None, CRT, Scanlines, LCD. In RT64's VI
pass (lib-patches/rt64/0005, self-contained for other recomps): the push
constants gain the filter and the frame's size in N64 pixels (480 lines
count as 240), `RT64::setScreenFilter` (render/rt64_screen_filter.h) sets it
from any thread; src/game/extras.cpp sets it each frame from the option.

- CRT: Timothy Lottes' CRT shader (public domain, libretro crt-lottes
  defaults: hardScan -8, mask 0.5/1.5, shadow mask 3, bloom 0.15), without
  the warp; three scanlines weighted by distance plus a five-line glow,
  read from the full-resolution picture horizontally, mask at screen pixels.
- Scanlines: a cosine dip between the N64's lines.
- LCD: LCD3x (Gigaherz, public domain): RGB stripes and faint row gaps.
- The lines fade out when a line is under about two screen pixels tall.

cmake/ApplyLibPatches.cmake now removes files a patch adds before applying
the set again (the reset only restores tracked files).

Tested in a match with each filter. One run with CRT crashed in a host
library (not game code) right after the level started; five runs after it
didn't, including CRT under gdb into a trial level.

## 2026-10-12, round 56: Unlocks: All Skins

Skins are a byte per character id at profile `+0x57 + id`: how many skins
that character has. `func_00425BC0(slot)` raises them from the medal count
(the FAQ's 2 medals per skin). Character select reads it in three places:
the skin boxes shown (`func_004204A8`), C-Up's next skin (`func_00420214`)
and a random pick (`func_004208C0`); each is hooked to `trw_skin_count`,
which answers the full count (what 50 medals give) while All Skins is on.
Ugetab's all-skins code doesn't match the game's table (it misses Turok,
id 0, and gives Lord of the Dead, id 6, a third skin), so the table is the
game's. Tested: Turok's boxes 1-3 cycle on R (C-Up), and a match starts
with the third skin.

## 2026-10-12, round 55: one Cheats menu (F2); the trials map with All Levels

- The game's Cheats menu (Z on the main menu, state 0x1C) is gone: the main
  menu's event-0xA handler (`func_004244B4`) opened it for Z (0xA) and now
  compares with -1. (Its "closed" result 4 at `0x00424418` is shared by every
  main menu choice; patching that left the menu closed with nothing next.)
  The Credits item went with it.
- The menu's Cheats tab (was Extra Cheats) has its four settings: All
  Characters (moved from Unlocks), All Weapons, Unlimited Ammo, Extra Modes
  (Time Trial and Frag Fest on the main menu, read when it's built). They
  set the bits of `0x80140208` each frame. All Medal Rewards is dropped (it
  only counted medals for that menu) with its two hooks.
- F2 opens the menu on the Cheats tab (and closes it):
  lib-patches/RecompFrontend/0004 adds `config::set_hotkey_tab`.
- The trials map (`*0x800DF2F0`) copies the level bits into its own
  (`+0xA74`, `func_004271E8`) and lights nodes from them when it opens; with
  All Levels every node was lit and the cursor (the node's frame pulsing)
  was lost. The build now reads the save's bits; afterwards the map's bits
  answer "open", so the cursor moves onto any level, lighting it, and starts it.
  Readers seen: the build (`func_00427690`, `func_00427F50`), the cursor
  start (`func_004280F0`), moves (`func_00428A30`, `func_00428CF8` from
  `func_00429058`). Cathedral (not opened in the save) starts.

## 2026-10-12, round 54: mouse look and the pause keys in some trial levels

The user: no mouse look in cheat-opened trial levels (the Warehouse), and the
pause list didn't take the arrow keys. The input code took the word at
`0x80140780` being 0 as the attract demo (title keys only, no mouse look);
it's 0 in the Warehouse too. The demo is now a match started from the title
screen (src/main/fps_controls.cpp). Mouse look and the pause list work there.

## 2026-10-11, round 53: Unlocks: All Levels

From Ugetab's unlock codes (user, GameHacking; docs/RAM_NOTES.md): the main
menu's extra modes and the levels of 1 Player Trials, Time Trial and Frag
Fest are bits at profile `+0x7C`-`+0x93`, read through the bit test
`func_002655F0`. All Levels hooks it to answer "set" for those bytes of any
profile (src/game/extras.cpp; the option is cached once a frame since the
bit test runs often). Medal bits are left alone (award code tends to test
before setting). Tested: Turok's trial tree opens to the boss; with a profile
loaded the main menu shows Player Stats, Time Trial and Frag Fest. All Skins
was tried the same way (`+0x58`) but character select doesn't read those
bytes, so it isn't offered yet.

## 2026-10-11, round 52: the co-op Frag Tag fix (from 1.1); title

**Co-op Frag Tag trials can be won** (the bug 1.1 fixed). Trials run Frag
Tag with a frag limit of 0: the match ends on the tag limit
(`func_002281CC`), which records the win (`0x80140888`). The frag-limit check
(`func_0022804C`) is run again at the end (`func_0042D240`); in 1.0 it took
every player as having reached a limit of 0, and when the first player in
the list was a bot it overwrote the result with a failure. Every other mode
has a real frag limit, so only Frag Tag was hit. 1.1 returns "not reached"
when the limit is 0 (its only logic change in that function); the hook at
the top of `func_0022804C` does the same. The match setup, mission data
(file at ROM `0x79CC78`, identical in 1.1), tag check, time check and results
screen were all compared with 1.1 and are the same. Trial types
(`0x80140215`): 0 Multiplayer, 1 one-player trial, 2 co-op trial, 3 Time
Trial, 4 Frag Fest. Tested: Multiplayer Frag Tag still ends on its tag limit
and a one-player trial starts; the co-op trial itself needs two players.

**Title:** the launcher and crash dialog read "PaxDino: Rage Wars Recomp";
the Steam entry keeps Turok in its name (user: easier to find), which also
keeps its app ID.

## 2026-10-11, round 51: Frag Tag crashed on start

Starting a Frag Tag match crashed: "Failed to find function at 0x00204030"
(called through a pointer). spimdisasm had started that function one word
early, on the padding word before it (`sll $at, $zero, 0` at `0x0020402C`;
the only function in the list starting on padding). The start is now
`func_00204030` (syms/spimdisasm_starts.txt, functions.txt and the syms toml
regenerated). Frag Tag loads into its level. Needs -Regenerate.

## 2026-10-11, round 50: Frag Tag (co-op) result, traced so far

- Match state at `0x801407E0` (`0x80140208` + `0x5D8`): `+0x18` score limit,
  `+0x24` team mode, `+0x2C` team scores (5), `+0xA8` = `0x80140888` trial
  won. `0x80140215` is the trial type (1: one-player trial).
- `func_0022804C` (score limit): when a player or team reaches the limit the
  match ends; in a trial it sets won if it's the human team (team mode) or a
  human player. 1.1 only adds a zero-limit check to the free-for-all path.
- `func_00228394` (time limit): at time-out, trial type 3 fails outright;
  otherwise won keeps its value (0 from the match start, `func_0042E080`).
  Identical in 1.1.
- Results screen `func_00437C30` (calls the trial-cleared handler
  `func_004251F4`): identical in 1.1 apart from addresses.
- 1.1's `func_0022A2FC` / `func_0022A36C` change only gates the HUD drawing
  on a new global (`0x800D0EBC`).
- So the judging is the same in both; the likely fault is that co-op Frag
  Tag's tags never reach the team score (`+0x2C`), so the team can't reach
  the limit and the match ends on time as a failure. Next: watch the team
  scores in a Multiplayer Frag Tag match with bots, find the tag scoring,
  compare it with 1.1.
- Trials: cleared trials are bits at profile `+0x125 + character * 5`
  (bit = trial number at `0x8014BD9C`); the trial tree doesn't read them
  when it opens, so All Trials isn't found yet.

## 2026-10-11, round 49: Add to Steam; Clear Menu Panels

- **Add to Steam** (launcher, src/main/steam_shortcut.cpp): adds this copy to
  every Steam account on the computer as a non-Steam game, in
  `userdata/<account>/config/shortcuts.vdf` (binary VDF, read and rewritten
  keeping every other entry; the old file is kept as
  `shortcuts.vdf.paxdino-backup`). App ID = CRC-32 of Exe + AppName with the
  top bit set, as Steam makes it. Steam must be closed (it rewrites the file
  when it exits): if it's running, the option offers to close it
  (`steam://exit`) and waits on a thread. Run again, it updates its entry.
  Tested on a fake Steam folder in the sandbox (existing entry kept, no
  duplicate, the Steam-running prompt). Written from Steam's file format, not
  from another project's code. User: works with real Steam.
  Artwork (user: none showed in Big Picture): two plain SteamGridDB images the
  user picked (no console branding), downloaded only when the option runs and
  written into the player's `config/grid/` (nothing is in the project): the
  portrait capsule (600x900) from the square cover, the wide capsule
  (920x430) and the hero (1920x620) from the wide one, each the picture whole
  over a blurred, darkened fill.
- **Clear Menu Panels** (Enhancements): the panels' scanlines are 8-pixel
  strips of image 0xBB; the image hook in func_0040DB84 skips them.

## 2026-10-11, round 48: Unlocks tab; Monkey Tag (Frag Tag) research

User: add Enhancements, Unlocks and Extra Cheats tabs to the F1 menu. The
Unlocks tab is in (src/main/extras_config.cpp, src/game/extras.cpp); the
other two are added once they have options. Unlocks never write the save:
- **All Medal Rewards:** the medal count (`func_004261E4`) answers 50, and
  the Cheats menu's debug path (40 medals) is taken, so all its items are
  enabled even with no profile loaded.
- **All Characters:** sets the game's own All Characters cheat bit
  (`0x80140208`, `0x08000000`) each frame while on, and clears it when turned
  off if it was set by the option. Tested: the whole roster shows.
Profile layout and cheat bits in docs/RAM_NOTES.md.

Frag Tag (the user calls it Monkey Tag): the user sent a 1.1 dump and an
article (co-op trials, Frag Tag always "mission failed", so the co-op medal
can't be earned). Re-compared with immediates kept: no trial logic among the
changed functions, so the fix is still unidentified. Plan: unlock the co-op
trials (Unlocks), reach the Frag Tag trial and trace the result check at
runtime, then compare that code with 1.1.

## 2026-10-11, round 47: credits jumping (an N64Recomp jump-table bug)

User: the credits (cheats menu) jumped around as they scrolled, at any
refresh rate. The scroll (func_00421844) moves the list up a pixel at a
time and, every line (15 pixels), puts it back and rewrites all ten lines
(func_004215D8: for each line slot, a jump table picks the widget, 0x3B8
plus the slot). N64Recomp switched on the addend saved at the table's addu,
which here runs once before the loop (the loop steps the entry's address),
so every pass wrote the first line and the others never moved on.

Fix (lib-patches/N64Recomp/0002): the switch takes the entry from the jump
register itself (the table load is turned into the entry's address), as the
live recompiler already does. Four functions in the game had a table
indexed in a loop (func_004215D8, func_004337EC, func_0043BA34,
func_0029758C); all 152 of the game's tables now use the register.
Needs -Regenerate. The menu no-interpolation change from earlier in this
round is reverted: it wasn't the cause.

## 2026-10-11, round 46: the level-load hang, found

A hang caught in the sandbox (no mashing) showed the real cause. The main
loop (`func_00291334`) waits on `0x801408C0` for its frame's graphics task,
which the scheduler thread (`func_0028E880`) finishes. The scheduler was
stuck in the controller code instead: `func_00263A94` (each retrace)
collects the last controller operation (`func_002636E0`, which waits for
its SI message on `0x80109C00` when the state byte at `0x80109BB8` is 0 or
1) and starts the next (state 0: read, or query and state 1).

Every 60 frames (counter `0x800CBB24`, while `0x800CD944` is 1) the main
thread (`func_002A1FD0`, `0x002A2108`) sets the state to 1 without taking
the lock (`0x80109BC0`). That's only right while a read is in flight. The
pak and rumble setup when a level loads collects the read itself and leaves
the state at 2 (nothing in flight) until the next retrace. If the 60-frame
write lands in that window, the scheduler waits for a query that was never
started. The hang dump had the state at 1, the flag at 1 and the counter
just reset to 60. On the N64 the window is a sliver; here thread timing
makes it wide enough to hit.

Fix (turokrw.us.toml hook): the write only goes through over state 0; the
query comes back 60 frames later. Round 43's change (no player writes
while a level loads) stays, as it is still right.

## 2026-10-11, round 45: Escape on the options from the pause list

Those options leave only through their Resume Game item. Escape now moves
the cursor there (short stick pushes) and presses A. The menu hook finds
the item each frame (`trw_pause_resume_direction`): the text widget with
string 781, two levels under an item of the list (list flag `0x8` while it
shows, `0x40` on the highlighted item, y at `+0x16`). In a sub-screen (sound
options, the quit question) the list is hidden, and Escape is B as before.
Tested from the top item, the bottom item and a sub-screen.

The level-load hang came back once in the sandbox (no mashing, same
backtrace: every game thread in `osRecvMesg`, the main loop in
`func_00291334`), so round 43 didn't fully fix it.

## 2026-10-11, round 44: PaxDino

The project is now "PaxDino: Turok Rage Wars Recomp". The CMake project,
the game target and the exe are `PaxDino` (`build\PaxDino.exe`). The config
folder keeps the `trwrecomp` id, so saved settings and controls carry over.

## 2026-10-11, round 43: rare hang loading a level (likely cause)

User: the hang right before the fly-through again (black this time), maybe
when giving input as the match starts. Mouse look writes the view angles
straight into the player through the game's player pointer (0x8013EF84),
and the weapon-wheel hold writes the wheel there too. While a level loads
that pointer can still name the last level's player, whose memory is
being reused; and the "HUD drawn" flag kept its value from the last match,
so the controls took the load for play. Now the HUD flag is cleared
outside matches and starts clear, and nothing is written to the player
unless a match is live (its HUD drawn). Compiled; a reproduction loop
(trial starts with keys and clicks mashed during the load) is running.

## 2026-10-11, round 42: skipping the level fly-through

The fly-through at the start of a level is told by no HUD drawn in the
last match frame (src/game/widescreen.cpp). There Escape, Enter, Space and
a left click send Start, which skips it; the press ends when the level
starts, so it doesn't go on to pause. All four tested.

## 2026-10-11, round 41: Enter in a match; clicks on the boot screens

- Enter no longer pauses in a match (Escape does); in the menus and the
  pause list it selects. The wheel's hold past the pause now follows A as
  the game gets it, the keys' own presses included (Enter's A on Resume
  still opened the wheel for the user).
- A left click (and Space) skips the boot screens like a button.

## 2026-10-11, round 40: Enter on the pause list

User: Enter on the pause list opened the weapon wheel, let the match run a
moment, and selected nothing. Enter's short press (four controller reads)
picks its button per read by screen: A selected Resume, the match
unpaused, and the rest of the press went on as Start (Enter in a match),
pausing again; the leftover A opened the wheel. A key press now ends as
soon as the screen it was meant for changes (KeyPulse::step). Enter on
Resume resumes, on Options opens them.

## 2026-10-11, round 39: WASD in the menus

In the menus (and the pause list) WASD navigate like the arrow keys (the
stick) instead of being the C buttons; R is C-up there (the default
loadout, skins). W/S tested on the main menu; R not reached in testing.

## 2026-10-11, round 38: weapons by keyboard and mouse

The game's weapon wheel (held A) has 8 slots picked by the stick's
direction (func_00217B3C: 0 up, then clockwise); the weapon switches as
soon as the stick points at a slot. Opening it fills the slots
(func_002181FC; player + 0x938 + slot * 0x18: weapon at +0x1C, usable at
+0x24) and marks the current one (+0x37C). A tap of A only flashes the
wheel; there's no cycling. B is the weapon's second function ("function
not available" for weapons without one).

- E (A) opens the wheel as before; while it's open the mouse moves a
  virtual stick instead of the view, so the weapon under the pointer is
  picked.
- Q and the mouse wheel step to the next / previous usable slot; 1-8 pick a
  slot. Both open the wheel for a moment, point at the slot and let go
  (WeaponPick in src/main/fps_controls.cpp).
- R is B (the second function).
- The D-pad (taunts) moved from 1-4 to Z, X, C, V in the defaults; saved
  controls with Q on B or 1-4 on the D-pad have those dropped while the
  keys are held.

## 2026-10-11, round 37: weapon wheel under the pause menu

User: Enter on the pause list brought the weapon wheel up behind it. The
pause list selects with A (Start resumes instead), and the game keeps
reading A as "next weapon" while paused (likely the same with a gamepad's
A). The wheel's state (player + 0x938, 0x380 bytes) is now held as it was
when the pause started: restored at each controller read and again just
before the HUD draws (src/main/fps_controls.cpp). No wheel under the pause
menu, the weapon unchanged after resuming, the wheel normal in play.
User: E on Resume still opened the wheel as play resumed (the key still
down). A held when the pause ends is now ignored until it's released (every
port, gamepads too). It still flashed for a frame: the game resumes and
cycles the weapon in the same frame, so the wheel is also held on past the
pause until that A is released. No wheel in frame captures of three resumes.

## 2026-10-11, round 36: modern keyboard and mouse controls

- The overlay (RecompFrontend) menu moved from Escape to F1 and F10, which
  also close it (lib-patches/RecompFrontend/0003).
- Escape and Enter by screen (src/main/fps_controls.cpp): in a match Escape
  opens the pause list and leaves it (Start), Enter pauses too; in the menus
  Escape goes back (B), Enter and E select (A); the title and the attract
  demo take Enter, Escape, Space or a left click (Start). Presses only (a
  key closing the overlay menu or held across it doesn't reach the game).
- The title is told from the other front-end menus by its widget tree (a
  full-screen image and one text, src/game/widescreen.cpp); the demo by
  0x80140780 (1 in every real match, 0 in the demo).
- Gamepads unchanged.
- Open: in the options opened from the pause list neither B nor Start
  leaves; only their Resume Game item does. Escape sends B there (for the
  sub-screens).

## 2026-10-11, round 35: the dark kill name

From the user's F8 dumps: the HUD's texts (func_002A8820, two-cycle) mix the
prim and env colours by a second texture (TEX1, a 16x16 pattern at
0x801450C0) whose tile 1 is set with a line width of 0. RT64 counts such a
tile as invalid and samples nothing defined for it. The kill name passes its
colour as prim (white, or red for yourself) with env (1,1,1), so it came out
at the dark env colour, translucent, with the black shadow showing over it.
The game's other faded text sets prim and env to the same grey (the mix
doesn't matter, and the grey runs from white to black). The name now gets
the same: a hook after its colour call (0x0021FEB8) writes its colour scaled
by its fade (+0x12F4) into both. User: fixed. The real fix would be RT64 sampling line-0 tiles as the RDP
does (rows at stride 0), if other texts turn up the same way.

## 2026-10-11, round 34: F8 frame dump

For rendering bugs that don't reproduce here (the dark kill name): F8 saves
the N64 memory at the next frame's start to frame_dump_<n>.bin (in the
working directory, like crash_log.txt), with frame_dump_<n>.txt giving where
the finished frame's display list starts, the game mode, the video mode
width and whether widescreen is on. tools/dlwalk.py-style walking of that
list shows what RT64 was given.

Kill name: dark in 4:3 at Original refresh rate too (user), so neither the
widescreen code nor the interpolation; the user sees the dark pass on top of
the white one.

## 2026-10-11, open: rare hang starting a trial; faded kill name

- User: once, starting a trial (4:3), the red screen wipe stayed up and
  the match never started (no crash log). Not reproduced here or by the
  user since, in many combinations. A loading-time hang (mode 20 into the
  match); to watch for.
- (round 35) The name shown by the skulls after a kill is drawn black and
  translucent from its first frame (user video). The HUD routine draws it
  twice (func_002A8E58): a shadow at +2,+2, then the name in white (red if
  the entry is the player) with the same fade (player +0x12F4); only the
  dark pass shows. Waiting on the user: white in 4:3? white at Original
  refresh rate?

## 2026-10-11, round 33: the mouse in the pause menu

User: couldn't select Options from the in-match pause menu. Keyboard
selection works here (the options take a few seconds of black to load), but
in a match the mouse kept its match controls while paused: left click fired
(Z), the wheel cycled weapons (A/B), so clicking an item did nothing. The
paused flag is 0x801407D4 (found by diffing RDRAM paused and running); while
it's set the mouse has the menus' controls: the wheel moves the selection,
left click selects (A), right click goes back (B).

## 2026-10-11, round 32: the weapon wheel (for real); options from the pause menu

- **Weapon wheel.** Round 30 had the wrong cause: the wheel isn't drawn
  after the HUD but inside it (func_002183E8, called from the HUD routine
  at 0x0021EFBC, found by marking every call in it). The HUD's automatic
  anchoring (round 11) sends rectangles in the top quarter to the screen's
  sides; as the wheel turns, its tiles crossing that line went to the sides
  while the rest stayed centred, so the icons came apart (the user's
  screenshot). Anchoring is now off around that call. Round 30's
  no-interpolation group after the HUD is removed.
- **Options opened from the pause menu.** In a match the game draws them
  like the front end's menus (284-pixel video mode, the trial select's
  frame), but the menu fixes were off in matches, so RT64 stretched some of
  it. A match frame in the menus' video mode now gets the menu handling
  (and the menu's frame clear). The in-match pause list itself (drawn over
  the match) is unchanged.

## 2026-10-11, round 31: menus put out of place again; trial select seam

- **Menus shifted / rock over the trial select (round 28 regression).** The
  frame clear's scissor was shortened by a pixel for matches (round 28), and
  the menus share it; RT64 judges the menus' passes against the whole
  framebuffer, and with the short scissor the character and trial select
  pictures came out of place (rock drawn over the trial select's panel). The
  menus' clear uses the full framebuffer scissor again.
- **A seam down the middle of the trial select.** The left part of the
  centre panel, up to the screen's centre line, was lighter than in 4:3 (the
  trial path icons hidden under it). It came from the menus' centred
  viewport (round 12), which the later RT64 framebuffer-aspect patch makes
  unnecessary: without it the trial select matches 4:3, and the main menu,
  Controller Pak, character select, weapon select (its 3D model), options and
  how-many-players screens all look right. Removed.
- The black bands above and below the menus are the game's own (the N64
  shows its 284x222 menus inside a border; the same in 4:3).
- The main menu shown wrong after backing out of the trial select (user, not
  every time) didn't reproduce here in three tries with this build.

## 2026-10-11, round 30: weapon wheel came apart (frame interpolation)

User: the weapon on fast turns is right now (round 29). The weapon wheel
showed its icons cut into pieces. The wheel is drawn after the HUD as 48x48
tiles of each icon and frame that slide as it turns; at Display frame rate
RT64 paired those tiles with the wrong ones of the previous frame and slid
their textures. With one view, everything drawn after the HUD is now in a
non-interpolated matrix group (popped at the next HUD or frame). Not
reproduced here; to be confirmed by the user. Split screen left as is (the
next view's world follows its HUD).

## 2026-10-11, round 29: the weapon drawn relative to the camera

User: the weapon still shook on fast turns at Display frame rate. The game
draws it as a model placed in the world just in front of the camera; the
camera is in the view's projection. RT64 interpolates the two separately
(the weapon's matrices on their own, the camera with the projection), so
between frames of a fast turn the weapon came loose from the view.

Now the weapon gets its own projection: the view's split into the camera
and a plain perspective (w = z). After the weapon is drawn, its commands are
walked; the matrices it loads are copied with the camera multiplied in (the
palette segment pointed at the copy, set back after), its light and look-at
directions turned the same way, and the perspective loaded before it. Each
real frame looks the same; RT64 now only interpolates the weapon's sway
relative to the view. If the commands aren't the expected kind (a matrix in
a static list, geometry before any matrix), the weapon is left as the game
drew it.

## 2026-10-11, round 28: smoothing for everything; match clear kept the 3D at 4:3

- **Interpolation base.** Each match frame now starts by setting the base
  of RT64's matrix-group stack to the decomposed components (position,
  rotation, scale always interpolated) with automatic matching. RT64's
  default stops interpolating an object's position on any frame where its
  movement jumps tenfold, so anything moving unevenly flipped between
  smoothed and snapped frames. User: the weapon's periodic jitter is better;
  it still shakes on fast camera turns.
- **Black sides in matches (round 27 regression).** The match frame clear
  set a scissor of the whole framebuffer (480x360), a pixel wider and taller
  than the game's own, so RT64 no longer counted the 3D as full width and
  kept it at 4:3 (same cause as round 26). The clear's scissor now ends a
  pixel short like the game's; the demo is widened again with its bottom rows
  still clean.

## 2026-10-11, round 27: weapon matrix group; split-screen bottom strip

- **Weapon jitter.** The first-person weapon is now wrapped in an RT64
  decomposed matrix group with a fixed ID per view (pushed at the weapon's
  depth clear, popped when the HUD starts). RT64 matches matrices across
  frames by ID instead of by order, and interpolates position, rotation
  and scale separately, so the weapon follows the camera's turn rather
  than a straight line between frames. Needs testing at Display frame rate.
- **Split-screen bottom strip.** In split screen the views end a few rows
  above the framebuffer's bottom, and those rows were never drawn, so the
  widened image kept stale HUD pieces there. Each match frame now starts
  by clearing the whole colour buffer to black (Expand only), like the
  menus already did. The demo's bottom rows are black now.

## 2026-10-11, round 26: the weapon clipped at the 4:3 edge

User video: in Expand the weapon's far end was cut by a vertical line at
the right edge of the 4:3 area. Traced RT64's projection passes in a
match: the weapon pass (6 calls, clip ratio 1) wasn't widened. RT64 widens
a pass's 3D only where it spans the framebuffer pair's scissor, the union
of every scissor drawn with; the HUD hook's scissor (round 11) reached the
exact right edge (480) while the game's own stops at 479, so the weapon's
0-479 no longer counted as full width and RT64 clipped it to 4:3. The HUD
scissor now ends where the game's does (a pixel short right and bottom).

The weapon jitter at Display frame rate is still open. The game's
projection matrix includes the camera, which RT64 has to split back into
view and projection to interpolate, and the weapon is placed in the world
just in front of the camera, so the two are interpolated separately (the
weapon's position along a straight line, the camera's turn along an arc)
and drift apart when turning. Can't be measured on this machine.

## 2026-10-10, round 25: weapon change reverted; frame-start detection

- Round 24 made the weapon jitter much worse on the user's PC; reverted.
  The weapon's matrices aren't the simple camera-fixed case assumed there;
  needs a proper look at how its bone matrices and the projection are
  built before tagging anything (frame timing work, PROGRESS step 4).
- The user saw the grey strips again on the main menu after entering and
  leaving 1 Player Trials (with a saved profile; not reproduced here). The
  start-of-frame clear only ran when `func_00291B00`'s call came within
  0x200 of the display list start. A call is now the frame's first when
  the display list is in the other buffer than at the previous call, or
  has started over in the same one.

## 2026-10-10, round 24: the weapon isn't interpolated

The weapon model jitters with Framerate at Display (RT64 interpolating to
the monitor's rate), worse since mouse look moves the camera in bigger
steps. In a match each view draws its weapon after a second depth clear
(`func_00291B00`, ~0xB28 into the display list; the first is at 0xD8),
as a skinned model with bone matrices in segment 1, then the HUD. RT64
interpolates each matrix on its own, so the weapon swam against the
interpolated camera. At that second clear the weapon's model matrices and
the view's projection (reloaded inside the group) now go in groups RT64
doesn't interpolate (`gEXMatrixGroupNoInterpolate`), popped before the HUD:
the weapon is drawn as on the last real frame, where it sits fixed on
screen. Checked: weapon and HUD render in a match at 120 Hz; smoothness
needs the user's PC.

## 2026-10-10, round 23: the allocator crash was ours (HUD scissor)

The intermittent crash in the game's allocator (`func_002559D4`, first seen
in round 11) came back with registers in the log: it wrote through the
address-order link of the heap block at 0x805FFFE0, which held 0x692587E3.
Match framebuffers are 480x360 at 0x80500000, 0x80554600 and 0x805A8C00
(0x54600 bytes each); the last ends at 0x805FD200, and the heap block's
header sits just after. The HUD hook (round 11) widened the scissor to 480
rows; RT64 writes back as many rows as were drawn to, so rows 360-479 of the
last framebuffer landed on the heap. A dump mid-match showed the header as
pixel data (6805C59B 6015CD5B ...); with the scissor limited to the view's
height (+0x2A0 of the first view, 360) it is a valid header
(804FFFE0 8069F660 ...).

## 2026-10-10, round 22: test switches removed

The menu fixes are confirmed on the user's PC, so the `TRW_MENU_MODE`
switches used to narrow down the flicker (rounds 14-20) are gone.

## 2026-10-10, round 21: the side bars fade with the screen

The game's screen fade (`func_00293008`, from `func_00292220` with an RGBA
colour at sp+0x18) covers the 4:3 area, so the stretched background in the
bars appeared before the title screen faded in. Stretching the game's own
fade doubles the picture on the boot screens, so in the front end the bars
get a translucent rectangle of the same colour instead, under scissors
anchored to the bars and limited to the framebuffer's rows (taller scissors
change RT64's judgement of the pass and double the picture too).

## 2026-10-10, round 20: no black block in the left bar

The user confirmed the flicker gone (the earlier "still flickering" runs had
a test switch left set). Left: the stretched background's hole (the level
preview's on the trial select) showed as a black block in the left bar.
The background's own draw is now clipped away, and each bar gets two copies
shifted sideways by a third of the width, then the image, then any hole
patch drawn before it (which those copies would otherwise cover). An
all-zero scissor can't be used for that clip: RT64 draws through it.

## 2026-10-10, round 19: the frame clear missed frames

User video: the same grey strips (the areas the start-of-frame clear
covers) still showed on some frames. The clear ran on the first
`func_00291B00` call after the display list buffer changed, which assumed
the two buffers strictly alternate; on the user's faster PC they evidently
don't always. It now runs on any call that comes at the start of a display
list (offset 0xD8; the second call per frame is at 0xA10 or later).

## 2026-10-10, round 18: full-width fills held at 4:3

User video: grey strips still blinked at the top and bottom, across the
bars. The trial select draws full-screen box widgets as fill rectangles;
RT64 stretches any fill spanning the scissor width to the whole window
(it becomes a clear), ignoring `gEXSetRectAspect(ADJUST)`. It now honours
ADJUST for fills too (lib-patches/rt64/0004). Checked with the user's
settings: top and bottom rows unchanged over 30 frames.

## 2026-10-10, round 17: skippable boot screens

Boot modes (`0x801407B8`): 0 Acclaim logo (~7 s), 1 expansion pak screen
(~8 s), 3 (~5 s), 4 title. Screens move on through `func_002937F0`, which
already switches mode when buttons were pressed (`0x80109194`). Mode 3
skipped at once; mode 1 (`func_00293E30`) holds a flag (`0x800CD68C`) that
blocks the skip until its timer passes 45. A press now clears it
(src/main/fps_controls.cpp). Mode 0 is left alone: its timer stays frozen
for most of it while the game loads (until `0x80144CE0` is set). Pressing
through: main menu at ~8 s instead of ~27 s.

## 2026-10-10, round 16: the trial select flicker, reproduced

The user's settings (2x, HUD ratio Original) reproduced it headless: a grey
band at the top of the trial select on some frames, never in Original. The
game redraws identical display lists every frame into three rotating colour
buffers (0x80500000, 0x80554600, 0x805A8C00) and never clears them; a strip
at the top of that screen is drawn by nothing, so it showed whatever each
buffer last held, which in Expand differed between the three. The colour
buffer is now cleared to black at the start of each front-end frame in
Expand, after the game's depth clear sets it (end of `func_00291B00`, first
call per display list buffer), before the level preview pass. This replaces
the bar-only clear of round 15. Checked: trial select steady over 30 frames
outside the animated portrait and preview; preview intact.

## 2026-10-10, round 15: the menu flicker was stale pixels in the bars

The user's video (trial select, Expand) showed the side bars alternating
grey and black frame to frame. The menus never clear the colour buffer: the
display list sets a black fill colour but draws no fill, relying on the
full-screen background (the depth buffer, at 0x80600000, is cleared with a
`FFFC` fill). With the menus held at 4:3, parts of the bars (above and below
the background, and its holes) are drawn by nothing and keep whatever each of
the two framebuffers last held. The user's switch runs fit: everything except
the framebuffer-shape flag left part of the window undrawn.

Fix: at the start of each menu frame (once per display list buffer) the two
bars are painted black: a scissor anchored LEFT..CENTER (CENTER..RIGHT), then
a stretched one-cycle rectangle in primitive black over that half of the
framebuffer. Fill mode doesn't work: RT64 turns a fill rectangle into a
clear, which ignores the scissor and wiped the level preview drawn just
before the menu.

Second report: the user's graphics config has the HUD ratio at Original,
which makes RT64 put every LEFT/RIGHT origin at the 4:3 edges, so the bars'
anchored scissors (the stretched background copies and the clear) were
empty and nothing was drawn in the bars at all. The flicker was the trial
select's blinking panel highlight and preview pass changing those undrawn
pixels. RT64 now resolves anchored scissors against the window's own edges
whatever the HUD ratio (lib-patches/rt64/0004); rectangles still follow it.
Checked with the user's settings (Original2x, HUD ratio Original): bars
steady over 30 frames on the trial select. Left: a black block in the left
bar on that screen, where the stretched background's preview hole lands.

Third report (video): still a blink, now in the left bar beside the panel.
The trial select's blinking panel highlight is itself a full-screen image,
so it also got stretched copies in the bars. Only the first full-screen
image drawn in a menu (its background) gets them now. Also: dark lines at
the 4:3 edges (title screen): RT64 puts the 4:3 image's edges a few pixels
off the centre-anchored ones, so the bars now reach 3 pixels into the 4:3
area, under the image. Checked with the user's settings: bars unchanged
over 30 frames on the trial select, no dark columns at the edges.

## 2026-10-10, round 14: crash reports; switches for the menu flicker

- **Crash while firing** (user report): `func_0022A2FC`, called from
  `func_002362C8`, walks a list of player objects (next at +0x16E0) and read
  through a bad pointer. Not reproduced in 60 shots. The crash handler never
  learned where N64 memory starts (`trw_crash_note_rdram` was never called),
  so every report said "outside N64 memory"; it's now called at boot
  (src/game/tlb_views.cpp). Reports also list the player chain from
  0x8013EF84 and save N64 memory to `crash_rdram.bin`, so the next one shows
  what the bad pointer was and where it came from.
- **Character select flashing white** (user report, Expand, D3D12): not
  reproduced headless. `TRW_MENU_MODE` (environment, comma-separated) turns
  parts of the menu widescreen work off to narrow it down: `off`, `nobars`,
  `noflag`, `nopoint`, `noadjust`, `noviewport`. The stretched background
  copies are now drawn only inside a menu. User test: it flickers with every
  switch but `off`, which leaves rectangles held at 4:3 (`noadjust`) and the
  centred viewports (`noviewport`, RT64 re-applying the last loaded viewport)
  as the suspects.
- **Crash in the demo**: `func_0028FCC4` (an offset-table lookup) read
  0xD4CF7CFC for `func_0025F50C`, which walks an asset's nested offset
  tables: a garbage offset, in the same resource code as the earliest
  intermittent crashes (round 11 and before). The crash handler now also
  looks through the host registers for the faulting thread's
  recomp_context (r0 zero, sp in KSEG0) and prints the MIPS registers.

## 2026-10-10, round 13: line at the bottom in matches

- **Flickering line at the bottom of the screen in matches (Expand).**
  Round 11 skipped the view border (`func_002372B4`) in Expand to lose its
  side lines at the old 4:3 edges, but the border's bottom bar also covers
  the last row, which the 3D view doesn't draw; with it gone that row showed
  stale framebuffer contents. The border is drawn again, and on its way out
  (`0x00237E58`) only its tall fill rectangles touching the screen's left or
  right edge are turned into no-ops.
- **Audio crackle** (reported in the main menu): the menu's own glitchy
  static effect, intentional. The queue was briefly deepened to five frames
  for it and put back to two (less latency).
- **Character select flashing** (reported in Expand): not reproduced
  headless at 60 or 120 fps; the display list peaks at 0x19D8 of its 0x17000
  bytes, so the background copies don't overflow it. Waiting on a capture.

## 2026-10-10, round 12: menus in widescreen; player 2 with mouse and keyboard

- **Menus in Expand.** The front end is a tree of widgets drawn by
  `func_0040DB84` (recursive; images by `func_0040CDD0`). Each widget has
  its parent at +0x0, next sibling at +0x4, first child at +0x8 (containers),
  type at +0xE (3 an image), position at +0x14/+0x16 and size at
  +0x18/+0x1A. The menus draw to a 284-wide framebuffer. RT64's Expand
  stretches only what happens to span the screen, so the background (a
  284x183 image with a hole) stretched while the image filling the hole
  (67x73 at 18,128) stayed 4:3: the black square. Frame pieces and text
  split apart in the same way, and the level preview drifted. Hooks on
  `func_0040DB84` (src/game/widescreen.cpp) now:
  - while a whole menu draws (a widget without a parent), keep rectangles
    at 4:3 (`gEXSetRectAspect(ADJUST)`) and viewports centred
    (`gEXSetViewportAlign(CENTER)`, which RT64 now also applies to the
    viewport already loaded);
  - draw each background image (a full-screen image, or an image inside one
    beside it) in 4:3 and a second time stretched and point-sampled,
    clipped to the bars beside the 4:3 area (anchored scissors, which RT64
    now keeps in screen space when the rectangle is stretched);
  - flag the frame (`G_EX_SETFRAMEBUFFERASPECT_V1`, new) so that RT64 judges
    a pass that only draws a preview window by its framebuffer's shape, which
    keeps the level preview in its 4:3 frame.
  RT64 changes: `lib-patches/rt64/0004-menu-widescreen.patch`. Checked: main
  menu, trials menu, player select, character select, level select, and into
  a match.
- **Lib patches re-apply as a set.** 0004 changes lines beside 0003, which
  then no longer reverse-applies, so "skip what reverse-applies" would try to
  apply 0003 again. `cmake/ApplyLibPatches.cmake` now keeps a stamp of the
  patch set in the build folder; when the set changes it resets the
  submodule's tracked files and applies the whole set in order.
- **Player 2 and the mouse.** The mouse (look and buttons) goes with the
  player the keyboard is assigned to (Controls, Assign players), or port 1
  in single player, and looks for that port's player object (+0x5D4) instead
  of always turning player 1. Other ports' controllers no longer clear the
  mouse's pending clicks.
- **HUD hooks gated.** The HUD hooks only write to the display list in a
  match and when its head is valid, as a guard while looking into a reported
  freeze after the transition into a match (not reproduced here).

## 2026-10-10, round 11: widescreen in matches (border lines, HUD anchoring)

Reached a 1 Player Trial headless (title, Start, 1 Player Trials, no
profile: NO, then OK on "unable to save", then A through character, level
and confirm), which made gameplay frames inspectable.

- **Lines at the 4:3 edges in Expand.** Each frame ends with
  `func_002372B4` drawing a thin black border (fill rectangles, 2-3
  pixels) around every player's view to hide the TV edges; RT64 keeps it at
  the old 4:3 edges. A hook at its entry returns while the aspect ratio is
  Expand (`trw_widescreen_active`, src/game/widescreen.cpp).
- **HUD anchoring.** The HUD is drawn per player by `func_0021EED8`
  (called from `func_0022A36C`, via the 2D sprite code at `0x002AAB58`), as
  texture rectangles in screen space (480x360 in matches). Around that call
  the hooks write RT64 extended commands into the display list (head at
  `0x8010A574`): widen the scissor to the window, and a new command,
  `G_EX_SETAUTOANCHOR_V1` (0x34, `lib-patches/rt64/0003-auto-anchor-hud.patch`),
  that makes RT64 anchor each rectangle without its own alignment by
  position: top quarter to the nearer side, bottom quarter left (right past
  60%), the rest (crosshair, messages) centred. Only with one player view
  filling the screen (first view's size at `0x8013F008`+0x29C/0x2A0 against
  the video mode width at `0x8014B510`), as split-screen views aren't
  widened.

Seen once in four runs, at level load: a fault in the game's allocator
(`func_002559D4`, reading a bad free-list pointer). Cause found in round 23.

## 2026-10-10, round 10: pause menu Options crash ("No threads left to run!")

With the crash handler now decoding C++ exceptions (and stderr kept in
trw_log.txt on Windows), the user's log showed `std::runtime_error: No
threads left to run!`, thrown by ultramodern's `run_next_thread` when the
running queue is empty while a thread blocks or ends. Two runtime fixes in
`lib-patches/N64ModernRuntime/0003-scheduler-fixes.patch`:

- `thread_queue_remove` only ever looked at the head of the list (it never
  advanced, and its loop variable mixed a thread pointer with a queue
  pointer), so `osSetThreadPri` on a thread that wasn't first in its queue
  could spin forever or corrupt the queue before re-inserting it. It now
  walks the list, and `osSetThreadPri` only re-inserts a thread it removed.
  The game re-prioritises its worker thread (`0x002551C8`) around jobs.
- `run_next_thread` no longer throws on an empty running queue: it waits for
  the next external event to ready a thread, as the console would idle.

Confirmed fixed by the user: the pause menu's Options screen now opens.

## 2026-10-10, round 9: direct mouse look

User report on round 8: aiming through the stick felt stiff. Mouse look
now writes the player's angles: the player object is at the pointer in
`0x8013EF84`, yaw (radians) in its character object at +0x6C, pitch at
+0x724 (head rotation offset). Also known: the frame-rate mode bytes (`0x80119B26`, `0x80119C66`, `0x80119DA6`)
and frame scale (`0x80140C3C`) for later. Each poll's mouse motion is added
once to the current angles (so respawns and the stick still work), pitch
clamped just under 90 degrees, 0.003 rad per pixel at 100% sensitivity.
Controllers still aim with the stick.

## 2026-10-10, round 8: mouse and keyboard controls (first version)

`src/main/fps_controls.cpp`, wrapping RecompFrontend's input callback:

- Keyboard defaults: WASD to the C buttons (Turok's move and strafe), arrow
  keys to the stick, Space R (jump), Left Ctrl / F Z (fire), E A, Q /
  Backspace B, Left Shift L, Enter Start, 1-4 the D-pad.
- Mouse: motion adds to the stick (scaled by RecompFrontend's Mouse
  Sensitivity, now on the General tab and 50% by default through
  `lib-patches/RecompFrontend/0002-mouse-sensitivity-default.patch`, which
  also locks the cursor while the game runs). Left click Z, right click L,
  wheel A / B (weapon cycling). Mouse buttons can't be bound in the
  controls menu (RecompFrontend's mouse input is a TODO), so they're fixed.
  Clicks are latched until the next controller read.
- Menus: the game's top-level mode (0x801407B8) is 0-4 for the boot screens
  and title, 8 for the whole front end and 20 between modes. There WASD
  navigate, left click is A, right click B, the wheel up/down, and mouse
  motion is ignored. Tested headless: WASD and the wheel move the
  selection, a click selects.
- An Invert Mouse Y option.

Mouse aim is a fake stick for now, so turn speed is capped at the stick's
maximum. Writing the view angles directly is next.

## 2026-10-10, round 7: crash after picking a character

User's crash log: access violation in host code, with `func_004288E0` on
the stack. N64Recomp had flagged it (and `func_0042144C`) with "Indirect
tail call": both walk a jump table with a moving pointer (`lw $v0, 0($sN)`,
`jr $v0`, `addiu $sN, $sN, 4` per loop) instead of indexing it, so the
generated code called a case label as a function. Both tables only map the
loop counter to a text ID (0x149 then 0x14B + n; 0x3B8 + n), so instruction
patches in `turokrw.us.toml` replace the jump with that arithmetic.
N64Recomp no longer reports any indirect tail calls.

## 2026-10-10, round 6: menu glitches are RT64's Expand mode

The black rectangle at the bottom left of the menus, and the offset copies
of the background behind the panels, both go away with the aspect ratio
set to Original. In Expand mode RT64 moves 2D rectangles to fill a wide
screen, and this game builds its menu backgrounds from many of them
(32x32 tiles, full-width strips, and a background patch redrawn at the
bottom left, (18,128)-(85,201)). Checked along the way: the patch's
palette and pixels in RDRAM are correct, nothing renders to an off-screen
target, the CPU never writes the framebuffer there, and RT64's own write
back has the right colours, so it's only the presented, expanded image.

`lib-patches/RecompFrontend/0001-default-aspect-ratio-original.patch` makes
Original the default for new configs. Widescreen comes back later with game
fixes, as in BattleTanx.

## 2026-10-10, round 5: menus; two crashes on the way to a match

Tested on Windows by the user (menu, then a crash) and reproduced headless
with keyboard input sent through xdotool.

1. **Missed function `0x0041A0E0`** (runtime exits with "Failed to find
   function", which then crashed inside RmlUi's shutdown): a leaf function
   behind a four-byte string fragment, reached only through a table. Data
   words pointing at code are only trusted after a function end, because one
   once split `func_002C1260`. The gap pass now also accepts a data-word
   target right after data, which can't split anything since it's outside
   every known function. Found it and one more (`0x0043EF20`).
2. **Deliberate crash after character select** (`sb $zero, 0($zero)` in the
   scheduler, `0x0028EBF0`): a watchdog that fires after 1200 retraces with
   no graphics task completed. RT64 was stuck: the display list ended in a
   branch to address 0, so it walked RDRAM from 0, met the same list again,
   and looped. The branch is a placeholder that `0x002A46BC` writes while it
   builds a cached sub-list, and patches before it returns, so the list was
   being written while RT64 read it. The game reuses a display list buffer
   once the RSP reports done, which on hardware means every command has been
   read; ultramodern reported done before RT64 started.
   `lib-patches/N64ModernRuntime/0002-sp-complete-after-display-list.patch`
   sends it after RT64 has read the list. The weapon setup screen, where it
   crashed, now runs.

Still to check: getting into a match (blind key presses didn't get past the
weapon setup screen here), and the menu rendering glitches (a black
rectangle at the bottom left and offset copies of the background behind
the panels).

## 2026-10-10, round 4: first picture

The black screen was the same on Windows with a real GPU, with sound
playing, so it wasn't the sandbox's software renderer.

**Cause.** The game builds its own video mode (`0x8014B508`) and sets the
field-0 origin to 0, so the VI starts reading right at the framebuffer.
libultra's modes start one row in, and RT64 relies on that: it finds the
framebuffer to show by stepping back one row from the VI origin
(`VI::fbAddress`). With this game's mode it looked one row before the
framebuffer, found nothing it had drawn, and showed raw RDRAM instead, which
RT64 doesn't write rendered frames back to: black.

**Fix.** `lib-patches/N64ModernRuntime/0001-vi-origin-row-zero.patch`: when
a mode's field origin is 0, ultramodern reports the origin one row in.
That only changes how RT64 finds the framebuffer; the picture is the same.
The intro credits, the Acclaim Studios Austin logo and the title screen
("Press Start") now show.

## 2026-10-09, round 3: graphics microcode, Controller Pak, FPU mode; the game runs its boot sequence

Headless runs as in round 2. Where it stops now: the game goes through its
boot modes and renders every frame (display lists with textured rectangles,
image data in the framebuffers), but the virtual screen in the sandbox shows
black. Next is telling apart a presentation problem in this software-GPU
setup from a real one (a Windows run on a real GPU would settle it).

1. **Graphics microcode.** RT64 crashed at the first display list after
   running off its end: it skipped every `G_DL`/`G_ENDDL`. RT64's database
   knows this microcode, "F3DEX2.NoN.fifo 2.05 (Acclaim)", but marks it
   `Unknown`, so no command table is set up. The display lists are plain
   F3DEX2 so far, so `lib-patches/rt64/0002-acclaim-f3dex2-variant.patch`
   maps it to F3DEX2 (and the matching L3DEX2 variant to L3DEX2). The
   banner string reads "F3DEX.NoN fifo 2.05"; it's F3DEX2, not F3DEX 1.x as
   docs/ROM_LAYOUT.md said.
2. **Black screen: boot mode 0.** The game's top-level mode (index at
   base + 0x26DB8, update table at `0x800CD69C`) stayed 0, which waits for
   the boot-time Controller Pak check to set `0x80144CE0`. The check probes
   each port with `osMotorInit` and only tries the Controller Pak when that
   fails; its pak library talked to the SI directly and stalled.
   - The pak filesystem functions in the second code block are now named
     (`osPfsInitPak` `0x004470D0`, `osPfsReadWriteFile` `0x00447C60`, ...;
     syms/libultra.txt), from the call graph and the game's call sites,
     whose arguments match libultra's. N64Recomp routes them to
     `src/game/controller_pak_hle.cpp` (the BattleTanx `.mpk` emulation).
   - The runtime's `osMotorInit` treats every pak as a Rumble Pak (its Pak
     enum has no Controller Pak), so the game never looked for one.
     `0x002B7F08` is now `trw_osMotorInit`, which fails on a port set to
     Controller Pak, as a real one does.
3. **Mode transitions never finished.** The fade between modes subtracts
   `speed x frame step` (`0x800CD648`) each frame; the step came out about
   0.0009 instead of ~0.8. The step is measured with `osGetTime` and turned
   into seconds x 15 with single-precision math in odd FPU registers (`f1`,
   `f21`, ...). The runtime starts every thread in FR=0 mode, where
   N64Recomp's generated code loads odd registers through `f_odd` (the upper
   half of the even register) but does arithmetic on the odd register
   itself, so `mul.s f20, f0, f1` multiplied by a stale value. The N64's boot
   code sets Status.FR, so every game thread (`thread_create_callback`) and
   the entry thread now run in FR=1 (`trw::set_fpu_mode`,
   src/game/tlb_views.cpp).
4. **Missed functions.** `Failed to find function` for `0x002909A0` (a mode
   table entry) and `0x004396C0`: functions reached by pointer, each behind
   a data fragment the boundary rules didn't recognise. The finder now
   accepts a stack-frame setup after data, and has a gap pass: any frame
   setup after a function's end that reads as a whole function is a start
   (49 more). One new split, `0x002B8E80`, is a libultra ROM-handle init
   with no callers that needs `osPiRawReadIo`, which the runtime lacks; it
   is stubbed.

Boot sequence observed: modes 0 (pak check), 1, 3, 4, 8, with mode 20
briefly between each; triple-buffered framebuffers at `0x80500000`,
`0x80554600`, `0x805A8C00`; the game builds its own VI mode in RAM
(`0x8014B508`): 480-wide hi-res during the first modes, then 284 wide.

## 2026-10-09, round 2: first boots (headless), up to the first display lists

Run headless in the cloud sandbox: Xvfb, Mesa's llvmpipe Vulkan driver, the
SDL dummy audio driver, and a new testing switch, `TRW_AUTOSTART=1`, which
boots the ROM already stored in the config folder without the launcher
(main.cpp). Each fix below got the game one step further.

1. **`break` after the entry point's `jalr`** (`0x00200484`): the runtime's
   osStartThread returns to the entry thread, which then hit the `break`
   meant to be unreachable. Patched to `nop`.
2. **The game's DMA thread** (`0x0025651C`) calls `osPiRawStartDma` directly
   (the runtime only has a stub) and waits for the PI interrupt message it
   registered with `osSetEventMesg(OS_EVENT_PI, ...)` at `0x0025626C`, which
   the runtime doesn't track. `0x002B9970` is now `trw_osPiRawStartDma`,
   implemented in `src/game/pi_dma.cpp`; a `[[patches.hook]]` records the PI
   event queue so each DMA posts its completion.
3. **ROM data mapped into TLB space.** Audio init asked `0x0023C76C` for a
   region and got NULL: it sends a request to a VM server thread, which hands
   out virtual ranges that the TLB-miss handler fills from ROM. The bank data
   holds absolute pointers (`0x00800014`, ...), so the region must land where
   the server would put it: pages from `0x800` up, each region on an even
   page. `trw_VmMapRomRegion` (`src/game/tlb_views.cpp`) does that and fills
   it from ROM at once. Two regions at boot: ROM `0x28B264` at `0x00800000`
   and ROM `0x503CB8` at `0x00810000`.
4. **The audio thread spun forever.** On each retrace it makes frames while
   `osAiGetStatus()` doesn't report `AI_STATUS_FIFO_FULL`; the runtime's
   always returns 0. It also makes empty frames before any sound is set up,
   so nothing was ever queued. `0x002B71F0` is now `trw_osAiGetStatus`
   (main.cpp): full once two frames of audio are queued, or after two polls
   with nothing queued in between.
5. **`Failed to find function at 0x0043C500`**: a function reached by a
   `lui`/`addiu` pointer, behind a string fragment. The finder now accepts a
   code-built pointer whose target follows data (`ends_function(after_data)`),
   but not plain data words, which split `func_002C1260` when tried.
6. **No retrace messages.** libultra's `osCreateViManager` sets a default
   mode (`__osViInit`, NTSC LPN1) and the game relies on it, waiting for a
   retrace before it ever calls `osViSetMode`. The runtime sets none, and its
   VI thread sends nothing until a mode is set. A hook after the
   `osCreateViManager` call (`0x00291358`) sets the game's own
   `osViModeNtscLpn1` (`0x800D31E0`).

**Where it stops now:** RT64 crashes processing the game's first display
lists (`RT64::Interpreter::processDisplayLists`, reading near address 0).
Likely cause: display lists or the data they point to are addressed through
the TLB (the `0x004xxxxx` block or the `0x008xxxxx` regions), which the game
turns into physical addresses with `osVirtualToPhysical`; the runtime's
version doesn't know these mappings. Next round.

## 2026-10-09, round 1: ROM layout, function list, first N64Recomp output and build

**Starting point.** The user's dump is byte-swapped (`.n64`, v64 order);
normalized with the BattleTanx repo's `tools/normalize_rom.py`. Hashes are in
docs/ROM_LAYOUT.md.

**The TLB.** The entry point at `0x80000400` writes TLB entry 31 (1 MB
pages, `0x00200000` -> physical 0) and `jalr`s to `0x002A70F0`. Every `jal`
in the first MB targets `0x002xxxxx`, so the code is linked at
`0x00200000 + physical`, while data is reached through kseg0. A `jr $ra`
density scan found a second code block at ROM `0x164000`, whose calls target
`0x004xxxxx`; `0x8003BB74` DMAs it to RAM and maps it with TLB entry 0 when
there is 8 MB, and the game's own TLB-miss handler pages it in 4 KB at a time
otherwise. Full write-up in docs/ROM_LAYOUT.md.

**Function boundaries** (`tools/find_functions.py`, output
`syms/functions.txt`). Signals: splat/spimdisasm glabels
(`tools/splat.yaml`, two code segments at `0x00200400` and `0x00400000`),
the hand-named libultra list, n64sym's libaudio matches, `jal` targets,
`lui`/`addiu` code pointers and data words that point at code (only where
the word before ends a function), and stack-frame setups right after a
function end. Problems found and handled:
- **Switch-case labels** look like code pointers in data. Jump tables are
  found by constant propagation (`lui`/`addiu` -> `addu` index -> `lw` ->
  `jr`, with the base often held in a saved register such as `$fp` from
  the top of the function), and their entries are excluded: 160 tables,
  1,242 labels.
- **Function ends.** "Last exit before the next start" ran into data. Ends
  are now found by following branches, `j`s and jump-table entries to the
  first exit nothing jumps past.
- **Data inside the text.** Fragments of the toolchain's type-information
  strings sit between some functions (e.g. `0x0023B984`, `0x00405DB4`).
  A "function" is rejected if it has an invalid instruction, a call or
  branch out of the code segments, a `$k0`/`$k1` operand outside the
  exception code, or a write to `$zero` other than `nop`; a stray word in
  front of a real function moves the start forward to it.
- **n64sym** matched about 150 names, but those inside game code were false
  positives (`osVoiceControlGain`, `osSetTime`, `osBbUsbDevGetHandle`,
  `__rmonPanic`, ...) and its `*_text_XXXX` names are labels inside
  functions (`timerintr_text_0300` is the middle of `__osInsertTimer`).
  Since N64Recomp replaces any function whose name is on its list with the
  runtime's version, a wrong name replaces game code. libultra
  (`0x002B71E0-0x002BD8A4`) was named by hand instead, from n64sym's
  in-range matches, the call graph and the hardware registers each function
  touches (`syms/libultra.txt`, with "?" where a name is from context).
  Notes: `0x002BAF90` is `__osSiRawReadIo`, not n64sym's `__osSpRawReadIo`;
  the game's DMA routine calls `osPiRawStartDma` directly; motor start/stop
  are told apart by the rumble init caller.

**N64Recomp.**
- It requires the entry function at ROM `0x1000`, so the entry point is
  `0x00200400` with its cop0/TLB instructions and its call to the TLB helper
  patched to `nop`; its `jalr` to `0x002A70F0` then goes through the
  function lookup.
- `trw_boot_main` re-copies the first MB from the cartridge bus
  (`lw` from `0xB0001000`) unless it detects a soft reset. The runtime has
  already loaded that MB, so its branch is patched to always take the
  soft-reset path (`0x002A71C4`, `bnez` -> `b`).
- The game's exception layer (`0x00202268-0x00202CA0`) is ignored or
  stubbed, as are its TLB helpers.
- **Jump tables in another section.** N64Recomp reads a jump table's ROM
  bytes relative to the calling function's section ("TODO this assumes that
  the jump table is in the same section"). Here the code is in the
  `0x00200400` section and reads its tables through kseg0 (`0x800C18F0`),
  so it read the wrong bytes. `lib-patches/N64Recomp/0001-...patch` looks
  up the section that holds the table instead, and the symbol file adds a
  function-less `.main_kseg0` section (same ROM, vram `0x80000400`) for it
  to find.
- Result: 3,794 functions, no errors. Two warnings: a tail call from
  `0x00207FF8` into the next function, and indirect tail calls.

**Runtime side** (`src/game/tlb_views.cpp`). librecomp registers the first
MB's functions at its load address (`0x80000400`), but the code calls
`0x002xxxxx`, and the second block is never loaded by the runtime at all.
At init, both views are registered with `load_overlays` at their TLB
addresses. For data: generated code maps an address `A` to
`rdram + (A - 0xFFFFFFFF80000000)`, so `0x00400000` is `0x80400000` bytes
into librecomp's 4 GB reservation, which is mapped no-access. Those ranges
are made read-write and filled from the ROM. A static scan found no loads
or stores through `0x002xxxxx` except in the exception code and
`osInitialize` (neither runs), and the `0x004xxxxx` block is re-read from
ROM on 4 MB consoles, so the game can't depend on writing to it; copies
should be enough. If not, the fix is shared-memory-backed rdram with real
aliases.

**Audio.** `aspMain` at ROM `0xDAAB0` (n64sym's `aspMainTextStart`), size
`0xE1C` (ends with `j` + delay slot at IMEM `0x1E94`). The dispatch table
is the 16 halfwords at data + 0x10. RSPRecomp output is in `rsp/` (generated,
not committed).

**Build.** The BattleTanx project (frontend, runtime shims, crash handler,
config) was copied and adapted. Game-specific BattleTanx fixes (widescreen,
frame step, scissor, cutscene projection, the UI-pump patch) were left out.
The SI access queue shim uses this ROM's addresses (`0x801467E8`,
`0x801467E0`, enable flag `0x800D3050`). The UI theme is amber. The project
started in a folder of the BattleTanx repository and moved here, with the
same submodule versions.
