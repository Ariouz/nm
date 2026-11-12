#include "ft_nm.h"

int main(int argc, char **argv)
{
    t_opts  opts;
    size_t  index;
    int     status;

    setlocale(LC_COLLATE, "");
    init_cli_options(&opts);
    if (!parse_cli_options(&opts, argc, argv))
        return (1);
    if (opts.help)
    {
        print_help();
        free(opts.files);
        return (0);
    }
    status = 0;
    index = 0;
    if (opts.size == 0) {
        if (opts.files) free(opts.files);
        printf("A file is required\n");
        return 1;
    }
    while (index < opts.size)
    {
        if (process_file(opts.files[index], &opts) != 0)
            status++;
        index++;
    }
    free(opts.files);
    if ((fflush(stdout) == EOF || ferror(stdout)) && !status)
        status = 1;
    return (status);
}
