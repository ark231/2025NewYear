#!/usr/bin/env python3
import struct
from logging import getLogger, config
import math
from dataclasses import dataclass

logger = getLogger(__name__)


class FourCC(bytes):
    pass


class Chunk:
    chunk_id: FourCC
    size: int
    data: bytes

    def __init__(self, chunk_id=b"NULL", data=b""):
        self.chunk_id = chunk_id
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
    list_type: FourCC
    children: list[Chunk]

    def __init__(self, list_type=b"NULL", children=None):
        super().__init__(chunk_id=List.chunk_id)
        self.list_type = list_type
        if children is None:
            children = []
        self.children = children

    def dumps(self):
        self.data = self.list_type
        for child in self.children:
            self.data += child.dumps()
        return super().dumps()


class FontMetadata(Chunk):
    version: int

    chunk_id = b"FTMT"

    def __init__(self):
        super().__init__(chunk_id=FontMetadata.chunk_id)
        self.version = 0

    def dumps(self):
        self.data = struct.pack("<h", self.version)
        return super().dumps()


@dataclass
class CMAPItem:
    codepoint: int
    glyphid: int

    def to_bytes(self):
        size = math.ceil((len(hex(self.codepoint)) - 1) / 2)
        assert size <= 4
        result = b""
        result += self.codepoint.to_bytes(size, "little", False)
        result += self.glyphid.to_bytes(2, "little", False)
        return result


class CMAPTable(Chunk):
    items: list[CMAPItem]

    def __init__(self, bytewidth=1, items=None):
        super().__init__(chunk_id=f"CM{bytewidth}B".encode("utf8"))
        if items is None:
            self.items = []

    def dumps(self):
        for item in self.items:
            self.data += item.to_bytes()
        return super().dumps()


class CMAP(List):
    list_type = b"CMAP"

    def __init__(self):
        super().__init__(list_type=CMAP.list_type)
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

    def __init__(self):
        super().__init__(chunk_id=GlyphShape.chunk_id)


class ShapeList(List):
    list_type = b"SPLI"

    def __init__(self):
        super().__init__(list_type=CMAP.list_type)


class Glyph(List):
    list_type = b"GLYF"

    def __init__(self):
        super().__init__(list_type=Glyph.list_type)
        self.shapes = ShapeList()

    def dumps(self):
        self.children.append(self.shapes)
        return super().dumps()


class RIFFHeader(List):
    chunk_id = b"RIFF"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.chunk_id = RIFFHeader.chunk_id


class FixedHeightFont(RIFFHeader):
    def __init__(self):
        super().__init__(list_type=b"FHFT")
        self.metadata = FontMetadata()
        self.cmap = CMAP()
        self.glyph = Glyph()

    def dumps(self):
        self.children.append(self.metadata)
        self.children.append(self.cmap)
        self.children.append(self.glyph)
        return super().dumps()
