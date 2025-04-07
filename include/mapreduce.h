#ifndef MAP_REDUCE_H_
#define MAP_REDUCE_H_

#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef enum task_state_e {
    IDLE = 0,
    IN_PROGESS,
    COMPLETED,
} task_state_t;

typedef enum task_type_e {
    NONE = 0,
    MAP,
    REDUCE,
} task_type_t;

typedef struct key_value_s {
    char *key;
    char *value;
} kv_t;

typedef struct key_value_array_s {
    kv_t *items;
    size_t count;
    size_t capacity;
} kva_t;

#endif /* MAP_REDUCE_H_ */
