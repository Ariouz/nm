#include "ft_nm.h"

bool    read_section(const t_elf *elf, uint64_t index, t_section *section)
{
    uint64_t    offset;
    size_t      word;

    word = elf->is_64 ? 8 : 4;
    if (elf->shentsize != (elf->is_64 ? 64 : 40) || !elf->shoff
        || index > (UINT64_MAX - elf->shoff) / elf->shentsize)
        return (false);
    offset = elf->shoff + index * elf->shentsize;
    if (!valid_range(elf->file, offset, elf->shentsize))
        return (false);
    section->name = read_number(elf, offset, 4);
    section->type = read_number(elf, offset + 4, 4);
    section->flags = read_number(elf, offset + 8, word);
    section->addr = read_number(elf, offset + 8 + word, word);
    section->offset = read_number(elf, offset + 8 + word * 2, word);
    section->size = read_number(elf, offset + 8 + word * 3, word);
    section->link = read_number(elf, offset + 8 + word * 4, 4);
    section->info = read_number(elf, offset + 12 + word * 4, 4);
    section->align = read_number(elf, offset + 16 + word * 4, word);
    section->entsize = read_number(elf, offset + 16 + word * 5, word);
    return (true);
}

static bool read_section_count(t_elf *elf)
{
    t_section  section;

    if (!elf->shoff)
        return (!elf->shnum && elf->shstrndx == SHN_UNDEF);
    if (!read_section(elf, 0, &section) || section.type != SHT_NULL)
        return (false);
    if (elf->shnum == 0)
        elf->shnum = section.size;
    if (elf->shstrndx == SHN_XINDEX)
        elf->shstrndx = section.link;
    return (elf->shnum > 0 && elf->shnum <= SIZE_MAX / sizeof(t_section)
        && elf->shnum <= (elf->file->size - elf->shoff) / elf->shentsize
        && elf->shstrndx < elf->shnum);
}

static bool read_section_names(t_elf *elf)
{
    t_section   *table;
    uint64_t    index;

    if (elf->shstrndx == SHN_UNDEF)
        return (true);
    table = &elf->sections[elf->shstrndx];
    if (table->type != SHT_STRTAB || !get_string(elf->file, table, 0))
        return (false);
    elf->section_names = (const char *) elf->file->data + table->offset;
    elf->section_names_size = table->size;
    index = 0;
    while (index < elf->shnum)
    {
        if (!get_string(elf->file, table, elf->sections[index].name))
            return (false);
        index++;
    }
    return (true);
}

bool    read_sections(t_elf *elf)
{
    uint64_t    index;
    t_section   *section;

    if (!read_section_count(elf))
        return (false);
    if (!elf->shnum)
        return (true);
    elf->sections = malloc(sizeof(t_section) * elf->shnum);
    if (!elf->sections)
        return (false);
    index = 0;
    while (index < elf->shnum)
    {
        section = &elf->sections[index];
        if (!read_section(elf, index, section))
            return (false);
        if (index && section->type != SHT_NOBITS
            && !valid_range(elf->file, section->offset, section->size))
            return (false);
        if (section->align && (section->align & (section->align - 1)))
            return (false);
        index++;
    }
    return (read_section_names(elf));
}

const char  *get_section_name(const t_elf *elf, uint64_t index)
{
    if (!elf->section_names || index >= elf->shnum)
        return ("");
    return (elf->section_names + elf->sections[index].name);
}
