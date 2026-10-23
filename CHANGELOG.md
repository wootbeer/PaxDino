# Changelog

## [Unreleased]

## [0.1.5 Beta] - 2026-10-23

### Fixed
- Another crash after quitting and restarting matches: the game's memory
  was written to through the last match's players

## [0.1.4 Beta] - 2026-10-22

### Fixed
- The red transition at the end of a trial played twice as fast with the
  Framerate above Original
- The game could crash on purpose when a frame took more than 20 seconds
  (a leftover hang check from the console)
- A crash after quitting and restarting matches several times

## [0.1.3 Beta] - 2026-10-20

### Changed
- Medals in Solo Matches also gives the match medals in trials, not just the
  trial medals

### Fixed
- Medals in Solo Matches didn't count wins toward the arena medals
- Crash logs now include which thread crashed and its call stack, for
  crashes outside the game code
- Bastille's shield came back twice as fast with the Framerate above
  Original
- Frag Tag's tag switch timer ran twice as fast with the Framerate above
  Original

## [0.1.2 Beta] - 2026-10-17

### Changed
- Matches run at 60 frames a second when the Framerate (Graphics) is above
  Original, so the mouse aims without the lag. Original keeps the game's 30
  frames a second
- Mouse Sensitivity covers a wider, more usable range, and the default is
  faster

### Fixed
- Mouse look stopped for a moment when switching weapons
- The level stuttered when turning fast
- Turning with the mouse wasn't as smooth as with a gamepad
- Character models showed over the level's name at the start of a match

## [0.1.1 Beta] - 2026-10-17

### Added
- Field of View slider in Graphics
- Double click a slider in the settings to put it back to its default
- Rapid Fire: Semi-Auto. Fires as fast as you press, holding fires at the
  normal rate

### Fixed
- The objective text at the start of a trial was missing
- The weapon wheel flashed on screen when picking a weapon with the mouse
  wheel, Q / E or the number keys
- Local Multiplayer forgot which controller or keyboard each player had on
  every launch
- Custom Music went silent in the pause menu. It now plays on through it
- Rapid Fire didn't fire some weapons while held
- Rapid Fire let the Cerebral Bore, PFM Mine, Iron Claw and Sentry Turret
  pickups be used over and over

## [0.1.0 Beta] - 2026-10-16

First public beta. Compared to the original N64 game:

### Added
- Native Windows build, plus experimental Linux and Steam Deck builds
- ROM picker. Accepts .z64, .n64 and .v64
- Widescreen, with the HUD on the screen edges
- High resolutions and framerates, with smooth motion
- Mouse and keyboard, with mouse aim and a mouse cursor in menus
- Weapon picks with the mouse wheel, Q / E and the number keys
- Modern controller layout by default (right stick aims, triggers fire and
  jump), all remappable
- Local multiplayer with up to 4 players, each on their own controller or
  the keyboard
- Classic / Modern presets on the launcher
- Bloom
- Screen filters: CRT, Scanlines, LCD, with an option for menus only
- Look Spring Override, per player
- Weapon Reverb
- Custom Music, with Keep Song and a volume slider
- All Music: any of the game's songs in a match
- Custom crosshair: style, color, size, opacity or your own image
- Loadout Restrictions: Hammers, Bullet, Energy, Explosive or Scorpion only
- Medals in solo matches
- Demo on/off on the title screen
- Unlocks: All Levels, All Skins
- Cheats: All Characters, All Weapons, Unlimited Ammo, Extra Modes,
  Infinite Health, Moon Jump, Rapid Fire, Infinite Time. F2 opens them
- Skip the boot screens and the level fly-through
- Add to Steam from the launcher, with artwork
- Controller Pak saves as .mpk files, one per player
- crash_log.txt generates upon crash

### Changed
- The Cheats menu moved from the main menu into the settings (F2). The
  Credits item went with it
- The pause menu's selection is easier to see
- Clear Menu Panels: no scanlines on the menu panels (can be turned off)

### Fixed
- Co-op Frag Tag trials could never be won (the bug Acclaim fixed in the
  1.1 cartridge)
