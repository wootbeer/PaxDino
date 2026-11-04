# Changelog

## [Unreleased]

## [0.1.7 Beta] - 2026-11-04

### Added
- Netplay (two to four players, direct connection): one player hosts on a
  UDP port, the others join by the host's address or join code. The host
  starts the game once everyone is in. The host's game settings are used
  for everyone, and players joining see them before joining. Each
  player keeps their own controls and sees their own view full screen.
  Controller Paks, Custom Music and custom crosshairs aren't used online
  for now
- Netplay chat: in a match Enter or T opens a chat line at the left of the
  screen, and in the game's menus T does (Enter still selects there); Enter
  sends, Escape clears the line, then closes it. The keys are in the
  Controls tab as Chat. In the pause menu the keys work as before
- Netplay tells a player on another version which versions each side has,
  and the Netplay screen says when a newer PaxDino is out (a check with
  GitHub's latest release when it opens)
- Netplay nicknames: set Your name on the Netplay screen (letters, digits and
  a little punctuation, 12 at most); shown in the lobby, the Netplay tab and
  the chat
- Netplay: the Netplay tab and the chat show each player's character
- Netplay: Leave Game in the Netplay tab (F2) leaves the session and opens
  the Netplay screen again
- Netplay: Unlock Everything on the Netplay screen (the host's choice, on by
  default) gives everyone every level, character, weapon and skin. Time Trial
  and Frag Fest (Extra Modes) stay off online

### Changed
- Controls and Mods are no longer in the launcher menu; they're tabs in
  Settings
- The Modern preset uses the Scanlines screen filter instead of CRT, whose
  glow made menu text bloom
- During netplay the menu shows a Netplay tab (F2) with the players, and
  hides Enhancements, Unlocks, Cheats and Mods; mods are off for the session
- Netplay join codes: hosting shows a six letter code the other player
  types instead of an address, and the two games connect without port
  forwarding on most home connections (tools/matchmaker has the free
  Cloudflare Worker it uses)
- During netplay 1 Player Trials, Controller Pak (and 2 Player Trials with
  more than two players) are greyed out on the main menu, How Many Players
  is set to the players connected, and Medals in Solo Matches is off
- Netplay: a short wait for the other player fades the sound instead of
  skipping it
- Netplay hosting asks the router to open its UDP port (UPnP), and says
  whether it did
- Netplay input delay: the host picks Auto (from the ping, the default) or a
  fixed number of controller reads, instead of always 3. The Netplay tab
  shows the ping and the delay
- During netplay the aspect ratio and field of view are the host's, Clear
  Menu Panels is on and Weapon Reverb off; those options are greyed out
- Netplay public games: a host can list its game (Public, with a lobby
  name), and Join a game shows the open ones to join with a click, four a
  page. Needs the join code Worker deployed again
- The Netplay screen is laid out again: Host and Join side by side, a lobby
  with the join code and the players, and larger text, and the menu's Netplay
  tab (F2) to match

### Fixed
- Windows: PaxDino's window comes to the front when it starts (it could open
  behind Explorer), also after Leave Game
- Netplay: when the host left, the others couldn't use the menus or pause
  the match to quit it
- Netplay hardened against bad data from other players, routers and the join
  code service (packet fields, chat, UPnP answers); the join code Worker
  limits public listings and refuses joiners to a full game
- Sound crackled in heavy scenes on slower PCs: the game now keeps more
  sound queued when it runs short (up to 100 ms), and less again once things
  settle
- Sound crackled in netplay, even on the title screen
- Controllers SDL doesn't know by itself weren't picked up; the community
  controller list (SDL_GameControllerDB) now comes with the game
- On a 4K screen with Windows display scaling, the picture could be bigger
  than the screen and cut off at the edges
- In split screen with a crosshair style other than Game, only one player's
  crosshair was drawn
- Netplay: How Many Players is skipped and set to the players connected; it
  used to flip between two counts
- Widescreen: on the main menu and the level select the background stopped
  short of the bottom of the screen; it's now filled in, dimmed
- Netplay: in Frag Tag both players' views showed at once (the game's
  display list was full)
- Netplay: in Frag Tag the monkey face showed twice while the monkey was
  picked
- Netplay: the other player's address is only shown if you typed it
- Netplay view: HUD text lost a line at the top, and the assault rifle's
  scope was stretched
- Netplay view: the gun and the view were too wide, the clock was cut off at
  the top, and the weapon wheel and sights were stretched; it now looks as
  it does playing alone

## [0.1.6 Beta] - 2026-10-26

### Fixed
- Mouse look could write to the last match's player after quitting, a
  possible cause of the crash after restarting matches

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
