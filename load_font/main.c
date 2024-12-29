#include <riff_reader.h>
#include <simple_logging.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>

#include "plain_chunk_list.h"
#define INDENT "  "

void visit_riff_list(FILE* file, uint32_t total, int indent, PlainChunkList* chunklist);

void print_help() {
    printf("load_font: load FixedHeightFont file and extract bitmap for specified characters\n");
    printf("usase: load_font [options] file chars\n");
    printf("options: \n");
    printf("--log-level <arg> set log level to <arg>\n");
    printf("                  log levels: DEBUG, INFO, WARNING, ERROR, FATAL\n");
    printf("--riff-view       show riff information only and don't extract font\n");
    printf("-h/--help         show this help\n");
}

typedef enum {
    LOAD_FONT_MODE,
    RIFF_VIEW_MODE,
} AppMode;

AppMode mode = LOAD_FONT_MODE;

char32_t cstr_to_codepoint_utf8(const char* cstr, size_t* n_used_cstr);
char32_t cstr_to_codepoint_native(const char* cstr, size_t* n_used_cstr);

uint32_t search_char(FILE* file, PlainChunkList* chunklist, char32_t ch);

int main(int argc, const char** argv) {
    int exit_status = 0;

    const char** positionals = malloc(sizeof(char*) * (argc - 1));  // It's 2024. I am very rich!
    size_t positional_count = 0;
    size_t positional_required = 2;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {  // long
            if (strcmp(argv[i] + 2, "log-level") == 0) {
                if (i == argc - 1) {
                    SIMPLE_LOG(FATAL, "log-level requires one argument but none was given");
                    return 1;
                }
                set_log_level(parse_log_level(argv[++i]));
            } else if (strcmp(argv[i] + 2, "help") == 0) {
                print_help();
                return 0;
            } else if (strcmp(argv[i] + 2, "riff-view") == 0) {
                mode = RIFF_VIEW_MODE;
                positional_required = 1;  // source
            } else {
                SIMPLE_LOG(FATAL, "unknown argument %s", argv[i]);
                return -1;
            }
        } else if (argv[i][0] == '-') {
            if (argv[i][1] == 'h') {
                print_help();
                return 0;
            } else {
                SIMPLE_LOG(FATAL, "unknown argument %s", argv[i]);
                return -1;
            }
        } else {
            positionals[positional_count] = argv[i];
            positional_count++;
        }
    }

    if (positional_count < positional_required) {
        SIMPLE_LOG(FATAL, "insufficient positional argument");
        return 1;
    }

    FILE* file = fopen(positionals[0], "rb");

    RIFFHeaderInfo header = riff_open(file);
    if (header.form_id == FOURCC("NULL")) {
        SIMPLE_LOG(FATAL, "failed to open riff file");
        exit_status = 1;
        goto quit;
    }

    if (mode == RIFF_VIEW_MODE) {
        printf("RIFF size: %u form: %s\n", header.size, cfourcc(header.form_id));
    }

    uint32_t parsed_len = 4;  // format name
    PlainChunkList* chunklist = malloc(sizeof(PlainChunkList));
    init_list(chunklist);
    while (parsed_len < header.size) {
        RIFFChunkInfo info = riff_read_chunk_info(file);
        if (info.type == ERROR_CHUNK) {
            SIMPLE_LOG(FATAL, "failed to parse chunk header");
            exit_status = 1;
            goto quit;
        }
        parsed_len += PLAININFO(info).totalsize;
        if (mode == RIFF_VIEW_MODE) {
            printf("- %s size: %u offset: %ld", cfourcc(PLAININFO(info).chunk_id), PLAININFO(info).size,
                   PLAININFO(info).pos);
        }
        if (info.type == LIST) {
            if (mode == RIFF_VIEW_MODE) {
                printf(" type: %s\n", cfourcc(info.info.list.list_type));
            }
            visit_riff_list(file, PLAININFO(info).size, 1, chunklist);
        } else {
            RIFFPlainChunkInfo plain = PLAININFO(info);
            list_append(chunklist, &plain);
            riff_skip_chunk(file, &plain);
            if (mode == RIFF_VIEW_MODE) {
                printf("\n");
            }
        }
    }
    if (mode == RIFF_VIEW_MODE) {
        goto quit;
    }
    RIFFPlainChunkInfo meta = search_list(chunklist, FOURCC("FTMT"));
    if (meta.chunk_id == FOURCC("NULL")) {
        SIMPLE_LOG(ERROR, "chunk FTMT was not found");
        exit_status = 1;
        goto quit;
    }
