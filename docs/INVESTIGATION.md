# Technical investigation

Environment: MacBook Pro (M4 Pro), macOS 27 (Darwin 27.2), Porting Kit with a
Sikarugir wrapper, `wine-10.0 (Sikarugir)`, WoW64 (32-bit game in a 64-bit
prefix), Windows XP mode, Wine Mac driver (`winemac.drv`), wined3d on OpenGL.
Game: GOG Star Trek: Armada, build “1.2 RDNA fix”.

Addresses below are virtual addresses in `Armada.exe` (image base 0x400000)
and in GOG's `ddraw.dll` (“GOG.com DirectX 1-7 wrapper”, image base
0x18000000).

---

## 1. Crash at startup: `page fault on read access to 0x00000004`

```
Unhandled exception: page fault on read access to 0x00000004 in wow64 32-bit code (0x0058972f).
0x0058972f armada+0x18972f: movl 4(%eax), %ecx      ; EAX = 0
```

Same crash with Wine's builtin ddraw and with GOG's wrapper, so it is not a
renderer problem.

**Analysis**

- `0x5895f0` receives a pointer to the selected display mode (`{w, h, bpp}`)
  and dereferences it: that pointer is NULL.
- The pointer comes from `0x586fc0`, which searches the adapter's mode list
  for an exact `width × height × bpp` and returns NULL when nothing matches.
  The game asks for the mode stored in `ARMADA.PRF` (`1024 768 32`), then
  falls back to `640 × 480`. Nobody checks for NULL afterwards.
- The mode list is filled by the `EnumDisplayModes` callback at `0x58ab50`,
  which only keeps **640×480, 800×600, 1024×768, 1280×1024 and 1600×1200**
  (16/32 bpp, never 640×480×16).
- A Mac display (built-in Retina or a modern external one) exposes none of
  these 4:3 modes through Wine's Mac driver → empty list → NULL → crash.

**Fix (Armada.exe)**

- `0x58ab6f`: replace the whitelist with “width ≥ 640, height ≥ 480,
  width ≤ 2048” (bpp checks unchanged).
- `0x586fc0`: if no exact match exists, return the first mode of the list
  instead of NULL (`0x586fff`: `mov eax,[ecx]; mov eax,[eax+8]`; the empty-
  list path jumps to a new `xor eax,eax` at `0x58700a`).

### Things tried that did not help

- `HKCU\Software\Wine\Direct3D\VideoMemorySize = 512` (kept, harmless).
- dgVoodoo2 (D3D11 output): with wined3d/OpenGL no D3D11 device → no
  DirectDraw device at all (NULL device at `0x55b762`); with DXVK-macOS
  1.10.3 it crashed in dgVoodoo's `CreateSurface` for a mip-mapped texture.
- dxwrapper (elishacloud, Dd7to9 → D3D9, the known Windows 10 fix for
  Armada): starts, but renders worse (`could not copy emulated surface:
  D3DFMT_P8`).
- Wine's builtin ddraw: clicks work, but the menu animations are hidden
  behind the menu background (see section 2 for why).

---

## 2. Menu shows, clicks only produce the macOS alert sound

With GOG's `ddraw.dll` (loaded via `DllOverrides ddraw=native,builtin`) the
menu renders perfectly, but every click beeps.

### How Armada's menu works

The main menu and all sub-menus are **dialog boxes** (`DialogBoxParamA`, dialog
templates in the `RT_DIALOG` resources, popup style `0x82000440`) owned by the game window
(class `Armada`). The game window is disabled while a modal dialog is open
(standard Windows behaviour). The menu graphics are drawn with Direct3D into
the game window; the dialog only receives input.

GOG's wrapper scales the 640×480 shell to the screen. To hide the real
dialog controls (it composites them itself), it hooks `MoveWindow`,
`GetWindowRect`, `ScreenToClient` etc. and **moves child controls 21000 px
(0x5208) to the right and widens the dialog by 21000 px**.

