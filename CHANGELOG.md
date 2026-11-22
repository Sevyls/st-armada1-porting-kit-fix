# Changelog

All changes so far were made on 2026-11-22. To update an existing
installation, download the latest version and run `tools/patch_armada.py`
again – it updates files patched by an older version (see
[INSTALL.md](INSTALL.md#updating-to-a-newer-version-of-the-fix)).

The version names below are only used here; the patch tool identifies
installed versions by checksum.

## Version 4 – Display Mode list, patch tool checks

- **Options → Graphics Settings → Display Mode** only lists 32-bit
  resolutions in 4:3 or 5:4 shape. Before, the list held dozens of modes,
  ran off the screen and couldn't scroll, so higher resolutions and 32-bit
  modes were unreachable. (`Armada.exe` patch; details:
  [INVESTIGATION.md §5](docs/INVESTIGATION.md#5-display-mode-list-runs-off-the-screen))
- Patch tool:
  - updates an `Armada.exe` patched by version 1–3,
  - reads the game's version info and explains unknown files (other 1.2
    build such as the CD version, older game version, not Armada),
  - shows the Wine prefix (32/64-bit, Windows version, Porting Kit engine)
    and warns when it differs from the tested setup,
  - new `--check` option: report only, change nothing.

## Version 3 – overlapping menus

- Fixed: buttons of a covered menu sometimes appeared on top of a sub-menu
  (e.g. *Options → Game Settings*) until *Previous Menu* was clicked.
  (armfix; [INVESTIGATION.md §4](docs/INVESTIGATION.md#4-buttons-of-a-covered-menu-drawn-over-a-sub-menu))
- Focus changes between the game's own menus reach the game again; only
  switching to another app is hidden from it.

## Version 2 – Cmd+Tab

- Fixed: Cmd+Tab during a loading screen crashed the game; in the menus it
  left an unrestorable 1×1 window. The game now keeps running in the
  background and becomes invisible while another app is in front.
  (armfix + registry value `WindowsFloatWhenInactive=none`;
  [INVESTIGATION.md §3](docs/INVESTIGATION.md#3-cmdtab-crash-during-loading-unrestorable-11-window))

## Version 1 – initial release

- Fixed: crash at startup (no usable display mode on a Mac).
- Fixed: menu clicks only produced the macOS alert sound.
- Patch tool, step-by-step guide ([INSTALL.md](INSTALL.md)), tested
  configuration ([CONFIGURATION.md](docs/CONFIGURATION.md)).

## Documentation only

- Internet multiplayer can't work (WON servers shut down in 2004); windowed
  mode attempts that did not work.
