#include "ft_nm.h"

static bool parse_option(t_opts *opts, const char *arg)
{
    size_t  index;

    if (strcmp(arg, "--help") == 0)
    {
        opts->help = true;
        return (true);
    }
    index = 1;
    while (arg[index])
    {
        if (!set_option(opts, arg[index]))
        {
            fprintf(stderr, "ft_nm: invalid option '%s' (use --help)\n", arg);
            return (false);
        }
        index++;
    }
    return (true);
}

bool    parse_cli_options(t_opts *opts, int argc, char **argv)
{
    int     index;
    bool    end_options;

    opts->files = malloc(sizeof(char *) * (size_t) argc);
    if (!opts->files)
        return (file_error(NULL, "Cannot allocate arguments"), false);
    index = 1;
    end_options = false;
    while (index < argc)
    {
        if (!end_options && strcmp(argv[index], "--") == 0)
            end_options = true;
        else if (!end_options && argv[index][0] == '-' && argv[index][1])
        {
            if (!parse_option(opts, argv[index]))
                return (free(opts->files), false);
        }
        else
            opts->files[opts->size++] = argv[index];
        index++;
    }
    return (true);
}
