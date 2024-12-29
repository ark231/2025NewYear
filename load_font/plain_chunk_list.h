#ifndef LOAD_FONT_PLAIN_CHUNK_LIST
#define LOAD_FONT_PLAIN_CHUNK_LIST
#include <riff_reader.h>

struct PlainChunkList;

struct PlainChunkList {
    RIFFPlainChunkInfo info;
    struct PlainChunkList* prev;
    struct PlainChunkList* next;
};

typedef struct PlainChunkList PlainChunkList;

bool list_is_head_sentinel(PlainChunkList* list);
bool list_is_tail_sentinel(PlainChunkList* list);
bool list_is_sentinel(PlainChunkList* list);

// initialize sentinel
PlainChunkList* new_list(void);

void list_seek_head(PlainChunkList** list);

void list_seek_tail(PlainChunkList** list);

// the first and last item is sentinel
void list_append(PlainChunkList* list, const RIFFPlainChunkInfo* info);

RIFFPlainChunkInfo* search_list(PlainChunkList* list, FourCC target_id);

void free_list(PlainChunkList* list);
#endif
