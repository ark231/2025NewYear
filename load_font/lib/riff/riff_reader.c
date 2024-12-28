#include "riff_reader.h"

#include <iso646.h>
#include <simple_logging.h>
#include <stdio.h>
#include <string.h>

size_t stringize_fourcc(FourCC fourcc, char* dst, size_t len) {
    if (len < 5) {
        return 0;
    }
    memcpy(dst, &fourcc, 4);
    dst[4] = '\0';
    return 5;
}
const char* cfourcc(FourCC fourcc) {
    static char result[5] = {0};
    memcpy(result, &fourcc, 4);
    return result;
}
RIFFHeaderInfo riff_open(FILE* file) {
    FourCC riff;
    RIFFHeaderInfo result = {FOURCC("NULL"), 0, ftell(file)};
    uint16_t bom = 0xfeff;
    uint8_t buf[2];
    memcpy(buf, &bom, sizeof(bom));
    if (not(buf[0] == 0xff && buf[1] == 0xfe)) {
        SIMPLE_LOG(FATAL, "incompatible bom");
        return result;
    }

    fread(&riff, sizeof(FourCC), 1, file);
    if (riff != FOURCC("RIFF")) {
        SIMPLE_LOG(ERROR, "not riff file got: %lX expected: %lX", riff, FOURCC("RIFF"));
        fseek(file, result.pos, SEEK_SET);
        return result;
    }
    fread(&result.size, sizeof(result.size), 1, file);
    SIMPLE_LOG(DEBUG, "RIFF");
    SIMPLE_LOG(DEBUG, "size: %d", result.size);
    fread(&result.form_id, sizeof(FourCC), 1, file);
    char typebuf[5];
    stringize_fourcc(result.form_id, typebuf, sizeof(typebuf));
    SIMPLE_LOG(DEBUG, "form: %s", typebuf);
    return result;
}
void riff_skip_all(FILE* file, const RIFFHeaderInfo* info) { fseek(file, info->pos + 4 + 4 + info->size, SEEK_SET); }
void riff_rewind_all(FILE* file, const RIFFHeaderInfo* info) { fseek(file, info->pos + 4 + 4, SEEK_SET); }
RIFFChunkInfo riff_read_chunk_info(FILE* file) {
    RIFFChunkInfo result = {.type = ERROR_CHUNK};
    RIFFPlainChunkInfo info = {FOURCC("NULL"), 0, ftell(file)};
    fread(&info.chunk_id, sizeof(info.chunk_id), 1, file);
    fread(&info.size, sizeof(info.size), 1, file);
    SIMPLE_LOG(DEBUG, "info.chunk_id: %s", cfourcc(info.chunk_id));
    if (info.chunk_id == FOURCC("LIST")) {
        result.type = LIST;
        result.info.list.plain_info = info;
        fread(&result.info.list.list_type, sizeof(result.info.list.list_type), 1, file);
    } else {
        result.type = PLAIN;
        result.info.plain = info;
    }
    return result;
}
void riff_skip_chunk(FILE* file, const RIFFPlainChunkInfo* info) {
    fseek(file, info->pos + 4 + 4 + info->size, SEEK_SET);
}
void riff_rewind_chunk(FILE* file, const RIFFPlainChunkInfo* info) { fseek(file, info->pos + 4 + 4, SEEK_SET); }
