# Tested configuration (Porting Kit wrapper, Wine, registry)

This is the complete configuration of the wrapper the fix was developed and
tested with. Values marked **(fix)** are set by `tools/patch_armada.py`;
everything else is what Porting Kit created by default for the custom port and
was left unchanged.

## Wrapper

| Setting | Value |
|---|---|
| Porting Kit wrapper | Wineskin **3.0.6_2** (Sikarugir) |
| Wine engine | **wine sikarugir 10.0 (revision 3)** – `wine-10.0 (Sikarugir)`; `prefix/info.json`: `WS12WineSikarugir10.0_3` |
| Prefix architecture | 64-bit prefix (`WINEARCH=win64`), game runs in WoW64 (32-bit) |
| Windows version | Windows XP 64-bit (“Microsoft Windows XP 64”, 5.2, build 3790 SP2) – Porting Kit default |

`python3 tools/patch_armada.py --check <wrapper>` shows these values for your
own wrapper (prefix architecture, Windows version incl. Wine's per-program
setting, engine) and whether they match the tested setup. 32-bit prefixes and
other Windows versions are untested; the tool warns but still applies the fix.
| Graphics driver | Mac driver (`winemac.drv`); XQuartz/X11 not used |
| Program | `/GOG Games/Star Trek Armada/Armada.exe`, no program flags |

`Info.plist` of the wrapper (`Star Trek Armada.app/Contents/Info.plist`):

| Key | Value | Meaning |
|---|---|---|
| `D3DMETAL` | `0` | Apple D3DMetal off (it only supports 64-bit games anyway) |
| `D3DMETAL_FORCE` | `1` | default |
| `MOLTENVKCX` | `1` | Vulkan via MoltenVK available (not used by the game) |
| `WINEESYNC` / `WINEMSYNC` | `1` / `1` | esync/msync on (default) |
| `FASTMATH` | `0` | default |
| `Use XQuartz` | `false` | Mac driver |
| `Debug Mode` | `false` | set to `true` (and `WINEDEBUG`, e.g. `-plugplay,+win,+cursor`) only for debugging; the log goes to `Contents/SharedSupport/Logs/LastRunWine.log` |
| `WINEDEBUG` | `-plugplay,+loaddll` | default |

Winetricks: Porting Kit tries to install `vcrun2022` when creating the
wrapper (in our case this aborted). Armada does not need it.

## Wine registry (`HKEY_CURRENT_USER`, file `prefix/user.reg`)

### Changed by the fix

