# RAM notes

Known Turok: Rage Wars (USA 1.0) RAM addresses, for features such as mouse
look, a cheat menu and the medal option. N64 (kseg0) addresses.

## Player and view

| Address | What |
|---|---|
| `0x8013EF84` | pointer to the local player object (`CPlayer`) |
| player + `0x000` | character object; its yaw (float, radians) at +`0x6C` |
| player + `0x2E8` | weapon object |
| player + `0x5D0` / `0x5D4` | active flag / controller number |
| player + `0x6C0` / `0x6C4` | forward / side speed |
| player + `0x6D0` | on ground |
| player + `0x71C` | duck mode |
| player + `0x724` | head rotation offset; pitch (float, radians) is its first component |
| player + `0x771` | requested weapon |
| `0x80119B26`, `0x80119C66`, `0x80119DA6` | frame-rate mode bytes (0 max, 1 60, 2 30, 3 15) |
| `0x80140C3C` | frame scale (float; 120 at 60 fps, 60 at 30, 30 at 15) |

## GameShark codes (community codes)

GameShark code types: `80aaaaaa 00vv` writes a byte, `81aaaaaa vvvv` writes a
halfword, `D0aaaaaa 00vv` / `D1aaaaaa vvvv` only run the next line while the
byte / halfword at the address equals the value (used for button
combinations). The `EE000000 0000` enable code isn't needed here.

| Code | Effect |
|---|---|
| `8033637E 0064` | infinite lives |
| `801522DD 00??` | frags |
| `81337170 430E` | power core items last forever |
| `8033653C 0003` | P1 infinite mines (pick up weapon, multiplayer) |
| `8110EB50 FFFF` + `8110EB52 FFFF` | unlock all characters |
| `D01451E5 0020` + `81335F60 4398` | P1 moon jump while holding L |
| `811527F4 47AF` + `811527F6 C4B1` | infinite time |
| `80147732 0000` + `8014773E 0000` | no shield for Bastile |
| `8014777A 0000` + `80147786 0000` | no life for Bastile |
| `D11571E4 2000` / `81336330 0002` / `D11571E4 2000` / `81336332 0002` | P1 rapid fire (while holding Z) |
| `D11451EC 2000` / `81337A18 0002` / `D11451EC 2000` / `81337A1A 0002` | P2 rapid fire |
| `D11451F4 2000` / `81339100 0002` / `D11451F4 2000` / `81339102 0002` | P3 rapid fire |
| `803366B0 00XX` / `80337D98 00XX` / `80339480 00XX` | P1 / P2 / P3 current weapon |
| `81336526 6400` | P1 infinite health |
| `80336535 0063` / `80336537 0063` / `80336539 0063` | P1 infinite bullet / energy / explosive ammo |
| `80336543` ... `8033656D 00??` | P1 weapon slot modifiers (one byte per weapon, in the order below, odd addresses from `0x80336543`; `0x80336563` and `0x80336565` are skipped: Raptor Claw and Mite Bite) |

In the port the player objects are elsewhere (the heap is laid out for the
Expansion Pak); found from the ammo bytes, the codes' P1 player is at
`0x80335F40`, so as offsets from the player pointer (`*0x8013EF84`):
moon jump `+0x20` (float, up), rapid fire `+0x3F0`/`+0x3F2`, lives `+0x43E`,
health `+0x5E6` (halfword, `0x6400` = 100), bullet / energy / explosive ammo
`+0x5F5`/`+0x5F7`/`+0x5F9`, current weapon `+0x770`. The button and
infinite-time addresses don't match this version; the match clock is the
float at `0x801407F4` (counting down; 0 with no time limit).

Player structures are 0x16E8 apart, confirmed by the codes for all four
players (P1 / P2 / P3 / P4):

| Field | P1 | P2 | P3 | P4 |
|---|---|---|---|---|
| health (halfword; `6400` = infinite) | `0x80336526` | `0x80337C0E` | `0x803392F6` | `0x8033A9DE` |
| bullet / energy / explosive ammo (bytes) | `0x80336535` / 37 / 39 | `0x80337C1D` / 1F / 21 | `0x80339305` / 07 / 09 | `0x8033A9ED` / EF / F1 |
| first weapon slot byte (Warhammer) | `0x80336543` | `0x80337C2B` | `0x80339313` | `0x8033A9FB` |
| current weapon | `0x803366B0` | `0x80337D98` | `0x80339480` | `0x8033AB68` (by the stride) |

The weapon slot bytes (one per weapon, every other byte, in weapon id order,
Raptor Claw and Mite Bite included in the spacing) hold which of the 8 slots
the weapon sits in: 00 = slot 1 ... 07 = slot 8. In the supplied P4 list the
PFM Mine, Iron Claw and Sentry Turret entries (`0x8033AA23`/`25`/`27`) are 2
higher than the stride gives (`0x8033AA21`/`23`/`25`); probably a typo in the
list, check before use.

The stride also matches the `+0x16E0` "next player" link seen in the code.

Weapon ids: 00 Warhammer, 01 Boomerang, 02 Tek Crossbow, 03 Mag 60,
04 Shotgun, 05 Assault Rifle, 06 Mini Gun, 07 Plasma Rifle, 08 Freeze Gun,
09 Flaregun, 0A Grenade Launcher, 0B Scorpion Launcher, 0C Napalm Gel,
0D Emaciator, 0E Inflator, 0F Chest Burster, 10 Raptor Claw, 11 Mite Bite,
12 Cerebral Bore, 13 PFM Mine, 14 Iron Claw, 15 Sentry Turret.

