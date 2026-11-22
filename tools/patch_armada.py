#!/usr/bin/env python3
"""
armada-macos-fix: patch tool for Star Trek: Armada (GOG, v1.2 "RDNA fix")
running under Wine on macOS (Porting Kit / Sikarugir / Wineskin wrappers).

Patches YOUR OWN copies of Armada.exe and GOG's DirectX wrapper (ddraw.dll),
installs the armfix helper DLL and sets the required Wine registry values.
Nothing from GOG/Activision is shipped with this tool.

Usage:
    python3 patch_armada.py "/Applications/Star Trek Armada.app"
    python3 patch_armada.py --restore "/Applications/Star Trek Armada.app"

Quit the game (and all Wine processes of the wrapper) before running it.
"""
import argparse
import hashlib
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ARMFIX_DLL = os.path.join(HERE, "..", "bin", "Strlkup.dll")

GAME_SUBDIR = os.path.join("drive_c", "GOG Games", "Star Trek Armada")

# --------------------------------------------------------------------------
# Armada.exe  (GOG "1.2 RDNA fix", md5 below)
# --------------------------------------------------------------------------
EXE_MD5_ORIG = "75e9b46b9765b17b11ae810b781d365a"
EXE_MD5_PATCHED = "b2c8914261185b0c998a83781d0bfbe4"

# (file offset, patched bytes hex). The original file is identified by its
# MD5 above, so no original game bytes need to be stored here.

# 1) Display-mode lookup (VA 0x586fc0): if the requested WxHxBPP is not in the
#    adapter's mode list, return the first available mode instead of NULL.
#    Without it the game dereferences NULL at startup (page fault at 0x4).
EXE_MODE_FALLBACK = [
    (0x186fcb, "3e"),
    (0x186fee, "15"),
    (0x186fff, "8b018b40085f5e5dc20c0033c0ebf6"),
]

# 2) EnumDisplayModes callback (VA 0x58ab50): accept every mode from 640x480
#    up to 2048 px width instead of only 640x480/800x600/1024x768/1280x1024/
#    1600x1200, which a Mac display usually does not offer.
EXE_MODE_WHITELIST = [
    (0x18ab6f, "8b460881fa800200000f82ab0000003de00100000f82a000000081fa000800000f8794000000eb329090909090909090909090909090909090909090909090909090909090909090909090909090909090909090909090909090"),
]

# 3) Shell dialog templates (RT_DIALOG, DIALOGEX style 0x82000440):
#    add WS_EX_TOPMOST to exStyle (32 dialogs).
EXE_DIALOG_TOPMOST = [
    (0x238ed0, "0c"),
    (0x239568, "0c"),
    (0x23965c, "0c"),
    (0x239690, "08"),
    (0x2397c4, "0c"),
    (0x2397f8, "0c"),
    (0x23982c, "0c"),
    (0x239860, "0c"),
    (0x2398b4, "0c"),
    (0x239908, "0c"),
    (0x23995c, "0c"),
    (0x239990, "0c"),
    (0x2399c4, "0c"),
    (0x2399f8, "0c"),
    (0x239a2c, "0c"),
    (0x239a60, "0c"),
    (0x23a11c, "0c"),
    (0x23a150, "0c"),
    (0x23b024, "0c"),
    (0x23b078, "0c"),
    (0x23b8b4, "08"),
    (0x23bf38, "08"),
    (0x23bfdc, "08"),
    (0x23c040, "08"),
    (0x23c0c4, "08"),
    (0x23c128, "08"),
    (0x23c16c, "08"),
    (0x23c1b0, "08"),
    (0x23c254, "08"),
    (0x23c2d8, "08"),
    (0x23c39c, "08"),
    (0x23c460, "08"),
]

EXE_PATCHES = EXE_MODE_FALLBACK + EXE_MODE_WHITELIST + EXE_DIALOG_TOPMOST

