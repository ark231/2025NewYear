#!/usr/bin/env python3
from pathlib import Path
from fontTools import ttLib
from fontTools.misc.xmlWriter import XMLWriter
from dataclasses import dataclass
import struct
import numpy as np
from PIL import Image

SRCDIR = Path(__file__).parent


@dataclass
class FontMetaData:
    totalsize: int
    cmap_count: int
    strike_metadata_count: int


@dataclass
class StrikeMetaData:
    firstGlyphIndex: int
    lastGlyphIndex: int
    height: int
    width: int

    def dumps(self):
        return struct.pack("<IIBB", self.firstGlyphIndex, self.lastGlyphIndex, self.height, self.width)

    def dump(self, out):
        try:
            out.write(self.dumps())
        except AttributeError:
            with open(out, "wb") as outfile:
                outfile.write(self.dumps())


def load_ttf(path, out):
    print(path)
    print(out)

    out.mkdir(parents=True, exist_ok=True)

    font = ttLib.TTFont(path)

    print(font["maxp"].numGlyphs)
    print(font.keys())
    print(font["glyf"])
    # print(font.getGlyphOrder())

    # font.saveXML(out / "font.xml")

    loc = font["EBLC"]
    print(loc)

    # writer = XMLWriter(out / "EBLC.xml", "  ")
    # loc.toXML(writer, font)

    dat = font["EBDT"]
    print(list(dat.strikeData[0].values())[4].__dict__)

    # writer = XMLWriter(out / "EBDT.xml", "  ")
    # dat.toXML(writer, font)

    strikedats = list(dat.strikeData[0].items())

    catdat = b""

    for strike in loc.strikes:
        bmsizetab = strike.bitmapSizeTable
        print(bmsizetab.__dict__.keys())
        print(bmsizetab.vert.__dict__.keys())
        for subtab in strike.indexSubTables:
            print(subtab.__dict__.keys())
            # print(subtab.metrics.__dict__.keys())
            first, last = subtab.firstGlyphIndex, subtab.lastGlyphIndex
            try:
                met = subtab.metrics
            except AttributeError:
                continue
            w, h = met.width, met.height
            print(f"{h}x{w}")
            for i in range(first, last + 1):
                name, strikedat = strikedats[i]
                if i == first:
                    print("from", name)
                elif i == last:
                    print("to  ", name)
                try:
                    imdat = strikedat.data
                except AttributeError as e:
                    imdat = strikedat.imageData
                catdat += imdat
                # im = Image.frombuffer("1", (w, h), imdat, "raw", "1", 0, 1)
                # im.save(out / f"{i:04X}.png")
                buf = np.frombuffer(imdat, np.dtype(">H"))
                imbuf = np.zeros((w), dtype=np.dtype("<H"))
                outdir = out / "pbm" / "raw"
                outdir.mkdir(parents=True, exist_ok=True)
                with open(outdir / f"{name}.pbm", "w", encoding="utf8") as outfile:
                    print("P1", file=outfile)
                    print(w, h, file=outfile)
                    for col in buf:
                        print(f"{col:016b}", file=outfile)
                # x = 0
                # y = 0
                # for b in imdat:
                #     pix = 0 if b & (1 << (x % 8)) == 0 else 1
                #     imbuf[x]
                #     x += 1
                #     if x == w:
                #         x = 0
                #         y += 1
                bitcount = 0
                for y in range(h - 1, -1, -1):
                    for x in range(w - 1, -1, -1):
                        pix = 0 if imdat[bitcount // 8] & (1 << (7 - (bitcount % 8))) == 0 else 1
                        imbuf[x] |= pix << y
                        bitcount += 1
                outdir = out / "pbm" / "rot"
                outdir.mkdir(parents=True, exist_ok=True)
                with open(outdir / f"{name}.pbm", "w", encoding="utf8") as outfile:
                    print("P1", file=outfile)
                    print(h, w, file=outfile)
                    for col in imbuf:
                        print(f"{col:016b}", file=outfile)

    print(len(catdat))
    catbuf = np.frombuffer(catdat[: 2 * (16 * 4)], np.uint16)
    catbuf = catbuf.reshape((1, -1))
    print(catbuf)
    nrow, ncol = catbuf.T.shape
    # im = Image.frombuffer("1", (8, int(8 * len(catdat) / 2)), catdat, "raw", "1", 0, 1)
    # im = Image.frombuffer("1", (ncol * 16, nrow * 16), catdat, "raw", "1", 0, 1)
    # im.save(out / "im.png")
    # im.show()
    # for col in catbuf[0, : 16 * 4]:
    #     print(f"{col:016b}")


def main():
    # load_ttf(SRCDIR / "data" / "DotGothic16-Regular-bitmap.ttf", SRCDIR / "out" / "dgr")
    # load_ttf(SRCDIR / "data" / "misaki_ttf_2021-05-05" / "misaki_gothic_2nd.ttf", SRCDIR / "out" / "misaki")
    # for pth in (SRCDIR / "data" / "KH-Dot").glob("*"):
    for pth in (SRCDIR / "data" / "KH-Dot").glob("*Akihabara*"):
        load_ttf(pth, SRCDIR / "out" / (pth.stem))


if __name__ == "__main__":
    main()
