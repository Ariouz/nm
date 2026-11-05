#include "ft_nm.h"

static bool read_symbol(t_elf *elf, t_section *table, uint64_t index,
                t_symbol *symbol)
{
    uint64_t    offset;
    uint64_t    name;
    uint64_t    info;

    offset = table->offset + index * table->entsize;
    name = read_number(elf, offset, 4);
    info = read_number(elf, offset + (elf->is_64 ? 4 : 12), 1);
    symbol->bind = ELF64_ST_BIND(info);
    symbol->type = ELF64_ST_TYPE(info);
    symbol->section = read_number(elf, offset + (elf->is_64 ? 6 : 14), 2);
    symbol->extended = symbol->section == SHN_XINDEX;
    symbol->value = read_number(elf, offset + (elf->is_64 ? 8 : 4),
            elf->is_64 ? 8 : 4);
    if (symbol->section == SHN_COMMON)
        symbol->value = read_number(elf, offset + (elf->is_64 ? 16 : 8),
                elf->is_64 ? 8 : 4);
    symbol->name = get_string(elf->file, &elf->sections[table->link], name);
    return (symbol->name != NULL);
}

static bool resolve_section(t_elf *elf, t_section *table, uint64_t index,
                t_symbol *symbol)
{
    uint64_t    section_index;
    t_section   *extended;

    if (symbol->section == SHN_XINDEX)
    {
        section_index = 0;
        while (section_index < elf->shnum)
        {
            extended = &elf->sections[section_index++];
            if (extended->type == SHT_SYMTAB_SHNDX
                && extended->link == (uint64_t) (table - elf->sections))
            {
                if (extended->entsize != 4 || extended->size % 4
                    || extended->size / 4 != table->size / table->entsize)
                    return (false);
                symbol->section = read_number(elf,
                        extended->offset + index * 4, 4);
                return (symbol->section < elf->shnum);
            }
        }
        return (false);
    }
    return (symbol->section >= SHN_LORESERVE || symbol->section < elf->shnum);
}

static bool append_table(t_elf *elf, t_section *table, t_symbols *symbols)
{
    size_t      count;
    size_t      index;
    t_symbol    *symbol;

    if (table->entsize != (uint64_t) (elf->is_64 ? 24 : 16)
        || table->size % table->entsize || table->link >= elf->shnum
        || elf->sections[table->link].type != SHT_STRTAB)
        return (false);
    count = table->size / table->entsize;
    if (count > (SIZE_MAX / sizeof(t_symbol)) - symbols->size)
        return (false);
    if (count > 0)
    {
        symbol = realloc(symbols->items,
                sizeof(t_symbol) * (symbols->size + count));
        if (!symbol)
            return (false);
        symbols->items = symbol;
    }
    index = 1;
    while (index < count)
    {
        symbol = &symbols->items[symbols->size];
        if (!read_symbol(elf, table, index, symbol)
            || !resolve_section(elf, table, index, symbol))
            return (false);
        if (elf->type == ET_REL && symbol->section != SHN_UNDEF
            && (symbol->extended || symbol->section < SHN_LORESERVE))
        {
            if (symbol->value > UINT64_MAX - elf->sections[symbol->section].addr)
                return (false);
            symbol->value += elf->sections[symbol->section].addr;
        }
        if (symbol->type == STT_SECTION && !symbol->name[0])
            symbol->name = get_section_name(elf, symbol->section);
        symbol->undefined = symbol->section == SHN_UNDEF;
        symbol->code = get_symbol_type(elf, symbol);
        symbols->size++;
        index++;
    }
    return (true);
}

bool    read_symbols(t_elf *elf, t_symbols *symbols)
{
    uint64_t    index;

    index = 0;
    while (index < elf->shnum)
    {
        if (elf->sections[index].type == SHT_SYMTAB
            && !append_table(elf, &elf->sections[index], symbols))
            return (false);
        index++;
    }
    return (true);
}
