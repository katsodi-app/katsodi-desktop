"""Installs the pinned Microsoft Edge WebView2 SDK for native-desktop.

    python native-desktop/tools/install-webview2.py <toolchain-directory>

Downloads Microsoft.Web.WebView2 from nuget.org, checks the SHA-256 pinned
below (the package's SHA-512 matched the hash nuget.org publishes for it
when it was pinned), and unpacks the headers, the static loader library and
the licence to <toolchain>/webview2/<version>/. The SDK builds the
provider-player helper; the WebView2 *runtime* itself is part of Windows 11
(and installable on Windows 10) and is not redistributed by Katsodi.
"""
import hashlib
import os
import sys
import urllib.request
import zipfile

VERSION = "1.0.4314.35"
SHA256 = "90778deb1b77e36292828b4b5833c9255c27533f40e0ddd55891ae718944edc4"
URL = (
    "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/"
    f"{VERSION}/microsoft.web.webview2.{VERSION}.nupkg"
)
KEEP = ("build/native/include/", "build/native/x64/", "LICENSE.txt")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = os.path.abspath(sys.argv[1])
    downloads = os.path.join(root, "downloads")
    target = os.path.join(root, "webview2", VERSION)
    os.makedirs(downloads, exist_ok=True)
    archive = os.path.join(downloads, f"microsoft.web.webview2.{VERSION}.nupkg")
    if not os.path.exists(archive) or hashlib.sha256(open(archive, "rb").read()).hexdigest() != SHA256:
        if not URL.startswith("https://api.nuget.org/"):
            raise SystemExit(f"refusing to download from {URL}")
        print(f"downloading Microsoft.Web.WebView2 {VERSION}", flush=True)
        with urllib.request.urlopen(URL, timeout=300) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != SHA256:
            raise SystemExit("checksum mismatch for the WebView2 SDK")
        with open(archive, "wb") as out:
            out.write(data)
    with zipfile.ZipFile(archive) as package:
        for name in package.namelist():
            if not name.startswith(KEEP) or name.endswith("/"):
                continue
            destination = os.path.normpath(os.path.join(target, name))
            if not destination.startswith(target + os.sep):
                raise SystemExit(f"unsafe path in package: {name}")
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            with package.open(name) as source, open(destination, "wb") as out:
                out.write(source.read())
    print(f"WebView2 SDK {VERSION} ready in {target}")


if __name__ == "__main__":
    main()
