"""Converts the website's self-hosted Manrope WOFF2 subsets into TrueType
files that Qt's font database loads on every platform.

    python native-desktop/tools/convert-fonts.py

Needs fontTools and brotli (build-time only). Outputs are committed to
native-desktop/resources/fonts together with the OFL licence.
"""
import os
import shutil
from fontTools.ttLib import TTFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(ROOT, "public", "fonts", "manrope")
TARGET = os.path.join(ROOT, "native-desktop", "resources", "fonts")
os.makedirs(TARGET, exist_ok=True)
for subset in ("latin", "cyrillic", "latin-ext", "cyrillic-ext"):
    font = TTFont(os.path.join(SOURCE, f"manrope-{subset}.woff2"))
    font.flavor = None
    font.save(os.path.join(TARGET, f"manrope-{subset}.ttf"))
    print(f"manrope-{subset}.ttf")
shutil.copy(os.path.join(SOURCE, "OFL.txt"), os.path.join(TARGET, "OFL.txt"))
