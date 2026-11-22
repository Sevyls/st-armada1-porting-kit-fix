# Star Trek: Armada on macOS (Apple Silicon) – Wine fix

Makes the **GOG version of Star Trek: Armada** (2000) playable on macOS with
Wine (Porting Kit / Sikarugir / Wineskin wrappers): no crash at startup, a
correctly rendered menu, working mouse clicks in all menus and dialogs, a
usable resolution list, and safe switching to other apps with Cmd+Tab.

Tested on a MacBook Pro M4 Pro, macOS 27, Porting Kit with a Sikarugir
wrapper running `wine-10.0 (Sikarugir)`, GOG offline installer
`patch_star_trek_armada_1.2_(50358)_to_1.2_RDNA_fix_(52907)`, both on the
built-in display (1352×878 pt) and on an external 3008×1692 pt display.

## 👉 Installation: see **[INSTALL.md](INSTALL.md)**

A step-by-step guide for players without technical background – from the GOG
download and the Porting Kit custom port to applying the fix, updating it and
troubleshooting. All tested wrapper, Wine and registry settings are listed in
[docs/CONFIGURATION.md](docs/CONFIGURATION.md); what changed between versions
is in [CHANGELOG.md](CHANGELOG.md).

> **Deutsch:** Dieses Repo macht die GOG-Version von Star Trek Armada auf dem
> Mac mit Wine spielbar. Die Schritt-für-Schritt-Anleitung steht in
> [INSTALL.md](INSTALL.md) (Englisch), die technische Ursachenanalyse in
> [docs/INVESTIGATION.md](docs/INVESTIGATION.md).

## macOS compatibility

| macOS | Status |
|---|---|
| **macOS 27** (Apple Silicon) | ✅ tested (macOS 27.2) |
| macOS 26 and earlier (Apple Silicon) | should work, not tested |
| Intel Macs | not tested |
| **macOS 28 and later** | ❌ expected **not** to work |

