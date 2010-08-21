/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

/* return string until token */
char *str_token(char **c, char delim) {
    char *begin = *c;
    if (!*begin) {
        *c = begin;
        return 0;
    }

    char *end = begin;
    while (*end && *end != delim) end++;
    if (*end) {
        *end = 0;
        *c = end + 1;
    }
    else
        *c = end;
    return begin;
}
