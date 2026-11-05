#include "ft_nm.h"

void    print_symbols(const t_elf *elf, const t_symbols *symbols,
                const t_opts *opts)
{
    const t_symbol  *symbol;
    size_t          index;
    int             width;

    width = elf->is_64 ? 16 : 8;
    index = 0;
    while (index < symbols->size)
    {
        symbol = &symbols->items[index++];
        if (!keep_symbol(symbol, opts))
            continue ;
        if (symbol->undefined)
            printf("%*s", width, "");
        else
            printf("%0*" PRIx64, width, symbol->value);
        printf(" %c %s\n", symbol->code, symbol->name);
    }
}

int process_object(t_file *file, const t_opts *opts, bool heading)
{
    t_elf       elf;
    t_symbols   symbols;
    int         status;

    memset(&symbols, 0, sizeof(symbols));
    status = 0;
    errno = 0;
    if (!init_elf(&elf, file) || !read_symbols(&elf, &symbols))
    {
        if (errno == ENOMEM)
            status = file_error(file->name, "Cannot allocate ELF data");
        else
            status = file_error(file->name,
                    "File format not recognized or corrupt ELF");
    }
    else if (!sort_symbols(&symbols, opts))
        status = file_error(file->name, "Cannot allocate sorting buffer");
    else
    {
        if (heading)
            printf("\n%s:\n", file->name);
        if (!symbols.size)
            fprintf(stderr, "ft_nm: %s: no symbols\n", file->name);
        print_symbols(&elf, &symbols, opts);
    }
    free(symbols.items);
    free(elf.sections);
    return (status);
}
