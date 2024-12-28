#include <riff_reader.h>
#include <simple_logging.h>
#include <stdio.h>
void visit_list(FILE* file, uint32_t total, int indent);

#define INDENT "  "

int main(int argc, const char** argv) {
    /* set_log_level(DEBUG);  // TODO: parse argument and set this with commandline argument */
    if (argc < 3) {
        SIMPLE_LOG(FATAL, "insufficient argument");
        return 1;
    }
    FILE* file = fopen(argv[1], "rb");
    RIFFHeaderInfo header = riff_open(file);
    if (header.form_id == FOURCC("NULL")) {
        SIMPLE_LOG(FATAL, "failed to open riff file");
        goto quit;
    }
    uint32_t parsed_len = 4;  // format name
    while (parsed_len < header.size) {
        RIFFChunkInfo info = riff_read_chunk_info(file);
        if (info.type == ERROR_CHUNK) {
            SIMPLE_LOG(FATAL, "failed to parse chunk header");
            goto quit;
        }
        parsed_len += PLAININFO(info).size + 8;
        printf("- %s size %u", cfourcc(PLAININFO(info).chunk_id), PLAININFO(info).size);
        if (info.type == LIST) {
            printf(" type: %s\n", cfourcc(info.info.list.list_type));
            visit_list(file, PLAININFO(info).size, 1);
        } else {
            RIFFPlainChunkInfo plain = PLAININFO(info);
            riff_skip_chunk(file, &plain);
            printf("\n");
        }
    }
quit:
    fclose(file);
    return 0;
}

void visit_list(FILE* file, uint32_t total, int indent) {
    RIFFChunkInfo info;
    uint32_t parsed_len = 4;
    while (parsed_len < total) {
        RIFFChunkInfo info = riff_read_chunk_info(file);
        if (PLAININFO(info).chunk_id == FOURCC("NULL")) {
            SIMPLE_LOG(FATAL, "failed to parse chunk header");
            return;
        }
        parsed_len += PLAININFO(info).size + 8;
        for (int i = 0; i < indent; i++) {
            printf(INDENT);
        }
        printf("-%s size: %u", cfourcc(PLAININFO(info).chunk_id), PLAININFO(info).size);

        if (info.type == LIST) {
            printf(" type: %s\n", cfourcc(info.info.list.list_type));
            visit_list(file, PLAININFO(info).size, indent + 1);
        } else {
            RIFFPlainChunkInfo plain = PLAININFO(info);
            riff_skip_chunk(file, &plain);
            printf("\n");
        }
    }
}
