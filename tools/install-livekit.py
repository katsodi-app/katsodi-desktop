"""Installs the pinned LiveKit C++ SDK (prebuilt) for native-desktop.

    python native-desktop/tools/install-livekit.py <toolchain-directory>

Downloads the official Windows x64 build of client-sdk-cpp from
GitHub releases, checks the SHA-256 that GitHub publishes for the asset
(pinned below), unpacks it to <toolchain>/livekit/<version>/ and adds the
SDK's Apache-2.0 LICENSE from the same tag. No Rust toolchain is needed.
"""
import hashlib
import os
import sys
import urllib.request
import zipfile

VERSION = "1.13.1"
ASSET = f"livekit-sdk-windows-x64-{VERSION}"
SHA256 = "6696ee87c78b958bdbf70e5eb39bf85e1c7528ca2a707e6c13b042c8e4360b9d"
URL = f"https://github.com/livekit/client-sdk-cpp/releases/download/v{VERSION}/{ASSET}.zip"
LICENSE_URL = f"https://raw.githubusercontent.com/livekit/client-sdk-cpp/v{VERSION}/LICENSE"


def fetch(url: str) -> bytes:
    if not url.startswith(("https://github.com/livekit/", "https://raw.githubusercontent.com/livekit/")):
        raise SystemExit(f"refusing to download from {url}")
    with urllib.request.urlopen(url, timeout=300) as response:
        return response.read()


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = os.path.abspath(sys.argv[1])
    downloads = os.path.join(root, "downloads")
    target = os.path.join(root, "livekit", VERSION)
    os.makedirs(downloads, exist_ok=True)
    archive = os.path.join(downloads, f"{ASSET}.zip")
    if not os.path.exists(archive) or hashlib.sha256(open(archive, "rb").read()).hexdigest() != SHA256:
        print(f"downloading {ASSET}.zip", flush=True)
        data = fetch(URL)
        if hashlib.sha256(data).hexdigest() != SHA256:
            raise SystemExit("checksum mismatch: the download is not the pinned SDK")
        with open(archive, "wb") as out:
            out.write(data)
    with zipfile.ZipFile(archive) as bundle:
        for name in bundle.namelist():
            # Refuse absolute paths and traversal inside the archive.
            if name.startswith(("/", "\\")) or ".." in name.replace("\\", "/").split("/"):
                raise SystemExit(f"unsafe path in archive: {name}")
        bundle.extractall(target)
    sdk = os.path.join(target, ASSET)
    with open(os.path.join(sdk, "LICENSE"), "wb") as out:
        out.write(fetch(LICENSE_URL))
    print(f"LiveKit C++ SDK {VERSION} ready in {sdk}")


if __name__ == "__main__":
    main()
