# Status

The working log: each investigation and fix, newest first, as "rounds".
PROGRESS.md has the short version.

# Log

## 2026-11-04, round 187: Leave Game from the AppImage

Inside an AppImage /proc/self/exe is the binary in its mount, which goes
when the process that started it exits: Leave Game starts $APPIMAGE (the
AppImage file) when it's set, else /proc/self/exe's path.

## 2026-11-03, round 186: the window in front on Windows

The user's Windows 11 mini PC opened PaxDino behind other windows (Explorer),
at start and after Leave Game. create_window now sets
SDL_HINT_FORCE_RAISEWINDOW and raises the window; ten launcher frames in, the
window is brought to the front again (BringWindowToTop and SetForegroundWindow
with the foreground window's thread input attached, as the hint does), since
the start can take a while and the focus may have gone elsewhere meanwhile;
Leave Game's restart calls AllowSetForegroundWindow for the new process.
Only the SDL part runs here (Linux); the Windows part is not tried.

## 2026-11-03, round 185: no Extra Modes online; Controller Pak greyed

Extra Modes (Time Trial and Frag Fest on the main menu, one player's) is
never on in a session: session_settings and sane_settings clear it, so the
host's own Unlocks tab can't turn it on either, and Unlock Everything is All
Levels, Skins, Characters and Weapons.

With paks held off, the main menu's Controller Pak (text 60) is greyed in a
session. The 0x100 flag alone makes the cursor skip it but it was drawn
bright: a menu text's +0x30 is its colour, 0xFF000000 (its own, drawn so
greyed or not; Multiplayer, Controller Pak) or 0x00030000 (the trials', which
follow their item's greyed state). grey_out_item now sets a greyed item's
text to the latter. Checked: dim like 1 Player Trials, and Down goes
Multiplayer, 2 Player Trials, Options.

## 2026-11-03, round 184: cleanup after the pak and profile rounds

- match_on and note_in_match went (only the old host-gone copy in exchange
  read it; stand_in_for_host reads the game's own mode and pause).
- watch_menu_texts (the Load Players walks, up to 8192 widgets a menu) runs
  only with kControllerPaks on; with paks held off nothing reads it.
- Comments and CHANGELOG brought in line with paks being held off (the
  netplay.cpp header, the hardening and greyed-items entries); an unused
  include gone; the Load Profiles block's indent.
- Checked: no debug output left (the test builds' [AL], [SCREEN], [TX] were
  only ever in the build copy); every net_log format is a literal (the
  template's warning is only that it can't tell); no new warnings.
Tested again: a session in sync (sync test, about 1500 frames), and the host
lost mid-match: Escape pauses, Quit, YES, level select.

## 2026-11-03, round 183: when the host leaves; Leave Game

A joiner whose host is gone plays on alone, and the game answers player 1's
controller alone in the menus and the pause. The menus already took the
joiner's controller as player 1's; a match didn't: Start was the host's
only (host_drives_menus), so nobody could pause and quit. stand_in_for_host
(fps_controls.cpp, replacing netplay.cpp's copy in exchange) now makes this
controller player 1's in the menus and while paused, and in a match passes
its Start to player 1 (who otherwise stands still). Tested: host killed
mid-match, the joiner's Escape pauses (the pause list shows on their view),
Quit, YES, back to the level select. Backing out of character and loadout
select after the host was lost worked before this too (tested the host lost
before and on character select). A host that's lost (not quit) is noticed
after kTimeout, 20 s, with "Waiting for player 1..." meanwhile.

Leave Game (the Netplay tab's Session panel): a prompt, then the game quits
(the others are told, as with closing it) and main() starts PaxDino again
with --netplay, which opens the Netplay screen once the launcher is up
(Linux: posix_spawn of /proc/self/exe's path; Windows: CreateProcessW of
the module's path, not tried here). PAXDINO_NETPLAY and PAXDINO_AUTOSTART are
cleared for it. Tested on Linux: the host was told at once, and the new
process came up on the Netplay screen. Also: a gone player's ping isn't shown,
and the Netplay screen no longer says each player has their own save.

Seen, not changed: player 2's B on character select (nothing picked) takes
everyone back to the main menu, as the game does.

## 2026-11-03, round 182: paks held off in netplay; Unlock Everything

The user's call: netplay goes without Controller Paks for now (Load Profiles
turned out more involved: the post-match save prompt is each player's too,
and players leaving would need thought). kControllerPaks (trw_netplay.h) is
false: no pak is read, sent, pulled or put in the session, so every port has
none (the No Controller Pak prompt is answered as before), and the Load
Profiles button isn't shown. The code stays (rounds 180-181) for later.

Unlock Everything, a host button on the Netplay screen (on by default, kept in
online.txt): the session's shared options get All Levels, All Skins, All
Characters, All Weapons and Extra Modes on, whatever the host's Unlocks tab
says. PAXDINO_UNLOCK_ALL=0 turns it off for the PAXDINO_NETPLAY host.
Tested both ways: character select has every character with it on, the
first four with it off.

## 2026-11-03, round 181: Load Profiles is a choice, off by default

The host's Load Profiles button (Netplay screen, host card; kept in
online.txt) goes in the shared settings (load_profiles), so every PC knows
it; load_profiles does nothing with it off. PAXDINO_LOAD_PROFILES=1 turns it
on for the PAXDINO_NETPLAY host. Tested both ways, in sync.

## 2026-11-03, round 180: each player's profile loaded at the session's start

Netplay now loads each player's first profile by itself, once a launch
(load_profiles, src/main/fps_controls.cpp), so a match can add to their
stats. Round 179's note had the save's layout; the note's four 0x190 records
are a pak's four profiles (name, 9 bytes; three bytes made when the note is,
the same for its four; the record's slot), not one player each.

How Load Players works, found by trying it: each controller has a corner.
That player's A opens their pak's list there ("ACCESSING", then the list,
the first profile highlighted), A takes the highlighted one, and its name
shows on the centre list at its cursor (the PLAYER n items' text pointer,
+0x38, goes to the name; the item gets 0x40). Their stick moves it, A puts
it on that row (the name stays, the 0x40 goes), and their list stays open
on the next row, the profile ticked. Their DONE closes it. Another player's
A while a list is open shows WAIT, and their list opens on that DONE; DONE
with nobody waiting closes Load Players, to the Controller Pak screen, and
the loaded profiles stay. A pak whose save doesn't check out says "no Rage
Wars players detected. Create player?" with NO highlighted (NO also leaves
Load Players). A profile already loaded shows greyed (0x100) and the cursor
skips it: two paks copied from one file have the same three bytes in their
records and the game takes them for the same player. A fresh pak got
different ones (34 DA 00 against 4F 01 12), so only copied paks meet that.

So load_profiles steers the reads of whoever's turn it is (the host's for
getting there, each player's own on Load Players), from the game's state
only, so every PC presses the same: Controller Pak, Load Players, then per
player with a Rage Wars note on their pak: A only once their list is open on
a profile it can take (row 368 highlighted, a name, not greyed), the name
moved to their row and put down, the next player's A (once; more picked from
their list as it opened), then DONE; NO on "create player?"; DONE when the
first is greyed; in again if Load Players closed; Back to the main menu at
the end. Each corner's list items have the same text ids (DONE 367, rows
368-371); its create prompt doesn't ((YES, NO): (411, 412), (569, 570),
(661, 662), (345, 344)). Load Players' widget tree is past a thousand
widgets, so those walks go to 8192 there (walk_texts, widescreen.cpp).

Tested with two players here: both loaded (A and Q) and still there when
Load Players is opened again; the host without a profile (player 2's goes on
row 2); player 2's save damaged (skipped, NO, back to the main menu); player
2's first profile the same as player 1's (skipped). In sync in each (sync
test, about 1350 frames), and neither pak file changed. Not tried: three or
four players, and a match's stats saved to each player's profile.

## 2026-11-03, round 179: scoping a loaded profile in netplay (not done)

Netplay matches don't add to a player's profile (stats, medals): the game
saves only to a profile loaded on Load Players (front end screen 19, under
the Controller Pak screen, 27), and in netplay the host drives the menus.
Letting each player's controller through on that screen works (tried, and
taken out again: the user wants no waiting on others, every session), so
the way to do it is loading each player's profile by itself at the session's
start, chosen in the launcher.

Found so far: the save is one note (NRWE51, 7 pages, 1792 bytes): a 16-byte
header, four player slots of 0x190 bytes (the four on Load Players), and a
tail. The game's per-player records at 0x800FCB10 + 0x190 * slot (where the
arena medal counters are, 0x800FCB26) are blank defaults and don't change
when a profile is loaded; loading reads the whole note into the heap
(0x8066FB50 here) and the profile goes elsewhere, not found yet. Next: a
watchpoint on the heap copy of slot 0 while loading, to find the code that
takes it and where it puts it, then the save path after a match.

## 2026-11-03, round 178: pages for the public games

The Join panel showed the first four public games of the up to 20 the Worker
sends. Now four a page with Previous, "Page N of M" and Next under them
(shown only with more than a page; the page kept in range as the list
changes). Buttons rather than a scrolling box, so a controller steps through
them too. Tested with fake games on wrangler dev: page 1 of 4, Next to the
later ones, and the count going down as unpolled ones expired.

## 2026-11-03, round 177: the menu's Netplay tab like the launcher's screen

