#include "ft_nm.h"

bool    open_file(t_file *file, const char *name)
{
    struct stat info;
    FILE        *stream;
    size_t      count;
    int         saved_errno;

    memset(file, 0, sizeof(*file));
    file->name = name;
    if (stat(name, &info) < 0)
        return (file_error(name, strerror(errno)), false);
    if (!S_ISREG(info.st_mode))
        return (file_error(name, "Not an ordinary file"), false);
    if (info.st_size <= 0 || (uintmax_t) info.st_size > SIZE_MAX)
        return (file_error(name, "File format not recognized"), false);
    stream = fopen(name, "rb");
    if (!stream)
        return (file_error(name, strerror(errno)), false);
    file->size = (size_t) info.st_size;
    file->data = malloc(file->size);
    if (!file->data)
    {
        fclose(stream);
        return (file_error(name, "Cannot allocate file buffer"), false);
    }
    count = fread(file->data, 1, file->size, stream);
    saved_errno = 0;
    if (fclose(stream) != 0)
        saved_errno = errno;
    if (saved_errno || count != file->size)
    {
        free(file->data);
        file->data = NULL;
        if (count != file->size)
            return (file_error(name, "Cannot read complete file"), false);
        return (file_error(name, strerror(saved_errno)), false);
    }
    return (true);
}

bool    valid_range(const t_file *file, uint64_t offset, uint64_t size)
{
    return (offset <= file->size && size <= file->size - offset);
}

int process_file(const char *name, const t_opts *opts)
{
    t_file  file;
    int     status;

    if (!open_file(&file, name))
        return (1);
    if (file.size >= 8 && memcmp(file.data, "!<arch>\n", 8) == 0)
        status = process_archive(&file, opts);
    else
        status = process_object(&file, opts, opts->size > 1);
    free(file.data);
    return (status);
}