The addresses in the 0x8033xxxx-0x8034xxxx range are inside heap-allocated
player objects; they're stable on real hardware for a given game state, but a
cheat menu should reach them through the player pointer (`0x8013EF84`)
instead where it can.

## 1.0 vs 1.1 (Rev 1)

1.1 fixed the 2-player Frag Tag trial, which in 1.0 always reports failure
(so the Co-Op medal can't be earned). Comparing the two ROMs function by
function (instruction patterns with addresses masked), 19 of the 3,885 1.0
functions differ in 1.1, all in the main code block; the second code block
(0x00400000) is unchanged apart from relocations:

`0x0020402C`, `0x0022804C`, `0x0022A2FC`, `0x0022A36C`, `0x00250E00`,
`0x002532D4`, `0x0025651C`, `0x00288FD4`, `0x0028E880`, `0x00291334`,
`0x00293A14`, `0x00293BD0`, `0x00294500`, `0x00294590`, `0x0029499C`,
`0x00294FF4`, `0x00295DEC`, `0x00295E54`, plus the exception vectors.

The fix is in `0x0022804C`, the frag-limit check: run again at the end of a
trial, in 1.0 it took every player as having reached a frag limit of 0 (Frag
Tag trials have none), and when the first player in the list was a bot it
overwrote the win with a failure. 1.1 returns "not reached" when the limit is
0; the port does the same with a hook at the top of the function (STATUS.md
round 52).

## Profiles, medals and cheats

Four player profiles of `0x190` bytes from `0x800FCB00` (slot n at
`0x800FCB00 + n * 0x190`), loaded from the Controller Pak:

| Offset | What |
|---|---|
| `+0x00` | name |
| `+0x0D` | slot number, `-1` (`0xFF`) when no profile is loaded there |
| `+0x10` / `+0x14`-`+0x16` | stats (accessors at `0x0022F2F4`-`0x0022F378`) |
| `+0x18` | one byte per character (2 on a new profile); `func_0022F434` |
| `+0x4A` | medals, 50 bits; counted by `func_004261E4(slot)` |
| `+0x51` | bits set at the end of a match per character played (`func_00425854`); get `func_0022F5A4`, set `func_0022F584` |
| `+0x78` / `+0x7B` | bits per character (get `func_0022F53C`) |
| `+0x79` | character bits (get `func_0022F4BC`, set `func_0022F49C`; 16 and 17 always count as set) |
| `+0x7E` bit 0 | a character's map position is saved (New Game asks before removing it) |

Bits are read and written with `func_002655F0(bits, n)` and
`func_00265628(bits, n, value)`.

The Cheats menu (Z on the main menu) adds up the medals of the loaded profiles
(`func_004212A4`) and enables All Characters at 6, All Weapons at 10, Unlimited
Ammo at 12 and Extra Modes at 14; a debug path counts 40 when the menu state's
`+0x38` is 1. Its settings are bits of `0x80140208`: All Characters
`0x08000000`, All Weapons `0x10000000`, Unlimited Ammo `0x8`, Extra Modes
`0x20000000`. The character select screens don't ask `func_0022F4BC`; with All
Characters on, the whole roster shows.

The main menu (state 3) opened the Cheats menu (state 0x1C) from its event
0xA handler (`func_004244B4`) on Z; the port leaves it out (turokrw.us.toml).
The 1 Player Trials map (`*0x800DF2F0`) keeps its own copy of the level bits
at `+0xA74` (36 bits, copied by `func_004271E8`), and more bits at `+0xA79`.

Skins: one byte per character id at `+0x57 + id`, the number of skins that
character has (0 or 1 means only the first). `func_00425BC0(slot)` sets them
from the medal count (2 skins from 4 medals up, 3 from 30 up, character by
character); full: 3 for ids 0-5 and 7-10, 2 for id 6, 3 for ids 17 and 18.
C-Up on character select (`func_00420214`) steps through them. (Ugetab's
all-skins code writes `+0x58`-`+0x65`: it misses id 0 and gives id 6 a
third skin.)

The GameShark "unlock all characters" code (`8010EB50`) isn't read by this
version.

### Unlock codes (Ugetab, from GameHacking)

Written to profile 1 (`0x800FCB00`); the offsets apply to every profile.

| Code | Effect | Profile bytes |
|---|---|---|
| `810FCB4A FFFF` ... `810FCB52 7F06` | all characters, full Cheats menu, all medals | `+0x4A`-`+0x53` |
| `810FCB58 0303` ... `810FCB64 0200` | all skins | `+0x58`-`+0x65` |
| `810FCB7C 6F06` | all main menu options | `+0x7C`-`+0x7D` |
| `810FCB7E FFFF` ... `810FCB86 FFFF` | all 1 Player Trials levels | `+0x7E`-`+0x87` |
| `810FCB88 FFFF` ... `810FCB92 079D` | all Time Trial / Frag Fest levels | `+0x88`-`+0x93` |

The menu option and level bits are read through the bit test
`func_002655F0`, which the Unlocks tab's All Levels hooks. The skin bytes
aren't read when C-Up is pressed on character select.
