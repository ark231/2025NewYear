#include <riff_reader.h>
#include <simple_logging.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
                set_log_level(parse_log_level(argv[i + 1]));
            } else if (strcmp(argv[i] + 2, "help") == 0) {
                print_help();
                return 0;
            } else if (strcmp(argv[i] + 2, "riff-view") == 0) {
                mode = RIFF_VIEW_MODE;
                positional_required = 1;  // source
            }
        } else if (argv[i][0] == '-') {
            if (argv[i][1] == 'h') {
                print_help();
                return 0;
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
        parsed_len += PLAININFO(info).size + 8;
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
        parsed_len += PLAININFO(info).size + 8;
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
