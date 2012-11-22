// +------------------------------------------------------------------+
// |             ____ _               _        __  __ _  __           |
// |            / ___| |__   ___  ___| | __   |  \/  | |/ /           |
// |           | |   | '_ \ / _ \/ __| |/ /   | |\/| | ' /            |
// |           | |___| | | |  __/ (__|   <    | |  | | . \            |
// |            \____|_| |_|\___|\___|_|\_\___|_|  |_|_|\_\           |
// |                                                                  |
// | Copyright Mathias Kettner 2010             mk@mathias-kettner.de |
// +------------------------------------------------------------------+
// 
// This file is part of Check_MK.
// The official homepage is at http://mathias-kettner.de/check_mk.
// 
// check_mk is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation in version 2.  check_mk is distributed
// in the hope that it will be useful, but WITHOUT ANY WARRANTY; with-
// out even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE. See the GNU General Public License for more de-
// ails.  You should have received a copy of the GNU General Public
// License along with GNU Make; see the file COPYING.  If not, write
// to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
// Boston, MA 02110-1301 USA.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "xutils.h"

void
n2a_rstrip(char *c)
{
    char *w = c + strlen(c) - 1;
    while (w >= c && isspace(*w))
        *w-- = '\0';
}

char *
n2a_lstrip(char *c)
{
    while (isspace(*c))
        c++;
    return c;
}

/*
 *c points to a string containing
 white space separated columns. This method returns
 a pointer to the zero-terminated next field. That
 might be identical with *c itself. The pointer c
 is then moved to the possible beginning of the
 next field. */
char *
n2a_next_field(char **c)
{
    /*
     *c points to first character of field */
    char *begin = n2a_lstrip(*c);    // skip leading spaces
    if (!*begin) {
        *c = begin;
        return 0;                // found end of string -> no more field
    }

    char *end = begin;            // copy pointer, search end of
    // field
    while (*end && !isspace(*end))
        end++;                    // search for \0 or white space
    if (*end) {                    // string continues -> terminate field
        // with '\0'
        *end = '\0';
        *c = end + 1;            // skip to character right *after* '\0'
    } else
        *c = end;                // no more field, point to '\0'
    return begin;
}

/*
 * similar to next_field() but takes one character as delimiter 
 */
char *
n2a_next_token(char **c, char delim)
{
    char *begin = *c;
    if (!*begin) {
        *c = begin;
        return 0;
    }

    char *end = begin;
    while (*end && *end != delim)
        end++;
    if (*end) {
        *end = 0;
        *c = end + 1;
    } else
        *c = end;
    return begin;
}

/*
 * these functions are part of the shelldone project developped by Benjamin
 * "Ziirish" Sans under the BSD licence
 */

int
xmin(int a, int b)
{
    return a < b ? a : b;
}

int
xmax(int a, int b)
{
    return a > b ? a : b;
}

void
xfree(void *ptr)
{
    if (ptr != NULL) {
        free(ptr);
    }
}

size_t
xstrlen(const char *in)
{
    const char *s;
    size_t cpt;
    if (in == NULL) {
        return 0;
    }
    /*
     * we want to avoid an overflow in case the input string isn't null
     * terminated 
     */
    for (s = in, cpt = 0; *s && cpt < UINT_MAX; ++s, cpt++) ;
    return (s - in);
}

void *
xmalloc(size_t size)
{
    void *ret = malloc(size);
    if (ret == NULL)
        err(2, "xmalloc can not allocate %lu bytes", (u_long) size);
    return ret;
}

char *
xstrdup(const char *dup)
{
    size_t len;
    char *copy;

    len = xstrlen(dup);
    if (len == 0)
        return NULL;
    copy = xmalloc(len + 1);
    if (copy != NULL)
        strncpy(copy, dup, len + 1);
    return copy;
}

int
xstrcmp (const char *c1, const char *c2)
{
    size_t s1 = xstrlen (c1);
    size_t s2 = xstrlen (c2);

    /* little optimisation based on the strings length */
    if (s1 == 0 && s2 == 0)
        return 0;
    if (s1 == 0)
        return -1;
    if (s2 == 0)
        return +1;
    return strncmp (c1, c2, xmax (s1, s2));
}
