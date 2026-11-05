/* Linked only into the test executable with GNU ld --wrap. */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ALLOCATIONS 16384

void    *__real_malloc(size_t size);
void    *__real_realloc(void *ptr, size_t size);
void    __real_free(void *ptr);

static void          *g_allocations[MAX_ALLOCATIONS];
static unsigned long g_calls;
static unsigned long g_fail_at;

static void tracker_error(const char *message)
{
    fprintf(stderr, "allocation tracker: %s\n", message);
    _Exit(99);
}

static size_t   find_allocation(void *ptr)
{
    size_t  index;

    index = 0;
    while (index < MAX_ALLOCATIONS && g_allocations[index] != ptr)
        index++;
    if (index == MAX_ALLOCATIONS)
        tracker_error("untracked pointer or double free");
    return (index);
}

static bool allocation_fails(void)
{
    g_calls++;
    if (g_calls == g_fail_at)
    {
        errno = ENOMEM;
        return (true);
    }
    return (false);
}

void    *__wrap_malloc(size_t size)
{
    void    *ptr;

    if (allocation_fails())
        return (NULL);
    ptr = __real_malloc(size);
    if (ptr)
        g_allocations[find_allocation(NULL)] = ptr;
    return (ptr);
}

void    *__wrap_realloc(void *ptr, size_t size)
{
    size_t  index;
    void    *result;

    index = find_allocation(ptr);
    if (allocation_fails())
        return (NULL);
    result = __real_realloc(ptr, size);
    if (result || !size)
        g_allocations[index] = result;
    return (result);
}

void    __wrap_free(void *ptr)
{
    if (!ptr)
        return ;
    g_allocations[find_allocation(ptr)] = NULL;
    __real_free(ptr);
}

static void check_allocations(void)
{
    size_t  index;

    index = 0;
    while (index < MAX_ALLOCATIONS)
    {
        if (g_allocations[index++])
            tracker_error("allocation was not freed");
    }
}

__attribute__((constructor)) static void init_tracker(void)
{
    const char  *value;

    value = getenv("NM_ALLOC_FAIL_AT");
    if (value)
        g_fail_at = strtoul(value, NULL, 10);
    if (atexit(check_allocations) != 0)
        tracker_error("cannot register allocation check");
}
