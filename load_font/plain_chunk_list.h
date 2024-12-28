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

void init_list(PlainChunkList* list);

[[nodiscard("this function takes VALUE of pointer, so this can't change the address the pointer points to.")]]
PlainChunkList* list_seek_head(PlainChunkList* list);

[[nodiscard("this function takes VALUE of pointer, so this can't change the address the pointer points to.")]]
PlainChunkList* list_seek_tail(PlainChunkList* list);

// there are no way but manual one to assign info to the very first item of the list
// but it shouldn't cause any harm, right? I'm a bit lazy and deadline is close.
void list_append(PlainChunkList* list, const RIFFPlainChunkInfo* info);

RIFFPlainChunkInfo search_list(PlainChunkList* list, FourCC target_id);

void free_list(PlainChunkList* list);
#endif
