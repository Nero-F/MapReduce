#pragma once

#include <stdbool.h>

typedef struct node_s {
    void *value;
    struct node_s *next;
} node_t;

typedef node_t *llist_t;

unsigned int list_get_size(llist_t list);
bool list_is_empty(llist_t list);

typedef void (*value_displayer_t)(const void *value);

void list_dump(llist_t list, value_displayer_t val_disp);

#define foreach_ll(type, item, ll)                                                    \
    for (node_t *n = (ll); n != NULL; n = n->next)                             \
        for (type *item = (type *)n->value; item != NULL; item = NULL)

bool list_add_elem_at_front(llist_t *front_ptr, void *elem);
bool list_add_elem_at_back(llist_t *front_ptr, void *elem);
bool list_add_elem_at_position(
    llist_t *front_ptr, void *elem, unsigned int position);

bool list_del_elem_at_front(llist_t *front_ptr);
bool list_del_elem_at_back(llist_t *front_ptr);
bool list_del_elem_at_position(llist_t *front_ptr, unsigned int position);

void list_clear(llist_t *front_ptr, bool is_value_allocated);

void *list_get_elem_at_front(llist_t list);
void *list_get_elem_at_back(llist_t list);
void *list_get_elem_at_position(llist_t list, unsigned int position);