The Netplay tab (F2) in a session is laid out as the launcher's Netplay
screen now: a Players panel, a line each (nickname and character, this PC's
in the theme's colour) with how they are and their ping under it in smaller
text; a Session panel beside it with the input delay, the settings for
everyone on one line, the chat keys and how to leave. Same text sizes and
panels; they wrap one above the other on a narrow window. Checked at 16:9
and 4:3 in a 2-player match (in sync, 1439 frames).

## 2026-11-03, round 176: chat in the game's menus

The user wants to talk over the map and mode in the menus. trw::chat::Where:
Match (as before), Menus (the front end, Screen::Menu) and Nowhere (paused,
the fly-through, the title and boot screens). In the menus the Chat keys
open it except Enter, which selects there, so by default T. While it's open
the read is nothing pressed, as in a match, so typing doesn't move the
menus; Escape clears, then closes, and is the game's (back) again after.
Names show without a character until a match has shown one.

Test (2 players, main menu): typed "which map? sss www" (the cursor didn't
move), sent; the joiner answered; Escape twice; Enter then selected
Multiplayer. In sync (775 frames).

## 2026-11-03, round 175: the loadout's portraits were gone

The user: no character portraits on the loadout select, on both PCs. Round
163's mirrored rows: the game draws the players' portraits (3D models) in
the rows under the background (190 to 222 of the menus' framebuffer) and
copies them up into the panels; the mirrored copies drawn over those rows
covered them first. Proved by turning the 4:3 strip off (still gone) and
then all of it (back). The rows under the background are now left alone on
the character and loadout selects (front end screens 7 and 8); the main
menu, level select, match options and the Rumble Pak notice still get them.
Checked: portraits on both panels, the level select's preview in place.

Tried after: filling only the bars' rows there, and at the end of the menu's
frame (after the game's drawing). The portraits went again, so the game
reads those rows a frame later: nothing can be drawn in them on these two
screens. Their bottom stays black, as on the N64.

## 2026-11-03, round 174: telling players who's out of date

- Netplay names the versions: Hello and Welcome carry this game's label
  ("0.1.7 (1a2b3c4)": version_string and the commit, cleaned to letters,
  digits, spaces and .()-) after the nickname in reads. A joiner on another
  version is told "The host has PaxDino X; you have Y. Everyone needs the
  same one: whoever has the older one can get the latest at
  github.com/wootbeer/PaxDino/releases"; the host's lobby says a player on X
  tried to join. The Worker stores the host's label (vn) and answers a
  mismatched /join with "version <label>", which the game shows the same way.
- Update check (src/main/update_check.cpp): when the Netplay screen first
  opens, one request to GitHub's latest release of wootbeer/PaxDino; its tag
  compared with version_string number by number; if newer, a line at the top
  of the screen: "A newer PaxDino is out (X; you have Y)..." with the
  releases page. Only the tag is read. Nothing is downloaded.
- version_string is now 0.1.7 (the next release).

Tests: a 0.1.0 joiner and a 0.1.7 host: both told, with both versions; the
0.1.0 build's Netplay screen showed the newer release (0.1.6, GitHub's
latest); the Worker's answer carried the host's label, markup stripped.
Needs the Worker deployed again.

## 2026-11-03, round 173: netplay security review and cleanup

An independent read of the netplay code for what a hostile peer, join code
service or router could do, and the fixes:

- Controller Pak: a note's page chain could name pages past the pak's 128
  (chain() checked only the low end): a crafted save read and wrote past the
  image. Now chain() checks both ends, and valid_image() walks every note's
  chain. (A damaged .mpk in single player benefits too.)
- The host's Welcome and Start `assigned` set this PC's port unchecked: now
  below 4, and below the players in Start.
- A joiner's id is its only credential (the host rebinds its address by it):
  it was passed on in relayed Inputs and listed in Start. Now relayed
  packets carry none, Start names paks by port, a joiner asks the host for a
  port's pak by port number, and a Hello without an id is ignored.
- A joiner's own port must be its own pak (the one its file is written
  from): if Start says otherwise, the join fails instead of writing the
  host's image to the joiner's file.
- Bounded growth: reads are taken only for sweeps not yet played and at most
  256 ahead (no wrap of the drop sweep), hashes only at their 120-sweep marks
  and within 16 checks; the local hashes pruned; pak images capped and
  pruned to those still in the lobby; chat ids rate-limited (10 in 30 s per
  player, on every PC) before they're remembered, the set capped; PakAsks
  answered at most every 20 ms per peer.
- Values from peers made sane: stick and mouse turn finite and in range
  (on every PC alike), the host's field of view in 60-120 and its Loadout
  Restriction a real one.
- Chat is queued from the UI's thread and sent from the game's, which owns
  the players' state.
- STUN: a random transaction id, answers only from the servers asked.
- The join code and token checked before use (six code letters, a short
  alphanumeric token).
- UPnP: SSDP answers only from a private address whose description is on
  that address; the control URL on the same host; no control characters in
  what goes into the request line and headers; the outside address shown
  only if it's an address; replies read for at most 10 s.
- curl (Linux): -g, http(s) only, no control characters in the URL.
- Worker: public games stored one key each, at most 200, three new ones per
  address in 10 minutes; a full game says "full" to a fourth joiner instead
  of pushing one out; a game's outside address must be the caller's (else
  dropped) and its local one private; a build version required.
- poll_lobby split into take_matchmaker_answer, poll_host_lobby and
  poll_joiner_lobby. The file's header no longer says two players.

Left as they are: UI updates from the game's thread (the existing pattern,
recompui's own locks), the session start's netplay.bak replaced each
session.

Tests: 2 players with different paks and nicknames, chat, in sync (1443
frames); the launcher's public list joined; the Worker under wrangler dev
(a made-up outside address dropped, a fourth joiner refused, no version
refused, a fourth listing from one address not listed). Needs the Worker
deployed again.

## 2026-11-03, round 172: telling a small desync from games far apart; a desync check

A desync is still reported once and the session goes on. The user wanted
extreme cases told more plainly, not ended: with each hash (every 120
sweeps, about 4 s) goes the game's place (trw_sync_place: the mode at
0x801407B8 and the arena at 0x8014BD9C, a byte each, in the Packet's spare
word). After a desync, a player whose place differs three checks in a row
(about 12 s) gets a second message, once: the games are far out of step,
quit (F1, then Quit) and start a new session. Nothing is ended. The cost is
two reads every 4 s in a packet already sent. PAXDINO_FAKE_DESYNC=<sweep>
(tests) makes one PC's hash and place read as different from that sweep.

Checked again that nothing a player does on their own PC changes the game:
round 147's audit (window size, renderer and sound settings, inputs) plus
what came since: chat and the menu (F1) send nothing pressed, the paks are
the same on every PC, nicknames and characters are only shown, How Many
Players goes by the game's state. Options not shared are taken when the
session starts, so changing them mid-session does nothing; the field of
view, Look Spring and the aspect ratio come from the session; high
framerate is off in deterministic mode.

Tests (2 players): resizing the host's window twice, F1 open and closed and
a chat message during a match, 1600 frames the same; with the fake desync
from sweep 600 on the joiner, both PCs reported the desync, then 12 s later
that the other's game was elsewhere.

## 2026-11-03, round 171: nicknames

Each player can set a short nickname (Your name, at the top of the Netplay
screen, kept in online.txt; PAXDINO_NICKNAME for tests). It goes in the
joiner's Hello (16 bytes in reads) and the host's Start (StartPaks.names,
every port's), so every PC has every name; trw::netplay::player_name(port)
gives it, or "Player N". Shown in the host's lobby ("Player 2: Bob, ready"),
the Netplay tab ("Bob (Adon): Connected") and the chat ("Bob (Adon): hi").
Cleaned when set and again when received: letters, digits and ' ! ? . , : ( )
_ - only, no spaces, 12 characters at most. Lobby names get the same
characters (spaces kept, 24 at most) in the game, before sending and after
the list comes, as in the Worker.

The pak save doesn't hold names as text (the game's own character codes,
and only for players who made a profile), so names are typed, not read from
it.

Test: 2 players, names "Wootbeer" and "Big<Head>&" + an accented letter + a
control character + more, which came out "BigHeadTooLo"; in the Netplay tab
and the chat; in sync (1423 frames). The screen checked at 4:3.

## 2026-11-03, round 170: public games

Players can find a game without swapping a code. The Host panel has Public
(on or off) and a lobby name; a public host's /host carries public=1 and the
name, and its polls the player count. The Worker keeps the public games in a
second Durable Object, Directory (one for all), and answers /list?v=BUILD
with those on the same build that aren't full, newest first, at most 20:
"CODE<tab>PLAYERS<tab>NAME". An entry goes when the host closes its code
(its game started, or it stopped hosting), when its game object expires, or
30 s after its last refresh (every 10 s at most). The Join panel lists up to
four, asking every 15 s while it's idle (and Refresh); a button joins by its
code. Names: printable ASCII, 24 characters, in the game before sending and
after receiving; the Worker keeps only letters, digits, spaces and
'!?.,:()_- (and RmlUi's set_text escapes markup anyway). The joiner's lobby
doesn't show the host's Start button.

Needs the Worker deployed again (wrangler.toml: the Directory class, migration
v2). Tested with wrangler dev: listed, other builds don't see it, the count
updates, full ones drop out, closing removes it, private ones never show,
markup stripped from a name; in the launcher a public host showed up on the
other's list as "PaxDino game (1/4)" and its button joined (the host's
settings came up to accept).

## 2026-11-03, round 169: the Netplay screen laid out again

The user found it cramped, the player list hard to find. Now: one line on
what netplay is; two panels side by side, Host a game (port, Host, input
delay, a hint) and Join a game (code or address, Join, a hint); a line for
messages (a bad port, a failed join); and, while hosting or joining, a lobby
panel: the join code large, the players a line each (Player 1: you (host),
Player 2: ready / getting ready...), Start and Cancel, and beside them in
smaller text how others reach the host, the delay and the settings on one
line. Text 26 and 22 dp (was 20), large buttons, solid panels. The UI scales
with the window's height, so the layout has to fit 1080 dp of height at any
shape: checked at 16:9 (1280x720) and 4:3 (960x720), room left for four
players; the panels wrap one above the other if a window is too narrow.

## 2026-11-03, round 168: the Controller Pak screen's changes greyed out

The user asked for the Controller Pak screen's options that change a pak to
be greyed out in netplay, so nobody even tries. Greyed (flag 0x100, the way
the trials are): Create Players (text 40), Copy Players (31), Delete Players
(59) and Manage Controller Pak (48). Load Players (57) and Back (36) stay.
The cursor skips the greyed items (Down goes Load Players, Back, Load
Players). Round 167's guard (no more writes to this PC's file after a note
is deleted in a session) stays as a second line.

## 2026-11-03, round 167: Controller Paks in netplay, each player's own

Until now netplay ran with no paks (neither PC's save could make the games
differ). Now every player's own pak (player 1's .mpk on their PC, if the
General tab gives player 1 a Controller Pak) goes to every PC in the lobby,
so all the games boot with the same paks, each on its player's port, as a
real N64 with a pak in each controller: unlocks, skins and trial progress
from the saves (the host's Unlocks settings and cheats still on top), and
2 Player Trials credit both players.

- Lobby: Hello says whether the joiner brings one and its hash (FNV-1a 64).
  The host pulls it in 256-byte chunks (PakAsk / PakData, 16 at a time
  every 50 ms), and counts the joiner ready only once it's all there and
  the hash holds. Start lists each port's pak (owner, hash); a joiner pulls
  the ones it lacks from the host and only then starts its game. 32 KB a
  player: a second or less on a LAN. Nothing is sent during play.
- PAXDINO_NETPLAY's host now waits in start() until everyone is in, so the
  paks are in place before its game boots.
- In the session (trw::pak::begin_session) the paks are in memory only and
  never read from disk again. Each PC writes back only its own player's, to
  its own player 1 file, from what its own game did; the others' are never
  written anywhere. Its file is copied to _pak1.mpk.netplay.bak first.
- Safety: a received pak is only data the emulated game reads; it's checked
  (size, hash, inode table) and the pak code bounds every access to the
  image. Chunks are only taken for a pak this PC expects, from its owner
  (the host) or the host (a joiner). The host drives the menus, so the main
  menu's Controller Pak screen could delete a note on a joiner's pak: if
  the game deletes a note from this PC's pak in a session, the file isn't
  written again that session (the games carry on the same). Greying that
  item (text 60, "CONTROLLER PAK") didn't hold: the game sets that item
  itself. Mods stay off in a session (round 135), and only inputs and pak
  images come from the others.
- Text ids on the main menu, from the game's string table (0x80698160 +
  4 * id): 6 Multiplayer, 24 2 Player Trials, 25 Options, 26 1 Player
  Trials, 38 Player Stats, 60 Controller Pak (8 and 33, Frag Fest and Time
  Trial, are hidden items).

Tests (2 players, PAXDINO_NETPLAY): both with the same pak, "yes yes", to
character select in sync (1731 frames); the joiner with none, "yes no", in
sync (1803), nothing written in its folder; the joiner's pak different from
the host's, in sync (1739), each folder's file unchanged and the joiner's
backup made. Not tried: a save written during a session, the launcher's
flow with join codes, Windows.

## 2026-11-03, round 166: chat on Enter and T

The user meant two keyboard keys, Enter and T (not Y, and nothing on a
controller). The Chat defaults are now Enter and T, with no controller
button (one can still be bound). Profiles saved with the first defaults
(Enter and Y; Y on a controller) get the new ones once
(trw::chat::update_profiles, marker chat_keys_v2); bindings changed by hand
stay. A controller's Y is the game's again in netplay (C Left).

Test (2 players): the saved profile moved to Enter and T, the controller's
cleared; T opened the chat in a match and its letter wasn't typed; in sync
(1271 frames).

## 2026-11-02, round 165: the mirrored rows: the main menu's box, dimmed

The user: the black box at the main menu's bottom left was back (round 6's,
the same place). The main menu's background also stops at row 190, so round
163 mirrored it too, and its hole at the bottom left ((18,128)-(85,201),
filled by a patch the game draws, only above the mirror line) came out black
in the mirrored rows. In the 4:3 area those rows now get copies shifted a
third of the width each way under the mirrored one, as the bars do, so rock
shows through. And as the user asked, the mirrored rows in the 4:3 area are
dimmed like the bars (kBarShade), so they sit behind the game's own picture.

