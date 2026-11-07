#ifndef LINKLIST_H
#define LINKLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct link_list link_list_t;

struct link_list{
  void *item;
  size_t size_item;
  link_list_t *next;
};
link_list_t * link_list_new(void* item,size_t size_item);
void link_list_delete(link_list_t *list);
int32_t link_list_add(link_list_t *list,void* item);
int32_t link_list_remove(link_list_t *list,void *item);


#ifdef __cplusplus
}
#endif
#endif