#define MIN_VER 1
    uint16_t version;
    riff_rewind_chunk(file, &meta);
    fread(&version, sizeof(version), 1, file);
    SIMPLE_LOG(INFO, "format version: %d", version);
    if (version < MIN_VER) {
        SIMPLE_LOG(FATAL, "format is older than %d", MIN_VER);
        exit_status = 1;
        goto quit;
    }

    const char* target_str = positionals[1];
    size_t maxlen = strlen(target_str) + 1;  // WARN: assuming c string with utf8
    size_t buflen = sizeof(char32_t) * maxlen;
    char32_t* utf32_chars = malloc(buflen);
    memset(utf32_chars, 0, buflen);
    char32_t* cursor = utf32_chars;
    size_t parsed_strlen = 0;
    size_t diff = -1;
    while (parsed_strlen < maxlen && diff != 0) {  // diff is 0 when it reaches NUL char
        *cursor = cstr_to_codepoint_native(target_str, &diff);
        target_str += diff;
        parsed_strlen += diff;
        cursor++;
    }
    cursor = utf32_chars;
    while (*cursor != '\0') {
        uint32_t gid = search_char(file, chunklist, *cursor);
        if (gid == -1) {
            SIMPLE_LOG(ERROR, "character 0x%04x wasn't found", *cursor);
            cursor++;
            continue;
        }
        printf("ch: 0x%08X gid: 0x%04X\n", *cursor, gid);
        cursor++;
    }

quit:
    fclose(file);
    free(positionals);
    free_list(chunklist);
    return exit_status;
}

void visit_riff_list(FILE* file, uint32_t total, int indent, PlainChunkList* chunklist) {
    RIFFChunkInfo info;
    uint32_t parsed_len = 4;
    while (parsed_len < total) {
        RIFFChunkInfo info = riff_read_chunk_info(file);
        if (PLAININFO(info).chunk_id == FOURCC("NULL")) {
            SIMPLE_LOG(FATAL, "failed to parse chunk header");
            return;
        }
        parsed_len += PLAININFO(info).totalsize;
        if (mode == RIFF_VIEW_MODE) {
            for (int i = 0; i < indent; i++) {
                printf(INDENT);
            }
            printf("- %s size: %u offset: %ld", cfourcc(PLAININFO(info).chunk_id), PLAININFO(info).size,
                   PLAININFO(info).pos);
        }

        if (info.type == LIST) {
            if (mode == RIFF_VIEW_MODE) {
                printf(" type: %s\n", cfourcc(info.info.list.list_type));
            }
            visit_riff_list(file, PLAININFO(info).size, indent + 1, chunklist);
        } else {
            RIFFPlainChunkInfo plain = PLAININFO(info);
            list_append(chunklist, &plain);
            riff_skip_chunk(file, &plain);
            if (mode == RIFF_VIEW_MODE) {
                printf("\n");
            }
        }
    }
}

// taken from my mod on cmatrix
#define c_die(msg)              \
    do {                        \
        SIMPLE_LOG(FATAL, msg); \
        return -1;              \
    } while (false)

