#include <iso646.h>
#include <riff_reader.h>
#include <simple_logging.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <uchar.h>

#include "plain_chunk_list.h"
#define INDENT "  "

void visit_riff_list(FILE* file, uint32_t total, int indent, PlainChunkList* chunklist);

void print_help() {
    printf("load_font: load FixedHeightFont file and extract bitmap for specified characters\n");
    printf("usase: load_font [options] file \n");
    printf("options: \n");
    printf("--log-level <level>  set log level to <level>\n");
    printf("                     <level> can be: DEBUG, INFO, WARNING, ERROR, FATAL\n");
    printf("--riff-view          show riff information only and don't extract font\n");
    printf("-o/--output <file>   output file path. required\n");
    printf("-c/--chars  <str>    string of characters to be converted into bitmap\n");
    printf("-C/--charfile <file> path to a file containing string to be converted\n");
    printf("--pbm-output <file>  pbm output file path. optional\n");
    printf("-h/--help            show this help\n");
}

typedef enum {
    LOAD_FONT_MODE,
    RIFF_VIEW_MODE,
} AppMode;

AppMode mode = LOAD_FONT_MODE;

char32_t cstr_to_codepoint_utf8(const char* cstr, size_t* n_used_cstr);
char32_t cstr_to_codepoint_native(const char* cstr, size_t* n_used_cstr);

uint32_t search_char(FILE* file, PlainChunkList* chunklist, char32_t ch);

size_t search_glyph(FILE* file, PlainChunkList* glyphlist, uint16_t gid, uint16_t* bitmap_buf);

bool is_option(const char* arg) {
    size_t len = strlen(arg);
    if (len < 2 || arg[0] != '-') {
        return false;
    } else {
        return true;
    }
}

bool match_arg(const char* arg, char short_name, const char* long_name) {
    if (not is_option(arg)) {
        return false;
    }
    if (arg[1] == '-') {
        return strcmp(arg + 2, long_name) == 0;
    } else if (short_name != '\0') {
        return arg[1] == short_name;
    } else {
        return false;
    }
}