Wine on Apple Silicon (and with it Porting Kit) runs x86 code through
Apple's **Rosetta 2**, which must be installed (macOS offers it automatically
the first time). Apple keeps full Rosetta 2 only through macOS 27; from
macOS 28 on only a limited subset for certain older, unmaintained games
remains ([announcement](https://www.macrumors.com/2025/06/10/apple-to-phase-out-rosetta-2/)).
Unless that subset still covers Wine, this fix – like Wine-based gaming on
the Mac in general – will stop working on macOS 28.

## Symptoms this fixes

| Symptom | Cause (short) |
|---|---|
| Crash right at startup: `page fault on read access to 0x00000004` in `armada+0x18972f` | Armada only accepts five 4:3 display modes; a Mac display offers none of them → NULL mode pointer |
| Menu renders, but clicks only produce the macOS alert sound | The GOG wrapper's input dialog is a window Wine/macOS treat as transparent and click-through |
| Cmd+Tab during loading crashes the game; in the menus it leaves an unrestorable 1×1 window | The game, the GOG wrapper and Wine react to losing focus by minimizing and releasing graphics resources |
| Buttons of the menu behind sometimes drawn over a sub-menu (e.g. *Options → Game Settings*) | The GOG wrapper composites all menu drawing into one picture, regardless of which window is in front |
| *Display Mode* list runs off the screen; higher resolutions and 32-bit modes unreachable | With the startup fix, a Mac offers dozens of modes; the game's drop-down can't scroll |
| Menu animations hidden behind the background (with Wine's builtin ddraw) | Not used anymore – the GOG wrapper renders the menu correctly |

Details: [docs/INVESTIGATION.md](docs/INVESTIGATION.md) · tested settings: [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

## What the fix consists of

1. **Small patches to `Armada.exe`** (your own copy, original kept as
   `Armada.exe.orig`):
   - accept every display mode from 640×480 up to 2048 px width,
   - fall back to the first available mode instead of crashing,
   - list only 32-bit 4:3 and 5:4 modes in *Options → Graphics Settings*
     (otherwise the drop-down runs off the screen and can't scroll);
   - plus `WS_EX_TOPMOST` on the 32 full-screen shell dialog templates.
2. **One patch to GOG's DirectX wrapper `ddraw.dll`**: the wrapper moves the
   real dialog controls 21000 px off-screen and widens the dialog by that
   amount; macOS can't handle such wide windows, so the offset becomes 6000 px.
3. **`armfix`** – a tiny helper DLL (source in [`armfix/`](armfix)) loaded
   through a proxy of the game's own `STRLKUP.DLL`. It makes Armada's
   invisible menu dialogs “real” windows for macOS (painted once, 5 % opacity)
   so clicks reach them. It also makes **Cmd+Tab** safe: the game is never
   told that it lost focus (which crashed it during loading and left an
   unrestorable 1×1 window), and its windows turn fully transparent while
   another app is in front. Finally it stops buttons of a covered menu from
   showing up on top of a sub-menu (e.g. *Options → Game Settings*).
4. **Four Wine registry values** (HKCU):
   - `Software\Wine\DllOverrides` → `ddraw` = `native,builtin` (use GOG's wrapper),
   - `Software\Wine\Direct3D` → `VideoMemorySize` = `512`,
   - `Software\Wine\Mac Driver` → `UseConfinementCursorClipping` = `n`,
   - `Software\Wine\Mac Driver` → `WindowsFloatWhenInactive` = `none`.

No files or code from GOG or Activision are distributed here: the patch
tables contain only file offsets and the new bytes (written for this fix);
the original files are identified by their MD5 checksum. The tool patches the
files of your own installation and refuses to touch any other version.

Everything here is published as source: the patch tool is a plain Python
script ([`tools/patch_armada.py`](tools/patch_armada.py)); the only binary,
`bin/Strlkup.dll`, is built from [`armfix/`](armfix) (see *Building armfix
yourself*).

## Installation (short version for experienced users)

Step-by-step guide: [INSTALL.md](INSTALL.md).

Requirements: the GOG offline installer (version “1.2 RDNA fix”) installed
into a Porting Kit wrapper (custom port), and Python 3 (`python3`, comes with
the Xcode command line tools).

1. Install the game with Porting Kit (custom port, GOG offline installer) and
   start it once. Quit it.
2. Download or clone this repository.
3. In Terminal:
   ```sh
   python3 tools/patch_armada.py "/Applications/Star Trek Armada.app"
   ```
   (Use the path of your wrapper. A Wine prefix folder works too.)
4. Start the game through Porting Kit.

Undo everything:
```sh
python3 tools/patch_armada.py --restore "/Applications/Star Trek Armada.app"
```

Check without changing anything:
```sh
python3 tools/patch_armada.py --check "/Applications/Star Trek Armada.app"
```

The tool
- identifies `Armada.exe` and `ddraw.dll` by MD5 and aborts without changing
  anything if they are not the expected GOG versions; for an unknown
  `Armada.exe` it reads the file's version info and says what it is (other
  1.2 build such as the CD version, older game version, not Armada);
- reports the patch level (original / older version of this fix / current)
  and **updates an installation patched by an older version of the fix** –
  just run it again after downloading a newer version;
- reports the Wine prefix (32/64-bit, Windows version, Porting Kit engine)
  and notes when it differs from the tested setup (64-bit prefix,
  Windows XP 64-bit). It only warns; it doesn't refuse.

## Tuning

`armfix.txt` in the game folder holds the opacity (0–255) of the invisible
menu dialogs. Default `13`. Values below about 13 make macOS treat the window
as click-through again; higher values darken the picture slightly. The file
is re-read every few seconds, so changes apply while the game is running.

Better 3D graphics: set `anisotropic=enabled` in `dxcfg.ini` (GOG wrapper,
game folder). Avoid the wrapper's anti-aliasing – it draws lines across the
in-engine cinematics (bridge scenes). Details: [CONFIGURATION.md](docs/CONFIGURATION.md#optional-better-3d-graphics).

## Known limitations

- While another app is in front, the game keeps running (including sound
  and a running mission) – it is only made invisible. Pause first if needed.
- To return to the game, use Cmd+Tab or its Dock icon; clicking where the
  (invisible) game window is does not bring it back.
- Only the GOG “1.2 RDNA fix” build is supported (exact byte patches).
- Tested with a 64-bit Wine prefix set to Windows XP 64-bit. 32-bit prefixes
  and other Windows versions should work the same way but are untested.
- *Display Mode* only offers 32-bit 4:3/5:4 resolutions (16-bit and
  widescreen modes are hidden from the menu).
- **Internet multiplayer can't work.** The *Internet* button connects to the
  World Opponent Network (WON), whose servers were shut down in 2004, so it
  hangs at “Trying to connect to a WON server” on every platform. GOG only
  supports multiplayer over *Local Area Network (TCP/IP)*; that was not
  tested under Wine/macOS yet.
- No windowed mode: the game's hidden `-window` switch opens a window, but
  under Wine on macOS the window layers and mouse input get mixed up (see
  [CONFIGURATION.md](docs/CONFIGURATION.md#settings-that-did-not-help-for-reference)).

## Building armfix yourself

```sh
brew install mingw-w64      # or: apt install gcc-mingw-w64-i686
./armfix/build.sh           # writes bin/Strlkup.dll
```

`bin/Strlkup.dll` in this repository is the exact build that was tested.

## Credits & license

Investigation and fix developed with the help of Claude (Anthropic).
Code in this repository: MIT License, © 2026 Michael Auß – see [LICENSE](LICENSE).
Star Trek: Armada is © Activision. STAR TREK is a trademark of CBS Studios Inc.
The DirectX wrapper shipped with the game is © GOG.com. This project is an
unofficial compatibility fix, not affiliated with or endorsed by any of them,
and contains no files or code from the game or from GOG – you need your own
legally purchased copy.