| Key | Value name | Data | Why |
|---|---|---|---|
| `Software\Wine\DllOverrides` | `ddraw` | `native,builtin` | **(fix)** load GOG's DirectX wrapper `ddraw.dll` from the game folder instead of Wine's builtin ddraw. Required: only the GOG wrapper renders the menus correctly. |
| `Software\Wine\Direct3D` | `VideoMemorySize` | `512` | **(fix)** report 512 MB video memory instead of the large unified memory of Apple Silicon (a common cause of trouble in old Direct3D games; harmless). |
| `Software\Wine\Mac Driver` | `UseConfinementCursorClipping` | `n` | **(fix)** Wine otherwise implements cursor clipping by tying the mouse to one Cocoa window, which can pin clicks to the game window. |
| `Software\Wine\Mac Driver` | `WindowsFloatWhenInactive` | `none` | **(fix)** by default Wine keeps “always on top” windows (Armada's patched menu dialogs) floating above other apps even when the game is in the background. |

### Left at Porting Kit / Wine defaults

| Key | Value name | Data |
|---|---|---|
| `Software\Wine\Direct3D` | `renderer` | `gl` (wined3d on OpenGL) |
| `Software\Wine\Drivers` | `Graphics` | `mac,x11` |
| `Software\Wine\Mac Driver` | `LeftCommandIsCtrl`, `RightCommandIsCtrl` | `Y` |
| `Software\Wine\Mac Driver` | `LeftOptionIsAlt`, `RightOptionIsAlt` | `Y` |
| `Software\Wine\Mac Driver` | `UsePreciseScrolling` | `N` |
| `Software\Wine\DllOverrides` | `*vcruntime140`, `*msvcp140`, … | `native,builtin` (Visual C++ runtime, from winetricks) |

Not needed (left over from experiments on the test machine, harmless):
`d3d11`, `dxgi`, `d3d10core` = `native,builtin` (only relevant when DXVK DLLs
are placed in the game folder).

Setting the values by hand instead of using the tool: open the wrapper's
**Wineskin / Sikarugir settings → Advanced → Tools → Registry Editor**
(`regedit`) and create the string values above, or use **Wine configuration
→ Libraries** for the `ddraw` override (add `ddraw`, *Edit* → *Native then
Builtin*). Quit the game before editing; Wine rewrites `user.reg` on exit.

## Game folder (`C:\GOG Games\Star Trek Armada`)

| File | State |
|---|---|
| `Armada.exe` | **(fix)** patched (display modes, Display Mode menu filter, dialog flags), original in `Armada.exe.orig` |
| `ddraw.dll` | **(fix)** GOG DirectX 1-7 wrapper, patched (control offset 21000 → 6000), original in `ddraw.dll.orig` |
| `STRLKUP.DLL` | **(fix)** armfix proxy DLL from this repository |
| `strlkup0.dll` | the game's original `STRLKUP.DLL`, renamed |
| `armfix.txt` | **(fix)** opacity of the menu dialogs for armfix, `13` (range 0–255), re-read every ~5 s |
| `dxcfg.ini` | GOG wrapper settings – **unchanged defaults** (see below) |
| `ARMADA.PRF` | the game's own settings (resolution etc.), written by the game |

### GOG DirectX wrapper settings (`dxcfg.ini`, defaults)

```ini
[dxcfg]
display=desktop          ; output at the desktop resolution
presentation=application ; fullscreen/windowed as the game requests
aspect=enabled           ; keep 4:3, black bars left/right
scaling=fit
vsync=application
gamma=1.0
anisotropic=application
antialiasing=application
```

`aspect=disabled` (stretched picture) also works. `presentation=windowed`
had no visible effect under Wine. `dxcfg.exe` in the game folder is GOG's
settings tool for this file.

#### Optional: better 3D graphics

Not set by the patch tool. Edit `dxcfg.ini` with TextEdit while the game is
closed (inside the app: right-click → **Show Package Contents** →
`Contents/SharedSupport/prefix/drive_c/GOG Games/Star Trek Armada`).

| Setting | Values the wrapper accepts | Recommendation |
|---|---|---|
| `anisotropic` | `application`, `enabled`, `disabled` | **`enabled`** – sharper textures on surfaces seen at an angle; tested, no side effects |
| `antialiasing` | `application`, `msaa_2x`, `msaa_4x`, `msaa_8x`, `disabled` | **leave at `application`** (see below) |
| `vsync` | `application`, `enabled`, `disabled` | `enabled` can prevent tearing when the camera pans (not tested) |

Anti-aliasing (`msaa_2x`/`msaa_4x`) does smooth the ship edges in missions,
but it puts thin horizontal and vertical **lines across the in-engine
cinematics** (e.g. the bridge scenes before missions). The pre-rendered Bink
videos (like the Armada logo intro) stay clean. The lines are single
segments of different lengths along the edges of the scene's polygons: with
multisampling, pixels on a polygon edge are shaded at the pixel centre, which
can lie just outside the polygon, so the texture is read slightly beyond the
polygon's part of the texture. Forcing anti-aliasing in the graphics driver
caused the same lines on Windows PCs back then. It can't be fixed in the
wrapper's settings (they apply to the whole game and are read only at
startup), and a fix in the game's rendering would affect every 3D scene.

Together with the highest *Display Mode* (Options → Graphics Settings) and
the *Graphics Detail* slider at maximum, `anisotropic=enabled` gives the best
picture without side effects.

## Settings that did *not* help (for reference)

| Attempt | Result |
|---|---|
| Wine's builtin ddraw (no GOG wrapper) | clicks work, but menu animations are hidden behind the background |
| dgVoodoo2 (+ DXVK-macOS 1.10.3) | no Direct3D device / crash in dgVoodoo's `CreateSurface` |
| dxwrapper “Armada fix” (Dd7to9 → D3D9) | starts, menus render worse |
| Wine virtual desktop | not available with the Mac driver (greyed out in winecfg) |
| `presentation=windowed` in `dxcfg.ini` | no visible effect |
| Armada's hidden command-line switch `-window` (Wineskin *Program Flags*) | the game opens in a window, but window layering and mouse input get mixed up; menus are unusable |

Details: [INVESTIGATION.md](INVESTIGATION.md).
