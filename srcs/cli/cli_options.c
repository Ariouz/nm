#include "ft_nm.h"

bool    set_option(t_opts *opts, char code)
{
#ifdef BONUS
    if (code == OPT_ALL)
        opts->all = true;
    else if (code == OPT_GLOBAL)
        opts->global = true;
    else if (code == OPT_UNDEFINED)
        opts->undefined = true;
    else if (code == OPT_REVERSE)
        opts->reverse = true;
    else if (code == OPT_NO_SORT)
        opts->no_sort = true;
    else
        return (false);
    return (true);
#else
    (void) opts;
    (void) code;
    return (false);
#endif
}
