/*
 * general implementation of scanf used by scanf, sscanf, fscanf,
 * _cscanf, wscanf, swscanf and fwscanf
 *
 * Copyright 1996,1998 Marcus Meissner
 * Copyright 1996 Jukka Iivonen
 * Copyright 1997,2000 Uwe Bonnes
 * Copyright 2000 Jon Griffiths
 * Copyright 2002 Daniel Gudbjartsson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "msvcrt.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);

extern MSVCRT_FILE MSVCRT__iob[];

/* helper function for *scanf.  Returns the value of character c in the
 * given base, or -1 if the given character is not a digit of the base.
 */
static int char2digit(char c, int base) {
    if ((c>='0') && (c<='9') && (c<='0'+base-1)) return (c-'0');
    if (base<=10) return -1;
    if ((c>='A') && (c<='Z') && (c<='A'+base-11)) return (c-'A'+10);
    if ((c>='a') && (c<='z') && (c<='a'+base-11)) return (c-'a'+10);
    return -1;
}

/* helper function for *wscanf.  Returns the value of character c in the
 * given base, or -1 if the given character is not a digit of the base.
 */
static int wchar2digit(MSVCRT_wchar_t c, int base) {
    if ((c>='0') && (c<='9') && (c<='0'+base-1)) return (c-'0');
    if (base<=10) return -1;
    if ((c>='A') && (c<='Z') && (c<='A'+base-11)) return (c-'A'+10);
    if ((c>='a') && (c<='z') && (c<='a'+base-11)) return (c-'a'+10);
    return -1;
}

/* vfscanf */
#undef WIDE_SCANF
#undef CONSOLE
#undef STRING
#include "scanf.h"

/* vfwscanf */
#define WIDE_SCANF 1
#undef CONSOLE
#undef STRING
#include "scanf.h"

/* vsscanf */
#undef WIDE_SCANF
#undef CONSOLE
#define STRING 1
#include "scanf.h"

/* vswscanf */
#define WIDE_SCANF 1
#undef CONSOLE
#define STRING 1
#include "scanf.h"

/* vcscanf */
#undef WIDE_SCANF
#define CONSOLE 1
#undef STRING
#include "scanf.h"


/*********************************************************************
 *		fscanf (MSVCRT.@)
 */
int CDECL MSVCRT_fscanf(MSVCRT_FILE *file, const char *format, ...)
{
    __ms_va_list valist;
    int res;

    __ms_va_start(valist, format);
    res = MSVCRT_vfscanf(file, format, valist);
    __ms_va_end(valist);
    return res;
}

/*********************************************************************
 *		scanf (MSVCRT.@)
 */
int CDECL MSVCRT_scanf(const char *format, ...)
{
    __ms_va_list valist;
    int res;

    __ms_va_start(valist, format);
    res = MSVCRT_vfscanf(MSVCRT_stdin, format, valist);
    __ms_va_end(valist);
    return res;
}

/*********************************************************************
 *		fwscanf (MSVCRT.@)
 */
int CDECL MSVCRT_fwscanf(MSVCRT_FILE *file, const MSVCRT_wchar_t *format, ...)
{
    __ms_va_list valist;
    int res;

    __ms_va_start(valist, format);
    res = MSVCRT_vfwscanf(file, format, valist);
    __ms_va_end(valist);
    return res;
}


/*********************************************************************
 *		wscanf (MSVCRT.@)
 */
int CDECL MSVCRT_wscanf(const MSVCRT_wchar_t *format, ...)
{
    __ms_va_list valist;
    int res;

    __ms_va_start(valist, format);
    res = MSVCRT_vfwscanf(MSVCRT_stdin, format, valist);
    __ms_va_end(valist);
    return res;
}


/*********************************************************************
 *		sscanf (MSVCRT.@)
 */
int CDECL MSVCRT_sscanf(const char *str, const char *format, ...)
{
    __ms_va_list valist;
    int res;

    __ms_va_start(valist, format);
    res = MSVCRT_vsscanf(str, format, valist);
    __ms_va_end(valist);
    return res;
}


/*********************************************************************
 *		swscanf (MSVCRT.@)
 */
int CDECL MSVCRT_swscanf(const MSVCRT_wchar_t *str, const MSVCRT_wchar_t *format, ...)
{
    __ms_va_list valist;
    int res;

    __ms_va_start(valist, format);
    res = MSVCRT_vswscanf(str, format, valist);
    __ms_va_end(valist);
    return res;
}


/*********************************************************************
 *		_cscanf (MSVCRT.@)
 */
int CDECL _cscanf(const char *format, ...)
{
    __ms_va_list valist;
    int res;

    __ms_va_start(valist, format);
    res = MSVCRT_vcscanf(format, valist);
    __ms_va_end(valist);
    return res;
}
