"""Installs the pinned Qt build for native-desktop from the official Qt
repository (download.qt.io), without the Qt online installer or an account.

    python native-desktop/tools/install-qt.py <toolchain-directory>

Downloads only the packages Katsodi needs, verifies every archive against
the SHA-1 published next to it, and extracts it with 7-Zip. Result:
<toolchain-directory>/Qt/6.11.3/msvc2022_64, used by CMakePresets.json
through the KATSODI_QT_DIR environment variable.

aqtinstall 3.3.0 does not understand the per-architecture repository
layout Qt introduced for 6.11 (qt6_6113/qt6_6113_msvc2022_64), hence this
small script. It downloads over HTTPS and follows no other host.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import urllib.request
import xml.etree.ElementTree as ET

QT_VERSION = "6.11.3"
ARCH = "win64_msvc2022_64"
FOLDER = "msvc2022_64"
TAG = QT_VERSION.replace(".", "")
BASE = (
    "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/"
    f"qt6_{TAG}/qt6_{TAG}_{FOLDER}/"
)
# The base package minus documentation, plus the add-ons the client uses:
# WebP artwork, Qt Multimedia, shader tools for effects, WebSockets for
# Realtime invalidation.
PACKAGES = [
    f"qt.qt6.{TAG}.{ARCH}",
    f"qt.qt6.{TAG}.addons.qtimageformats.{ARCH}",
    f"qt.qt6.{TAG}.addons.qtmultimedia.{ARCH}",
    f"qt.qt6.{TAG}.addons.qtshadertools.{ARCH}",
    f"qt.qt6.{TAG}.addons.qtwebsockets.{ARCH}",
]
SKIP_ARCHIVES = ("qtdoc-",)


def fetch(url: str) -> bytes:
    if not url.startswith("https://download.qt.io/"):
        raise SystemExit(f"refusing to download from {url}")
    with urllib.request.urlopen(url, timeout=120) as response:
        return response.read()


def download(url: str, target: str) -> None:
    if not url.startswith("https://download.qt.io/"):
        raise SystemExit(f"refusing to download from {url}")
    with urllib.request.urlopen(url, timeout=120) as response, open(
        target + ".part", "wb"
    ) as out:
        shutil.copyfileobj(response, out, 1 << 20)
    os.replace(target + ".part", target)


def sha1(path: str) -> str:
    digest = hashlib.sha1()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def find_7z() -> str:
    candidates = [
        shutil.which("7z"),
        r"C:\Program Files\7-Zip\7z.exe",
        os.path.join(
            os.path.dirname(__file__),
            "..", "..", "desktop", "node_modules", "electron-winstaller",
            "vendor", "7z.exe",
        ),
    ]
    for candidate in candidates:
        if candidate and os.path.exists(candidate):
            return os.path.abspath(candidate)
    raise SystemExit("7-Zip (7z.exe) is required to extract the Qt archives.")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = os.path.abspath(sys.argv[1])
    target = os.path.join(root, "Qt", QT_VERSION, FOLDER)
    cache = os.path.join(root, "downloads", f"qt-{TAG}-{FOLDER}")
    os.makedirs(cache, exist_ok=True)
    os.makedirs(target, exist_ok=True)
    seven = find_7z()

    updates = ET.fromstring(fetch(BASE + "Updates.xml"))
    index = {
        node.findtext("Name"): node for node in updates.findall("PackageUpdate")
    }
    for name in PACKAGES:
        package = index.get(name)
        if package is None:
            raise SystemExit(f"package {name} is not in the repository")
        version = package.findtext("Version")
        archives = [
            item.strip()
            for item in (package.findtext("DownloadableArchives") or "").split(",")
            if item.strip() and not item.strip().startswith(SKIP_ARCHIVES)
        ]
        for archive in archives:
            url = f"{BASE}{name}/{version}{archive}"
            local = os.path.join(cache, archive)
            expected = fetch(url + ".sha1").decode().split()[0].lower()
            if not (os.path.exists(local) and sha1(local) == expected):
                print(f"downloading {archive}", flush=True)
                download(url, local)
            actual = sha1(local)
            if actual != expected:
                os.remove(local)
                raise SystemExit(f"checksum mismatch for {archive}")
            subprocess.run(
                [seven, "x", "-y", "-bso0", "-bsp0", f"-o{target}", local],
                check=True,
            )
            print(f"installed {archive}", flush=True)

    # Prebuilt Qt finds its own files relative to bin/.
    with open(os.path.join(target, "bin", "qt.conf"), "w", encoding="utf-8") as conf:
        conf.write("[Paths]\nPrefix=..\n")
    print(f"Qt {QT_VERSION} ready in {target}")


if __name__ == "__main__":
    main()
