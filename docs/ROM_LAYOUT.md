# Turok: Rage Wars (USA): ROM and memory layout

ROM: `Turok - Rage Wars (USA)`, game code `NRWE`, revision 0, 8 MB.

| Format | SHA-1 |
|---|---|
| `.n64` dump as supplied (byte-swapped, v64 order) | `11c2dd51ffcecc5d8134cec96ffcf9c44b15db48` |
| normalized big-endian `.z64` | `9f8cb190831945f6f707aa8c2b20e04fa4276795` |

The runtime accepts either; it byte-swaps before checking the hash
(XXH3-64 of the `.z64`: `0x21667bf153183fa0`).

Header: entry point `0x80000400`, clock rate `0xF`, CRCs `ADB9498B DAF28F55`.

## The TLB

Unlike most N64 games, this one runs its code through the TLB:

1. The IPL3 copies ROM `0x1000-0x101000` to `0x80000400` as usual.
2. The entry point (`0x80000400` in kseg0, linked as `0x00200400`) writes TLB
   entry 31: 1 MB pages, virtual `0x00200000` -> physical `0x000000`,
   virtual `0x00300000` -> physical `0x100000`. It then `jalr`s to
   `0x002A70F0` (`trw_boot_main`).
3. From then on, all code runs at `0x00200000 + physical address`. A `jal`
   only holds 26 bits, so the code itself only works at those addresses, and
   function pointers in data are `0x002xxxxx`.
4. Data is reached through kseg0 (`0x800xxxxx`) as usual: about 9,000 `lui`
   instructions load `0x800C-0x8015`, and almost none load `0x002x`.

A second block of code is linked at `0x00400000`:

| | ROM | vram | |
|---|---|---|---|
| text | `0x164000-0x1B22B4` | `0x00400000-0x0044E2B4` | 597 functions found by spimdisasm |
| rodata | `0x1B22B4-0x1B5A50` | `0x0044E2B4-0x00451A50` | strings, tables and floats read through `0x0045xxxx` |

`0x8003BB74` (in kseg0 terms) sets it up. With 8 MB of RAM it DMAs the whole
block into RAM and maps it with TLB entry 0 as one 1 MB page
(`trw_MapTLB(0, 0x1FE000, 0x400000, phys, -1, 7)`). With 4 MB, the game's own
TLB-miss handler (`trw_TLBMissHandler`, `0x0023C1D0`, reached from the
exception code at `0x00202460`) pages it in 4 KB at a time from ROM, into a
pool at kseg0 `0x8014C000`. Since evicted pages are re-read from ROM, the
game can't rely on writing to this block.

The exception layer at `0x00202268-0x00202CA0` (vector stubs, an installer
that copies them to `0x80000000`/`0x80000180`, SR/cache/TLB helpers, and the
handler) replaces libultra's handler. libultra's own exception code is also
in the ROM (`0x002BBB98-0x002BCD50`).

How the port handles this: `turokrw.us.toml` patches the TLB writes out of
the entry point and stubs or ignores the exception layer, and
`src/game/tlb_views.cpp` registers every function at its TLB address and
fills the two views with the ROM bytes (see that file for why copies are
enough).

## ROM map (first 2 MB)

| ROM | vram | Contents |
|---|---|---|
| `0x000000` | | header, IPL3 |
| `0x001000-0x0C24F0` | `0x00200400-0x002C18F0` | main code: game, libaudio (`0x002AE000-0x002B71E0`), libultra (`0x002B71E0-0x002BD8A4`), libc/libm, then more game code to `0x002C18F0` |
| `0x0C24F0-0x101000` | kseg0 `0x800C18F0-0x80100400` | rodata (jump tables first), data, RSP microcode (rspboot `0x800D78C0`, aspMain `0x800D9EB0`, Acclaim F3DEX2 and L3DEX2 "fifo 2.05" text/data), and some unlinked object-code fragments in padding |
| | kseg0 `0x800E2000-0x80164000` | bss (cleared by `trw_boot_main`, except a few soft-reset-preserved ranges) |
| `0x101000-0x164000` | | type information left in by the toolchain: class and member names such as `CEngineApp_t`, `CGameRumble_t`, `m_vGrappl...`, with their sizes. Not loaded. Useful for naming structures later. |
| `0x164000-0x1B5A50` | `0x00400000-0x00451A50` | second code block (above) |
| `0x1B5A50-` | | more type information, then assets |

The text sections also contain fragments of string data between some
functions (the same kind of type-information text), which
`tools/find_functions.py` has to step over.

## Microcode

- Graphics: Acclaim's variant of F3DEX2 ("RSP Gfx ucode F3DEX.NoN fifo
  2.05"; RT64's database calls it "F3DEX2.NoN.fifo 2.05 (Acclaim)") and
  L3DEX2. RT64 handles them in HLE as F3DEX2 / L3DEX2 with
  `lib-patches/rt64/0002-acclaim-f3dex2-variant.patch`.
- Audio: libultra's `aspMain`, text ROM `0xDAAB0` (size `0xE1C`), data ROM
  `0xDC0E0`. Recompiled with RSPRecomp from `aspMain.us.toml`.

## Saves

No EEPROM/SRAM/Flash code; saves go to the Controller Pak. The `osPfs*`
functions are in the second code block (`0x004460F0-0x00448E34`, named in
syms/libultra.txt). At boot the game probes each port with `osMotorInit`
first and tries `osPfsInitPak` only when that fails (`0x00404E28`).

## Known game addresses

| Address | What |
|---|---|
| `0x00200400` | entry point (`trw_entrypoint`) |
| `0x002A70F0` | `trw_boot_main` |
| `0x0029FF5C` | `memset` |
| `0x0025651C` | the game's DMA routine (calls `osPiRawStartDma` directly) |
| `0x800CD69C` | top-level mode table: update, draw(?) and init per mode, 12 bytes each; current mode at base + 0x26DB8 |
| `0x800CD648` | frame step (seconds x 15), from `osGetTime` (`0x00292818`) |
| `0x80144CE0` | set when the boot-time Controller Pak check finishes |
