#include "ft_nm.h"

static int  compare_symbols(const t_symbol *left, const t_symbol *right,
                const t_opts *opts)
{
    int result;

    result = strcoll(left->name, right->name);
    if (result == 0)
        result = strcmp(left->name, right->name);
    if (result == 0 && left->value != right->value)
        result = left->value < right->value ? -1 : 1;
    if (opts->reverse)
        return (result > 0 ? -1 : result < 0);
    return (result);
}

/* Stable merge sort: bounded stack depth and O(n log n), even for hostile input. */
static void merge_sort(t_symbol *items, t_symbol *scratch, size_t size,
                const t_opts *opts)
{
    size_t  middle;
    size_t  left;
    size_t  right;
    size_t  index;

    if (size < 2)
        return ;
    middle = size / 2;
    merge_sort(items, scratch, middle, opts);
    merge_sort(items + middle, scratch + middle, size - middle, opts);
    left = 0;
    right = middle;
    index = 0;
    while (index < size)
    {
        if (right == size || (left < middle
                && compare_symbols(&items[left], &items[right], opts) <= 0))
            scratch[index++] = items[left++];
        else
            scratch[index++] = items[right++];
    }
    memcpy(items, scratch, size * sizeof(*items));
}

bool    sort_symbols(t_symbols *symbols, const t_opts *opts)
{
    t_symbol    *scratch;

    if (opts->no_sort || symbols->size < 2)
        return (true);
    scratch = malloc(symbols->size * sizeof(*scratch));
    if (!scratch)
        return (false);
    merge_sort(symbols->items, scratch, symbols->size, opts);
    free(scratch);
    return (true);
}
