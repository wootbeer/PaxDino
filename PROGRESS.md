# Progress

Where PaxDino stands, and what could come next. STATUS.md has the working
log with the reasoning behind each step.

## Approach

The same route as the BattleTanx: Global Assault recomp: N64Recomp for the
game code, N64ModernRuntime for libultra, RT64 for rendering,
RecompFrontend for the launcher, config and input. Fixes are small and
targeted (hooks in `turokrw.us.toml`, `src/game/`, and local patches to the
libraries in `lib-patches/`), and the game itself stays original apart from
what's needed to run well on PC and the options in the settings.

## Done

- ROM layout, including the TLB: code runs at `0x00200400` and
  `0x00400000`, data through kseg0 (docs/ROM_LAYOUT.md,
  `src/game/tlb_views.cpp`).
- Function list (`tools/find_functions.py`), libultra identified by hand,
  N64Recomp and RSPRecomp (audio) running on the whole game.
- The whole game playable: menus, matches, trials, split screen.
- Controller Pak saves (`.mpk` files).
- Widescreen in matches and menus, HUD at the screen edges, split screen in
  widescreen.
- High framerates with smooth motion (RT64 frame interpolation, matrix
  groups for the weapon and each split-screen view).
- Mouse and keyboard, modern controller layout, local multiplayer with each
  player on their own controller or the keyboard.
- Settings: presets, bloom, screen filters, Look Spring Override, Weapon
  Reverb, custom music, crosshair, Loadout Restrictions, unlocks and cheats.
- The co-op Frag Tag fix from the 1.1 cartridge.
- Windows and Linux (AppImage) packages.

## Ideas

- **More bots.** The game caps them; find where.
- **Pickup toggles for Loadout Restrictions.**
- **HD textures.** RT64 can replace textures with packs matched by hash;
  the game's textures are stored uncompressed in the ROM.
- **Online play.** Lockstep input sharing, as N64 emulator netplay does.
  Needs the game to run deterministically on every machine (frame-locked
  timing, no host-clock randomness). Big.

## Known issues

- In split screen, player 1's HUD was seen jumping to the window's edges
  for a frame, rarely. Not seen since the last fix (STATUS.md round 90).
- The TLB views are copies of ROM data, not aliases of RAM; see
  `src/game/tlb_views.cpp` for why this should be enough, and what to change
  if it isn't.
