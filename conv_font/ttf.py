from pathlib import Path
from fontTools import ttLib

SRCDIR = Path(__file__).parent


def load_ttf(path, out):
    print(path)
    font = ttLib.TTFont(path)
    print(font["maxp"].numGlyphs)
    print(font.keys())
    print(font["glyf"])
    # print(font.getGlyphOrder())
    # font.saveXML(out)


def main():
    load_ttf(SRCDIR / "data" / "DotGothic16-Regular-bitmap.ttf", SRCDIR / "out" / "dgr.xml")
    load_ttf(SRCDIR / "data" / "misaki_ttf_2021-05-05" / "misaki_gothic_2nd.ttf", SRCDIR / "out" / "misaki.xml")


if __name__ == "__main__":
    main()
