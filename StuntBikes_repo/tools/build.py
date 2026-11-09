#!/usr/bin/env python3
"""Build the Stunt Bikes release folder.

    python tools/build.py                 # dist/@StuntBikes with the PBO and mod.cpp
    python tools/build.py --sign KEY      # also sign it (Windows, DayZ Tools installed)
    python tools/build.py --test          # also pack the test helper into dist/@StuntBikesTest

KEY is the path to your .biprivatekey without extension, or a folder that contains
exactly one .biprivatekey. Create one once with DSCreateKey.exe (DayZ Tools\\Bin\\DsUtils).
Never commit the .biprivatekey.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import pbo  # noqa: E402

DIST = os.path.join(ROOT, "dist")
DEFAULT_DSUTILS = r"C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\DsUtils"

MOD_CPP = """name = "Stunt Bikes";
picture = "";
logo = "";
logoSmall = "";
logoOver = "";
tooltip = "Realistic wheelies, stoppies and flips for vanilla DayZ motorbikes";
overview = "Throttle-controlled wheelies with a balance point, front-brake stoppies, air control with gas / rear brake / body lean, safe landings. Server and client stay in sync during stunts.";
action = "";
author = "V";
version = "1.0";
"""


def find_key(key):
    if os.path.isdir(key):
        keys = glob.glob(os.path.join(key, "*.biprivatekey"))
        if len(keys) != 1:
            sys.exit(f"expected exactly one .biprivatekey in {key}, found {len(keys)}")
        return keys[0]
    if not key.endswith(".biprivatekey"):
        key += ".biprivatekey"
    if not os.path.isfile(key):
        sys.exit(f"key not found: {key}")
    return key


def sign(pbo_path, key_path, dsutils):
    signer = os.path.join(dsutils, "DSSignFile.exe")
    if not os.path.isfile(signer):
        sys.exit(f"DSSignFile.exe not found in {dsutils} (use --dsutils)")
    for old in glob.glob(pbo_path + ".*.bisign"):
        os.remove(old)
    subprocess.run([signer, key_path, pbo_path], check=True)
    bikey = key_path[: -len(".biprivatekey")] + ".bikey"
    keys_dir = os.path.join(os.path.dirname(os.path.dirname(pbo_path)), "keys")
    os.makedirs(keys_dir, exist_ok=True)
    shutil.copy2(bikey, keys_dir)
    print(f"signed with {os.path.basename(key_path)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sign", metavar="KEY", help="sign the PBO with this .biprivatekey")
    ap.add_argument("--dsutils", default=DEFAULT_DSUTILS, help="folder with DSSignFile.exe")
    ap.add_argument("--test", action="store_true", help="also build the test helper server mod")
    args = ap.parse_args()

    mod = os.path.join(DIST, "@StuntBikes")
    addon = os.path.join(mod, "addons", "StuntBikes.pbo")
    pbo.pack(os.path.join(ROOT, "StuntBikes"), addon)
    with open(os.path.join(mod, "mod.cpp"), "w", encoding="utf-8", newline="\r\n") as f:
        f.write(MOD_CPP)
    shutil.copy2(os.path.join(ROOT, "config.example.json"), os.path.join(mod, "config.example.json"))

    if args.sign:
        sign(addon, find_key(args.sign), args.dsutils)

    if args.test:
        pbo.pack(os.path.join(ROOT, "test", "StuntBikesTest"),
                 os.path.join(DIST, "@StuntBikesTest", "addons", "StuntBikesTest.pbo"))

    print(f"done: {mod}")


if __name__ == "__main__":
    main()
