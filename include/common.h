#pragma once

#define FAILURE 84
#define SUCCESS 0

#define INIT_CAP 256

#define da_append(arr, item)                                                   \
    do {                                                                       \
        if ((arr)->count >= (arr)->capacity) {                                 \
            (arr)->capacity                                                    \
                = (arr)->capacity == 0 ? INIT_CAP : (arr)->capacity * 2;       \
            (arr)->items = realloc(                                            \
                (arr)->items, (arr)->capacity * sizeof(*(arr)->items));        \
            assert((arr)->items != NULL);                                      \
        }                                                                      \
        (arr)->items[(arr)->count++] = (item);                                 \
    } while (0)

#define foreach(type, item, da)                                                \
    for (type *item = (da)->items; item < (da)->items + (da)->count; item++)
