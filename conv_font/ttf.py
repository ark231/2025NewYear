#!/usr/bin/env python3
import argparse
from pathlib import Path
from fontTools import ttLib
from fontTools.misc.xmlWriter import XMLWriter
from dataclasses import dataclass
import struct
import numpy as np
import tomllib
from PIL import Image

from logging import getLogger, config

logger = getLogger(__name__)

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


def load_ttf(path, out, out_xml, out_pbm):
    logger.info(path)
    logger.info(out)

    out.mkdir(parents=True, exist_ok=True)

    font = ttLib.TTFont(path)

    logger.debug(font["maxp"].numGlyphs)
    logger.debug(font.keys())
    logger.debug(font["glyf"])
    # print(font.getGlyphOrder())

    if out_xml:
        font.saveXML(out / "font.xml")

    loc = font["EBLC"]
    logger.debug(loc)

    if out_xml:
        writer = XMLWriter(out / "EBLC.xml", "  ")
        loc.toXML(writer, font)

    dat = font["EBDT"]
    logger.debug(list(dat.strikeData[0].values())[4].__dict__)

    if out_xml:
        writer = XMLWriter(out / "EBDT.xml", "  ")
        dat.toXML(writer, font)

    strikedats = list(dat.strikeData[0].items())

    catdat = b""

    for strike in loc.strikes:
        bmsizetab = strike.bitmapSizeTable
        logger.debug(bmsizetab.__dict__.keys())
        logger.debug(bmsizetab.vert.__dict__.keys())
        for subtab in strike.indexSubTables:
            logger.debug(subtab.__dict__.keys())
            # print(subtab.metrics.__dict__.keys())
            first, last = subtab.firstGlyphIndex, subtab.lastGlyphIndex
            try:
                met = subtab.metrics
            except AttributeError:
                continue
            w, h = met.width, met.height
            logger.info(f"{h}x{w}")
            for i in range(first, last + 1):
                name, strikedat = strikedats[i]
                if i == first:
                    logger.info("from %s", name)
                elif i == last:
                    logger.info("to   %s", name)
                try:
                    imdat = strikedat.data
                except AttributeError as e:
                    imdat = strikedat.imageData
                buf = np.frombuffer(imdat, np.dtype(">H"))

                if out_pbm:
                    outdir = out / "pbm" / "raw"
                    outdir.mkdir(parents=True, exist_ok=True)
                    with open(outdir / f"{name}.pbm", "w", encoding="utf8") as outfile:
                        print("P1", file=outfile)
                        print(w, h, file=outfile)
                        for col in buf:
                            print(f"{col:016b}", file=outfile)

                imbuf = np.zeros((w), dtype=np.dtype("<H"))
                bitcount = 0
                for y in range(h - 1, -1, -1):
                    for x in range(w):
                        pix = 0 if imdat[bitcount // 8] & (1 << (7 - (bitcount % 8))) == 0 else 1
                        imbuf[x] |= pix << y
                        bitcount += 1
                catdat += imbuf.tobytes()

                if out_pbm:
                    outdir = out / "pbm" / "rot"
                    outdir.mkdir(parents=True, exist_ok=True)
                    with open(outdir / f"{name}.pbm", "w", encoding="utf8") as outfile:
                        print("P1", file=outfile)
                        print(h, w, file=outfile)
                        for col in imbuf:
                            print(f"{col:016b}", file=outfile)

    logger.debug(len(catdat))
    catbuf = np.frombuffer(catdat[: 2 * (16 * 4)], np.uint16)
    catbuf = catbuf.reshape((1, -1))
    logger.debug(catbuf)


def init_argument_parser():
    parser = argparse.ArgumentParser("ttf.py", description="extract embedded bitmap data from ttf file")
    parser.add_argument("--src", type=Path, help="source font file", required=True)
    parser.add_argument("--dst", type=Path, help="destination directory", required=True)
    parser.add_argument(
        "--log-level",
        type=str,
        help="log level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
    )
    parser.add_argument("--xml", action="store_true")
    parser.add_argument("--pbm", action="store_true")
    return parser


def main(args):
    logger.setLevel(args.log_level)
    # load_ttf(SRCDIR / "data" / "DotGothic16-Regular-bitmap.ttf", SRCDIR / "out" / "dgr")
    # load_ttf(SRCDIR / "data" / "misaki_ttf_2021-05-05" / "misaki_gothic_2nd.ttf", SRCDIR / "out" / "misaki")
    # for pth in (SRCDIR / "data" / "KH-Dot").glob("*"):
    # for pth in (SRCDIR / "data" / "KH-Dot").glob("*Akihabara*"):
    #     load_ttf(pth, SRCDIR / "out" / (pth.stem))
    load_ttf(args.src, args.dst, args.xml, args.pbm)


if __name__ == "__main__":
    with open(Path(__file__).parent / "log_config.toml", "rb") as infile:
        config.dictConfig(tomllib.load(infile))
    args = init_argument_parser().parse_args()
    main(args)