int main(int argc, const char** argv) {
    int exit_status = 0;

    const char** positionals = malloc(sizeof(char*) * (argc - 1));  // It's 2024. I am very rich!
    size_t positional_count = 0;
    size_t positional_required = 1;
    const char* outfname = NULL;
    char* source_str = NULL;
    const char* pbmfname = NULL;

    for (int i = 1; i < argc; i++) {
        if (match_arg(argv[i], '\0', "log-level")) {
            if (i == argc - 1) {
                SIMPLE_LOG(FATAL, "log-level requires one argument but none was given");
                return 1;
            }
            set_log_level(parse_log_level(argv[i + 1]));
            ++i;
        } else if (match_arg(argv[i], 'h', "help")) {
            print_help();
            return 0;
        } else if (match_arg(argv[i], '\0', "riff-view")) {
            mode = RIFF_VIEW_MODE;
            positional_required = 1;  // source
        } else if (match_arg(argv[i], 'o', "output")) {
            if (i == argc - 1) {
                SIMPLE_LOG(FATAL, "output requires one argument but none was given");
                return 1;
            }
            outfname = argv[i + 1];
            ++i;
        } else if (match_arg(argv[i], 'c', "chars")) {
            if (i == argc - 1) {
                SIMPLE_LOG(FATAL, "chars requires one argument but none was given");
                return 1;
            }
            size_t len = strlen(argv[i + 1]) + 1;
            source_str = malloc(len);
            strncpy(source_str, argv[i + 1], len - 1);
            source_str[len - 1] = '\0';
            ++i;
        } else if (match_arg(argv[i], 'C', "charfile")) {
            if (i == argc - 1) {
                SIMPLE_LOG(FATAL, "charfile requires one argument but none was given");
                return 1;
            }
            const char* fname = argv[i + 1];
            struct stat charf_stat;
            stat(fname, &charf_stat);
            FILE* charfile = fopen(fname, "r");
            size_t len = charf_stat.st_size + 1;
            source_str = malloc(len);
            source_str[len - 1] = '\0';
            fread(source_str, sizeof(char), len, charfile);
            fclose(charfile);

            ++i;
        } else if (match_arg(argv[i], '\n', "pbm-output")) {
            if (i == argc - 1) {
                SIMPLE_LOG(FATAL, "pbm-output requires one argument but none was given");
                return 1;
            }
            pbmfname = argv[i + 1];
            ++i;
        } else if (is_option(argv[i])) {
            SIMPLE_LOG(FATAL, "unknown argument %s", argv[i]);
            return -1;
        } else {
            positionals[positional_count] = argv[i];
            positional_count++;
        }
    }

    if (positional_count < positional_required) {
        SIMPLE_LOG(FATAL, "insufficient positional argument");
        return 1;
    }

    if (mode == LOAD_FONT_MODE) {
        if (outfname == NULL) {
            SIMPLE_LOG(FATAL, "required argument 'output' is missing");
            return 1;
        }
        if (source_str == NULL) {
            SIMPLE_LOG(FATAL, "required argument 'char' or 'charfile' is missing");
            return 1;
        }
    }

    FILE* file = fopen(positionals[0], "rb");
    if (file == NULL) {
        SIMPLE_LOG(FATAL, "failed to open font file");
        exit_status = 1;
        goto quit;
    }

    RIFFHeaderInfo header = riff_open(file);
    if (header.form_id == FOURCC("NULL")) {
        SIMPLE_LOG(FATAL, "failed to parse riff header");
        exit_status = 1;
        goto quit;
    }

    if (mode == RIFF_VIEW_MODE) {
        printf("RIFF size: %u form: %s\n", header.size, cfourcc(header.form_id));
    }

    uint32_t parsed_len = 4;  // format name
    PlainChunkList* chunklist = new_list();
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
    RIFFPlainChunkInfo* meta = search_list(chunklist, FOURCC("FTMT"));
    if (meta == NULL) {
        SIMPLE_LOG(FATAL, "FTMT chunk was not found");
        exit_status = 1;
        goto quit;
    }
#define MIN_VER 2
    uint16_t version;
    uint16_t namelen;
    char* name;
    riff_rewind_chunk(file, meta);
    fread(&version, sizeof(version), 1, file);
    SIMPLE_LOG(INFO, "format version: %d", version);
    if (version < MIN_VER) {
        SIMPLE_LOG(FATAL, "format is older than %d", MIN_VER);
        exit_status = 1;
        goto quit;
    }
    fread(&namelen, sizeof(namelen), 1, file);
    name = malloc(sizeof(char) * (namelen + 1));
    fread(name, sizeof(char), namelen, file);
    name[namelen + 1 - 1] = '\0';  // last index is length-1
    SIMPLE_LOG(INFO, "font name: %s", name);

    const char* target_str = source_str;
    size_t maxlen = strlen(target_str) + 1;  // WARN: assuming c string with utf8
    size_t buflen = sizeof(char32_t) * maxlen;
    char32_t* utf32_chars = malloc(buflen);
    memset(utf32_chars, 0, buflen);
    char32_t* cursor = utf32_chars;
    size_t parsed_strlen = 0;
    size_t diff = -1;
    size_t utf32_strlen = 0;
    while (parsed_strlen < maxlen && diff != 0) {  // diff is 0 when it reaches NUL char
        *cursor = cstr_to_codepoint_native(target_str, &diff);
        target_str += diff;
        parsed_strlen += diff;
        cursor++;
        utf32_strlen++;
    }
    cursor = utf32_chars;

    free(source_str);
    target_str = source_str = NULL;

    uint16_t max_width;
    uint16_t height;
    RIFFPlainChunkInfo* glyph_meta = search_list(chunklist, FOURCC("GLMT"));
    if (glyph_meta == NULL) {
        SIMPLE_LOG(FATAL, "GLMT chunk was not found");
        exit_status = 1;
        goto quit;
    }
    riff_rewind_chunk(file, glyph_meta);
    fread(&max_width, sizeof(max_width), 1, file);
    fread(&height, sizeof(height), 1, file);
    if (height != 16) {
        SIMPLE_LOG(FATAL, "unsupported glyph height %d", height);
    }
    size_t bitmap_maxlen = (max_width + 2) * utf32_strlen;  // absolute maximum
    uint16_t* bitmap = malloc(sizeof(uint16_t) * bitmap_maxlen);
    size_t bitmap_len = 0;
    size_t maxlinecount = utf32_strlen;
    size_t* linewidths = malloc(sizeof(size_t) * maxlinecount);
    size_t* lineends = malloc(sizeof(size_t) * maxlinecount);
    size_t linecount = 0;
    size_t previous_bitmap_len = 0;

    PlainChunkList* glyphlist = new_list();
    list_seek_head(&chunklist);
    while (chunklist->next != NULL) {
        if ((not list_is_sentinel(chunklist)) && chunklist->info.chunk_id == FOURCC("GLSP")) {
            list_append(glyphlist, &chunklist->info);
        }
        chunklist = chunklist->next;
    }

    while (*cursor != '\0') {
        if (*cursor == '\n') {
            linewidths[linecount] = bitmap_len - previous_bitmap_len;
            previous_bitmap_len = bitmap_len;
            lineends[linecount] = bitmap_len;
            linecount++;
            cursor++;
            continue;
        }
        uint32_t gid = search_char(file, chunklist, *cursor);
        if (gid == -1) {
            SIMPLE_LOG(ERROR, "character 0x%04x wasn't found", *cursor);
            cursor++;
            continue;
        }
        SIMPLE_LOG(INFO, "ch: 0x%06X gid: 0x%04X", *cursor, gid);
        size_t diff = 0;
        diff = search_glyph(file, glyphlist, gid, bitmap + bitmap_len);
        bitmap_len += diff;
        cursor++;
    }
    if (linecount == 0) {
        linecount = 1;
        linewidths[0] = bitmap_len;
        lineends[0] = bitmap_len;
    }

    FILE* outfile = fopen(outfname, "wb");
    if (outfile == NULL) {
        SIMPLE_LOG(FATAL, "failed to open output file");
        exit_status = 1;
        goto quit;  // NOTE: this results in fclose(NULL), which may be problematic
    }
    FILE* pbmfile = NULL;
    if (pbmfname != NULL) {
        SIMPLE_LOG(DEBUG, "pbmfname: %s\n", pbmfname);
        if (strcmp(pbmfname, "-") == 0) {
            pbmfile = stdout;
        } else {
            pbmfile = fopen(pbmfname, "wb");
        }
    }
    if (pbmfile == NULL) {
        SIMPLE_LOG(FATAL, "failed to open pbm output file");
        exit_status = 1;
        goto quit;
    }
    if (pbmfile != NULL) {
        fprintf(pbmfile, "P4\n16 %zu\n", bitmap_len);
    }

    fprintf(outfile, "uint16_t bitmap[] = {\n    ");
    for (size_t i = 0; i < bitmap_len; i++) {
        fprintf(outfile, "0x%X, ", bitmap[i]);
        if (pbmfile != NULL) {
            uint8_t upper = (bitmap[i] & 0xff00) >> 8;
            uint8_t lower = bitmap[i] & 0xff;
            fwrite(&upper, sizeof(uint8_t), 1, pbmfile);
            fwrite(&lower, sizeof(uint8_t), 1, pbmfile);
        }
    }
    if (pbmfile != NULL && pbmfile != stdout) {
        fclose(pbmfile);
    }
    fprintf(outfile, "\n};\n");
    fprintf(outfile, "size_t line_widths[] = {\n    ");
    for (size_t i = 0; i < linecount; i++) {
        fprintf(outfile, "%zu, ", linewidths[i]);
    }
    fprintf(outfile, "\n};\n");
    fprintf(outfile, "size_t line_ends[] = {\n    ");
    for (size_t i = 0; i < linecount; i++) {
        fprintf(outfile, "%zu, ", lineends[i]);
    }
    fprintf(outfile, "\n};\n");

quit:
    fclose(file);
    fclose(outfile);
    free(positionals);
    free_list(chunklist);
    free_list(glyphlist);
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
    SIMPLE_LOG(DEBUG, "pivot: %u chsize: %u ch: %04x pivot_ch: %04x, range_min: %ld, range_max: %ld", pivot, charsize,
               ch, pivot_ch, range_min, range_max);
    if (pivot_ch == ch) {
        uint16_t pivot_gid;
        fread(&pivot_gid, sizeof(uint16_t), 1, file);
        return pivot_gid;
    } else if (range_min == range_max) {
        return -1;
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
    RIFFPlainChunkInfo* cmap = search_list(chunklist, cmap_id);
    if (cmap == NULL) {
        SIMPLE_LOG(ERROR, "%s chunk not found", cfourcc(cmap_id));
        return -1;
    }
    size_t itemcount = cmap->size / itemsize;
    if (cmap->size % itemsize != 0) {
        SIMPLE_LOG(ERROR, "the size of cmap %s (%lu) is not multiple of itemsize (%lu)", cfourcc(cmap->chunk_id),
                   cmap->size, itemsize);
    }
    SIMPLE_LOG(DEBUG, "cmap: %s itemcount: %d", cfourcc(cmap->chunk_id), itemcount);
    return bin_search_char(file, cmap, ch, itemsize, charsize, 0, itemcount - 1);
}
size_t search_glyph(FILE* file, PlainChunkList* glyphlist, uint16_t gid, uint16_t* bitmap_buf) {
    list_seek_head(&glyphlist);
    while (glyphlist->next != NULL) {
        if ((not list_is_sentinel(glyphlist))) {
            uint16_t first_gid;
            uint16_t last_gid;
            riff_rewind_chunk(file, &glyphlist->info);
            fread(&first_gid, sizeof(first_gid), 1, file);
            fread(&last_gid, sizeof(last_gid), 1, file);
            if (first_gid <= gid && gid <= last_gid) {
                uint16_t width;
                uint16_t result;
                fread(&width, sizeof(width), 1, file);
                SIMPLE_LOG(DEBUG, "gid: 0x%x, width: %d", gid, width);
                riff_seek_in_chunk(file, &glyphlist->info,
                                   sizeof(uint16_t) * 3 + (sizeof(uint16_t) * width * (gid - first_gid)));
                fread(bitmap_buf, sizeof(uint16_t), width, file);
                result = width;
                // insert space between character if it's not in font
                if (bitmap_buf[0] != 0) {
                    memmove(bitmap_buf + 1, bitmap_buf,
                            sizeof(uint16_t) * width);  // since to area is overwrapping, memcpy cannot be used
                    ++result;
                }
                if (bitmap_buf[result - 1] != 0) {
                    bitmap_buf[result] = 0;
                    ++result;
                }
                return result;
            }
        }
        glyphlist = glyphlist->next;
    }
    return 0;
}
