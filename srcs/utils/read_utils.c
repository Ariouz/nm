#include "ft_nm.h"

/* Callers validate the containing header/table before reading its fields. */
uint64_t    read_number(const t_elf *elf, uint64_t offset, size_t size)
{
    uint64_t    value;
    size_t      index;
    size_t      shift;

    value = 0;
    index = 0;
    while (index < size)
    {
        shift = index;
        if (elf->big_endian)
            shift = size - index - 1;
        value |= (uint64_t) elf->file->data[offset + index] << (shift * 8);
        index++;
    }
    return (value);
}

const char  *get_string(const t_file *file, const t_section *table,
                uint64_t offset)
{
    const char  *str;

    if (!valid_range(file, table->offset, table->size) || offset >= table->size)
        return (NULL);
    str = (const char *) file->data + table->offset + offset;
    if (!memchr(str, '\0', (size_t) (table->size - offset)))
        return (NULL);
    return (str);
}

bool    parse_decimal(const unsigned char *data, size_t size, uint64_t *value)
{
    size_t  index;
    bool    has_digit;

    *value = 0;
    index = 0;
    has_digit = false;
    while (index < size && data[index] == ' ')
        index++;
    while (index < size && data[index] >= '0' && data[index] <= '9')
    {
        if (*value > (UINT64_MAX - (data[index] - '0')) / 10)
            return (false);
        *value = *value * 10 + data[index++] - '0';
        has_digit = true;
    }
    while (index < size && data[index] == ' ')
        index++;
    return (has_digit && index == size);
}
