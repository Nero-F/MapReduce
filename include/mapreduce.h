#ifndef MAP_REDUCE_H_
#define MAP_REDUCE_H_

#include <assert.h>
#include <dlfcn.h>

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

#endif /* MAP_REDUCE_H_ */