char32_t cstr_to_codepoint_utf8(const char* cstr, size_t* n_used_cstr) {
    uint32_t result = 0;
    size_t n = 1;
    if (cstr[0] == '\0') {
        result = '\0';
        n = 0;
    } else if ((*cstr & 0x80) == 0) {
        result = *cstr;
        n = 1;
    } else if ((*cstr & 0xe0) == 0xc0) {
        result |= (*cstr & 0x1f) << 6;
        cstr++;
        n++;
        if ((*cstr & 0xc0) != 0x80) c_die("Invalid utf8 sequence");
        result |= (*cstr & 0x3f);
        if (result < 0x0080) c_die("Invalid utf8 sequence");
    } else if ((*cstr & 0xf0) == 0xe0) {
        result |= (*cstr & 0x0f) << 12;
        for (int i = 2; i > 0; i--) {
            cstr++;
            n++;
            if ((*cstr & 0xc0) != 0x80) c_die("Invalid utf8 sequence");
            result |= (*cstr & 0x3f) << (6 * (i - 1));
        }
        if (result < 0x0800) c_die("Invalid utf8 sequence");
    } else if ((*cstr & 0xf8) == 0xf0) {
        result |= (*cstr & 0x07) << 18;
        for (int i = 3; i > 0; i--) {
            cstr++;
            n++;
            if ((*cstr & 0xc0) != 0x80) c_die("Invalid utf8 sequence");
            result |= (*cstr & 0x3f) << (6 * (i - 1));
        }
        if (result < 0x10000) c_die("Invalid utf8 sequence");
    } else {
        c_die("Invalid utf8 sequence");
    }
    if (result > 0x10ffff) {
        c_die("Invalid utf8 sequence");
    }
    if (n_used_cstr != NULL) {
        *n_used_cstr = n;
    }
    return result;
}

char32_t cstr_to_codepoint_native(const char* cstr, size_t* n_used_cstr) {
    // some environment specific things will be here
    return cstr_to_codepoint_utf8(cstr, n_used_cstr);
}

#define SAFE_MEAN(a, b) (((a) / 2) + ((b) / 2) + (((a) % 2) + ((b) % 2)) / 2)

uint32_t bin_search_char(FILE* file, const RIFFPlainChunkInfo* cmap, char32_t ch, size_t itemsize, size_t charsize,
                         long range_min, long range_max) {
    if (range_min > range_max) {  // just in case
        return -1;                // gid is 16 bit, so 32bit 0xffs is out of range
    }
    long pivot = SAFE_MEAN(range_min, range_max);
    char32_t pivot_ch = 0;
    riff_seek_in_chunk(file, cmap, itemsize * pivot);
    fread(&pivot_ch, charsize, 1, file);
    SIMPLE_LOG(DEBUG, "pivot: %u chsize: %u ch: %04x pivot_ch: %04x", pivot, charsize, ch, pivot_ch);
    if (pivot_ch == ch) {
        uint16_t pivot_gid;
        fread(&pivot_gid, sizeof(uint16_t), 1, file);
        return pivot_gid;
    } else if (pivot_ch < ch) {
        return bin_search_char(file, cmap, ch, itemsize, charsize, pivot, range_max);
    } else {
        return bin_search_char(file, cmap, ch, itemsize, charsize, range_min, pivot);
    }
}

uint32_t search_char(FILE* file, PlainChunkList* chunklist, char32_t ch) {
    FourCC cmap_id;
    size_t itemsize;
    size_t charsize;
    if (ch <= 0xff) {
        cmap_id = FOURCC("CM1B");
        charsize = 1;
    } else if (ch <= 0xff'ff) {
        cmap_id = FOURCC("CM2B");
        charsize = 2;
    } else if (ch <= 0xff'ff'ff) {
        cmap_id = FOURCC("CM3B");
        charsize = 3;
    } else {
        cmap_id = FOURCC("CM4B");
        charsize = 4;
    }
    itemsize = charsize + 2;
    RIFFPlainChunkInfo cmap = search_list(chunklist, cmap_id);
    if (cmap.chunk_id == FOURCC("NULL")) {
        SIMPLE_LOG(ERROR, "chunk %s not found", cfourcc(cmap_id));
        return -1;
    }
    size_t itemcount = cmap.size / itemsize;
    if (cmap.size % itemsize != 0) {
        SIMPLE_LOG(ERROR, "the size of cmap %s (%lu) is not multiple of itemsize (%lu)", cfourcc(cmap.chunk_id),
                   cmap.size, itemsize);
    }
    SIMPLE_LOG(DEBUG, "cmap: %s itemcount: %d", cfourcc(cmap.chunk_id), itemcount);
    return bin_search_char(file, &cmap, ch, itemsize, charsize, 0, itemcount - 1);
}
