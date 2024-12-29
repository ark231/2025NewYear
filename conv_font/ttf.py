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
import math
import itertools

from logging import getLogger, config

import FixedHeightFontFormat as FHFT

logger = getLogger(__name__)

SRCDIR = Path(__file__).parent


@dataclass
class Glyph:
    width: int
    name: str
    data: bytes
    gid: int = 0


def load_ttf(path, out, out_xml, out_pbm):
    result = FHFT.FixedHeightFont()

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

    glyphs: list[Glyph] = []

    heights: list[int] = []

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
            heights.append(h)
            logger.info("%dx%d", h, w)
            for i in range(first, last + 1):
                name, strikedat = strikedats[i]
                if i == first:
                    logger.info("from %s", name)
                elif i == last:
                    logger.info("to   %s", name)
                try:
                    rawimdat = strikedat.data
                except AttributeError:
                    rawimdat = strikedat.imageData
                buf = np.frombuffer(rawimdat, np.dtype(">H"))

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
                        pix = 0 if rawimdat[bitcount // 8] & (1 << (7 - (bitcount % 8))) == 0 else 1
                        imbuf[x] |= pix << y
                        bitcount += 1
                glyphs.append(Glyph(w, name, imbuf.tobytes()))

                if out_pbm:
                    outdir = out / "pbm" / "rot"
                    outdir.mkdir(parents=True, exist_ok=True)
                    with open(outdir / f"{name}.pbm", "w", encoding="utf8") as outfile:
                        print("P1", file=outfile)
                        print(h, w, file=outfile)
                        for col in imbuf:
                            print(f"{col:016b}", file=outfile)
    glyphs.sort(key=lambda item: item.width)

    if any(heights[i] != heights[i + 1] for i in range(len(heights) - 1)):
        logger.fatal("font is not fixed height!")
        return

    cmap = font["cmap"]
    rev = cmap.buildReversed()

    gsub = font["GSUB"]

    if out_xml:
        writer = XMLWriter(out / "GSUB.xml", "  ")
        gsub.toXML(writer, font)

    cmapitems = []
    for i, glyph in enumerate(glyphs):
        glyph.gid = i
        try:
            for c in rev[glyph.name]:
                cmapitems.append(FHFT.CMAPItem(c, i))
        except KeyError as e:
            # GSUBで置換される先のグリフはCMAPにのっていないのでこうなる。
            # TODO: GSUBに対応
            logger.debug("key error: %s", e)
    cmapitems.sort(key=lambda item: item.codepoint)

    def bytelen(value):
        return math.ceil((len(hex(value)) - 2) / 2)

    result.cmap.table_1byte.items = [item for item in cmapitems if bytelen(item.codepoint) == 1]
    result.cmap.table_2byte.items = [item for item in cmapitems if bytelen(item.codepoint) == 2]
    result.cmap.table_3byte.items = [item for item in cmapitems if bytelen(item.codepoint) == 3]
    result.cmap.table_4byte.items = [item for item in cmapitems if bytelen(item.codepoint) == 4]

    for w, gs in itertools.groupby(glyphs, lambda item: item.width):
        logger.debug(w)
        logger.debug(gs)
        gs = list(gs)
        result.glyph.shapes.children.append(
            FHFT.GlyphShape(gs[0].gid, gs[-1].gid, w, b"".join(item.data for item in gs))
        )
    result.glyph.metadata.max_width = max(glyph.width for glyph in glyphs)
    result.glyph.metadata.height = 16
    name_table = font["name"]
    result.metadata.name = name_table.getBestFullName()
    return result


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
    font = load_ttf(args.src, args.dst, args.xml, args.pbm)
    font.dump(args.dst / "font.fhft")


if __name__ == "__main__":
    with open(Path(__file__).parent / "log_config.toml", "rb") as infile:
        config.dictConfig(tomllib.load(infile))
    args = init_argument_parser().parse_args()
    main(args)
