# PaxDino: Rage Wars Modernized

You will need the game file from a licensed copy in order to play it.

A native PC port of Turok: Rage Wars, made by statically recompiling the N64
game with [N64Recomp](https://github.com/N64Recomp/N64Recomp) and running it
on [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) with
[RT64](https://github.com/rt64/rt64) for rendering. 

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own dump of the game to build or play this.

## Features
- Runs natively on Windows. Linux and Steam Deck builds are experimental.
- Widescreen. 
- High resolutions and framerates.
- Bloom, anti-aliasing, and more.
- Mouse and keyboard with mouse aim, plus modern controller layout.
- Mouse and keyboard controls in game menus.
- Local multiplayer, up to 4 players, each with their own controller or the
  keyboard.
- Earn medals in bot matches.
- Frag tag bugged fixed in original rom. (do not use rev1)
- Classic and Modern settings presets.
- Bloom, CRT, Scanlines and LCD screen filters.
- Custom music. Put your own songs in the Music folder.
- Adjustable reverb.
- Unlocks and cheats in the settings, no codes needed.
- Add to Steam from the launcher, with artwork.
- Custom crosshairs.
- And much, much more.

## Game Files
You need your own data file, Turok - Rage Wars (USA).
SHA-1:
11C2DD51FFCECC5D8134CEC96FFCF9C44B15DB48 (.n64)
9F8CB190831945F6F707AA8C2B20E04FA4276795 (.z64)
`.v64` files work too.

Start the game, pick **Load ROM** and select your file. It only has to be
done once.

Add a blank portable.txt in the application's folder to make it portable,
if you want to save settings and saves locally.

## Download
Get the latest build from the [Releases](https://github.com/wootbeer/PaxDino/releases) page.
- **Windows:** unzip it anywhere and run `PaxDino.exe`.
- **Linux / Steam Deck (experimental):** extract it and run
  `PaxDino-x86_64.AppImage` (make it executable first if needed). Settings and
  saves go in `~/.config/trwrecomp`, the Music folder next to the AppImage.

On Steam Deck, in Desktop Mode, use **Add to Steam** on the launcher (or "Add a
Non-Steam Game" in Steam), then play it from Gaming Mode.

## Controls
Mouse and keyboard defaults. Change them on the Controls tab. F1 opens the
settings, F2 opens the Cheats tab.

| | In a match | In menus |
|---|---|---|
| W A S D | move | C buttons |
| Mouse | aim | point and click |
| Left click | fire | select |
| Right click / R | weapon's second function | back |
| Mouse wheel / E / Q | next / previous weapon | |
| 1 - 8 | pick a weapon | |
| Space | jump | |
| Enter | | select |
| Esc | pause | back |

Controllers: left stick moves, right stick aims, right trigger fires, left
trigger jumps.

## Building
See [BUILDING.md](BUILDING.md).

## Issues and Limitations

Known:
- Beta.
- Linux and Steam Deck builds are experimental.

Else:
- Please see the "Issues" section in GitHub. Screenshots or a short video
  help a lot.
- If the game crashes, attach `crash_log.txt` from the game's folder.

## Changes
See [CHANGELOG.md](CHANGELOG.md).

## Credits
- [N64Recomp and N64ModernRuntime](https://github.com/N64Recomp) by Mr-Wiseguy
  and contributors, which this port is built with
- [RT64](https://github.com/rt64/rt64) by Dario and contributors, the renderer
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by the N64Recomp
  contributors, the launcher and input layer
- The CRT filter is based on Timothy Lottes' CRT shader, and the LCD filter on
  LCD3x by Gigaherz (both public domain)
- [dr_mp3, dr_flac and dr_wav](https://github.com/mackron/dr_libs) by David
  Reid and [stb_vorbis](https://github.com/nothings/stb) by Sean Barrett (both
  public domain), the custom music decoders
- [splat](https://github.com/ethteck/splat), spimdisasm,
  [rabbitizer](https://github.com/Decompollaborate/rabbitizer) and
  [n64sym](https://github.com/shygoo/n64sym), used for the function analysis
  in `tools/`

## License
The project's own code is GPL-3.0; see [COPYING](COPYING). The game itself
remains the property of its rights holders.
