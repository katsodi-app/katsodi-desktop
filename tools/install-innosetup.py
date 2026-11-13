"""Installs the pinned Inno Setup compiler for building the Katsodi installer.

    python native-desktop/tools/install-innosetup.py <toolchain-directory>

Downloads the official Inno Setup release from GitHub (jrsoftware/issrc),
checks the SHA-256 GitHub publishes for the asset (pinned below) and runs
it in portable, per-user, silent mode into <toolchain>/innosetup/<version>/.
Nothing is registered system-wide and no admin rights are needed.
"""
import hashlib
import os
import subprocess
import sys
import urllib.request

VERSION = "6.7.4"
SHA256 = "a549929319f9120716677c685535ca4671e4b937b5fdc87e2585f32d8eb30e8c"
URL = f"https://github.com/jrsoftware/issrc/releases/download/is-{VERSION.replace('.', '_')}/innosetup-{VERSION}.exe"


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = os.path.abspath(sys.argv[1])
    downloads = os.path.join(root, "downloads")
    target = os.path.join(root, "innosetup", VERSION)
    if os.path.exists(os.path.join(target, "ISCC.exe")):
        print(f"Inno Setup {VERSION} already in {target}")
        return
    os.makedirs(downloads, exist_ok=True)
    setup = os.path.join(downloads, f"innosetup-{VERSION}.exe")
    if not os.path.exists(setup) or hashlib.sha256(open(setup, "rb").read()).hexdigest() != SHA256:
        if not URL.startswith("https://github.com/jrsoftware/"):
            raise SystemExit(f"refusing to download from {URL}")
        print(f"downloading innosetup-{VERSION}.exe", flush=True)
        with urllib.request.urlopen(URL, timeout=300) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != SHA256:
            raise SystemExit("checksum mismatch for Inno Setup")
        with open(setup, "wb") as out:
            out.write(data)
    subprocess.run(
        [setup, "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/NOICONS",
         "/CURRENTUSER", "/PORTABLE=1", f"/DIR={target}"],
        check=True,
    )
    if not os.path.exists(os.path.join(target, "ISCC.exe")):
        raise SystemExit("Inno Setup did not install ISCC.exe")
    print(f"Inno Setup {VERSION} ready in {target}")


if __name__ == "__main__":
    main()