# --------------------------------------------------------------------------
# ddraw.dll  (GOG.com DirectX 1-7 wrapper shipped with Armada, md5 below)
# --------------------------------------------------------------------------
DDRAW_MD5_ORIG = "096f1531c89e85032827d782a3108ad4"
DDRAW_MD5_PATCHED = "e48a7fcde2b28cb31b20f20475f6dbce"

# The wrapper hides the real dialog controls by moving them 21000 px
# (0x5208) to the right and widens the dialog by the same amount. macOS can't
# handle windows that wide (>10000 pt). Reduce the offset to 6000 (0x1770)
# in all 14 places (MoveWindow / GetWindowRect / ScreenToClient hooks).
DDRAW_PATCHES = [
    (0x0451c0, "7017"),
    (0x0451eb, "7017"),
    (0x045277, "7017"),
    (0x0452a2, "7017"),
    (0x045591, "7017"),
    (0x0455b0, "7017"),
    (0x0469ea, "7017"),
    (0x046a12, "7017"),
    (0x046a90, "7017"),
    (0x046ab9, "7017"),
    (0x0488e7, "7017"),
    (0x04893c, "7017"),
    (0x0489f0, "7017"),
    (0x048a29, "7017"),
]

# --------------------------------------------------------------------------
# Wine registry (HKCU, user.reg)
# --------------------------------------------------------------------------
REG_VALUES = [
    # load GOG's ddraw.dll from the game folder instead of Wine's builtin one
    ("Software\\\\Wine\\\\DllOverrides", "ddraw", "native,builtin"),
    # some old D3D games misbehave with the huge unified memory of Apple Silicon
    ("Software\\\\Wine\\\\Direct3D", "VideoMemorySize", "512"),
    # don't tie cursor clipping to one Cocoa window (it pins clicks to it)
    ("Software\\\\Wine\\\\Mac Driver", "UseConfinementCursorClipping", "n"),
    # "always on top" menu dialogs must not float above other apps after Cmd+Tab
    ("Software\\\\Wine\\\\Mac Driver", "WindowsFloatWhenInactive", "none"),
]


