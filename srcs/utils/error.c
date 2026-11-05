#include "ft_nm.h"

int file_error(const char *name, const char *message)
{
    if (name)
        fprintf(stderr, "ft_nm: '%s': %s\n", name, message);
    else
        fprintf(stderr, "ft_nm: %s\n", message);
    return (1);
}
