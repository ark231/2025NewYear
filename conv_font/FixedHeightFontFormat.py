#!/usr/bin/env python3
import struct
from logging import getLogger, config
import math
from dataclasses import dataclass

logger = getLogger(__name__)


class FourCC(bytes):
    pass


class Chunk:
    chunk_id: FourCC = b"NULL"

    def __init__(self, data=b""):
        self.size = len(data)
        self.data = data

    def dumps(self):
        self.size = len(self.data)
        return struct.pack("<4sI", self.chunk_id, self.size) + self.data + (b"\x00" if self.size % 2 == 1 else b"")

    def dump(self, out):
        try:
            out.write(self.dumps())
        except AttributeError:
            with open(out, "wb") as outfile:
                outfile.write(self.dumps())


class List(Chunk):
    chunk_id = b"LIST"
    list_type: FourCC = b"NULL"

    def __init__(self, children=None):
        super().__init__()
        if children is None:
            children = []
        self.children: list[Chunk] = children

    def dumps(self):
        self.data = self.list_type
        for child in self.children:
            self.data += child.dumps()
        return super().dumps()


class FontMetadata(Chunk):
    chunk_id = b"FTMT"
    version = 1

    def __init__(self):
        super().__init__()

    def dumps(self):
        self.data = struct.pack("<h", self.version)
        return super().dumps()


@dataclass
class CMAPItem:
    codepoint: int
    glyphid: int

    def to_bytes(self):
        size = math.ceil((len(hex(self.codepoint)) - 2) / 2)
        assert size <= 4
        result = b""
        result += self.codepoint.to_bytes(size, "little", signed=False)
        result += self.glyphid.to_bytes(2, "little", signed=False)
        return result


class CMAPTable(Chunk):

    def __init__(self, bytewidth=1, items=None):
        super().__init__()
        self.chunk_id = f"CM{bytewidth}B".encode("utf8")
        if items is None:
            self.items: list[CMAPItem] = []

    def dumps(self):
        for item in self.items:
            self.data += item.to_bytes()
        return super().dumps()


class CMAP(List):
    list_type = b"CMAP"

    def __init__(self):
        super().__init__()
        self.table_1byte = CMAPTable(1)
        self.table_2byte = CMAPTable(2)
        self.table_3byte = CMAPTable(3)
        self.table_4byte = CMAPTable(4)

    def dumps(self):
        self.children.append(self.table_1byte)
        self.children.append(self.table_2byte)
        self.children.append(self.table_3byte)
        self.children.append(self.table_4byte)
        return super().dumps()


class GlyphShape(Chunk):
    chunk_id = b"GLSP"

    def __init__(self, firstgid=-1, lastgid=-1, width=-1, bitmaps=b""):
        super().__init__()
        self.firstgid = firstgid
        self.lastgid = lastgid
        self.width = width
        self.bitmaps = bitmaps

    def dumps(self):
        self.data += self.firstgid.to_bytes(2, "little", signed=False)
        self.data += self.lastgid.to_bytes(2, "little", signed=False)
        self.data += self.width.to_bytes(2, "little", signed=False)
        self.data += self.bitmaps
        return super().dumps()


class ShapeList(List):
    list_type = b"SPLI"

    def __init__(self):
        super().__init__()


class Glyph(List):
    list_type = b"GLYF"

    def __init__(self):
        super().__init__()
        self.shapes = ShapeList()

    def dumps(self):
        self.children.append(self.shapes)
        return super().dumps()


class RIFFHeader(List):
    chunk_id = b"RIFF"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)


class FixedHeightFont(RIFFHeader):
    list_type = b"FHFT"

    def __init__(self):
        super().__init__()
        self.metadata = FontMetadata()
        self.cmap = CMAP()
        self.glyph = Glyph()

    def dumps(self):
        self.children.append(self.metadata)
        self.children.append(self.cmap)
        self.children.append(self.glyph)
        return super().dumps()
