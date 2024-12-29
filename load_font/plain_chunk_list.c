#include "plain_chunk_list.h"

#include <simple_logging.h>
#include <stdlib.h>
bool list_is_head_sentinel(PlainChunkList* list) { return list->prev == NULL; }
bool list_is_tail_sentinel(PlainChunkList* list) { return list->next == NULL; }
bool list_is_sentinel(PlainChunkList* list) { return list_is_head_sentinel(list) || list_is_tail_sentinel(list); }

void init_list(PlainChunkList* list) {
    list->info.chunk_id = FOURCC("NULL");
    list->info.size = 0;
    list->info.pos = 0;
    list->next = NULL;
    list->prev = NULL;
}
PlainChunkList* new_list(void) {
    PlainChunkList* result = malloc(sizeof(PlainChunkList));
    init_list(result);
    result->next = malloc(sizeof(PlainChunkList));
    init_list(result->next);
    result->next->prev = result;
    return result;
}
void list_seek_head(PlainChunkList** list) {
    while ((*list)->prev != NULL) {
        (*list) = (*list)->prev;
    }
}
void list_seek_tail(PlainChunkList** list) {
    while ((*list)->next != NULL) {
        (*list) = (*list)->next;
    }
}
void list_append(PlainChunkList* list, const RIFFPlainChunkInfo* info) {
    if (list->next != NULL) {
        list_seek_tail(&list);
    }
    PlainChunkList* sentinel = list;
    list = list->prev;
    SIMPLE_LOG(DEBUG, "appending %s", cfourcc(info->chunk_id));
    list->next = malloc(sizeof(PlainChunkList));
    list->next->prev = list;
    list = list->next;
    list->info = *info;
    sentinel->prev = list;
    list->next = sentinel;
}
void free_list(PlainChunkList* list) {
    list_seek_tail(&list);
    while (list->prev != NULL) {
        if (list->next != NULL) {
            free(list->next);
        }
        list = list->prev;
    }
}
RIFFPlainChunkInfo* search_list(PlainChunkList* list, FourCC target_id) {
    list_seek_head(&list);
    RIFFPlainChunkInfo* result = NULL;
    while (list->next != NULL) {
        if (list->info.chunk_id == target_id) {
            result = &list->info;
            break;
        }
        list = list->next;
    }
    return result;
}