def md5(path):
    with open(path, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()


def apply_patches(path, patches, md5_orig, md5_patched, name):
    cur = md5(path)
    if cur == md5_patched:
        print(f"  {name}: already patched")
        return
    if cur != md5_orig:
        sys.exit(f"  {name}: unknown version (md5 {cur}); expected the GOG "
                 f"'1.2 RDNA fix' build ({md5_orig}). Aborting, nothing changed.")
    data = bytearray(open(path, "rb").read())
    for off, new in patches:
        new_b = bytes.fromhex(new)
        data[off:off + len(new_b)] = new_b
    if hashlib.md5(data).hexdigest() != md5_patched:
        sys.exit(f"  {name}: result hash mismatch; aborting, nothing written.")
    backup = path + ".orig"
    if not os.path.exists(backup):
        shutil.copy2(path, backup)
    with open(path, "wb") as f:
        f.write(data)
    print(f"  {name}: patched (backup: {os.path.basename(backup)})")


def install_armfix(game):
    strl = find_file(game, "strlkup.dll")
    orig = os.path.join(game, "strlkup0.dll")
    if not os.path.exists(ARMFIX_DLL):
        sys.exit(f"  armfix: {ARMFIX_DLL} not found (build it with armfix/build.sh)")
    if not os.path.exists(orig):
        if not strl:
            sys.exit("  armfix: STRLKUP.DLL not found in the game folder")
        os.rename(strl, orig)
        strl = None
    target = strl or os.path.join(game, "STRLKUP.DLL")
    shutil.copy2(ARMFIX_DLL, target)
    cfg = os.path.join(game, "armfix.txt")
    if not os.path.exists(cfg):
        with open(cfg, "w") as f:
            f.write("13")
    print("  armfix: installed (original STRLKUP.DLL kept as strlkup0.dll)")


def find_file(folder, lower_name):
    for n in os.listdir(folder):
        if n.lower() == lower_name:
            return os.path.join(folder, n)
    return None


def set_reg_values(user_reg):
    with open(user_reg, "r", encoding="utf-8", errors="surrogateescape", newline="") as f:
        text = f.read()
    nl = "\r\n" if "\r\n" in text else "\n"
    if not os.path.exists(user_reg + ".orig"):
        shutil.copy2(user_reg, user_reg + ".orig")
    for key, name, value in REG_VALUES:
        header = "[" + key + "]"
        line = f'"{name}"="{value}"'
        start = text.find(header)
        if start < 0:
            text = text.rstrip(nl) + nl + nl + header + f" {int(time.time())}" + nl + line + nl
            continue
        body_start = text.find(nl, start) + len(nl)
        end = text.find(nl + "[", body_start)
        end = len(text) if end < 0 else end + len(nl)
        body = text[body_start:end]
        lines = body.split(nl)
        prefix = f'"{name}"='
        replaced = False
        for i, l in enumerate(lines):
            if l.lower().startswith(prefix.lower()):
                lines[i] = line
                replaced = True
        if not replaced:
            # insert after the last non-empty line of the section
            idx = len(lines)
            while idx > 0 and lines[idx - 1].strip() == "":
                idx -= 1
            lines.insert(idx, line)
        text = text[:body_start] + nl.join(lines) + text[end:]
    with open(user_reg, "w", encoding="utf-8", errors="surrogateescape", newline="") as f:
        f.write(text)
    print("  registry: ddraw override, VideoMemorySize, UseConfinementCursorClipping,\n"
          "            WindowsFloatWhenInactive set")


def restore(game, prefix):
    for name in ("Armada.exe", "ddraw.dll"):
        p = find_file(game, name.lower())
        if p and os.path.exists(p + ".orig"):
            shutil.copy2(p + ".orig", p)
            print(f"  {name}: restored")
    orig = os.path.join(game, "strlkup0.dll")
    if os.path.exists(orig):
        strl = find_file(game, "strlkup.dll")
        if strl:
            os.remove(strl)
        os.rename(orig, os.path.join(game, "STRLKUP.DLL"))
        print("  armfix: removed, original STRLKUP.DLL restored")
    reg = os.path.join(prefix, "user.reg")
    if os.path.exists(reg + ".orig"):
        shutil.copy2(reg + ".orig", reg)
        print("  registry: restored")


def locate(path):
    path = os.path.abspath(os.path.expanduser(path))
    if path.endswith(".app"):
        prefix = os.path.join(path, "Contents", "SharedSupport", "prefix")
    elif os.path.exists(os.path.join(path, "user.reg")):
        prefix = path
    else:
        sys.exit("Pass the wrapper (.app) or the Wine prefix folder.")
    game = os.path.join(prefix, GAME_SUBDIR)
    if not os.path.isdir(game):
        sys.exit(f"Game folder not found: {game}")
    return prefix, game


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", help="Porting Kit wrapper (.app) or Wine prefix")
    ap.add_argument("--restore", action="store_true", help="undo all changes")
    args = ap.parse_args()
    prefix, game = locate(args.target)
    print(f"Game folder: {game}")
    if args.restore:
        restore(game, prefix)
        return
    apply_patches(find_file(game, "armada.exe"), EXE_PATCHES,
                  EXE_MD5_ORIG, EXE_MD5_PATCHED, "Armada.exe")
    apply_patches(find_file(game, "ddraw.dll"), DDRAW_PATCHES,
                  DDRAW_MD5_ORIG, DDRAW_MD5_PATCHED, "ddraw.dll")
    install_armfix(game)
    set_reg_values(os.path.join(prefix, "user.reg"))
    print("Done. Start the game through Porting Kit.")


if __name__ == "__main__":
    main()
