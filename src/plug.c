#include <stdio.h>

int map(const char *key, const char *value)
{
    printf("map\n");
    return 0;
}

int reduce(int a, const char *key, const void *iterator[])
{
    printf("reduce\n");
    return 0;
}
