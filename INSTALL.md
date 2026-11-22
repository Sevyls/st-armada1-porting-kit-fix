# Installation guide

Play the GOG version of **Star Trek: Armada** on your Mac – step by step, no
technical knowledge needed. Plan about 20–30 minutes.

> Throughout this guide, **Terminal** means the app *Terminal* (open it with
> Spotlight: press <kbd>⌘ Cmd</kbd> + <kbd>Space</kbd>, type `Terminal`,
> press <kbd>Return</kbd>). You only need to copy and paste a few lines.

---

## What you need

- A Mac with **Apple Silicon** and **macOS 27 or earlier** (tested on
  macOS 27.2). **macOS 28 and later are not expected to work**: Apple removes
  most of Rosetta 2 there, which Wine/Porting Kit need. Intel Macs: not tested.
- **Rosetta 2** – if it isn't installed yet, macOS asks you to install it the
  first time Porting Kit starts a Windows game. Click **Install**.
- **Star Trek: Armada from GOG.com** – your own purchased copy.
- **Porting Kit** (free): <https://www.portingkit.com>
- About 1 GB of free disk space.

---

## Step 1 – Download the game installer from GOG

1. Log in at <https://www.gog.com>, open **Account → Games** and click
   *Star Trek: Armada*.
2. Under **Download offline backup game installers** choose **Windows**
   and download the installer file(s). If GOG also lists a **patch** to
   version **“1.2 RDNA fix”**, download it as well.
3. Leave the files in your **Downloads** folder.

> This fix only works with the GOG version **1.2 RDNA fix**. The tool in
> step 4 checks this and won't change anything if you have a different version.

---

## Step 2 – Install the game with Porting Kit

Porting Kit has no ready-made package for Armada 1 (the “Armada” you may see
there is *Star Trek: Armada II* or an unrelated *Wing Commander* game), so we
create a **Custom Port**:

1. Install and open **Porting Kit**.
2. Go to **Games → All Games** and click the tile **Custom Port**.
3. Name it **`Star Trek Armada`** and continue.
4. When asked what kind of port to create, choose **Wine**.
5. Keep the suggested engine and settings and click **Next** until Porting Kit
   asks for the installer. (Tested with the engine *wine sikarugir 10.0* –
   the full tested configuration is listed in
   [docs/CONFIGURATION.md](docs/CONFIGURATION.md).)
6. Select the GOG installer from your **Downloads** folder (the `.exe` file;
   if you also downloaded a patch, install the main installer first, then run
   the patch the same way via the wrapper or Porting Kit).
7. The normal Windows installer appears. Click through it **without changing
   the install folder** (it must stay `C:\GOG Games\Star Trek Armada`).
8. At the end, **don't start the game** from the installer – just close it.
9. If Porting Kit asks which program to start, pick
   `GOG Games/Star Trek Armada/Armada.exe`.

You now have an app called **Star Trek Armada** (usually in your
**Applications** folder). If you start it now, the game will most likely
crash or the menu won't react to clicks – that's what this fix is for.
**Make sure the game is closed** before you continue.

---

## Step 3 – Download this fix

1. On this project's GitHub page, click the green **Code** button →
   **Download ZIP**.
2. Open your **Downloads** folder and double-click the ZIP to unpack it.
   You get a folder named `st-armada1-porting-kit-fix-main`.

---

## Step 4 – Apply the fix

1. Make sure Star Trek Armada is **not running**. (If in doubt: press
   <kbd>⌘ Cmd</kbd> + <kbd>⌥ Option</kbd> + <kbd>Esc</kbd>, select
   *Star Trek Armada* or *wine* and click **Force Quit**.)
2. Open **Terminal**.
3. Type `cd ` (with a space after it), then **drag the unpacked
   `st-armada1-porting-kit-fix-main` folder** from Finder into the Terminal
   window and press <kbd>Return</kbd>.
4. Type `python3 tools/patch_armada.py ` (with a space at the end), then
   **drag the Star Trek Armada app** (from your Applications folder) into the
   Terminal window and press <kbd>Return</kbd>.

   It will look roughly like this:
   ```
   python3 tools/patch_armada.py /Applications/Star\ Trek\ Armada.app
   ```

   > **First time using `python3`?** macOS may say that the *command line
   > developer tools* are required and offer to install them. Click
   > **Install**, wait until it's finished, and run the command again.

5. If everything worked, you see:
   ```
     Armada.exe: patched (backup: Armada.exe.orig)
     ddraw.dll: patched (backup: ddraw.dll.orig)
     armfix: installed (original STRLKUP.DLL kept as strlkup0.dll)
     registry: ddraw override, VideoMemorySize, UseConfinementCursorClipping,
               WindowsFloatWhenInactive set
   Done. Start the game through Porting Kit.
   ```

---

## Step 5 – Play

Start **Star Trek Armada** from Porting Kit or your Applications folder.

- The game starts in full screen. If the intro plays behind other windows,
  click once into the game.
- You can skip the intro with <kbd>Esc</kbd>.
- In the main menu, **Single Player**, **Options** and all sub-menus should
  now react to your clicks.

To change the resolution, use the game's **Options** menu.

---

## Good to know

- **Switching to other apps with ⌘ Cmd + Tab works.** The game becomes
  invisible and keeps running in the background (including sound – pause a
  running mission first). To come back, press <kbd>⌘ Cmd</kbd> + <kbd>Tab</kbd>
  again or click the *Star Trek Armada* icon in the Dock.
- When a new menu opens, it may take a fraction of a second until clicks work.
- The menus may look a tiny bit darker than usual – that's part of the fix.

---

## Troubleshooting

**The tool says “unknown version … Aborting, nothing changed.”**
Your game files are not the GOG version *1.2 RDNA fix* (or they were
modified). Re-download the current installer from GOG and install again
(Step 1 and 2). Nothing was changed.

**“Game folder not found”**
The game was installed into a different folder than
`C:\GOG Games\Star Trek Armada`. Reinstall and keep the default folder.

**The game still crashes at startup**
Run the tool again – it tells you what is already patched. Make sure the game
was closed while patching.

**Clicks in the menu still only make the Mac “alert” sound**
In the game folder (inside the app: right-click **Star Trek Armada** →
**Show Package Contents** → `Contents/SharedSupport/prefix/drive_c/GOG Games/Star Trek Armada`)
open `armfix.txt` with TextEdit and change the number `13` to `20`. Save,
restart the game.

**I want everything back the way it was**
In Terminal, from the fix folder (see Step 4):
```
python3 tools/patch_armada.py --restore /Applications/Star\ Trek\ Armada.app
```
(again, you can drag the app into the Terminal window instead of typing the path).

**Still stuck?**
Open an issue on GitHub and describe what happens. If the game crashes, the
crash window shows a log – copy it into the issue.

---

Tested wrapper, Wine and registry settings: [docs/CONFIGURATION.md](docs/CONFIGURATION.md)  
Technical background: [docs/INVESTIGATION.md](docs/INVESTIGATION.md)
