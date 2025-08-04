#ifndef _COMMON_H_
#define _COMMON_H_

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>

#define FAILURE 84
#define TERMINATE 42 // COORD DISCONNECTION
#define SUCCESS 0

#define INIT_CAP 256

#define ASSERT_MEM(v) assert((v) != NULL && "No memory available");
#define ASSERT_MEM_CTX(v, ctx_str) assert((v) != NULL && #ctx_str);

#define da_append(arr, item)                                                   \
  do {                                                                         \
    if ((arr)->count >= (arr)->capacity) {                                     \
      (arr)->capacity = (arr)->capacity == 0 ? INIT_CAP : (arr)->capacity * 2; \
      (arr)->items =                                                           \
          realloc((arr)->items, (arr)->capacity * sizeof(*(arr)->items));      \
      ASSERT_MEM((arr)->items);                                                \
    }                                                                          \
    (arr)->items[(arr)->count++] = (item);                                     \
  } while (0);

#define foreach(type, item, da)                                                \
  for (type *item = (da)->items; item < (da)->items + (da)->count; item++)

#define foreach_p(type, idx, item, da)                                         \
  size_t idx = 0;                                                              \
  for (type *item = (da)->items; item < (da)->items + (da)->count;             \
       item++, idx++)

typedef struct files_s {
  size_t count;
  size_t capacity;
  char **items;
} files_t;

#define ATOI_ARG(c, v, arg)                                                    \
  do {                                                                         \
    if ((c = atoi(v)) == 0) {                                                  \
      fprintf(stderr, "arg '%s' %s must be an integer\n", arg, v);             \
      return FAILURE;                                                          \
    }                                                                          \
  } while (0);

#define LOCK(mutex)                                                            \
  if (pthread_mutex_lock(mutex) != 0) {                                        \
    fprintf(stderr, "Could not wrlock resource\n");                            \
    return FAILURE;                                                            \
  }

#define UNLOCK(mutex)                                                          \
  if (pthread_mutex_unlock(mutex) != 0) {                                      \
    fprintf(stderr, "Could not unlock resource\n");                            \
    return FAILURE;                                                            \
  }

#endif /* _COMMON_H_ */
