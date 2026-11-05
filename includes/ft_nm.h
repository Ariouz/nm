#ifndef FT_NM_H
# define FT_NM_H

# include <elf.h>
# include <errno.h>
# include <inttypes.h>
# include <locale.h>
# include <stdbool.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>

# define OPT_ALL 'a'
# define OPT_GLOBAL 'g'
# define OPT_UNDEFINED 'u'
# define OPT_REVERSE 'r'
# define OPT_NO_SORT 'p'

typedef struct s_opts {
    bool            all;
    bool            global;
    bool            undefined;
    bool            reverse;
    bool            no_sort;
    bool            help;
    char            **files;
    size_t          size;
}   t_opts;

typedef struct s_file {
    unsigned char   *data;
    size_t          size;
    const char      *name;
}   t_file;

typedef struct s_section {
    uint32_t        name;
    uint32_t        type;
    uint64_t        flags;
    uint64_t        addr;
    uint64_t        offset;
    uint64_t        size;
    uint32_t        link;
    uint32_t        info;
    uint64_t        align;
    uint64_t        entsize;
}   t_section;

typedef struct s_elf {
    t_file          *file;
    bool            is_64;
    bool            big_endian;
    uint16_t        type;
    uint64_t        shoff;
    uint16_t        shentsize;
    uint64_t        shnum;
    uint64_t        shstrndx;
    t_section       *sections;
    const char      *section_names;
    size_t          section_names_size;
}   t_elf;

typedef struct s_symbol {
    const char      *name;
    uint64_t        value;
    uint64_t        section;
    unsigned char   bind;
    unsigned char   type;
    char            code;
    bool            undefined;
    bool            extended;
}   t_symbol;

typedef struct s_symbols {
    t_symbol        *items;
    size_t          size;
}   t_symbols;

void        init_cli_options(t_opts *opts);
bool        parse_cli_options(t_opts *opts, int argc, char **argv);
bool        set_option(t_opts *opts, char code);
void        print_help(void);

int         file_error(const char *name, const char *message);
bool        open_file(t_file *file, const char *name);
bool        valid_range(const t_file *file, uint64_t offset, uint64_t size);
uint64_t    read_number(const t_elf *elf, uint64_t offset, size_t size);
const char  *get_string(const t_file *file, const t_section *table,
                uint64_t offset);
int         process_file(const char *name, const t_opts *opts);
int         process_object(t_file *file, const t_opts *opts, bool heading);

bool        init_elf(t_elf *elf, t_file *file);
bool        read_section(const t_elf *elf, uint64_t index, t_section *section);
bool        read_sections(t_elf *elf);
const char  *get_section_name(const t_elf *elf, uint64_t index);
bool        read_symbols(t_elf *elf, t_symbols *symbols);
char        get_symbol_type(const t_elf *elf, const t_symbol *symbol);
bool        keep_symbol(const t_symbol *symbol, const t_opts *opts);
bool        sort_symbols(t_symbols *symbols, const t_opts *opts);
void        print_symbols(const t_elf *elf, const t_symbols *symbols,
                const t_opts *opts);

int         process_archive(t_file *file, const t_opts *opts);
bool        parse_decimal(const unsigned char *data, size_t size,
                uint64_t *value);

#endif
