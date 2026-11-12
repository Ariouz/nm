#include "ft_nm.h"

void    init_cli_options(t_opts *opts)
{
    memset(opts, 0, sizeof(*opts));
}

void    print_help(void)
{
    printf("Usage: ft_nm [options] [files...]\n");
    printf("Display ELF symbols\n");
#ifdef BONUS
    printf("Options:\n");
    printf("    -a    Display all symbols, including debug symbols\n");
    printf("    -g    Display only external symbols\n");
    printf("    -u    Display only undefined symbols\n");
    printf("    -r    Reverse the symbol sort\n");
    printf("    -p    Preserve symbol table order\n");
#endif
    printf("    --    End options (allows filenames starting with '-')\n");
    printf("    --help    Display this help\n");
}
