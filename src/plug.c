#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "mapreduce.h"

/*
 * DISTRIBUTED WC
 */

kva_t map(char *filename, char *content)
{
    char *word = strtok(content, "\n \t");
    kva_t kva = { 0 };
    printf("%s\n", filename);
    // kva_t *kva = malloc(sizeof(kva_t));
    // ASSERT_MEM(kva);

    while (word != NULL) {
        kv_t kv = { .key = strdup(word), .value = "1" };
        da_append(&kva, kv);
        word = strtok(NULL, "\n \t");
    }

    // foreach (kv_t, item, &kva) {
    //     printf("item key: %s -- value: %s\n", item->key, item->value);
    // }
    return kva;
}

char *reduce(const char *key, const void *values[])
{
    size_t count = 0;
    while (values[count] != NULL)
        ++count;

    char *s_count = NULL;
    asprintf(&s_count, "%ld", count);
    return s_count;
}
