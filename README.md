# Star Trek: Armada on macOS (Apple Silicon) – Wine fix

Makes the **GOG version of Star Trek: Armada** (2000) playable on macOS with
Wine (Porting Kit / Sikarugir / Wineskin wrappers): no crash at startup, a
correctly rendered menu, and working mouse clicks in all menus and dialogs.

Tested on a MacBook Pro M4 Pro, macOS 27, Porting Kit with a Sikarugir
wrapper running `wine-10.0 (Sikarugir)`, GOG offline installer
`patch_star_trek_armada_1.2_(50358)_to_1.2_RDNA_fix_(52907)`, both on the
built-in display (1352×878 pt) and on an external 3008×1692 pt display.

> **Deutsch:** Dieses Repo macht die GOG-Version von Star Trek Armada auf dem
> Mac mit Wine spielbar. Installation siehe unten; die technische
> Ursachenanalyse steht in [docs/INVESTIGATION.md](docs/INVESTIGATION.md).

## Symptoms this fixes

| Symptom | Cause (short) |
|---|---|
| Crash right at startup: `page fault on read access to 0x00000004` in `armada+0x18972f` | Armada only accepts five 4:3 display modes; a Mac display offers none of them → NULL mode pointer |
| Menu renders, but clicks only produce the macOS alert sound | The GOG wrapper's input dialog is a window Wine/macOS treat as transparent and click-through |
| Menu animations hidden behind the background (with Wine's builtin ddraw) | Not used anymore – the GOG wrapper renders the menu correctly |

Details: [docs/INVESTIGATION.md](docs/INVESTIGATION.md) · tested settings: [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

## What the fix consists of

1. **Two small patches to `Armada.exe`** (your own copy, original kept as
   `Armada.exe.orig`):
   - accept every display mode from 640×480 up to 2048 px width,
   - fall back to the first available mode instead of crashing;
   - plus `WS_EX_TOPMOST` on the 32 full-screen shell dialog templates.
2. **One patch to GOG's DirectX wrapper `ddraw.dll`**: the wrapper moves the
   real dialog controls 21000 px off-screen and widens the dialog by that
   amount; macOS can't handle such wide windows, so the offset becomes 6000 px.
3. **`armfix`** – a tiny helper DLL (source in [`armfix/`](armfix)) loaded
   through a proxy of the game's own `STRLKUP.DLL`. It makes Armada's
   invisible menu dialogs “real” windows for macOS (painted once, 5 % opacity)
   so clicks reach them.
4. **Three Wine registry values** (HKCU):
   - `Software\Wine\DllOverrides` → `ddraw` = `native,builtin` (use GOG's wrapper),
   - `Software\Wine\Direct3D` → `VideoMemorySize` = `512`,
   - `Software\Wine\Mac Driver` → `UseConfinementCursorClipping` = `n`.

No files or code from GOG or Activision are distributed here: the patch
tables contain only file offsets and the new bytes (written for this fix);
the original files are identified by their MD5 checksum. The tool patches the
files of your own installation and refuses to touch any other version.

## Installation

**New to Terminal and Porting Kit? Follow the step-by-step guide:
[INSTALL.md](INSTALL.md).** All tested wrapper, Wine and registry settings
are listed in [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

Short version – requirements: the GOG offline installer (version “1.2 RDNA fix”) installed
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

The tool checks the MD5 of `Armada.exe` and `ddraw.dll` and aborts without
changing anything if they are not the expected GOG versions.

## Tuning

`armfix.txt` in the game folder holds the opacity (0–255) of the invisible
menu dialogs. Default `13`. Values below about 13 make macOS treat the window
as click-through again; higher values darken the picture slightly.

## Known limitations

- Switching away with Cmd+Tab minimizes the game (to a 1×1 window) and it
  can't be restored; quit and restart instead.
- When a new sub-menu opens, the click target appears up to 250 ms later.
- Only the GOG “1.2 RDNA fix” build is supported (exact byte patches).
- Multiplayer was not tested.

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
