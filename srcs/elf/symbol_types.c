#include "ft_nm.h"

static char get_section_type(const t_elf *elf, const t_symbol *symbol)
{
    const t_section *section;
    const char      *name;

    if (!symbol->extended && symbol->section == SHN_UNDEF)
        return ('U');
    if (!symbol->extended && symbol->section == SHN_ABS)
        return ('A');
    if (!symbol->extended && symbol->section == SHN_COMMON)
        return ('C');
    if (symbol->section >= elf->shnum)
        return ('?');
    section = &elf->sections[symbol->section];
    name = get_section_name(elf, symbol->section);
    if (section->type == SHT_GROUP)
        return ('n');
    if (section->flags & SHF_EXECINSTR)
        return ('T');
    if (section->type == SHT_NOBITS)
        return ('B');
    if (section->flags & SHF_ALLOC)
    {
        if (!(section->flags & SHF_WRITE))
            return ('R');
        return ('D');
    }
    if (strncmp(name, ".debug", 6) == 0 || strncmp(name, ".zdebug", 7) == 0
        || strncmp(name, ".stab", 5) == 0)
        return ('N');
    if (!(section->flags & SHF_WRITE))
        return ('n');
    return ('?');
}

char    get_symbol_type(const t_elf *elf, const t_symbol *symbol)
{
    char    code;

    if (symbol->bind == STB_GNU_UNIQUE && !symbol->undefined)
        return ('u');
    if (symbol->type == STT_GNU_IFUNC && !symbol->undefined)
        return ('i');
    if (symbol->bind == STB_WEAK)
    {
        if (symbol->type == STT_OBJECT)
            return (symbol->section == SHN_UNDEF ? 'v' : 'V');
        return (symbol->section == SHN_UNDEF ? 'w' : 'W');
    }
    code = get_section_type(elf, symbol);
    if (code == 'n' && symbol->bind != STB_LOCAL)
        code = 'N';
    if (symbol->bind == STB_LOCAL && code >= 'A' && code <= 'Z'
        && code != 'N' && code != 'U' && code != 'C')
        code += 'a' - 'A';
    return (code);
}

bool    keep_symbol(const t_symbol *symbol, const t_opts *opts)
{
    if (!opts->all && (symbol->type == STT_SECTION || symbol->type == STT_FILE))
        return (false);
    if (opts->global && symbol->bind == STB_LOCAL)
        return (false);
    if (opts->undefined && !symbol->undefined)
        return (false);
    return (true);
}
