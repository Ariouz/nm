#include "ft_nm.h"

typedef struct s_archive {
    const unsigned char *names;
    size_t              names_size;
    uint64_t            offset;
}   t_archive;

static char *copy_name(const unsigned char *data, size_t size)
{
    char    *name;

    if (size == SIZE_MAX || memchr(data, '\0', size))
        return (NULL);
    name = malloc(size + 1);
    if (!name)
        return (NULL);
    memcpy(name, data, size);
    name[size] = '\0';
    return (name);
}

static char *long_name(const unsigned char *header, const t_archive *archive)
{
    uint64_t            offset;
    const unsigned char *end;
    size_t              size;

    if (!parse_decimal(header + 1, 15, &offset)
        || offset >= archive->names_size)
        return (NULL);
    end = memchr(archive->names + offset, '\n', archive->names_size - offset);
    if (!end)
        return (NULL);
    size = (size_t) (end - archive->names - offset);
    if (size && archive->names[offset + size - 1] == '/')
        size--;
    return (copy_name(archive->names + offset, size));
}

static char *member_name(const unsigned char *header, t_file *member,
                const t_archive *archive)
{
    uint64_t    length;
    size_t      size;

    if (memcmp(header, "#1/", 3) == 0)
    {
        if (!parse_decimal(header + 3, 13, &length) || length > member->size)
            return (NULL);
        size = length;
        while (size && member->data[size - 1] == '\0')
            size--;
        member->data += length;
        member->size -= length;
        return (copy_name(member->data - length, size));
    }
    if (header[0] == '/')
        return (long_name(header, archive));
    size = 16;
    while (size && header[size - 1] == ' ')
        size--;
    if (size && header[size - 1] == '/')
        size--;
    return (copy_name(header, size));
}

static int  process_member(t_file *file, t_archive *archive, uint64_t size,
                const t_opts *opts)
{
    const unsigned char *header;
    t_file              member;
    char                *name;
    int                 status;

    header = file->data + archive->offset;
    member.data = file->data + archive->offset + 60;
    member.size = size;
    if (memcmp(header, "//              ", 16) == 0)
    {
        archive->names = member.data;
        archive->names_size = member.size;
        return (0);
    }
    if (memcmp(header, "/               ", 16) == 0
        || memcmp(header, "/SYM64/         ", 16) == 0)
        return (0);
    name = member_name(header, &member, archive);
    if (!name || !name[0])
    {
        free(name);
        return (file_error(file->name, "Invalid archive member name"));
    }
    member.name = name;
    status = 0;
    if (strncmp(name, "__.SYMDEF", 9) != 0)
        status = process_object(&member, opts, true);
    free(name);
    return (status);
}

int process_archive(t_file *file, const t_opts *opts)
{
    t_archive   archive;
    uint64_t    size;
    int         status;

    memset(&archive, 0, sizeof(archive));
    archive.offset = 8;
    status = 0;
    if (opts->size > 1)
        printf("\n%s:\n", file->name);
    while (archive.offset < file->size)
    {
        if (!valid_range(file, archive.offset, 60)
            || memcmp(file->data + archive.offset + 58, "`\n", 2)
            || !parse_decimal(file->data + archive.offset + 48, 10, &size)
            || !valid_range(file, archive.offset + 60, size)
            || !valid_range(file, archive.offset + 60 + size, size % 2))
            return (file_error(file->name, "Malformed archive"));
        if (process_member(file, &archive, size, opts) != 0)
            status = 1;
        archive.offset += 60 + size + size % 2;
    }
    return (status);
}
