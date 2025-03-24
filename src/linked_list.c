#include "linked_list.h"
#include <stdlib.h>

bool list_add_elem_at_front(llist_t *front_ptr, void *elem)
{
    node_t *new_node = malloc(sizeof(node_t));
    if (new_node == NULL) return false;

    new_node->value = elem;
    new_node->next = *front_ptr;
    *front_ptr = new_node;
    return true;
}

bool list_add_elem_at_back(llist_t *front_ptr, void *elem)
{
    node_t *node = *front_ptr;
    node_t *new_node = malloc(sizeof(node_t));
    if (!new_node) return false;
    new_node->value = elem;
    new_node->next = NULL;
    if (!*front_ptr) {
        *front_ptr = new_node;
        return true;
    }
    while (node->next) {
        node = node->next;
    }
    node->next = new_node;
    return true;
}

bool list_add_elem_at_position(
    llist_t *front_ptr, void *elem, unsigned int position)
{
    if (position == 0) return list_add_elem_at_front(front_ptr, elem);
    node_t *node = *front_ptr;
    node_t *new_node = NULL;
    unsigned int index = 0;

    while (node->next) {
        if (index == position) break;
        index++;
        node = node->next;
    }
    if (index < position) return false;
    new_node = malloc(sizeof(node_t));
    if (!new_node) return false;
    return true;
}

bool list_del_elem_at_front(llist_t *front_ptr)
{
    node_t *node = *front_ptr;

    if (*front_ptr == NULL) return false;
    *front_ptr = node->next;
    free(node);
    return true;
}

bool list_del_elem_at_back(llist_t *front_ptr)
{
    node_t *node = *front_ptr;

    if (!*front_ptr) return false;

    if (!node->next) {
        free(node);
        *front_ptr = NULL;
        return true;
    }
    while (node->next->next) {
        node = node->next;
    }
    free(node->next);
    node->next = NULL;
    return true;
}

bool list_del_elem_at_position(llist_t *front_ptr, unsigned int position)
{
    node_t *node = *front_ptr;
    node_t *prev = node;
    unsigned int index = 0;

    if (!*front_ptr) return false;
    if (position == 0) return list_del_elem_at_front(front_ptr);
    while (node) {
        if (index == position) {
            prev->next = node->next;
            free(node);
            break;
        }
        prev = node;
        node = node->next;
        index++;
    }
    if (index < position) return false;

    return true;
}

void list_clear(llist_t *front_ptr, bool is_value_allocated)
{
    node_t *node = *front_ptr;

    if (!front_ptr) return;
    while ((*front_ptr)->next) {
        node = (*front_ptr)->next;
        if (is_value_allocated) free((*front_ptr)->value);
        free(*front_ptr);
        *front_ptr = node;
    }
    free(*front_ptr);
}

void *list_get_elem_at_front(llist_t list)
{
    if (list == NULL) return NULL;
    return list->value;
}

void *list_get_elem_at_back(llist_t list)
{
    node_t *node = list;
    if (list == NULL) return NULL;

    while (node->next) {
        node = node->next;
    }
    return node->value;
}

void *list_get_elem_at_position(llist_t list, unsigned int position)
{
    node_t *node = list;
    unsigned int index = 0;
    if (list == NULL) return NULL;
    if (position == 0) return list_get_elem_at_front(list);

    while (node) {
        if (index == position) return node->value;
        index++;
        node = node->next;
    }
    return node->value;
}