Checked: main menu and loadout select, no box, the rows under the
background dimmed.

The far room's black doorway (round 153's video): the user saw the same in
Original aspect, so it's the game's own.

## 2026-10-29, round 164: Frag Tag's monkey face twice

The user: in Frag Tag the monkey face that flashes while the monkey is
picked still showed twice, top and bottom. It's drawn after the views, in
the full-screen overlay, once inside each view. Now overlay rectangles lying
wholly inside one view (not the clock, already moved; not while paused,
when the pause list is in the middle) are that view's: moved like its HUD on
this player's PC, left out (G_SPNOOP) on the others'.

Test: 2 players, Frag Tag (Extra Modes on for the host), the face once in
the middle of the host's screen while it became the monkey; in sync (1755
frames).

## 2026-10-29, round 163: menu backgrounds that stop short of the bottom

The user: on the loadout screen (Expand) the rock doesn't reach the bottom.
The game draws that background (also the character select's, level
select's, match options' and Rumble Pak notice's) to row 190 of the menus'
222, black under it, in Original aspect too; the bars copy the background's
rows, so they stopped there as well and it showed more. In widescreen the
rows under such a background are now filled with it mirrored about its
bottom edge (the texture rectangles flipped: negative dtdy, starting at
their last row), in the bars (shaded like them) and in the 4:3 area under
the panels, so there's no seam. Backgrounds reaching the bottom are as
before. Original aspect is unchanged.

Checked in the sandbox: loadout, level select, match options and the Rumble
Pak notice reach the bottom; the main menu unchanged.

## 2026-10-29, round 162: characters in the Netplay tab and the chat

The user asked for each player's character next to their name. In a match,
the player's +0x5E0 is the character's number in the game's table of
display names (17 pointers at 0x800D1FB4: TUROK 0, MANTID DRONE 1, ...,
ADON 5, ELITE GUARD 7, ...): found by swapping two players' characters
between two matches and checked with a third pair. Each sweep of a match the
names are read from the game's table (title case: "Lord of the Dead") and
kept per port (trw::controls::character_name), so the menus show the last
match's.

- Netplay tab: "Player 1 (Adon, host): This computer".
- Chat: "Player 2 (Elite Guard): hi", the name as it was when the line came.

Test: 2 players, Adon and Elite Guard, both shown; in sync (1807 frames).

## 2026-10-29, round 161: How Many Players skipped

The user still saw the count flip back and forth. Reproduced: once the
steering pressed Left or Right, the count kept stepping round (3, 2, 1, 3...
every two sweeps) with nothing pressed, so it never settled. With the
steering off the screen behaves (1, 2, stays 2, back to 1).

The screen is now skipped: each sweep on it the count is set in the
screen's object (the word 8 bytes before the digit text, and the copy 12
bytes after it; both move with Left and Right, found by RDRAM diffs) and the
digit, Left and Right are taken out, and A is pressed for one sweep in
eight until the screen goes. Setting only the first word wasn't enough:
character select came up with one player. With both, it has two.

That also explains the earlier "1 bot, can't reach 0" and Frag Tag tests: the
matches were one player. Tests since round 157 that reached a match with the
joiner idle were one-player matches too.

Test: 2 players, character select with both players' panels, in sync (2119
frames).

## 2026-10-29, round 160: chat in matches only

The user wants Enter only in matches, and nothing from the chat in the way
of the settings. The Chat binding now opens the line only on the game's
match screen (not paused, the HUD drawn: the same test as the controls'
Screen::Game), set each sweep (trw::chat::set_allowed). In the front end,
the pause menu and the fly-through the keys are the game's as before (Enter
selects again), and the Chat binding's buttons aren't taken out of the read
there. If the match pauses or ends while the line is open, it closes,
keeping what was typed, and the key that found it closed goes nowhere.

Test (2 players): menus by Enter as before; chat in the match; Escape,
Escape, Escape to the pause menu, Enter there picks Resume. In sync (1987
frames). Text typed on the character select went to the game as keys.

## 2026-10-29, round 159: netplay chat

A chat box at the left of the screen (src/main/chat.cpp), for netplay
sessions only.

