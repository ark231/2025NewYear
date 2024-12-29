#ifndef LIB_RIFF_RIFF_READER
#define LIB_RIFF_RIFF_READER
#include <stdint.h>
#include <stdio.h>

typedef uint32_t FourCC;

#define FOURCC(str) (((str)[3] << 24) | ((str)[2] << 16) | ((str)[1] << 8) | ((str)[0]))

size_t stringize_fourcc(FourCC fourcc, char* dst, size_t len);
const char* cfourcc(FourCC fourcc);

typedef struct {
    FourCC form_id;
    uint32_t size;
    long pos;
} RIFFHeaderInfo;

typedef struct {
    FourCC chunk_id;
    uint32_t size;
    long pos;
} RIFFPlainChunkInfo;

typedef struct {
    RIFFPlainChunkInfo plain_info;
    FourCC list_type;
} RIFFListChunkInfo;

typedef enum {
    PLAIN,
    LIST,
    ERROR_CHUNK,
} RIFFChunkType;

typedef struct {
    RIFFChunkType type;
    union {
        RIFFPlainChunkInfo plain;
        RIFFListChunkInfo list;
    } info;
} RIFFChunkInfo;

#define PLAININFO(chunk) (((chunk).type == LIST) ? (chunk).info.list.plain_info : (chunk).info.plain)

/**
 * @brief read riff header
 *
 * @param file riff file. cursor must be at the first byte of the riff header.
 *
 * @return riff header information
 *
 * @note this function moves the file position to the beginning of the riff data
 */
RIFFHeaderInfo riff_open(FILE* file);
void riff_skip_all(FILE* file, const RIFFHeaderInfo* info);
void riff_rewind_all(FILE* file, const RIFFHeaderInfo* info);
RIFFChunkInfo riff_read_chunk_info(FILE* file);
void riff_skip_chunk(FILE* file, const RIFFPlainChunkInfo* info);
void riff_rewind_chunk(FILE* file, const RIFFPlainChunkInfo* info);
void riff_seek_in_chunk(FILE* file, const RIFFPlainChunkInfo* info, long offset);  // SEEK_SET only

#endif
