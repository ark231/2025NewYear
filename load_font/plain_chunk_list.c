#include "plain_chunk_list.h"

#include <simple_logging.h>
#include <stdlib.h>

void init_list(PlainChunkList* list) {
    list->info.chunk_id = FOURCC("NULL");
    list->info.size = 0;
    list->info.pos = 0;
    list->next = NULL;
    list->prev = NULL;
}
PlainChunkList* list_seek_head(PlainChunkList* list) {  // this is VALUE OF POINTER, so changing the address this
                                                        // variable has does NOT affect caller's value
    while (list->prev != NULL) {
        list = list->prev;
    }
    return list;
}
PlainChunkList* list_seek_tail(PlainChunkList* list) {
    while (list->next != NULL) {
        SIMPLE_LOG(DEBUG, "%p -> %p", list, list->next);
        list = list->next;
    }
    return list;
}
void list_append(PlainChunkList* list, const RIFFPlainChunkInfo* info) {
    if (list->next != NULL) {
        list = list_seek_tail(list);
    }
    SIMPLE_LOG(DEBUG, "appending %s", cfourcc(info->chunk_id));
    list->next = malloc(sizeof(PlainChunkList));
    list->next->prev = list;
    list = list->next;
    list->info = *info;
}
void free_list(PlainChunkList* list) {
    list = list_seek_tail(list);
    while (list->prev != NULL) {
        if (list->next != NULL) {
            free(list->next);
        }
        list = list->prev;
    }
}
RIFFPlainChunkInfo search_list(PlainChunkList* list, FourCC target_id) {
    list = list_seek_head(list);
    RIFFPlainChunkInfo result = {FOURCC("NULL"), 0, 0};
    while (list->next != NULL) {
        if (list->info.chunk_id == target_id) {
            result = list->info;
            break;
        }
        list = list->next;
    }
    return result;
}