- Opened by a new binding, Chat (lib-patches/RecompFrontend/0017 adds it to
  the inputs, so it's in the Controls tab like the others): Enter and Y on
  the keyboard, Y on a controller. Not while the menu (F1) is open, and the
  line closes (keeping what's typed) if the menu opens over it.
- While it's open every key, the mouse and the controller go to it, and this
  PC's read is sent with nothing pressed. The key or button that opened it
  doesn't reach the game either: the N64 buttons bound to the same key or
  button are taken out of the read while it's held (Y is also C Left on a
  controller). The Y that opens it isn't typed.
- Enter sends. Escape clears what's typed, then closes the line; after that
  Escape is the game's (pause) again. On a controller B is Escape and the
  Chat button sends. Backspace, Ctrl+V. Steam's on-screen keyboard is asked
  for when a controller opens it and Steam started the game.
- Messages: printable ASCII only, no line breaks, up to 255 characters, ten
  in any 30 seconds (then a note says how long to wait). Shown as "Player N:
  text" for 12 seconds; while typing the last ten lines show.
- Network: Chat packets carry the text where an Input's reads go. A
  joiner's goes to the host, which shows it and passes it on to the other
  joiners (at most 12 of a player's in 30 seconds); the host's goes to each
  joiner. Each is resent every 250 ms until its ChatAck, and known by its
  player and number so a repeat isn't shown twice.
- In netplay Enter opens the chat everywhere, so in the menus it no longer
  selects (E or a click do; Space or Escape on the title).

Tests: 2 players on this machine. Markup and quotes show as typed, Escape
then Escape, the 10-in-30 limit (the 11th and 12th held back), a reply from
the joiner, typing during a match with W held (the player doesn't move).
Both games stayed in sync (795 and 2027 frames).

## 2026-10-29, round 158: netplay for up to four players

The host now takes up to three joiners. It's a star: each joiner sends its
input to the host, and the host passes it on to the others (resending the
newest it has every send tick, so one lost packet doesn't hold anyone up).

- Lobby: Hello carries a random joiner id, so a joiner that reached the
  host on two addresses (STUN and local) still gets one slot; the host goes
  by the id and keeps the address it last heard from. Welcome gives the
  joiner its port (0 = full). Joiners send Ready every 250 ms; the host's
  Start button is on once one is ready. Start carries the player count, the
  input delay and the final settings (a joiner that took the settings from
  an early Welcome desynced in a 3-player test: Look Spring).
- Ports are packed when the game starts, so players are 1 to N with no gaps.
- Drops: when the host loses a joiner, it marks that port dropped from the
  sweep it had read up to, sends that in every packet, and every game gives
  that port neutral input from there. If the host goes, joiners carry on
  alone as before.
- Auto input delay counts both hops (joiner to host to joiner) and half a
  sweep when there are two or more joiners.
- How Many Players and the connected controllers follow the session's
  count; 2 Player Trials is greyed out with three or four players.
- Own view: the clock seam is found from the lower views' tops, and thin
  view border fills are dropped, for 3 and 4 player layouts.
- Join codes: the Worker keeps up to three joiners and /poll returns a line
  each. Needs `npx wrangler deploy` again.

Tests (this machine, one display): 2 players 2231 frames in sync; 3
players 3975; 4 players 1611, each view right. 3 players through the
launcher with join codes (local Worker): the lobby list, Start, pings 90
and 99 ms, Auto delay 5, in sync after. Not tried over the internet or on
Windows.

## 2026-10-29, round 157: Frag Tag's two views, and How Many Players again

Round 155's "a press that didn't move the count ends the steering" gave up
on the first press: the game ignores presses while the screen comes in. So
every netplay match since was one player (the user's 0 bots, our 1 to 3:
one player can't have none), and the Frag Tag tests of rounds 154 and 156
weren't two-player at all. Now six presses in a row that don't move the
count end it.

With two players, Frag Tag (the user's Monkey Tag) fills the display list
buffer to its end ("[NET] own view: no room left in the display list
buffer (0 bytes free)"), so there was nowhere for the remade projections
and the frame was left as the game drew it: both views. When the buffer's
end is taken, the copies now go over the other views' own commands, which
are jumped over: runs of eight top-level commands in a row in the buffer
(not the first, where the jump is, and never the data the commands point
at). Not enough of them: logged and left as drawn. Tested: two players, 0
bots (the default), the host the monkey: each PC its own view before and
after, no frame left, 2903 frames the same.

## 2026-10-28, round 156: Frag Tag both views (not reproduced); a log for it

User: in Frag Tag, whenever nobody is the monkey (the match's start too),
both PCs show the game's own split, both views stacked; widescreen, default
options. Here, 2-player netplay over 127.0.0.1, Frag Tag on Courtyard (the
Gauntlet starts there too) with 1 and 3 bots (2 players can't have none):
the own view throughout, before and after a monkey, and no frame that looked
split was left as it was. The own view now logs such frames (a few a
session, "[NET] own view: ..."): split viewports not laid out as one view
after another (with each viewport's size, place, depth and projection), a
display list it can't follow, or no room left in the buffer.

## 2026-10-28, round 155: How Many Players kept pressing

User's video: the other player gone, How Many Players stayed on 1 and the
menu's sound repeated. The game doesn't step past the controllers it sees,
so a press it refuses was sent again every 8 sweeps. Now a press that
didn't move the count ends the steering until the screen opens again, and
the target is the session's players (2). Tested here: the host killed at
the main menu, the joiner alone opened How Many Players: one press, the
count stays on 1, nothing more.

## 2026-10-28, round 154: How Many Players in netplay

How Many Players (front end screen 0x1A) is one item whose value Left and
Right step from 1 to 4; the count is in the screen's object, 8 bytes before
the digit its text shows (the screen stack's top entry holds the highlighted
item; its item holds a bar and the text, whose string is at +0x38). In
netplay player 1's read on that screen loses Left and Right and gets a press
toward the session's players (2, or 1 once the other player is gone) every
8 sweeps until the count matches, from the game's state on both PCs. The
screen opens on 1; it's 2 a few reads later and stays there. Tested:
Left and Right after it, still 2.

The user saw both players' views at once in Frag Tag while a new monkey was
picked. Not reproduced: a Frag Tag match here (the host made the monkey at
the start) showed only the player's own view, and in four minutes neither PC
logged a frame the own view gave up on (no split frame left as it was, no
full buffer, no failed walk). Waiting on a capture.

## 2026-10-28, round 153: netplay polish from the user's test

- HUD text cut off at the top by a line (user's TV): the stretched HUD
  sprites landed part way into a pixel, and a texture rectangle starts its
  texture at its top edge, so part of the top row was lost. Stretched
  texture rectangles are now on whole pixels (both edges rounded alike, so a
  sprite's pieces still meet). The "100" has its round tops again.
- The assault rifle's scope stretched tall (user's photo): the sights aren't
  squashed in split screen, and only sprites drawn at exactly 1 by 1 kept
  their size. Now any sprite as wide as high does, and where a sprite's
  commands end is noted at its end (a hook at 0x002AAC24, on the drawn path,
  in func_002AAB58) instead of guessing 0x100 bytes. The scope itself wasn't
  seen here (the test loadouts don't have the rifle).
- Sound: a stall (waiting for the other player) stopped the game's sound
  and the music skipped. The last 8 ms of each chunk are held back; if the
  queue nearly runs dry while netplay waits, they go out fading to silence,
  and the next sound fades in.
- Medals in Solo Matches: off for the session and no longer in the host's
  settings shown to the joiner.
- 1 Player Trials greyed out on the main menu during netplay (flag 0x100 on
  its item and holder, text 26, as Player Stats is without a Controller
  Pak); the highlight skips it.
- The Netplay tab shows the other player's address only to a joiner who
  typed it (not by join code, not to the host).
- The host gone, the joiner couldn't do anything: the menus answer player
  1's controller. Playing on alone as player 2, outside a match, this
  player's input goes to player 1's port too. Tested: host killed at the main
  menu, the joiner moved to Options and opened it.
- 2667 frames the same in a match with all of it.

The user's video of a far room black through a doorway until closer: a
straight-edged black area that moves with the view, as the game's own
culling of rooms seen through doorways. The user has seen it playing alone
too, so it isn't netplay's; possibly widescreen showing more than the game
culls for. Not fixed.

## 2026-10-28, round 152: the join code service is up

The Worker is deployed at https://paxdino-match.wootbeer.workers.dev and is
now kMatchmaker. Checked live: a code, waiting, a join (the host's three
addresses back, the third the address Cloudflare saw with the game's port),
the joiner's on the next poll, a different build refused, close, and the
code gone after. The sandbox's game got a code from it too.

## 2026-10-28, round 151: UPnP

Hosting now also asks the router to forward the UDP port (src/main/upnp.cpp):
an SSDP search on the local network for an InternetGatewayDevice, its
description for the WANIPConnection (or WANPPPConnection) service, then
AddPortMapping for UDP, the same port outside and in, to this PC (a 2 hour
lease, or none for routers that only take mappings that don't expire), and
GetExternalIPAddress to say on the host's screen whether it worked. An
outside address that's private (10/8, 172.16/12, 192.168/16, 100.64/10)
means another router or the internet provider's NAT is in the way, and the
screen says so. Cancel or closing the menu takes the mapping away again. With
a mapping, the join code service's SEEN address (where the request came
from, with the game's port) is the way in, and the address shown for joining
over the internet works without port forwarding.

Tested against a fake router (PAXDINO_UPNP=<description URL> skips the
search): the lease refused, the permanent one taken, the outside address
read, the screen's line, and DeletePortMapping on Cancel. The search itself
needs a real router. Compiles for Windows with MinGW.

## 2026-10-28, round 150: join codes, STUN and hole punching

Joining took the host's address and a forwarded port. Now hosting also gets
a six letter join code from a small Cloudflare Worker (tools/matchmaker,
the free plan: one Durable Object per game, gone after 15 minutes), and the
joiner can type the code in the same box as an address.

The host's game asks a STUN server (Google's, Cloudflare's) on its own UDP
socket what it looks like from the internet, and gives that, its address on
its own network and its build to the Worker (/host). The Worker also notes
the address the request came from with the game's port, for a forwarded
port when STUN fails. The joiner does the same and gets the host's three
(/join); the host asks every second who joined (/poll). Then the host sends
Punch packets and the joiner Hellos to every one of the other's addresses,
every 100 ms; the joiner takes the first to answer with a Welcome as the
host. The code is closed once the session starts or hosting stops. The STUN
request and the Worker's run on a thread of their own (the socket is that
thread's until STUN is done); HTTPS is WinHTTP on Windows and the curl
command elsewhere (src/main/http_get.cpp). On Windows, ICMP port
unreachable from an address that doesn't answer no longer shows up as an
error on the next receive (SIO_UDP_CONNRESET off).

Not testable here: the sandbox has no UDP to the internet, so no real STUN
or NAT. Tested with the Worker under wrangler dev and a fake STUN server
(PAXDINO_STUN, with a padded attribute before XOR-MAPPED-ADDRESS): the code
shown, the joiner trying an unreachable public address first and getting
through on the local one, the prompt, 211 frames the same, the code closed,
and a wrong code saying so. The Windows code compiles with MinGW
(-fsyntax-only). The build has no Worker address yet (kMatchmaker): hosting
then works as before, and a code says join codes aren't set up.

## 2026-10-28, round 149: ping and Auto input delay

The input delay was a fixed 3 controller reads. Now the host picks it in the
Netplay screen: Auto (the default) or 1 to 8 reads, kept in online.txt.
Ping and Pong packets carry a time and come back as they went; the joiner
sends one every 100 ms while it reads the host's settings, and both send one
a second during the game. Auto, worked out by the joiner when it accepts:
half the median round trip, plus the 90th percentile's distance from the
median (jitter), plus 8 ms, in reads of about 33 ms (the game reads the
controllers about 60 times a second in menus and 27 to 45 in a sandbox
match), from 1 to 8; 3 if no Pong came back. The Welcome carries the host's
choice (0 Auto) and the Ready the delay settled on, which the host takes
before the session starts. PAXDINO_NETDELAY's host sends its own as fixed.
The joiner's prompt says which it is; the menu's Netplay tab shows the ping
and the delay. Every 1800 reads the log has the game's pace and the ping, to
check the 33 ms on real PCs.

Round trips include waiting for the next frame on the other side (the
launcher's frame in the lobby, the next read in the game), so the sandbox's
slow renderer shows 20 to 60 ms over 127.0.0.1. Tested through the launcher
on two displays: Auto picked 2 at a 55 ms round trip, the host's fixed 2
was used as such, and 715 frames were the same.

## 2026-10-28, round 148: own view drawn as one player's

The user's screenshots of their own view: the clock cut off at the top, the
gun big, the weapon wheel's icons pulled apart from their labels, the
shotgun's sights stretched.

The camera (func_00234FCC) picks its vertical angle as atan(h / aspect),
the aspect 4:3 alone and 8:3 for a stacked view (0x800C319C-31A4), so a
stacked view gets 60.8 degrees to one player's 74.4. Own view kept that
projection's x scale and halved its y: a very wide picture, the gun's hidden
lower half shown. Each projection in the view whose shape is the view's is
now remade as one player's: the game's angle (the Field of View setting
undone), atan(tan(angle) * 2), the setting again. The world's projection
comes with the camera's turn in it, so its x and y scales are its columns'
lengths. Other projections (the 47.5 degree one for the HUD's 3D skull) are
left alone, which is how one player's are drawn.

The HUD: in split screen the game draws its sprites (func_002AAB58) with y
scale times 0.496, so they're squashed as well as placed closer; only the
crosshair (751) stays 1 by 1. Pieces were moved by their centres, their
sizes kept, so the wheel's spacing doubled while its pieces stayed half
height. Now the HUD's rectangles are stretched with the view, their texture
steps (the G_RDPHALF_2 after each G_TEXRECT) divided to match, and the
sprite call notes where each sprite's commands start (kDlHead) and whether
it was 1 by 1: the crosshair's rectangle keeps its size and its distance
from the middle. The clock between the views is stretched too and placed as
one player's (its digits 20 lines down). Compared with one player's frame
the wheel, health, frags and clock land within a few lines.

Tested over 127.0.0.1 in three levels, wheel open on both: 3943 frames the
same. Not seen: the shotgun's sights (the test match's loadout didn't
change weapons); either way they're drawn as one player's, stretched if the
game squashed them, left as they are if it didn't.

## 2026-10-28, round 147: settings audit for netplay; the audio target leak

The games parted when the players' aspect ratios differed (frame 5), and
also, from one run to the next at different frames, when a window was
resized mid-session or the renderers ran at different speeds. The first
word to differ was 0x800CB610, the audio thread's (0x00256C30) count of
audio frames made: round 145's growing audio target was also the game's
FIFO target in trw_osAiGetStatus, and it grows when the real device runs
dry, so each PC's game made a different number of audio frames. In
deterministic mode the target is two retraces again; the cushion of silence
still follows the growing value, which only touches what's queued to the
device. With it, three resizes mid-session: 2867 frames the same.

Every setting checked for what it touches:
- The renderer only (each player's own): bloom, screen filters, resolution,
  MSAA, framerate, HUD ratio, volumes.
- Input, before it's sent (each player's own): bindings, mouse, invert,
  the menu cursor, look spring (per port).
- The game's memory, so the host's: unlocks, cheats, the Enhancements game
  options, the field of view, and now the aspect ratio: the widescreen
  fixes add to the game's display lists and move its pointer into them
  (kDlHead). ultramodern::renderer::set_aspect_ratio_override
  (lib-patches/N64ModernRuntime/0006) holds the runtime's to the host's for
  the session whatever the settings send, without changing them; the
  joiner sees it in the host's settings.
- The game's memory, fixed for the session: Clear Menu Panels on (the
  user's call), Custom Music and Weapon Reverb off (reverb is in the
  game's own synth: alSndpSetFXMix), the game's own crosshair.
The shared and fixed options still shown (Aspect Ratio, Field of View,
Clear Menu Panels, Weapon Reverb) are greyed out during a session.
Tested over 127.0.0.1, host widescreen and joiner Original, Clear Menu
Panels and Weapon Reverb different, main volume 100 and 40, both firing:
2880 frames the same (and 2699, 2655 for the aspect ratio alone).

## 2026-10-27, round 146: netplay pacing bug; session menu; mods off; build check

- The game ran too fast in netplay (seen once the other player quit): the
  retrace pacing (deterministic_vi_tick, lib-patches/N64ModernRuntime/0004)
  computed `due` with the retrace count's unsigned type in it, so a retrace
  not yet due came out far behind, was re-anchored to now and not slept
  for. Paced only by the wait for the other player's input; alone, about
  100 retraces a second. Now signed: 60 a second alone, measured. Probably
  also behind the netplay crackle (audio arriving fast, dropped at the soft
  cap).
- During a session the menu's Enhancements, Unlocks, Cheats and Mods tabs
  are hidden and a Netplay tab (F2) lists the players: this PC, the other's
  address and Connected / Connection lost / Left the game, and the input
  delay (trw::online_menu::use_session_menu, create_netplay_tab).
- Mods are all off for the session, without touching mods.json
  (recomp::mods::disable_all_mods_unsaved, lib-patches/N64ModernRuntime/0005),
  just before the game starts.
- The netplay version check includes the commit (TRW_BUILD_ID, from git at
  configure time), so two test builds of the same version don't play.
- The Modern preset's screen filter is Scanlines (was CRT): the CRT filter's
  glow made the menus' bright text bloom, more on a TV.

## 2026-10-27, round 145: audio buffer that grows when it runs dry

The mini PC crackled in heavy scenes. The game is told the AI FIFO is full
once two retraces of audio are queued (trw_osAiGetStatus, src/main/main.cpp),
about 33 ms: a frame held up longer than that (the renderer behind, on a
slower GPU) empties the queue. Now the target starts at two retraces and
goes up by one each time the queue is found empty while the game is still
making sound (a chunk within 250 ms of the last), to six (100 ms), and down
by one after each minute without running dry; the headroom the game is
given in its remaining-audio count follows it. In this sandbox (slow,
software rendering) it went to six during boot; a fast PC stays at two.
In deterministic mode (netplay) the game's audio is paced by its own
retraces, kept in step with the other player, not by the device, so it
reaches the queue unevenly and crackled on both PCs even on the title
screen: there, a queue found under a retrace's worth is topped up with
silence to max(4, target) retraces before the chunk. Two copies over
127.0.0.1 in this sandbox: the queue found low in 6 of each 600 chunks,
16 to 55 without the top-up.

## 2026-10-27, round 144: Windows test PC: debug runtime, DPI, a DirectInput pad

On a second Windows PC (a mini PC on a 4K TV):
- The packaged exe wanted MSVCP140D.dll: build-release had been configured
  as Debug (fmtd.lib, every object /MDd). tools/package-windows.ps1 now
  starts a folder of another build type over, checks the type after
  configuring, and refuses an exe that names a debug runtime.
- Settings and the launcher were bigger than the screen and cropped: the
  process wasn't DPI-aware, and at 350% Windows took the screen for about
  1097 x 617 while the window was 1280 x 720. Now per-monitor DPI aware
  (SDL_HINT_WINDOWS_DPI_AWARENESS), and the first window is two thirds of
  the screen's height.
- An 8BitDo Ultimate 2C on its dongle in DirectInput mode ("6 axis 16
  button gamepad with hat switch", 030027fac82d00001b30...) wasn't used:
  only pads SDL knows are. assets/gamecontrollerdb.txt (SDL_GameController
  DB) now comes with the game, given to SDL before it starts
  (SDL_HINT_GAMECONTROLLERCONFIG_FILE): added after SDL_Init, the pad was
  known but had already been announced, and was never opened. The log names
  each joystick and whether it's used. Works on that PC.
- Defender removed the new unsigned exe until allowed; reporting builds to
  Microsoft as false positives is the way for releases.

## 2026-10-27, round 143: launcher on a TV

On a TV the launcher looked crowded (a TV may crop the picture's edges) and
the grain much stronger. The logo is 26% of the window's height (was 30%),
centred at 19%; the menu's entries have 11 above and below (the frontend's
16) and the menu sits 17% from the bottom (was 14%), so Start Game is at
about half the height and Exit at about 80%. The grain is at 0.06 (was
0.10) and the scanlines 0.32 dark (was 0.38), the same on every screen.

## 2026-10-27, round 142: Netplay in the menu; the game's crosshair online

The launcher's entry is Netplay (was Online Play), and Controls and Mods are
gone from the menu: both only opened Settings at their tabs, which are still
there. With two entries fewer the menu is back at 14% from the bottom.

The crosshair is the game's own for both players during a session, whatever
either PC's style (it was the host's): any other style skips the game's
drawing of it, which has to match on both, and the host's Image style would
have shown each player a picture of their own choosing, or one the host
couldn't vouch for. The crosshair style is no longer in the shared settings.
Tested over 127.0.0.1 with the host on Dot: the game's crosshair on both
screens, 2759 frames the same.

## 2026-10-27, round 141: netplay messages on screen

A line over the game (src/main/online_menu.cpp, its own UI context, no
input), from src/main/netplay.cpp:
- "Waiting for player 2..." once the game has waited a second for the other
  player's input, gone when it comes. The game sends no frames while it
  waits, and RT64 presents only when the VI or the framebuffer changes, so
  the screen is shown again every 100 ms: ultramodern::deterministic_present
  (lib-patches/N64ModernRuntime/0004) queues a screen update, which the
  renderer context (src/main/main.cpp) turns into
  RT64Context::update_screen_held (lib-patches/RecompFrontend/0016): RT64
  repeats its last workload and present while its debugger pauses, paused
  for that call only, so no display list is skipped.
- "Lost the connection to player 2. You're playing on without them." after
  20 s without their input (10 s on screen).
- "Player 2 left the game." when they quit: the game's quit (main.cpp,
  before its _Exit, which runs no atexit handlers) sends Leave three times
  (trw::netplay::shutdown); before, the other player waited out the 20 s.
- The desync line for 20 s when the hashes differ.

The retrace pacing (deterministic_vi_tick) catches up no more than 100 ms:
after a wait it carries on from the time it is, where it used to run the
missed retraces as fast as it could.

Tested over 127.0.0.1: the joiner stopped (SIGSTOP) for 3 s, the message
up and gone after, the games the same afterwards (2647 and 2791 frames);
for 23 s, the lost line; a byte poked into the host's RDRAM, the desync
line on both; the joiner's window closed and Quit, the left line on the
host at once. Patches checked on a reset submodule (configure reapplies
the set).

## 2026-10-27, round 140: netplay crosshairs

A crosshair style other than Game in a split screen: the crosshair
overlay (src/main/crosshair.cpp) kept only the last batch of crosshairs the
game drew between two colour buffers, and a split screen's come in
different batches, so one player's was lost. Now each is kept (one within
40 pixels of where it was is the same one) until 6 calls in a row go by
without it. In netplay only this player's is drawn, moved as their view is
(trw_own_view_place, src/game/own_view.cpp, from the last frame's view).

The HUD's pieces in netplay's own view now move by their centre, not their
top left (size kept): the game's crosshair at the view's centre was about 9
pixels above the screen's centre; now on it, with the Dot style's too.
Tested over 127.0.0.1: the host's Dot style and the Game style, both
players' crosshairs on their screens' centres; 2659, 2723 and 2752 frames
the same on both.

## 2026-10-27, round 139: Online Play in the launcher; settings that split the games

Online Play, after Start Game (src/main/online_menu.cpp): a UDP port and
Host, the host's address (IP, or IP:port) and Join, a status line, Cancel.
The host waits and is told its address on its own network (the one a UDP
socket would reach the internet from) and, over the internet, to give its
public address with the port forwarded; its settings for the session are
listed. The joiner says Hello every 100 ms; the host answers with its
settings (Welcome) and the joiner sees them in a prompt, Join or Cancel.
Join sends Ready and starts the game the next launcher frame (from inside
the prompt's own callback the UI contexts weren't hidden and the game
didn't run); the host starts on Ready, or on the joiner's first input.
Cancel sends Leave and the host goes back to waiting. No answer in 30 s, or
a different build (the host answers a Hello it won't take with its own
version), and the joiner is told why. The port and address are kept in
online.txt in the config folder. PAXDINO_NETPLAY still works, for testing;
deterministic mode is now set when a session begins (netplay.cpp), not at
launch.

The launcher menu has one more entry, which pushed Start Game under the
logo's box (30% of the window's height, from 5% to 35%): the logo no longer
takes clicks, and the menu sits at 10% from the bottom instead of 14%.

Settings that split the games, found by giving the joiner's copy different
ones and comparing the runs:
- Clear Menu Panels: its hook skips the game's drawing of the panels'
  lines (a menu object the game builds otherwise): now the host's.
- Field of View: it's in the game's camera (the projection at 0x8013F1E8
  and its fixed-point copy): now the host's.
- Crosshair style: any but Game skips the game's crosshair sprite, whose
  texture is then never loaded (a 6 KB block missing from the heap at the
  match's start): the style is the host's; colour, size and opacity stay
  each player's.
- Custom Music: it gives the game's music player volume 0 once the file has
  loaded (in real time): off for the session.
- Randomize Game Music picked with std::random_device: in deterministic
  mode its generator has a fixed seed, so both pick the same songs.
- The host's settings were taken before the config loaded when started by
  PAXDINO_NETPLAY (every option off), parting the games at the main menu;
  that host now takes them when the joiner says Hello, as before.
Bloom, resolution, screen filter and look spring differing: no difference.

Tested over 127.0.0.1 with each copy on its own X display: Host, Join, the
settings prompt, a two-player match; 2883, 2299 and 2819 frames the same on
both, with the joiner's field of view, crosshair, bloom and resolution
different, and with Randomize Game Music on. Cancel at the prompt and a
join with nobody hosting give the messages above.

This sandbox's software renderer (llvmpipe) can stall a copy's graphics
thread when two copies start their games at once with the CRT filter at 2x
resolution (in RT64's dummy frame before the game; it happens without
netplay too), so the tests run the copies on separate displays without it.

## 2026-10-27, round 138: netplay views paused, widescreen, the layouts

Widescreen (Expand): each PC's view fills the wider screen, the HUD in
place, as in round 137. Paused: the pause list is drawn in the overlay,
around the middle of the screen, so the overlay's rectangles near the seam
(the clock) aren't moved up while paused (0x801407D4).

The 2 Player Layout option (Options): its byte is 0x80140227 (0 Horizontal,
1 Vertical; found toggling it with RDRAM dumps at the main menu before and
after). Vertical cut out of a side-by-side frame showed the right third of
the world culled and the HUD's digits spread apart. Since each PC shows one
view full screen, the layout only changes how that view is cut out, so
netplay keeps the byte 0 at every controller sweep, on both PCs
(keep_horizontal_layout, src/main/fps_controls.cpp). Tested: the byte set
to 1 on both before a two-player match, both views came out as Horizontal,
no difference reported.

## 2026-10-27, round 137: netplay, each PC its own player's view

src/game/own_view.cpp, from the renderer's send (src/main/main.cpp) in
netplay. A split-screen frame, as the game builds it: at the display list's
top level, per player a projection load and a viewport over its part of the
screen (two players stacked: scale 240,89, translate 240,89 and 240,267 at
480x360, scissors rows 0-178 and 178-357), then the world, the weapon and
that player's HUD; after the last view a projection and the full-screen
viewport for what goes over everything (the match clock, on the seam).

The other player's view: its projection load becomes a branch to the next
view's (all at the top level, so nothing on the call stack is skipped).
This PC's: its viewport command points at the game's full-screen one, its
scissor is opened to the screen, each projection it loads is copied (to
the last 0x400 bytes of the display list buffer, unused) with the x and y
columns scaled by the view's size over the screen's (the y column by 89/180
stacked: the game's projections include the camera, scales 0.639 and 1.704,
8:3), and its HUD's rectangles move to the same place on the screen,
keeping their size (a rectangle over the whole view, the colour and depth
clears, covers the screen). The overlay's rectangles within 12 pixels of
the seam (the clock) move to the top. Only the display list changes, never
the game's memory.

Tested: a two-player Gauntlet over 127.0.0.1, the host's window player 1's
view and the joiner's player 2's, full screen and the right shape, the HUD
in place, the clock at the top; 3651 frames the same on both. Not tried
yet: the side-by-side layout, widescreen, more than two players.

## 2026-10-27, round 136: a two-player netplay match

The game offers as many players as controllers it finds (osContInit's
device info): in netplay both PCs report a controller on ports 1 and 2 and
no pak (src/main/main.cpp), and get_accessory answers None
(src/main/game_config.cpp), so neither PC's Controller Pak save makes the
games differ; they start the same, the host's unlock settings standing in
for a save. The game then asks at boot (No Controller Pak Found): the host
picks Continue Without Saving (A). Since then answered by itself
(answer_pak_prompt, src/main/fps_controls.cpp): while the front end's
screen is 0x1D in mode 0, player 1's read becomes Down, then A, every 24
sweeps, counted from the game's state so both PCs press alike; the prompt
went by at frame 40.

Tested over 127.0.0.1: the host set How Many Players to 2; each picked a
character and a loadout on their own panel; the joiner's presses on the
level select did nothing; the host started a Gauntlet match, and both
players moved in the split screen. 4999 frames the same on both (881 in
the match), no difference reported.

## 2026-10-27, round 135: netplay settings, per-player controls

The settings that change what the game does are the host's for the session
(src/main/shared_settings.cpp): the joiner says Hello before its game
starts, the host answers (Welcome) with them, and the joiner uses them for
the session without saving (its own stay as they are); the list goes to the
log, for the join screen later. Shared: All Levels, All Characters, All
Weapons, Extra Modes, All Skins, Unlimited Ammo, Infinite Health, Moon
Jump, Rapid Fire and Semi-Auto, Infinite Time, Medals in Solo Matches, All
Music, Demo, Loadout Restrictions. Picture and sound options stay each
player's, and so do the controls: bindings, sensitivity and invert apply
before the input is sent, and Look Spring Override is each player's own on
their port (the joiner's goes in its Hello; both PCs use the same set). The
game's own 30 frames a second in the deterministic mode (no 60 fps logic),
and packets carry a hash of the version, so different builds don't play.

The sync hashes now leave out what the renderer writes back on its own time
(the framebuffers at 0x80500000 and the depth buffer after them, the area at
0x80700000 drawn before the first frame) and the display list buffers,
which carry each player's picture options: one run showed only the depth
buffer differing for a few frames in a match.

Tested: the joiner with every cheat on, Energy weapons and its own look
spring, the host with none and Bullet: 1903 and 1871 frames the same.

Each PC's input reaches only its own player: in netplay only this PC's
player 1 controls are read (its other players' bindings and devices not at
all), for its own port; from the network only the other player's port is
taken, and once connected only packets from the other player's address and
port count (input only after the Hello).

The host drives the menus (host_drives_menus, src/main/fps_controls.cpp):
every other player's read is filtered from the game's state, the same on
both PCs. In the front end it counts only on character select and loadout
select (each picks their own); in a match all but Start (only the host
pauses); nothing while paused. The front end's screen is func_00299898's
answer (*0x80144E18, or the top of the screen stack at *0x80144E00): 3 the
main menu, 0x1A how many players, 7 character select, 8 loadout select,
0x14 level select, 0x15 the match's options, 0x19 the Rumble Pak notice.
Tested: the joiner's Return, Down and Return left both games on the title;
the host's two Returns took both to How Many Players; 1007 frames the same.

## 2026-10-26, round 134: netplay, first version

src/main/netplay.cpp. PAXDINO_NETPLAY=host:<udp port> (player 1) or
join:<address>:<udp port> (player 2) runs the game in the runtime's
deterministic mode. At each sweep of the game's controller reads (at port
0's call), this PC's player's read (its own controls, as player 1's, the
controls' extras included) is sent for the sweep PAXDINO_NETDELAY on (3 by
default) and the other player's for this sweep is waited for (log after
0.5 s; after 20 s without it the game carries on without them). Packets
repeat the last 16 sweeps (no acknowledgements needed). Every 120 sweeps
both send a hash of RDRAM (trw_sync_rdram_hash, the sync test's, after the
running tasks finish); a difference is logged with its sweep.

For two mice the controls' mouse aim and weapon pick state is per port; a
boot screen skip is made as the read reaches the game (apply_read_extras);
the paused wheel hold runs once a sweep on both PCs.

Tested on one machine (host and joiner over 127.0.0.1, each writing a sync
file): boot to the title, 2907 frames the same; the host keying into a
match and moving, jumping and turning with the mouse, 1847 frames (765 in
the match) the same on both.

Not yet: the joiner's player in a match (a two-player setup), each PC
showing its own player's view, settings sync at connect (options that
change the game must match), a Host/Join screen, NAT.

## 2026-10-26, round 133: input record and replay

src/main/input_record.cpp: PAXDINO_INPUTREC=<file> writes what each
controller read hands the game (port, answered, buttons, stick) and
PAXDINO_INPUTPLAY=<file> plays it back instead of the live input. With the
sync test on, a run that keys through the menus into a match and moves,
jumps and turns, and its replay, hash the same every frame (2355 frames;
frame 0's framebuffer aside, the renderer's first clear).

What the controls decide outside the buttons and stick goes with the read
in the deterministic mode (trw::inputrec::Extras): the mouse's turn, taken
at the read in radians (sensitivity and invert applied) and applied per
port at the next frame's camera, instead of the pending motion at the
camera; whether the mouse aims the port's player (trw_look_pitch_target);
a weapon pick in progress (trw_wheel_hidden); a boot screen skipped. A
replay sets them from the record. The wheel held through the pause runs at
the same point in both. Still local and not in the stream: options that
change the game (Look Spring Override, cheats, Loadout Restrictions,
medals...) have to be the same on every machine.

## 2026-10-26, round 132: a deterministic runtime (netplay groundwork)

Lockstep netplay needs two PCs given the same input to run the game the
same way. Sync test (src/game/sync_test.cpp, PAXDINO_SYNCTEST=<file>):
RDRAM hashed in 64 KB chunks each frame, the frame step fixed at two
retraces; PAXDINO_SYNCDUMP=<frames> saves whole RDRAM there;
tools/sync_compare.py finds the first frame and chunks that differ. Test:
boot with no input into the attract demo, twice.

What differed, in order:
- Screens timed on osGetTime/osGetCount (18 and 2 calls): half the boots
  were at another screen (320x240 against 480x360 buffers) by frame 10.
- Events from the runtime's own threads (retraces, RSP and display list
  completions, controller reads) were handed to whichever game OS call
  came next, so the game's threads interleaved by host timing.
- Audio: the runtime's remaining-bytes count and our trw_osAiGetStatus
  (src/main/main.cpp) read the audio device, which drains in real time; the
  audio thread made more or fewer frames (its counter, 0x800CB610).
- Hashes taken while an RSP task or display list ran on another thread.
- The runtime's host pointer in each OSThread (+0x18 past the game's
  fields), different every run: masked (the game's nine threads).

lib-patches/N64ModernRuntime/0004-deterministic-mode.patch:
ultramodern::set_deterministic(true) (src/main/main.cpp, with the sync
test). External events reach the game only at its idle point (all game
threads waiting: run_next_thread, pause_self); there the RSP tasks and
display lists still running are waited for (no game time), what was raised
is handed over in a fixed order (PI, SI, SP, DP, AI, VI), and with nothing
raised the game's time moves on one retrace (VI and AI messages, the VI
swap and screen update), paced to the host's clock but never skipped. The
CPU counter is retraces x 781250; audio queued drains a retrace's worth per
retrace. The VI thread stands down once the game has started. The sync
test waits for running tasks before hashing.

Result: two runs, 4951 frames (boot, front end, the attract demo's match),
the same every frame.

## 2026-10-26, round 131: mouse look's kept player

Issue 7 on 0.1.5: the same crash as the reporter's 0.1.3 one, the allocator
(func_002559D4, 0xF00 bytes for func_0044A4E0 as the next match loads)
reading a free block's +0x10/+0x14 as a small positive float (about 0.0035,
0.007 in the earlier log). A float written into freed memory. Mouse look
keeps its player from the last controller read (look_player) and turns it
when the camera is built (camera_begin), with no check that it's still in
the match, so after quitting it could write the old player's yaw and pitch
(+0x6C, +0x724; pitch near 0 looking level) while the next match loaded.
camera_begin now drops the turn unless the match is on and the player is in
the list (trw_player_live). Not reproduced here: 8 restarts with the mouse
moving through play and each quit, no crash, and the check never had a
stale turn to drop. Mouse look still turns in play.

## 2026-10-23, round 130: our own writes to players already gone

Issue 7 on 0.1.4: a crash in the free (func_00255ACC) in play, no "match
list emptied" in the log, so round 129 only covered one symptom. Tracing the
broken list here: its 6th element's back link held 0x00FF00FF, and a
watchpoint caught the writer, trw_apply_weapon_rules (Loadout Restrictions,
on in this sandbox as Bullet) writing a player's loadout bytes (0 and 0xFF
pairs). The player manager at 0x8013EF80 keeps the match's players in a list
at +0x20 (head; tail +0x24; count +0x30; linked through +0x16E0), but its +4
(0x8013EF84, the first player, which everything here used) is left as it was
when a match is quit. While the next match loads, that memory goes to other
things, and the player's +0x5D0 can still read nonzero. Everything that
writes to players through it does so then: Loadout Restrictions, the cheats
(health, moon jump, rapid fire), the look spring override, the mouse's turn,
and the weapon wheel held while paused (0x380 bytes written back each frame,
and quitting or restarting is done from the pause list, so that one is the
likely cause with nothing turned on).

trw_player_live (src/game/extras.cpp): a player is written only if it's in
the list at 0x8013EFA0. player_on_port (src/main/fps_controls.cpp), the
cheats, the look spring and Loadout Restrictions check it. Round 129's check
stays as a guard.

## 2026-10-22, round 129: a stale list at match setup

Issue 7 again (crash a few matches after quitting and restarting). In the
sandbox it reproduced on the first or second restart as func_00255E78
(list unlink) reading a wild address, called from func_0044C100 (match
setup). Setup frees the elements of the list at s2+0x1B500 (0x801352C8:
head, tail, prev offset, next offset, count) after func_0028D4EC; on the
crashing runs the list's head block had already been freed and reused, so
its links were garbage, and the walk either faulted or freed the block a
second time, corrupting the heap (the reporter's crash in the allocator,
func_002559D4, matches that). The PI change from round 128's chase
(osSendMesg for the PI event) did not change it.

trw_check_match_list (src/game/extras.cpp), hooked before 0x0044C220,
walks the list first: every element must be RDRAM, each back link must
match, the walk must end at the tail. If not, the memory is already gone,
so the list is emptied (head, tail, count) and setup skips the walk and
the second free. A whole list is left alone. The free after the walk
(func_00254778, which reads the block header at -0x10) is then given 0 and
crashed reading 0xFFFFFFF0 in the first test, so it now returns on 0. It logs
"[TRW] match list ... emptied" when it fires.

## 2026-10-22, round 128: the console's hang check

Chasing issue 7 (crashes quitting and restarting matches) in the sandbox,
one crash was func_0028E9DC writing N64 address 0 (reported as reading
0x3): the scheduler's retrace handler counts 0x800CD610 up each retrace,
the graphics task done handler resets it (0x0028EE38), and at 0x4B0 (1200
retraces, 20 seconds without a finished frame) it stores to address 0 on
purpose (0x0028EBF0, sb $zero, 0($zero)). Here gdb and the software
renderer stall that long; on a PC a long stall, a minimized window or a
debugger can too. The store is now a nop (turokrw.us.toml). The same crash
came earlier from keys pressed while the game was still starting.

## 2026-10-21, round 127: the screen wipe at 60

Reported (a website comment): the red transition at the end of a single
player stage is far too fast. The type info has CWipeScreen_t (+0x00 id,
+0x04/+0x08/+0x0C colour, +0x10 step, +0x14 count, +0x18 state) and
WIPE_MAX_FRAMES. func_0043C458 starts a wipe (step 1, count 0, state 1)
and func_0043C484 adds the step each frame, done at 13: 13 frames of 30 a
second. A trial ends while the match still runs at 60. Hook at
func_0043C484's start: while trw_frames_scale() is 2, a wipe moves on every
other call (kept per wipe, src/game/extras.cpp trw_wipe_tick). Checked:
won Proving Grounds, the wipe at 0x8066CE60 took 26 calls, each count
twice.

## 2026-10-20, round 126: timers counted in frames at 60

Issue 11: Bastille's shield (Cathedral) came back far sooner at high
framerates, over twice as long at Original. The matches at 60 (round 103)
run the game's logic twice as often, which the frame step covers, but not
timers the game counts in frames. The ROM keeps the game's type info
(member names and offsets, around 0x132FB8): CGameStatus_t, at 0x801407E0
(0x80140888 being m_bScenarioSuccess, the trial won flag from round 123,
checks it), has m_bIsBossLevel +0x98, m_nTalsetLives +0x9C,
m_nBastilleShield +0xA0 and m_nBastilleTimer +0xA4. A hit on character 0xB
(+0x80 of its character data) takes the shield down (func_0021BFBC); when
it's gone the timer is set to 75 frames (easy) or 60, and func_00208158
counts it down by one a frame (0x00208330) before the shield is back at
100. Scanned the struct's fields for other one-a-frame counts: Frag Tag's
flip timer (m_FragTagFlipTimer +0x68, set to 225 in func_00227014 and
counted down there) is the other one. trw_frames_scale()
(src/game/extras.cpp) is 2 while matches run at 60, and both timers are
scaled by it as they're set. The logic never runs above 60 (one retrace a
frame), so higher refresh rates are covered. Checked: 2 in a match at 60,
the hooks in the generated code after the timer's label (every difficulty's
value goes through it). Not played to Bastille here.

The Linux crash handler now writes one report when several threads fault
at once (seen in the sandbox: llvmpipe's threads), like the Windows one.

## 2026-10-20, round 125: crash logs for crashes outside game code

Issue 7, a second report (Windows 11, RTX 5070 Ti): an access violation in
VCRUNTIME140.dll (memcpy) writing host address 0x20, not in game code, no
game functions on the stack. Not enough to say where: the log only named
recompiled functions. crash_handler.cpp now also writes the faulting
thread's name (the runtime and RT64 name theirs: "Gfx Thread", "VI Thread",
RT64's queues) and its host call stack, each frame as module+offset:
Windows unwinds from the fault with the x64 unwind data
(RtlLookupFunctionEntry / RtlVirtualUnwind, there with or without symbols),
Linux takes backtrace() from the faulting frame on. Windows Release builds
now write line tables to PaxDino.pdb (/Z7 with -gline-tables-only;
clang-cl has no /Zd; not RecompiledFuncs, -g0), with
/OPT:REF kept and /PDBALTPATH so only the file name goes into the exe;
tools/package-windows.ps1 keeps the .pdb in dist\ beside the zip, not in
it (it has the build machine's paths), for resolving a release's offsets.
Checked on Linux (SIGSEGV sent to the game): the log shows the thread and
eight frames, and addr2line resolves the exe's offsets (main,
recomp::start, ultramodern::sleep_milliseconds). The Windows path is not
built here.

## 2026-10-18, round 124: trial medals under Medals in Solo Matches

Of func_004245C0's medals, 0x00-0x23 are the arena medals (two wins counted
by func_00424E30, not run in trials) and 0x24-0x2C are judged from the
match's own stats (several tied to its frag limit or time, which trials
don't set per trial), so trials gain at most those nine. Too few for an
option of their own: Medals in Trials (option 17) is gone and its two hooks
go with Medals in Solo Matches (option 11), described as bot matches and
trials. Checked again: a trial won with only Medals in Solo Matches on gave
medal 5 (counter set to 2), then the trial's own bits.

## 2026-10-18, round 123: medals in trials; arena medals in solo matches

Checked Medals in Solo Matches (round 59): its hook in func_00426454 (one
human who fired 76 shots or more counts as two) is in the generated code
where it should be. But the arena medals have their own check: at a
multiplayer match's end (func_0042BD40, mode 0 at 0x80140215),
func_00424E30 counts a win for the winner on the arena (0x8014BD9C) in the
counters at 0x800FCB26 (0x190 a player, capped at 5), only with a frag or
time limit of 10 or more and two or more humans who fired 76 shots, its own
count (t1). func_004245C0's first loop gives an arena's medal (bits at
0x800FCB4A) at two. So solo, those never counted. Hook at 0x00424F2C: one
will do with the option on. Checked with gdb in a bot match (shot count set
to 100, func_00424E30 called for slot 0 on arena 18): the counter goes 0 to
1 with the option on, stays 0 with it off.

Medals in Trials (Enhancements, option 17; folded into Medals in Solo
Matches in round 124). A trial (modes 1 to 4 at
0x80140215, its tier) never reaches the match medals: a single trial won
runs func_00425EF4 from its results (func_00437C30, three places), which
sets the trial's done bit; co-op trials (func_0042BD40, mode 2) run
func_0042D3A8 for the trial medals, then func_00425EF4 for each human. A
failed trial runs neither. Hook at func_00425EF4's start: with the option on,
the player also gets func_004245C0 (a0 the slot, a1 a scratch on the stack
for the two medal ids it reports), and a second hook makes its first check
(func_00426454: limits, two humans) pass in a trial. Arena wins aren't
counted in trials (0x8014BD9C is the trial's number there, not an arena),
so the medals from that match's own stats are the ones earnable. Checked:
won Proving Grounds (Infinite Health, held fire): func_00425EF4, then
func_004245C0 for slot 0; with medal 5's counter set to 2 before the win,
its bit (0x800FCB4A bit 5) was set, then the trial's own bits.

## 2026-10-17, round 122: the renderer's camera turn removed

Reported after round 121: the walls still blurred while turning, and much
less with PAXDINO_NO_TURN=1 (no frame tags), which turned about the same.
The turn added the mouse motion since the frame's camera was built, but
the game runs one to two frames ahead of the renderer, by varying
amounts, so that motion changed from one game frame to the next by up to
a frame's turn: the view jumped back and forth between batches, seen as
blur. With the mouse applied once a frame (rounds 117 to 119) and matches
at 60, RT64's own interpolation is smooth and the turn's lower latency
wasn't felt, so it's gone: lib-patches/rt64/0008 and the frame tags
(src/main/fps_controls.cpp) removed. The mouse is still added up on the
event thread and applied at each frame's first camera call; the frame
timing log stays. Checked: 300 pixels turns 1.0 rad, pitch moves.

## 2026-10-17, round 121: ghosting while turning

Reported after round 120: smooth, but ghosting. Round 120 turned the
interpolated views by the motion since the frame's camera was built, so
they show the angles between the last frame's and this one's plus that
motion. Views RT64 doesn't interpolate (not matched to one in the last
frame) were turned the same way from this frame's angles, so they showed
up to a game frame's turn ahead of the rest: parts of the scene apart
while turning. The workload now also gets the last frame's tag (kept as
each tag is taken), and those views are turned back toward it by the
frame's weight first, so they land on the same angles. Checked: builds,
renders and turns in a match.

## 2026-10-17, round 120: the turned view stood still between game frames

The frame timing log from a match (Framerate Display, 144 Hz, fullscreen):
600 game frames every 10 seconds, none over 24 ms, every display list to
the renderer on time. So the remaining mouse-only stutter is after the
game. RT64 renders the 2 or 3 frames shown from one game frame together,
as it comes (rt64_workload_queue.cpp, displayFrames), and the present
queue shows them a refresh apart. Patch 0008 turned each to the latest
mouse angles: the interpolated rotation turned by latest - lerp(previous
tag, tag, weight), which comes out as the latest angles on every frame
of the batch, all read within a few milliseconds. So the view stood still
for 2 or 3 refreshes and stepped at the next game frame, a 2-3-2-3 cadence
at 144 Hz that a gamepad (no turn) doesn't have. Now every frame from a
game frame is turned by the same latest - tag (the motion since its camera
was built): the view follows RT64's interpolation, ahead of it by that
motion. The previous frame's tag is no longer needed. Checked: builds,
turns in a match (sandbox renders too slowly to show the cadence).

## 2026-10-17, round 119: one mouse step per frame; frame timing log

Recordings after round 118: the turn steps even out to within what a
60 fps recording of a 144 Hz screen can show; a few single frames repeat,
which OBS put down to the game (one frame missed on its side). A frame
timing log for that (PAXDINO_FRAMELOG=1 in the environment): game frames
over 24 ms apart and late display lists, to frametimes.log in the working
directory, with totals every 10 seconds.

The log's first count found the camera function (func_00234FCC) runs three
times a frame in a match: twice from func_00236854 at the start of the
display list (a0 0x8013F008, then the player's camera), then from
func_00220A5C after the sky. Mouse look was applied at each, so motion
coming in between them turned the later views of the same frame further
than the first, and the frame's tag was the last call's. Now only the
frame's first camera call (a new display list buffer, or the head moved
back) takes the motion and the tag. Checked: one per frame sent, 300
pixels still turns 1.0 rad.

## 2026-10-17, round 118: mouse motion taken as it comes

A recording after round 117 (Framerate Display, 144 Hz, recorded at 60):
a new picture every frame, but the turn per picture went uneven, about one
in five barely moving and the next making up for it (-21 then -2, 22 then
0 then 5). Round 117 applied the motion once a frame, but the motion
itself was still the frontend's, handed over at the controller thread's
poll, so a frame got the motion up to that poll and the rest waited for the
next one. The motion is now the event thread's own count (already kept for
the renderer's latest angles), added up as each event comes and applied in
full at the start of each frame's camera; the controller reads only say
whether plain mouse look is on. Checked: 300 pixels still turns 1.0 rad.

## 2026-10-17, round 117: mouse look applied once a frame

Reported after round 115: the turning stutter is with the mouse only, not a
gamepad (mouse polling about 904 Hz), the same at original and higher
framerates. The game reads the stick once a frame and turns by it; the
mouse was turned in at each controller read, which the controller thread
does at retraces, not in step with the game's frames, so a frame got one,
two or no reads' worth of motion and the turn per frame was uneven. Now a
controller read only adds the motion up, and it's applied to the player's
yaw and pitch once a frame at the start of the camera function
(trw_camera_begin, 0x00234FCC), like a stick. The frame's camera tag is
taken at the same point, so the hold from the start of the camera to its
end (round 112) is gone. Checked: 300 pixels still turns 1.0 rad, pitch
moves and springs back with Look Spring Override off.

## 2026-10-17, round 116: the level name under character models

Reported: in the level fly-through a character model showed over the
level's name. The name (round 98) was drawn at a hook in func_00288DF8, and
character models are drawn later in the frame, after that function. Moving
the hook to the function's end didn't help, and drawing it after the
display list's last end (DF) didn't show at all: RT64 ends the frame at its
full sync (E9). The name is now put in as the display list goes to the
renderer (trw_place_early_text, src/game/widescreen.cpp, called from the
renderer wrapper in src/main/main.cpp): the list is walked to its last E9,
the text and that E9 go in a block at the top of the display list buffer,
and the E9 is replaced with a call to the block. The text is kept per
display list buffer, as the game builds a frame ahead. Checked: the name
and the trial objective show over the models.

## 2026-10-17, round 115: the turned view keeps RT64's interpolation

Reported after round 114: still some stutter moving and turning; smooth
before the mouse work. The turned view took the newest game frame's rotation
and added the mouse's turn since: the yaw and pitch were right, but the rest
of the game's camera motion, the walking bob and tilt, then stepped at the
game's 60 frames a second instead of being interpolated. Now the rotation is
RT64's interpolated one, turned by the mouse's angles past those
interpolated the same way between the last frame's tag and this one's (the
previous workload keeps its tag); the position is interpolated between the
two frames' camera positions (round 114). At a frame's end (weight 1) and
the next one's start (weight 0) the result is the same view, so it joins
without a step. Still not seen in the sandbox: it shows only whole game
frames (weight 1, checked again with Manual 120), where the turn and the
walk logged smooth.

## 2026-10-17, round 114: the turned view's position, moving and turning

Reported after round 113: snappy, but almost stuttering moving forward and
turning at once. The turned view (rt64/0008) took the camera's position from
RT64's interpolated view, and RT64 interpolates a view's rotation (slerp)
and offset (lerp) apart (RigidBody::lerp): the offset is the position
rotated into the view, so the position read back from the mix isn't the
straight line between the frames' positions while the view turns, and swings
off it (more the farther the camera is from the origin). The position is now
interpolated itself, between the last frame's camera position and this
one's, with the frame weights (and the motion-vector view with the previous
weight). Not seen in the sandbox: it renders slower than the game, so it
never shows frames between the game's.

## 2026-10-17, round 113: the frame tags went to the wrong frames

Reported after round 112: worse, stuttering even on slight turns. Logged in
the sandbox (a steady synthetic turn, the angle each frame shown ended at):
the tag a workload carried wasn't its frame's. Workload 4121 carried yaw
0.0400 while its camera matrix had 0.0133 (yaw = pi minus the matrix's),
the angle two frames before: the game runs a frame or two ahead of the
renderer, so the tag set at the frame end went to an older frame. Each frame
was turned by the wrong amount (round 109 already; round 112's held reads
made the mismatch uneven, hence worse). Pitch can't be read back from the
matrix instead: while turning the camera's pitch is minus the player's, but
when the mouse stops the game eases the camera toward level and the
player's pitch stays.
So the tag now goes with the display list: the camera notes the angles for
the display list buffer being built (0x806D2000 or 0x806E9000, from the head
at 0x8010A574), and a wrapper around the renderer context (main.cpp,
TrwRendererContext) hands that buffer's tag to RT64 in send_dl, before the
list is processed and its workload set up (advanceWorkload, same thread).
Checked: the same turn, 193 frames, every tag matching its matrix; the angle
shown moving one way only, in even steps.

## 2026-10-17, round 112: the level stuttering on fast turns

Reported after round 109: turning fast, the level stuttered. Two ways the
turn given to RT64 could step back and forth:
- The pending motion was zeroed at the game's controller read, but the
  motion the game took had been added up when its input was polled, a
  moment before: motion in between left the turn for a read, and came back
  with the next. The read's motion (raw pixels) now comes off the pending
  count instead, so it always holds what the game hasn't taken. Motion
  is also left out while game input is off (the settings overlay), as
  RecompFrontend does.
- The controller is read on its own thread, at the same retraces as the
  game's frame: a read in the middle of the camera (func_00234FCC) gave the
  camera one angle and the frame's tag another. A hook at the camera's start
  marks it; a read meanwhile keeps its motion for the next read (at most
  8 ms), so the angles hold until the tag is noted.
Checked: 300 pixels of motion turned the player 1.0005 rad (300 x
0.003335), none lost.

## 2026-10-17, round 111: mouse look during weapon picks (issue #9)

Reported: switching weapons with the mouse wheel stopped mouse look for a
moment (at 60 frames a second only a brief stutter). A quick pick (wheel,
Q / E, 1-8) opens the game's wheel for a few reads, and the mouse's motion
was thrown away meanwhile. Mouse look sets the angles itself, apart from
the stick the wheel is steered by, so it now goes on through the pick
(fps_controls.cpp, mouse_look, which the plain look uses too).
Checked: two wheel steps switched the weapon (3, 0, 6) as before.

## 2026-10-17, round 110: mouse sensitivity, second pass

Requested: the default as fast as 80% was (0.003335 rad per pixel, about
twice round 108's), 0% not so slow, more room at the top. 50% is now
0.003335, 0% half that (about the original default), 100% three times it
(0.01); evenly on each side of 50%.

## 2026-10-17, round 109: the view turned with the latest mouse motion

Reported after round 107 (144 Hz monitor): still a slight lag, most of all
turning around fast. With the Framerate above the game's rate RT64 shows
frames between the game's, the camera interpolated between the last two
(render/rt64_projection_processor.cpp), so the view trailed the mouse by up
to a game frame. lib-patches/rt64/0008: the game tags each frame with the
yaw and pitch its camera was built with (the FOV hook in func_00234FCC notes
the mouse player's angles; trw_frame_end hands them to RT64, which takes them
into the next workload in advanceWorkload, after the frame's display list is
processed), and keeps RT64 up to date with the latest angles: the player's
after the last controller read plus the mouse motion since, from SDL's
motion events as they come (fps_controls.cpp). For each frame shown, every
perspective view with a camera in it (the weapon's own plain view is left
alone) gets the frame's rotation turned by the difference, about the world's
up axis and the camera's sideways axis, around the interpolated camera
position. In a match, single view, plain mouse look (not the menus, the
pause, the wheel or the team select), Framerate above Original.
Checked (sandbox, with a test offset): 0.3 rad of extra yaw gave the same
picture as the game turning the player 0.3 rad (walls and an item on the
floor in the same places); extra pitch looks up for a positive offset, the
game's sign. The end hook also runs for display lists without the camera, so
only frames that built it are tagged, and RT64 uses a tag once.

## 2026-10-17, round 108: mouse sensitivity curve; no camera easing

Requested: the Mouse Sensitivity slider's low end was too low to use, its
high end not high enough, and the default should be about 15% faster. The
slider stays 0-100% (0 still turns mouse aim off), but the turn per pixel
now follows an even curve: 0.001725 rad at 50% (15% over the old 0.0015),
three times that at 100%, a third at 0%, each step the same ratio
(fps_controls.cpp, mouse_look_radians). Saved settings at the default get
the new default.
Also checked, on the question whether the game smooths the view itself:
no. The mouse writes the player's yaw and pitch directly (the game's stick
speed doesn't apply), and with the yaw turned 0.5 rad at one frame's start,
the camera matrix the game builds (0x8069FA00 / 0x8069FA40, alternating)
had the whole turn the next frame.

## 2026-10-17, round 107: matches at 60 frames a second

Reported (issue #1): mouse aim felt smoothed. Matches ran at 30 frames a
second, with RT64 interpolating between frames above that, so the view
caught up with the mouse a frame or more late. Each frame func_00292818
averages the retraces the last frames took (the four counters
0x80140C34, 0x80140CA4, 0x801199F8 and 0x801408AC, minus the largest,
over 3), with a floor of 2 (4 in one case), into 0x801408A8, and makes the
frame step from it (0x800CD648 = retraces x 15 / 60.0 at 0x80140C3C);
func_00292220 then stores the same count as the frame buffer setup's wait
(+0x124, the three setups at *0x801408B0), which the scheduler
(func_0028E9DC) holds the next frame for. (The "frame-rate mode bytes" in
RAM_NOTES are inside that word.) With the Graphics tab's Framerate above
Original (Display, or Manual at 60 or more), in a match, a hook before the
step is stored (0x00292CA4) sets the wait to 1 and the step from the same
average with a floor of 1: 0.25 at 60 frames a second, the same game time
per second as 0.5 at 30, and more on a PC that can't keep 60. At Original
nothing changes. One option, not a separate one: a Framerate above Original
with the game at 30 was the mismatch.
Checked (sandbox, which renders at about 15-25 frames a second, so not 60):
waits 1 with Display, the game's own with Original; game time against real
time as close or closer than the original's; a match plays.

## 2026-10-17, round 106: double click a slider for its default

Requested: double clicking a slider in the settings puts it back to its
default (still applied with Apply, like any change). RecompFrontend patch
0015: the slider watches left presses on any of its parts (bar, handle, the
number), and two within 0.5 s and 6 px of each other call its double-click
callback; the number options set their default through the same path as a
drag. RmlUi's own dblclick wasn't usable: it needs both clicks on the same
element, and the first click moves the handle under the cursor, so the
second lands on the handle.
Checked (Rumble Strength, default 25%): 92%, double click on the bar: 25%;
92%, double click on the number: 25%.

## 2026-10-17, round 105: osClockRate

Checked after the same bug turned up in another N64Recomp port: N64Recomp
skips the game's osInitialize, and the runtime's never sets the game's
osClockRate. Here osInitialize doesn't load it from data, it writes it
(62,500,000, then x 3/4) to 0x80147B00, so it stayed 0, not 62.5 MHz. One
game function reads it: func_00414430, osGetTime in seconds (a double).
Through func_002A180C, times 1000, it is the menu system's millisecond
clock: the timer event 0xE08 (func_00297974, with the step since the last
one) and 0xE0A (func_00298F28), and front-end code at 0x0041xxxx. Divided by
0, every timestamp was the same and every step 0. Gameplay timing doesn't
use it (the frame step reads osGetTime with its own constant, round 3).
A hook after the game's osInitialize call (0x002A7444 in trw_boot_main) sets
46,875,000, the rate the runtime's osGetTime counts at.
Checked: the menu clock reads 54559 then 59047 ms about 4.5 s apart; menus
and a match as before.

## 2026-10-17, round 104: Rapid Fire and the level pickups

Reported: with Rapid Fire (and Semi-Auto) on, clicking fast fired the
Cerebral Bore pickup again and again. The weapons picked up in a level
(0x12-0x15: Cerebral Bore, PFM Mine, Iron Claw, Sentry Turret) go after one
use, and cutting the fire state short let them fire again first. Rapid Fire
now leaves the held weapon (player +0x62E) alone when it's one of them.
Checked: the held weapon reads 3 (Mag 60) in a match and Rapid Fire on it is
unchanged; the pickups weren't reachable in the sandbox. On Windows, the
Cerebral Bore with Rapid Fire and Semi-Auto now fires once.

## 2026-10-17, round 103: Field of View (issue #2)

Requested: a field of view slider. The match camera (func_00234FCC) makes
its perspective with guPerspectiveF (0x002B6ECC, at 0x0023567C) from a
vertical angle in a2: 74.4 degrees, which is 90 across a 4:3 picture, and
less when zoomed. Graphics tab, Field of View, right under Aspect Ratio
(RecompFrontend patch 0014 adds the hook for options there): 60 to 120,
degrees across 4:3, 90 the game's own. It scales the game's angle in
tangents, so zoom still works; widescreen adds to the sides as before. The
other guPerspectiveF call (0x0023590C, a fixed 47.5) is left alone.
Checked at 60, 90 and 120: the view widens and narrows, the weapon and HUD
stay where they were.

## 2026-10-17, round 102: Rapid Fire's Semi-Auto (issue #5)

Requested: a Rapid Fire that fires as fast as you can press, not while
held, so ammo lasts. The fire state (player +0x3F0 and +0x3F2, the
GameShark code's halfwords) is 0 before the first shot, 5 while a shot is
busy and 2 when the weapon can fire again. Semi-Auto (Cheats tab, under
Rapid Fire, greyed out with it off) sets it to 2 only for a new press of Z
made while the weapon is busy.
Rapid Fire itself had two problems, found on the starting burst weapon: set
to 2 from 0 the weapon never fired, and set to 2 in the first calls of a
shot, the shot restarted before its round went out, so holding Z fired
nothing until it was let go. It now waits for the shot to be out (the
ammo has gone down, or 6 calls for weapons without ammo and Unlimited Ammo)
and never writes from 0.
Checked (hold 3 s, then 8 quick taps, rounds used): off 6 / 6; Rapid Fire
10 / 11; Semi-Auto 6 / 9.

## 2026-10-16, round 101: Custom Music through the pause menu (issue #3)

Reported: with Custom Music on, the music often went silent when paused.
The pause menu ducks the game's music to silence (func_0025DAD4 sets the
music object's +0x38 and the volume steps down to 0; func_0025DAE4 on
unpause puts it straight back), and the file followed the game's volumes
except while the match read as paused. The duck starts a frame before that,
so the first ducked volume got through and the file sat at it for the whole
pause, and after the first pause that volume was 0. Volumes set while the
duck is on are now left out entirely, so the file plays on at its level
through the pause menu and its options. The game's own music is unchanged
(it fades out on pause, as on the N64).
Checked: six pauses and the options page in a match, the file stayed at
full level every time.

## 2026-10-16, round 100: player assignment kept across launches (issue #6)

Reported: with Local Multiplayer on, the players had to be assigned again
on every launch. RecompFrontend keeps the assignment only in memory. Patch
0013 adds `playerassignment::describe()` ("keyboard" or "pad:<GUID>:<n>"
per player), `restore()` and a commit callback; the per-player profile
setup moved into one function both paths use. The game saves the list in
general.json ("player_assignment", hidden) on Confirm and restores it from
the launcher once, when Local Multiplayer is on and nothing is assigned yet.
If a saved controller isn't connected yet, nothing is assigned; the
launcher tries again each frame until it is.
Checked: assigned the keyboard, confirmed, relaunched: player 1 was the
keyboard. With nothing saved, player 1 stayed empty.

## 2026-10-16, round 99: no weapon wheel on quick picks (issue #4)

Reported: the mouse wheel flashed the weapon wheel on screen. Q, the mouse
wheel and 1-8 pick by opening the game's wheel for a moment (round 38), so
it was drawn for those frames. The HUD's call to the wheel's drawing
(func_002183E8, at 0x0021EFBC) is now skipped for the player a pick is
running for, until the wheel has closed (trw_wheel_hidden; the generated
label after_11 is right after the call). Holding E shows it as before.
Checked: scrolling switched the weapon with no wheel in any frame; E showed
the wheel.

## 2026-10-16, round 98: trial objective text (issue #8)

Reported: the objective a trial shows at its start ("PROVING GROUNDS /
acquire 2 frags / with 2 lives") was missing. The game draws it through its
text routine (func_002A8820) every frame of the fly-through, but at the very
start of the frame's display list, before func_00291B00 sets the frame's
colour image: on the N64 it lands on the colour buffer the last frame left
set, the finished frame about to be shown, and the new buffer is then filled
and the level drawn. In RT64 nothing of it showed. In a match, the text
block (from its scissor up to the frame's own setup: the next G_DL, colour
or depth image) is now copied out and NOPed at frame start
(trw_frame_buffers_set) and put back just before the frame's G_ENDDL
(func_00288DF8, hook at 0x00288F38). Found with gdb (the strings and their
draw calls; func_00288888 and func_0024EB70 end other display lists).
Checked (widescreen): the three lines show centred through the fly-through.

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