### Step 1 – window too wide for macOS

Trace (`WINEDEBUG=+win`):

```
NtUserMoveWindow 0x3005e 90,0 22171x878 1
```

macOS clamps such a window (seen as `set_raw_window_pos … 2704x878`).
Patch in `ddraw.dll`: all 14 occurrences of the immediate `0x5208` →
`0x1770` (6000). The dialog now gets a sane size – but clicks still beep.

### Step 2 – it is not z-order, capture or clipping

- `WS_EX_TOPMOST` on the dialog templates: no change (kept, harmless).
- No `SetCapture` involved (`macdrv_SetCapture` never traced).
- Cursor clipping: the game and Wine (fullscreen auto-clip) call
  `ClipCursor`; Wine's Mac driver by default implements it with
  `-[NSWindow setMouseConfinementRect:]` on one window.
  `UseConfinementCursorClipping=n` was set (kept), still no change.
- `CGWindowListCopyWindowInfo` (small Swift script, see below) while the
  menu was open:

  ```
  #0 owner=wine64 layer=27 alpha=1 bounds=375,0 8257x1693   <- dialog, front
  #1 owner=wine64 layer=27 alpha=1 bounds=0,0 3008x1692     <- game window
  ```

  The dialog is **in front** and on screen, yet Cocoa delivers the clicks to
  the game window (`macdrv_mouse_button win 0x50074 …`). Wine then hit-tests
  only inside that (disabled) window → `HTERROR` → `MessageBeep`.

### Step 3 – the real cause: never-drawn windows are transparent

In `dlls/winemac.drv/cocoa_window.m` every new `WineWindow` starts with
`setOpaque:NO` and a clear background; only after its content view has drawn
for the first time (`windowDidDrawContent` → `checkTransparency`) does it
become opaque. GOG's wrapper never lets the dialog paint, so the window stays
fully transparent forever – and macOS passes clicks through transparent
windows to the window below.

Experiments with an injected helper (opacity via
`SetLayeredWindowAttributes`, one paint via `GetDC`/`FillRect`):

| dialog opacity | result |
|---|---|
| never painted | clicks fall through (original problem) |
| 1/255 | clicks fall through |
| 8/255 | clicks fall through |
| **13/255** | **clicks work**, picture practically unchanged |
| 255/255 | clicks work, but the opaque dialog hides the menu (white) |

So macOS treats windows below ~5 % opacity as click-through.

**Fix: `armfix`** (see `armfix/armfix.c`), loaded via a proxy `STRLKUP.DLL`
that forwards the game's five imports to the renamed original
`strlkup0.dll`. A background thread checks every 250 ms for visible,
caption-less popup dialogs (`#32770`) whose owner chain leads to the `Armada`
window (sub-menus are owned by other dialogs, e.g. the mission selection),
makes them layered with alpha 13/255 and paints them black once.

---

## Diagnostic helpers used

- Porting Kit wrapper: `Info.plist` → `Debug Mode` = true,
  `WINEDEBUG` = `-plugplay,+win,+cursor` (or `+macdrv`), log in
  `Contents/SharedSupport/Logs/LastRunWine.log`.
- The game's own log: `GOG Games/Star Trek Armada/Logs/logfile.txt`.
- Window list from macOS (run from Terminal, start the game right after):

  ```swift
  import CoreGraphics
  let list = (CGWindowListCopyWindowInfo([.optionOnScreenOnly], kCGNullWindowID) as? [[String: Any]]) ?? []
  for w in list where ((w[kCGWindowOwnerName as String] as? String) ?? "").lowercased().contains("wine") {
      print(w[kCGWindowNumber as String]!, w[kCGWindowLayer as String]!,
            w[kCGWindowAlpha as String]!, w[kCGWindowBounds as String]!)
  }
  ```

## Open issues

- Cmd+Tab minimizes the game to an unrestorable 1×1 window (the game handles
  `WM_ACTIVATEAPP` by minimizing; restoring under Wine/macOS fails).
