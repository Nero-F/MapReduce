#include "worker.h"
#include <stdio.h>

#define go_defer(x)                                                            \
    do {                                                                       \
        res = (x);                                                             \
        goto defer;                                                            \
    } while (0);

int load_mr_plugs(const char *filename, worker_t *worker)
{
    printf("plug filename: %s\n", filename);
    void *handle = dlopen(filename, RTLD_NOW);
    int res = SUCCESS;
    char *err = NULL;

    if (handle == NULL) go_defer(FAILURE);

#define PLUG(func_name, ...)                                                   \
    worker->func_name = dlsym(handle, #func_name);                             \
    if (worker->func_name == NULL) go_defer(FAILURE);

    PLUGS_MR_LIST
#undef PLUG
    worker->plug_path = handle;

defer:
    err = dlerror();
    if (err) {
        printf("[ERROR]: %s\n", err);
        if (handle) dlclose(handle);
    }
    return res;
}
