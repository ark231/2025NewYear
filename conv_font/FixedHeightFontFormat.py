#!/usr/bin/env python3
import struct
from logging import getLogger, config
import math
from dataclasses import dataclass

logger = getLogger(__name__)


class FourCC(bytes):
    pass


class Chunk:
    __chunk_id: FourCC
    size: int
    data: bytes

    def __init__(self, chunk_id=b"NULL", data=b""):
        self.__chunk_id = chunk_id
        self.size = len(data)
        self.data = data

    def dumps(self):
        self.size = len(self.data)
        return struct.pack("<4sI", self.__chunk_id, self.size) + self.data + (b"\x00" if self.size % 2 == 1 else b"")

    def dump(self, out):
        try:
            out.write(self.dumps())
        except AttributeError:
            with open(out, "wb") as outfile:
                outfile.write(self.dumps())


class Form(Chunk):
    chunk_id = b"FORM"
    __form_id: FourCC
    members: list[Chunk]

    def __init__(self, form_id=b"NULL", members=None):
        super().__init__(chunk_id=Form.chunk_id)
        self.__form_id = form_id
        if members is None:
            members = []
        self.members = members

    def dumps(self):
        self.data = self.__form_id
        for member in self.members:
            self.data += member.dumps()
        return super().dumps()


class List(Chunk):
    item_type: FourCC
    items: list[Chunk]

    chunk_id = b"LIST"

    def __init__(self, item_type=b"NULL", items=None):
        super().__init__(chunk_id=List.chunk_id)
        self.item_type = item_type
        if items is None:
            items = []
        self.items = items

    def dumps(self):
        self.data = self.item_type
        for item in self.items:
            self.data += item.dumps()
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


class CMAP(Form):
    form_id = b"CMAP"

    def __init__(self):
        super().__init__(form_id=CMAP.form_id)
        self.table_1byte = CMAPTable(1)
        self.table_2byte = CMAPTable(2)
        self.table_3byte = CMAPTable(3)
        self.table_4byte = CMAPTable(4)

    def dumps(self):
        self.members.append(self.table_1byte)
        self.members.append(self.table_2byte)
        self.members.append(self.table_3byte)
        self.members.append(self.table_4byte)
        return super().dumps()


class GlyphShape(Chunk):
    chunk_id = b"GLSP"

    def __init__(self):
        super().__init__(chunk_id=GlyphShape.chunk_id)


class Glyph(Form):
    form_id = b"GLYF"

    def __init__(self):
        super().__init__(form_id=Glyph.form_id)
        self.shapes = List(GlyphShape.chunk_id)

    def dumps(self):
        self.members.append(self.shapes)
        return super().dumps()


class FixedHeightFont(Form):
    def __init__(self):
        super().__init__(form_id=b"FHFT")
        self.metadata = FontMetadata()
        self.cmap = CMAP()
        self.glyph = Glyph()

    def dumps(self):
        self.members.append(self.metadata)
        self.members.append(self.cmap)
        self.members.append(self.glyph)
        return super().dumps()
