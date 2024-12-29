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
    version = 2

    def __init__(self, name: str = ""):
        super().__init__()
        self.namelen = 0
        self.name = name

    def dumps(self):
        name = self.name.encode("utf8")
        self.namelen = len(name)
        self.data = b""
        self.data += self.version.to_bytes(2, "little")
        self.data += self.namelen.to_bytes(2, "little")  # 数万字のフォント名など無かろう
        self.data += name
        return super().dumps()


@dataclass
class CMAPItem:
    codepoint: int
    glyphid: int

    def to_bytes(self):
        # e.g. chars with 9~15 bit will be 3 digits long in hex, resulting odd number of bytes. so ceil it
        size = math.ceil((len(hex(self.codepoint)) - 2) / 2)
        assert 0 < size <= 4
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


class GlyphMetaData(Chunk):
    chunk_id = b"GLMT"

    def __init__(self, max_width=-1, height=-1):
        super().__init__()
        self.max_width = max_width
        self.height = height

    def dumps(self):
        self.data += self.max_width.to_bytes(2, "little", signed=False)
        self.data += self.height.to_bytes(2, "little", signed=False)
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
        self.metadata = GlyphMetaData()

    def dumps(self):
        self.children.append(self.metadata)
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
