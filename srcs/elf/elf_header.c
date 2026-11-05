#include "ft_nm.h"

static bool read_ident(t_elf *elf)
{
    const unsigned char *ident;

    if (!valid_range(elf->file, 0, EI_NIDENT))
        return (false);
    ident = elf->file->data;
    if (memcmp(ident, ELFMAG, SELFMAG) != 0
        || (ident[EI_CLASS] != ELFCLASS32 && ident[EI_CLASS] != ELFCLASS64)
        || (ident[EI_DATA] != ELFDATA2LSB && ident[EI_DATA] != ELFDATA2MSB)
        || ident[EI_VERSION] != EV_CURRENT)
        return (false);
    elf->is_64 = ident[EI_CLASS] == ELFCLASS64;
    elf->big_endian = ident[EI_DATA] == ELFDATA2MSB;
    return (true);
}

static bool read_header(t_elf *elf)
{
    uint64_t    phoff;
    uint64_t    phnum;
    uint64_t    phentsize;
    uint64_t    machine;

    if (!valid_range(elf->file, 0, 52 + elf->is_64 * 12))
        return (false);
    elf->type = read_number(elf, 16, 2);
    machine = read_number(elf, 18, 2);
    if ((elf->type != ET_REL && elf->type != ET_EXEC && elf->type != ET_DYN)
        || machine != (uint64_t) (elf->is_64 ? EM_X86_64 : EM_386)
        || read_number(elf, 20, 4) != EV_CURRENT
        || read_number(elf, elf->is_64 ? 52 : 40, 2)
            != (uint64_t) (elf->is_64 ? 64 : 52))
        return (false);
    elf->shoff = read_number(elf, elf->is_64 ? 40 : 32, elf->is_64 ? 8 : 4);
    elf->shentsize = read_number(elf, elf->is_64 ? 58 : 46, 2);
    elf->shnum = read_number(elf, elf->is_64 ? 60 : 48, 2);
    elf->shstrndx = read_number(elf, elf->is_64 ? 62 : 50, 2);
    phoff = read_number(elf, elf->is_64 ? 32 : 28, elf->is_64 ? 8 : 4);
    phnum = read_number(elf, elf->is_64 ? 56 : 44, 2);
    phentsize = read_number(elf, elf->is_64 ? 54 : 42, 2);
    if (phnum == PN_XNUM)
    {
        t_section section;

        if (!read_section(elf, 0, &section))
            return (false);
        phnum = section.info;
    }
    return (!phnum || (phentsize == (uint64_t) (elf->is_64 ? 56 : 32)
            && valid_range(elf->file, phoff, phnum * phentsize)));
}

bool    init_elf(t_elf *elf, t_file *file)
{
    memset(elf, 0, sizeof(*elf));
    elf->file = file;
    return (read_ident(elf) && read_header(elf) && read_sections(elf));
}
