/*
 * Win32 processes
 *
 * Copyright 1996, 1998 Alexandre Julliard
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

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_PRCTL_H
# include <sys/prctl.h>
#endif
#include <sys/types.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "winternl.h"
#include "kernel_private.h"
#include "wine/server.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(process);
WINE_DECLARE_DEBUG_CHANNEL(file);
WINE_DECLARE_DEBUG_CHANNEL(relay);

#ifdef __APPLE__
extern char **__wine_get_main_environment(void);
#else
extern char **__wine_main_environ;
static char **__wine_get_main_environment(void) { return __wine_main_environ; }
#endif

typedef struct
{
    LPSTR lpEnvAddress;
    LPSTR lpCmdLine;
    LPSTR lpCmdShow;
    DWORD dwReserved;
} LOADPARMS32;

static UINT process_error_mode;

static DWORD shutdown_flags = 0;
static DWORD shutdown_priority = 0x280;
static DWORD process_dword;
static BOOL is_wow64;

HMODULE kernel32_handle = 0;

const WCHAR *DIR_Windows = NULL;
const WCHAR *DIR_System = NULL;
const WCHAR *DIR_SysWow64 = NULL;

/* Process flags */
#define PDB32_DEBUGGED      0x0001  /* Process is being debugged */
#define PDB32_WIN16_PROC    0x0008  /* Win16 process */
#define PDB32_DOS_PROC      0x0010  /* Dos process */
#define PDB32_CONSOLE_PROC  0x0020  /* Console process */
#define PDB32_FILE_APIS_OEM 0x0040  /* File APIs are OEM */
#define PDB32_WIN32S_PROC   0x8000  /* Win32s process */

static const WCHAR comW[] = {'.','c','o','m',0};
static const WCHAR batW[] = {'.','b','a','t',0};
static const WCHAR cmdW[] = {'.','c','m','d',0};
static const WCHAR pifW[] = {'.','p','i','f',0};
static const WCHAR winevdmW[] = {'w','i','n','e','v','d','m','.','e','x','e',0};

static void exec_process( LPCWSTR name );

extern void SHELL_LoadRegistry(void);


/***********************************************************************
 *           contains_path
 */
static inline int contains_path( LPCWSTR name )
{
    return ((*name && (name[1] == ':')) || strchrW(name, '/') || strchrW(name, '\\'));
}


/***********************************************************************
 *           is_special_env_var
 *
 * Check if an environment variable needs to be handled specially when
 * passed through the Unix environment (i.e. prefixed with "WINE").
 */
static inline int is_special_env_var( const char *var )
{
    return (!strncmp( var, "PATH=", sizeof("PATH=")-1 ) ||
            !strncmp( var, "HOME=", sizeof("HOME=")-1 ) ||
            !strncmp( var, "TEMP=", sizeof("TEMP=")-1 ) ||
            !strncmp( var, "TMP=", sizeof("TMP=")-1 ));
}


/***************************************************************************
 *	get_builtin_path
 *
 * Get the path of a builtin module when the native file does not exist.
 */
static BOOL get_builtin_path( const WCHAR *libname, const WCHAR *ext, WCHAR *filename, UINT size )
{
    WCHAR *file_part;
    UINT len = strlenW( DIR_System );

    if (contains_path( libname ))
    {
        if (RtlGetFullPathName_U( libname, size * sizeof(WCHAR),
                                  filename, &file_part ) > size * sizeof(WCHAR))
            return FALSE;  /* too long */

        if (strncmpiW( filename, DIR_System, len ) || filename[len] != '\\')
            return FALSE;
        while (filename[len] == '\\') len++;
        if (filename + len != file_part) return FALSE;
    }
    else
    {
        if (strlenW(libname) + len + 2 >= size) return FALSE;  /* too long */
        memcpy( filename, DIR_System, len * sizeof(WCHAR) );
        file_part = filename + len;
        if (file_part > filename && file_part[-1] != '\\') *file_part++ = '\\';
        strcpyW( file_part, libname );
    }
    if (ext && !strchrW( file_part, '.' ))
    {
        if (file_part + strlenW(file_part) + strlenW(ext) + 1 > filename + size)
            return FALSE;  /* too long */
        strcatW( file_part, ext );
    }
    return TRUE;
}


/***********************************************************************
 *           open_builtin_exe_file
 *
 * Open an exe file for a builtin exe.
 */
static void *open_builtin_exe_file( const WCHAR *name, char *error, int error_size,
                                    int test_only, int *file_exists )
{
    char exename[MAX_PATH];
    WCHAR *p;
    UINT i, len;

    *file_exists = 0;
    if ((p = strrchrW( name, '/' ))) name = p + 1;
    if ((p = strrchrW( name, '\\' ))) name = p + 1;

    /* we don't want to depend on the current codepage here */
    len = strlenW( name ) + 1;
    if (len >= sizeof(exename)) return NULL;
    for (i = 0; i < len; i++)
    {
        if (name[i] > 127) return NULL;
        exename[i] = (char)name[i];
        if (exename[i] >= 'A' && exename[i] <= 'Z') exename[i] += 'a' - 'A';
    }
    return wine_dll_load_main_exe( exename, error, error_size, test_only, file_exists );
}


/***********************************************************************
 *           open_exe_file
 *
 * Open a specific exe file, taking load order into account.
 * Returns the file handle or 0 for a builtin exe.
 */
static HANDLE open_exe_file( const WCHAR *name )
{
    HANDLE handle;

    TRACE("looking for %s\n", debugstr_w(name) );

    if ((handle = CreateFileW( name, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, 0, 0 )) == INVALID_HANDLE_VALUE)
    {
        WCHAR buffer[MAX_PATH];
        /* file doesn't exist, check for builtin */
        if (contains_path( name ) && get_builtin_path( name, NULL, buffer, sizeof(buffer) ))
            handle = 0;
    }
    return handle;
}


/***********************************************************************
 *           find_exe_file
 *
 * Open an exe file, and return the full name and file handle.
 * Returns FALSE if file could not be found.
 * If file exists but cannot be opened, returns TRUE and set handle to INVALID_HANDLE_VALUE.
 * If file is a builtin exe, returns TRUE and sets handle to 0.
 */
static BOOL find_exe_file( const WCHAR *name, WCHAR *buffer, int buflen, HANDLE *handle )
{
    static const WCHAR exeW[] = {'.','e','x','e',0};
    int file_exists;

    TRACE("looking for %s\n", debugstr_w(name) );

    if (!SearchPathW( NULL, name, exeW, buflen, buffer, NULL ) &&
        !get_builtin_path( name, exeW, buffer, buflen ))
    {
        /* no builtin found, try native without extension in case it is a Unix app */

        if (SearchPathW( NULL, name, NULL, buflen, buffer, NULL ))
        {
            TRACE( "Trying native/Unix binary %s\n", debugstr_w(buffer) );
            if ((*handle = CreateFileW( buffer, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE,
                                        NULL, OPEN_EXISTING, 0, 0 )) != INVALID_HANDLE_VALUE)
                return TRUE;
        }
        return FALSE;
    }

    TRACE( "Trying native exe %s\n", debugstr_w(buffer) );
    if ((*handle = CreateFileW( buffer, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, 0, 0 )) != INVALID_HANDLE_VALUE)
        return TRUE;

    TRACE( "Trying built-in exe %s\n", debugstr_w(buffer) );
    open_builtin_exe_file( buffer, NULL, 0, 1, &file_exists );
    if (file_exists)
    {
        *handle = 0;
        return TRUE;
    }

    return FALSE;
}


/***********************************************************************
 *           build_initial_environment
 *
 * Build the Win32 environment from the Unix environment
 */
static BOOL build_initial_environment(void)
{
    SIZE_T size = 1;
    char **e;
    WCHAR *p, *endptr;
    void *ptr;
    char **env = __wine_get_main_environment();

    /* Compute the total size of the Unix environment */
    for (e = env; *e; e++)
    {
        if (is_special_env_var( *e )) continue;
        size += MultiByteToWideChar( CP_UNIXCP, 0, *e, -1, NULL, 0 );
    }
    size *= sizeof(WCHAR);

    /* Now allocate the environment */
    ptr = NULL;
    if (NtAllocateVirtualMemory(NtCurrentProcess(), &ptr, 0, &size,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) != STATUS_SUCCESS)
        return FALSE;

    NtCurrentTeb()->Peb->ProcessParameters->Environment = p = ptr;
    endptr = p + size / sizeof(WCHAR);

    /* And fill it with the Unix environment */
    for (e = env; *e; e++)
    {
        char *str = *e;

        /* skip Unix special variables and use the Wine variants instead */
        if (!strncmp( str, "WINE", 4 ))
        {
            if (is_special_env_var( str + 4 )) str += 4;
            else if (!strncmp( str, "WINEPRELOADRESERVE=", 19 )) continue;  /* skip it */
        }
        else if (is_special_env_var( str )) continue;  /* skip it */

        MultiByteToWideChar( CP_UNIXCP, 0, str, -1, p, endptr - p );
        p += strlenW(p) + 1;
    }
    *p = 0;
    return TRUE;
}


/***********************************************************************
 *           set_registry_variables
 *
 * Set environment variables by enumerating the values of a key;
 * helper for set_registry_environment().
 * Note that Windows happily truncates the value if it's too big.
 */
static void set_registry_variables( HANDLE hkey, ULONG type )
{
    UNICODE_STRING env_name, env_value;
    NTSTATUS status;
    DWORD size;
    int index;
    char buffer[1024*sizeof(WCHAR) + sizeof(KEY_VALUE_FULL_INFORMATION)];
    KEY_VALUE_FULL_INFORMATION *info = (KEY_VALUE_FULL_INFORMATION *)buffer;

    for (index = 0; ; index++)
    {
        status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                      buffer, sizeof(buffer), &size );
        if (status != STATUS_SUCCESS && status != STATUS_BUFFER_OVERFLOW)
            break;
        if (info->Type != type)
            continue;
        env_name.Buffer = info->Name;
        env_name.Length = env_name.MaximumLength = info->NameLength;
        env_value.Buffer = (WCHAR *)(buffer + info->DataOffset);
        env_value.Length = env_value.MaximumLength = info->DataLength;
        if (env_value.Length && !env_value.Buffer[env_value.Length/sizeof(WCHAR)-1])
            env_value.Length -= sizeof(WCHAR);  /* don't count terminating null if any */
        if (info->Type == REG_EXPAND_SZ)
        {
            WCHAR buf_expanded[1024];
            UNICODE_STRING env_expanded;
            env_expanded.Length = env_expanded.MaximumLength = sizeof(buf_expanded);
            env_expanded.Buffer=buf_expanded;
            status = RtlExpandEnvironmentStrings_U(NULL, &env_value, &env_expanded, NULL);
            if (status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)
                RtlSetEnvironmentVariable( NULL, &env_name, &env_expanded );
        }
        else
        {
            RtlSetEnvironmentVariable( NULL, &env_name, &env_value );
        }
    }
}


/***********************************************************************
 *           set_registry_environment
 *
 * Set the environment variables specified in the registry.
 *
 * Note: Windows handles REG_SZ and REG_EXPAND_SZ in one pass with the
 * consequence that REG_EXPAND_SZ cannot be used reliably as it depends
 * on the order in which the variables are processed. But on Windows it
 * does not really matter since they only use %SystemDrive% and
 * %SystemRoot% which are predefined. But Wine defines these in the
 * registry, so we need two passes.
 */
static BOOL set_registry_environment(void)
{
    static const WCHAR env_keyW[] = {'M','a','c','h','i','n','e','\\',
                                     'S','y','s','t','e','m','\\',
                                     'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                     'C','o','n','t','r','o','l','\\',
                                     'S','e','s','s','i','o','n',' ','M','a','n','a','g','e','r','\\',
                                     'E','n','v','i','r','o','n','m','e','n','t',0};
    static const WCHAR envW[] = {'E','n','v','i','r','o','n','m','e','n','t',0};

    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    HANDLE hkey;
    BOOL ret = FALSE;

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    /* first the system environment variables */
    RtlInitUnicodeString( &nameW, env_keyW );
    if (NtOpenKey( &hkey, KEY_ALL_ACCESS, &attr ) == STATUS_SUCCESS)
    {
        set_registry_variables( hkey, REG_SZ );
        set_registry_variables( hkey, REG_EXPAND_SZ );
        NtClose( hkey );
        ret = TRUE;
    }

    /* then the ones for the current user */
    if (RtlOpenCurrentUser( KEY_ALL_ACCESS, &attr.RootDirectory ) != STATUS_SUCCESS) return ret;
    RtlInitUnicodeString( &nameW, envW );
    if (NtOpenKey( &hkey, KEY_ALL_ACCESS, &attr ) == STATUS_SUCCESS)
    {
        set_registry_variables( hkey, REG_SZ );
        set_registry_variables( hkey, REG_EXPAND_SZ );
        NtClose( hkey );
    }
    NtClose( attr.RootDirectory );
    return ret;
}


/***********************************************************************
 *           get_reg_value
 */
static WCHAR *get_reg_value( HKEY hkey, const WCHAR *name )
{
    char buffer[1024 * sizeof(WCHAR) + sizeof(KEY_VALUE_PARTIAL_INFORMATION)];
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    DWORD len, size = sizeof(buffer);
    WCHAR *ret = NULL;
    UNICODE_STRING nameW;

    RtlInitUnicodeString( &nameW, name );
    if (NtQueryValueKey( hkey, &nameW, KeyValuePartialInformation, buffer, size, &size ))
        return NULL;

    if (size <= FIELD_OFFSET( KEY_VALUE_PARTIAL_INFORMATION, Data )) return NULL;
    len = (size - FIELD_OFFSET( KEY_VALUE_PARTIAL_INFORMATION, Data )) / sizeof(WCHAR);

    if (info->Type == REG_EXPAND_SZ)
    {
        UNICODE_STRING value, expanded;

        value.MaximumLength = len * sizeof(WCHAR);
        value.Buffer = (WCHAR *)info->Data;
        if (!value.Buffer[len - 1]) len--;  /* don't count terminating null if any */
        value.Length = len * sizeof(WCHAR);
        expanded.Length = expanded.MaximumLength = 1024 * sizeof(WCHAR);
        if (!(expanded.Buffer = HeapAlloc( GetProcessHeap(), 0, expanded.MaximumLength ))) return NULL;
        if (!RtlExpandEnvironmentStrings_U( NULL, &value, &expanded, NULL )) ret = expanded.Buffer;
        else RtlFreeUnicodeString( &expanded );
    }
    else if (info->Type == REG_SZ)
    {
        if ((ret = HeapAlloc( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) )))
        {
            memcpy( ret, info->Data, len * sizeof(WCHAR) );
            ret[len] = 0;
        }
    }
    return ret;
}


/***********************************************************************
 *           set_additional_environment
 *
 * Set some additional environment variables not specified in the registry.
 */
static void set_additional_environment(void)
{
    static const WCHAR profile_keyW[] = {'M','a','c','h','i','n','e','\\',
                                         'S','o','f','t','w','a','r','e','\\',
                                         'M','i','c','r','o','s','o','f','t','\\',
                                         'W','i','n','d','o','w','s',' ','N','T','\\',
                                         'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
                                         'P','r','o','f','i','l','e','L','i','s','t',0};
    static const WCHAR profiles_valueW[] = {'P','r','o','f','i','l','e','s','D','i','r','e','c','t','o','r','y',0};
    static const WCHAR all_users_valueW[] = {'A','l','l','U','s','e','r','s','P','r','o','f','i','l','e','\0'};
    static const WCHAR usernameW[] = {'U','S','E','R','N','A','M','E',0};
    static const WCHAR userprofileW[] = {'U','S','E','R','P','R','O','F','I','L','E',0};
    static const WCHAR allusersW[] = {'A','L','L','U','S','E','R','S','P','R','O','F','I','L','E',0};
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    WCHAR *user_name = NULL, *profile_dir = NULL, *all_users_dir = NULL;
    HANDLE hkey;
    const char *name = wine_get_user_name();
    DWORD len;

    /* set the USERNAME variable */

    len = MultiByteToWideChar( CP_UNIXCP, 0, name, -1, NULL, 0 );
    if (len)
    {
        user_name = HeapAlloc( GetProcessHeap(), 0, len*sizeof(WCHAR) );
        MultiByteToWideChar( CP_UNIXCP, 0, name, -1, user_name, len );
        SetEnvironmentVariableW( usernameW, user_name );
    }
    else WARN( "user name %s not convertible.\n", debugstr_a(name) );

    /* set the USERPROFILE and ALLUSERSPROFILE variables */

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, profile_keyW );
    if (!NtOpenKey( &hkey, KEY_ALL_ACCESS, &attr ))
    {
        profile_dir = get_reg_value( hkey, profiles_valueW );
        all_users_dir = get_reg_value( hkey, all_users_valueW );
        NtClose( hkey );
    }

    if (profile_dir)
    {
        WCHAR *value, *p;

        if (all_users_dir) len = max( len, strlenW(all_users_dir) + 1 );
        len += strlenW(profile_dir) + 1;
        value = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        strcpyW( value, profile_dir );
        p = value + strlenW(value);
        if (p > value && p[-1] != '\\') *p++ = '\\';
        if (user_name) {
            strcpyW( p, user_name );
            SetEnvironmentVariableW( userprofileW, value );
        }
        if (all_users_dir)
        {
            strcpyW( p, all_users_dir );
            SetEnvironmentVariableW( allusersW, value );
        }
        HeapFree( GetProcessHeap(), 0, value );
    }

    HeapFree( GetProcessHeap(), 0, all_users_dir );
    HeapFree( GetProcessHeap(), 0, profile_dir );
    HeapFree( GetProcessHeap(), 0, user_name );
}

/***********************************************************************
 *              set_library_wargv
 *
 * Set the Wine library Unicode argv global variables.
 */
static void set_library_wargv( char **argv )
{
    int argc;
    char *q;
    WCHAR *p;
    WCHAR **wargv;
    DWORD total = 0;

    for (argc = 0; argv[argc]; argc++)
        total += MultiByteToWideChar( CP_UNIXCP, 0, argv[argc], -1, NULL, 0 );

    wargv = RtlAllocateHeap( GetProcessHeap(), 0,
                             total * sizeof(WCHAR) + (argc + 1) * sizeof(*wargv) );
    p = (WCHAR *)(wargv + argc + 1);
    for (argc = 0; argv[argc]; argc++)
    {
        DWORD reslen = MultiByteToWideChar( CP_UNIXCP, 0, argv[argc], -1, p, total );
        wargv[argc] = p;
        p += reslen;
        total -= reslen;
    }
    wargv[argc] = NULL;

    /* convert argv back from Unicode since it has to be in the Ansi codepage not the Unix one */

    for (argc = 0; wargv[argc]; argc++)
        total += WideCharToMultiByte( CP_ACP, 0, wargv[argc], -1, NULL, 0, NULL, NULL );

    argv = RtlAllocateHeap( GetProcessHeap(), 0, total + (argc + 1) * sizeof(*argv) );
    q = (char *)(argv + argc + 1);
    for (argc = 0; wargv[argc]; argc++)
    {
        DWORD reslen = WideCharToMultiByte( CP_ACP, 0, wargv[argc], -1, q, total, NULL, NULL );
        argv[argc] = q;
        q += reslen;
        total -= reslen;
    }
    argv[argc] = NULL;

    __wine_main_argc = argc;
    __wine_main_argv = argv;
    __wine_main_wargv = wargv;
}


/***********************************************************************
 *              update_library_argv0
 *
 * Update the argv[0] global variable with the binary we have found.
 */
static void update_library_argv0( const WCHAR *argv0 )
{
    DWORD len = strlenW( argv0 );

    if (len > strlenW( __wine_main_wargv[0] ))
    {
        __wine_main_wargv[0] = RtlAllocateHeap( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) );
    }
    strcpyW( __wine_main_wargv[0], argv0 );

    len = WideCharToMultiByte( CP_ACP, 0, argv0, -1, NULL, 0, NULL, NULL );
    if (len > strlen( __wine_main_argv[0] ) + 1)
    {
        __wine_main_argv[0] = RtlAllocateHeap( GetProcessHeap(), 0, len );
    }
    WideCharToMultiByte( CP_ACP, 0, argv0, -1, __wine_main_argv[0], len, NULL, NULL );
}


/***********************************************************************
 *           build_command_line
 *
 * Build the command line of a process from the argv array.
 *
 * Note that it does NOT necessarily include the file name.
 * Sometimes we don't even have any command line options at all.
 *
 * We must quote and escape characters so that the argv array can be rebuilt
 * from the command line:
 * - spaces and tabs must be quoted
 *   'a b'   -> '"a b"'
 * - quotes must be escaped
 *   '"'     -> '\"'
 * - if '\'s are followed by a '"', they must be doubled and followed by '\"',
 *   resulting in an odd number of '\' followed by a '"'
 *   '\"'    -> '\\\"'
 *   '\\"'   -> '\\\\\"'
 * - '\'s that are not followed by a '"' can be left as is
 *   'a\b'   == 'a\b'
 *   'a\\b'  == 'a\\b'
 */
static BOOL build_command_line( WCHAR **argv )
{
    int len;
    WCHAR **arg;
    LPWSTR p;
    RTL_USER_PROCESS_PARAMETERS* rupp = NtCurrentTeb()->Peb->ProcessParameters;

    if (rupp->CommandLine.Buffer) return TRUE; /* already got it from the server */

    len = 0;
    for (arg = argv; *arg; arg++)
    {
        int has_space,bcount;
        WCHAR* a;

        has_space=0;
        bcount=0;
        a=*arg;
        if( !*a ) has_space=1;
        while (*a!='\0') {
            if (*a=='\\') {
                bcount++;
            } else {
                if (*a==' ' || *a=='\t') {
                    has_space=1;
                } else if (*a=='"') {
                    /* doubling of '\' preceding a '"',
                     * plus escaping of said '"'
                     */
                    len+=2*bcount+1;
                }
                bcount=0;
            }
            a++;
        }
        len+=(a-*arg)+1 /* for the separating space */;
        if (has_space)
            len+=2; /* for the quotes */
    }

    if (!(rupp->CommandLine.Buffer = RtlAllocateHeap( GetProcessHeap(), 0, len * sizeof(WCHAR))))
        return FALSE;

    p = rupp->CommandLine.Buffer;
    rupp->CommandLine.Length = (len - 1) * sizeof(WCHAR);
    rupp->CommandLine.MaximumLength = len * sizeof(WCHAR);
    for (arg = argv; *arg; arg++)
    {
        int has_space,has_quote;
        WCHAR* a;

        /* Check for quotes and spaces in this argument */
        has_space=has_quote=0;
        a=*arg;
        if( !*a ) has_space=1;
        while (*a!='\0') {
            if (*a==' ' || *a=='\t') {
                has_space=1;
                if (has_quote)
                    break;
            } else if (*a=='"') {
                has_quote=1;
                if (has_space)
                    break;
            }
            a++;
        }

        /* Now transfer it to the command line */
        if (has_space)
            *p++='"';
        if (has_quote) {
            int bcount;
            WCHAR* a;

            bcount=0;
            a=*arg;
            while (*a!='\0') {
                if (*a=='\\') {
                    *p++=*a;
                    bcount++;
                } else {
                    if (*a=='"') {
                        int i;

                        /* Double all the '\\' preceding this '"', plus one */
                        for (i=0;i<=bcount;i++)
                            *p++='\\';
                        *p++='"';
                    } else {
                        *p++=*a;
                    }
                    bcount=0;
                }
                a++;
            }
        } else {
            WCHAR* x = *arg;
            while ((*p=*x++)) p++;
        }
        if (has_space)
            *p++='"';
        *p++=' ';
    }
    if (p > rupp->CommandLine.Buffer)
        p--;  /* remove last space */
    *p = '\0';

    return TRUE;
}


/***********************************************************************
 *           init_current_directory
 *
 * Initialize the current directory from the Unix cwd or the parent info.
 */
static void init_current_directory( CURDIR *cur_dir )
{
    UNICODE_STRING dir_str;
    char *cwd;
    int size;

    /* if we received a cur dir from the parent, try this first */

    if (cur_dir->DosPath.Length)
    {
        if (RtlSetCurrentDirectory_U( &cur_dir->DosPath ) == STATUS_SUCCESS) goto done;
    }

    /* now try to get it from the Unix cwd */

    for (size = 256; ; size *= 2)
    {
        if (!(cwd = HeapAlloc( GetProcessHeap(), 0, size ))) break;
        if (getcwd( cwd, size )) break;
        HeapFree( GetProcessHeap(), 0, cwd );
        if (errno == ERANGE) continue;
        cwd = NULL;
        break;
    }

    if (cwd)
    {
        WCHAR *dirW;
        int lenW = MultiByteToWideChar( CP_UNIXCP, 0, cwd, -1, NULL, 0 );
        if ((dirW = HeapAlloc( GetProcessHeap(), 0, lenW * sizeof(WCHAR) )))
        {
            MultiByteToWideChar( CP_UNIXCP, 0, cwd, -1, dirW, lenW );
            RtlInitUnicodeString( &dir_str, dirW );
            RtlSetCurrentDirectory_U( &dir_str );
            RtlFreeUnicodeString( &dir_str );
        }
    }

    if (!cur_dir->DosPath.Length)  /* still not initialized */
    {
        MESSAGE("Warning: could not find DOS drive for current working directory '%s', "
                "starting in the Windows directory.\n", cwd ? cwd : "" );
        RtlInitUnicodeString( &dir_str, DIR_Windows );
        RtlSetCurrentDirectory_U( &dir_str );
    }
    HeapFree( GetProcessHeap(), 0, cwd );

done:
    if (!cur_dir->Handle) chdir("/"); /* change to root directory so as not to lock cdroms */
    TRACE( "starting in %s %p\n", debugstr_w( cur_dir->DosPath.Buffer ), cur_dir->Handle );
}


/***********************************************************************
 *           init_windows_dirs
 *
 * Initialize the windows and system directories from the environment.
 */
static void init_windows_dirs(void)
{
    extern void CDECL __wine_init_windows_dir( const WCHAR *windir, const WCHAR *sysdir );

    static const WCHAR windirW[] = {'w','i','n','d','i','r',0};
    static const WCHAR winsysdirW[] = {'w','i','n','s','y','s','d','i','r',0};
    static const WCHAR default_windirW[] = {'C',':','\\','w','i','n','d','o','w','s',0};
    static const WCHAR default_sysdirW[] = {'\\','s','y','s','t','e','m','3','2',0};
    static const WCHAR default_syswow64W[] = {'\\','s','y','s','w','o','w','6','4',0};

    DWORD len;
    WCHAR *buffer;

    if ((len = GetEnvironmentVariableW( windirW, NULL, 0 )))
    {
        buffer = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        GetEnvironmentVariableW( windirW, buffer, len );
        DIR_Windows = buffer;
    }
    else DIR_Windows = default_windirW;

    if ((len = GetEnvironmentVariableW( winsysdirW, NULL, 0 )))
    {
        buffer = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        GetEnvironmentVariableW( winsysdirW, buffer, len );
        DIR_System = buffer;
    }
    else
    {
        len = strlenW( DIR_Windows );
        buffer = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) + sizeof(default_sysdirW) );
        memcpy( buffer, DIR_Windows, len * sizeof(WCHAR) );
        memcpy( buffer + len, default_sysdirW, sizeof(default_sysdirW) );
        DIR_System = buffer;
    }

#ifndef _WIN64  /* SysWow64 is always defined on 64-bit */
    if (is_wow64)
#endif
    {
        len = strlenW( DIR_Windows );
        buffer = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) + sizeof(default_syswow64W) );
        memcpy( buffer, DIR_Windows, len * sizeof(WCHAR) );
        memcpy( buffer + len, default_syswow64W, sizeof(default_syswow64W) );
        DIR_SysWow64 = buffer;
    }

    if (!CreateDirectoryW( DIR_Windows, NULL ) && GetLastError() != ERROR_ALREADY_EXISTS)
        ERR( "directory %s could not be created, error %u\n",
             debugstr_w(DIR_Windows), GetLastError() );
    if (!CreateDirectoryW( DIR_System, NULL ) && GetLastError() != ERROR_ALREADY_EXISTS)
        ERR( "directory %s could not be created, error %u\n",
             debugstr_w(DIR_System), GetLastError() );

    TRACE_(file)( "WindowsDir = %s\n", debugstr_w(DIR_Windows) );
    TRACE_(file)( "SystemDir  = %s\n", debugstr_w(DIR_System) );

    /* set the directories in ntdll too */
    __wine_init_windows_dir( DIR_Windows, DIR_System );
}


/***********************************************************************
 *           start_wineboot
 *
 * Start the wineboot process if necessary. Return the handles to wait on.
 */
static void start_wineboot( HANDLE handles[2] )
{
    static const WCHAR wineboot_eventW[] = {'_','_','w','i','n','e','b','o','o','t','_','e','v','e','n','t',0};

    handles[1] = 0;
    if (!(handles[0] = CreateEventW( NULL, TRUE, FALSE, wineboot_eventW )))
    {
        ERR( "failed to create wineboot event, expect trouble\n" );
        return;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS)  /* we created it */
    {
        static const WCHAR command_line[] = {'\\','w','i','n','e','b','o','o','t','.','e','x','e',' ','-','-','i','n','i','t',0};
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        WCHAR cmdline[MAX_PATH + sizeof(command_line)/sizeof(WCHAR)];

        memset( &si, 0, sizeof(si) );
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput  = 0;
        si.hStdOutput = 0;
        si.hStdError  = GetStdHandle( STD_ERROR_HANDLE );

        GetSystemDirectoryW( cmdline, MAX_PATH );
        lstrcatW( cmdline, command_line );
        if (CreateProcessW( NULL, cmdline, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi ))
        {
            TRACE( "started wineboot pid %04x tid %04x\n", pi.dwProcessId, pi.dwThreadId );
            CloseHandle( pi.hThread );
            handles[1] = pi.hProcess;
        }
        else
        {
            ERR( "failed to start wineboot, err %u\n", GetLastError() );
            CloseHandle( handles[0] );
            handles[0] = 0;
        }
    }
}


/***********************************************************************
 *           start_process
 *
 * Startup routine of a new process. Runs on the new process stack.
 */
static DWORD WINAPI start_process( PEB *peb )
{
    IMAGE_NT_HEADERS *nt;
    LPTHREAD_START_ROUTINE entry;

    nt = RtlImageNtHeader( peb->ImageBaseAddress );
    entry = (LPTHREAD_START_ROUTINE)((char *)peb->ImageBaseAddress +
                                     nt->OptionalHeader.AddressOfEntryPoint);

    if (!nt->OptionalHeader.AddressOfEntryPoint)
    {
        ERR( "%s doesn't have an entry point, it cannot be executed\n",
             debugstr_w(peb->ProcessParameters->ImagePathName.Buffer) );
        ExitThread( 1 );
    }

    if (TRACE_ON(relay))
        DPRINTF( "%04x:Starting process %s (entryproc=%p)\n", GetCurrentThreadId(),
                 debugstr_w(peb->ProcessParameters->ImagePathName.Buffer), entry );

    SetLastError( 0 );  /* clear error code */
    if (peb->BeingDebugged) DbgBreakPoint();
    return entry( peb );
}


/***********************************************************************
 *           set_process_name
 *
 * Change the process name in the ps output.
 */
static void set_process_name( int argc, char *argv[] )
{
#ifdef HAVE_SETPROCTITLE
    setproctitle("-%s", argv[1]);
#endif

#ifdef HAVE_PRCTL
    int i, offset;
    char *p, *prctl_name = argv[1];
    char *end = argv[argc-1] + strlen(argv[argc-1]) + 1;

#ifndef PR_SET_NAME
# define PR_SET_NAME 15
#endif

    if ((p = strrchr( prctl_name, '\\' ))) prctl_name = p + 1;
    if ((p = strrchr( prctl_name, '/' ))) prctl_name = p + 1;

    if (prctl( PR_SET_NAME, prctl_name ) != -1)
    {
        offset = argv[1] - argv[0];
        memmove( argv[1] - offset, argv[1], end - argv[1] );
        memset( end - offset, 0, offset );
        for (i = 1; i < argc; i++) argv[i-1] = argv[i] - offset;
        argv[i-1] = NULL;
    }
    else
#endif  /* HAVE_PRCTL */
    {
        /* remove argv[0] */
        memmove( argv, argv + 1, argc * sizeof(argv[0]) );
    }
}


/***********************************************************************
 *           __wine_kernel_init
 *
 * Wine initialisation: load and start the main exe file.
 */
void CDECL __wine_kernel_init(void)
{
    static const WCHAR kernel32W[] = {'k','e','r','n','e','l','3','2',0};
    static const WCHAR dotW[] = {'.',0};
    static const WCHAR exeW[] = {'.','e','x','e',0};

    WCHAR *p, main_exe_name[MAX_PATH+1];
    PEB *peb = NtCurrentTeb()->Peb;
    RTL_USER_PROCESS_PARAMETERS *params = peb->ProcessParameters;
    HANDLE boot_events[2];
    BOOL got_environment = TRUE;

    /* Initialize everything */

    setbuf(stdout,NULL);
    setbuf(stderr,NULL);
    kernel32_handle = GetModuleHandleW(kernel32W);
    IsWow64Process( GetCurrentProcess(), &is_wow64 );

    LOCALE_Init();

    if (!params->Environment)
    {
        /* Copy the parent environment */
        if (!build_initial_environment()) exit(1);

        /* convert old configuration to new format */
        convert_old_config();

        got_environment = set_registry_environment();
        set_additional_environment();
    }

    init_windows_dirs();
    init_current_directory( &params->CurrentDirectory );

    set_process_name( __wine_main_argc, __wine_main_argv );
    set_library_wargv( __wine_main_argv );
    boot_events[0] = boot_events[1] = 0;

    if (peb->ProcessParameters->ImagePathName.Buffer)
    {
        strcpyW( main_exe_name, peb->ProcessParameters->ImagePathName.Buffer );
    }
    else
    {
        if (!SearchPathW( NULL, __wine_main_wargv[0], exeW, MAX_PATH, main_exe_name, NULL ) &&
            !get_builtin_path( __wine_main_wargv[0], exeW, main_exe_name, MAX_PATH ))
        {
            MESSAGE( "wine: cannot find '%s'\n", __wine_main_argv[0] );
            ExitProcess( GetLastError() );
        }
        update_library_argv0( main_exe_name );
        if (!build_command_line( __wine_main_wargv )) goto error;
        start_wineboot( boot_events );
    }

    /* if there's no extension, append a dot to prevent LoadLibrary from appending .dll */
    p = strrchrW( main_exe_name, '.' );
    if (!p || strchrW( p, '/' ) || strchrW( p, '\\' )) strcatW( main_exe_name, dotW );

    TRACE( "starting process name=%s argv[0]=%s\n",
           debugstr_w(main_exe_name), debugstr_w(__wine_main_wargv[0]) );

    RtlInitUnicodeString( &NtCurrentTeb()->Peb->ProcessParameters->DllPath,
                          MODULE_get_dll_load_path(main_exe_name) );

    if (boot_events[0])
    {
        DWORD timeout = 30000, count = 1;

        if (boot_events[1]) count++;
        if (!got_environment) timeout = 300000;  /* initial prefix creation can take longer */
        if (WaitForMultipleObjects( count, boot_events, FALSE, timeout ) == WAIT_TIMEOUT)
            ERR( "boot event wait timed out\n" );
        CloseHandle( boot_events[0] );
        if (boot_events[1]) CloseHandle( boot_events[1] );
        /* if we didn't find environment section, try again now that wineboot has run */
        if (!got_environment)
        {
            set_registry_environment();
            set_additional_environment();
        }
    }

    if (!(peb->ImageBaseAddress = LoadLibraryExW( main_exe_name, 0, DONT_RESOLVE_DLL_REFERENCES )))
    {
        char msg[1024];
        DWORD error = GetLastError();

        /* if Win16/DOS format, or unavailable address, exec a new process with the proper setup */
        if (error == ERROR_BAD_EXE_FORMAT ||
            error == ERROR_INVALID_ADDRESS ||
            error == ERROR_NOT_ENOUGH_MEMORY)
        {
            if (!getenv("WINEPRELOADRESERVE")) exec_process( main_exe_name );
            /* if we get back here, it failed */
        }
        else if (error == ERROR_MOD_NOT_FOUND)
        {
            if ((p = strrchrW( main_exe_name, '\\' ))) p++;
            else p = main_exe_name;
            if (!strcmpiW( p, winevdmW ) && __wine_main_argc > 3)
            {
                /* args 1 and 2 are --app-name full_path */
                MESSAGE( "wine: could not run %s: 16-bit/DOS support missing\n",
                         debugstr_w(__wine_main_wargv[3]) );
                ExitProcess( ERROR_BAD_EXE_FORMAT );
            }
        }
        FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, msg, sizeof(msg), NULL );
        MESSAGE( "wine: could not load %s: %s", debugstr_w(main_exe_name), msg );
        ExitProcess( error );
    }

    LdrInitializeThunk( start_process, 0, 0, 0 );

 error:
    ExitProcess( GetLastError() );
}


/***********************************************************************
 *           build_argv
 *
 * Build an argv array from a command-line.
 * 'reserved' is the number of args to reserve before the first one.
 */
static char **build_argv( const WCHAR *cmdlineW, int reserved )
{
    int argc;
    char** argv;
    char *arg,*s,*d,*cmdline;
    int in_quotes,bcount,len;

    len = WideCharToMultiByte( CP_UNIXCP, 0, cmdlineW, -1, NULL, 0, NULL, NULL );
    if (!(cmdline = HeapAlloc( GetProcessHeap(), 0, len ))) return NULL;
    WideCharToMultiByte( CP_UNIXCP, 0, cmdlineW, -1, cmdline, len, NULL, NULL );

    argc=reserved+1;
    bcount=0;
    in_quotes=0;
    s=cmdline;
    while (1) {
        if (*s=='\0' || ((*s==' ' || *s=='\t') && !in_quotes)) {
            /* space */
            argc++;
            /* skip the remaining spaces */
            while (*s==' ' || *s=='\t') {
                s++;
            }
            if (*s=='\0')
                break;
            bcount=0;
            continue;
        } else if (*s=='\\') {
            /* '\', count them */
            bcount++;
        } else if ((*s=='"') && ((bcount & 1)==0)) {
            /* unescaped '"' */
            in_quotes=!in_quotes;
            bcount=0;
        } else {
            /* a regular character */
            bcount=0;
        }
        s++;
    }
    if (!(argv = HeapAlloc( GetProcessHeap(), 0, argc*sizeof(*argv) + len )))
    {
        HeapFree( GetProcessHeap(), 0, cmdline );
        return NULL;
    }

    arg = d = s = (char *)(argv + argc);
    memcpy( d, cmdline, len );
    bcount=0;
    in_quotes=0;
    argc=reserved;
    while (*s) {
        if ((*s==' ' || *s=='\t') && !in_quotes) {
            /* Close the argument and copy it */
            *d=0;
            argv[argc++]=arg;

            /* skip the remaining spaces */
            do {
                s++;
            } while (*s==' ' || *s=='\t');

            /* Start with a new argument */
            arg=d=s;
            bcount=0;
        } else if (*s=='\\') {
            /* '\\' */
            *d++=*s++;
            bcount++;
        } else if (*s=='"') {
            /* '"' */
            if ((bcount & 1)==0) {
                /* Preceded by an even number of '\', this is half that
                 * number of '\', plus a '"' which we discard.
                 */
                d-=bcount/2;
                s++;
                in_quotes=!in_quotes;
            } else {
                /* Preceded by an odd number of '\', this is half that
                 * number of '\' followed by a '"'
                 */
                d=d-bcount/2-1;
                *d++='"';
                s++;
            }
            bcount=0;
        } else {
            /* a regular character */
            *d++=*s++;
            bcount=0;
        }
    }
    if (*arg) {
        *d='\0';
        argv[argc++]=arg;
    }
    argv[argc]=NULL;

    HeapFree( GetProcessHeap(), 0, cmdline );
    return argv;
}


/***********************************************************************
 *           build_envp
 *
 * Build the environment of a new child process.
 */
static char **build_envp( const WCHAR *envW )
{
    static const char * const unix_vars[] = { "PATH", "TEMP", "TMP", "HOME" };

    const WCHAR *end;
    char **envp;
    char *env, *p;
    int count = 1, length;
    unsigned int i;

    for (end = envW; *end; count++) end += strlenW(end) + 1;
    end++;
    length = WideCharToMultiByte( CP_UNIXCP, 0, envW, end - envW, NULL, 0, NULL, NULL );
    if (!(env = HeapAlloc( GetProcessHeap(), 0, length ))) return NULL;
    WideCharToMultiByte( CP_UNIXCP, 0, envW, end - envW, env, length, NULL, NULL );

    for (p = env; *p; p += strlen(p) + 1)
        if (is_special_env_var( p )) length += 4; /* prefix it with "WINE" */

    for (i = 0; i < sizeof(unix_vars)/sizeof(unix_vars[0]); i++)
    {
        if (!(p = getenv(unix_vars[i]))) continue;
        length += strlen(unix_vars[i]) + strlen(p) + 2;
        count++;
    }

    if ((envp = HeapAlloc( GetProcessHeap(), 0, count * sizeof(*envp) + length )))
    {
        char **envptr = envp;
        char *dst = (char *)(envp + count);

        /* some variables must not be modified, so we get them directly from the unix env */
        for (i = 0; i < sizeof(unix_vars)/sizeof(unix_vars[0]); i++)
        {
            if (!(p = getenv(unix_vars[i]))) continue;
            *envptr++ = strcpy( dst, unix_vars[i] );
            strcat( dst, "=" );
            strcat( dst, p );
            dst += strlen(dst) + 1;
        }

        /* now put the Windows environment strings */
        for (p = env; *p; p += strlen(p) + 1)
        {
            if (*p == '=') continue;  /* skip drive curdirs, this crashes some unix apps */
            if (!strncmp( p, "WINEPRELOADRESERVE=", sizeof("WINEPRELOADRESERVE=")-1 )) continue;
            if (!strncmp( p, "WINELOADERNOEXEC=", sizeof("WINELOADERNOEXEC=")-1 )) continue;
            if (!strncmp( p, "WINESERVERSOCKET=", sizeof("WINESERVERSOCKET=")-1 )) continue;
            if (is_special_env_var( p ))  /* prefix it with "WINE" */
            {
                *envptr++ = strcpy( dst, "WINE" );
                strcat( dst, p );
            }
            else
            {
                *envptr++ = strcpy( dst, p );
            }
            dst += strlen(dst) + 1;
        }
        *envptr = 0;
    }
    HeapFree( GetProcessHeap(), 0, env );
    return envp;
}


/***********************************************************************
 *           fork_and_exec
 *
 * Fork and exec a new Unix binary, checking for errors.
 */
static int fork_and_exec( const char *filename, const WCHAR *cmdline, const WCHAR *env,
                          const char *newdir, DWORD flags, STARTUPINFOW *startup )
{
    int fd[2], stdin_fd = -1, stdout_fd = -1;
    int pid, err;
    char **argv, **envp;

    if (!env) env = GetEnvironmentStringsW();

#ifdef HAVE_PIPE2
    if (pipe2( fd, O_CLOEXEC ) == -1)
#endif
    {
        if (pipe(fd) == -1)
        {
            SetLastError( ERROR_TOO_MANY_OPEN_FILES );
            return -1;
        }
        fcntl( fd[0], F_SETFD, FD_CLOEXEC );
        fcntl( fd[1], F_SETFD, FD_CLOEXEC );
    }

    if (!(flags & (CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE | DETACHED_PROCESS)))
    {
        HANDLE hstdin, hstdout;

        if (startup->dwFlags & STARTF_USESTDHANDLES)
        {
            hstdin = startup->hStdInput;
            hstdout = startup->hStdOutput;
        }
        else
        {
            hstdin = GetStdHandle(STD_INPUT_HANDLE);
            hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
        }

        if (is_console_handle( hstdin ))
            hstdin = wine_server_ptr_handle( console_handle_unmap( hstdin ));
        if (is_console_handle( hstdout ))
            hstdout = wine_server_ptr_handle( console_handle_unmap( hstdout ));
        wine_server_handle_to_fd( hstdin, FILE_READ_DATA, &stdin_fd, NULL );
        wine_server_handle_to_fd( hstdout, FILE_WRITE_DATA, &stdout_fd, NULL );
    }

    argv = build_argv( cmdline, 0 );
    envp = build_envp( env );

    if (!(pid = fork()))  /* child */
    {
        close( fd[0] );

        if (flags & (CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE | DETACHED_PROCESS))
        {
            int pid;
            if (!(pid = fork()))
            {
                int fd = open( "/dev/null", O_RDWR );
                setsid();
                /* close stdin and stdout */
                if (fd != -1)
                {
                    dup2( fd, 0 );
                    dup2( fd, 1 );
                    close( fd );
                }
            }
            else if (pid != -1) _exit(0);  /* parent */
        }
        else
        {
            if (stdin_fd != -1)
            {
                dup2( stdin_fd, 0 );
                close( stdin_fd );
            }
            if (stdout_fd != -1)
            {
                dup2( stdout_fd, 1 );
                close( stdout_fd );
            }
        }

        /* Reset signals that we previously set to SIG_IGN */
        signal( SIGPIPE, SIG_DFL );
        signal( SIGCHLD, SIG_DFL );

        if (newdir) chdir(newdir);

        if (argv && envp) execve( filename, argv, envp );
        err = errno;
        write( fd[1], &err, sizeof(err) );
        _exit(1);
    }
    HeapFree( GetProcessHeap(), 0, argv );
    HeapFree( GetProcessHeap(), 0, envp );
    if (stdin_fd != -1) close( stdin_fd );
    if (stdout_fd != -1) close( stdout_fd );
    close( fd[1] );
    if ((pid != -1) && (read( fd[0], &err, sizeof(err) ) > 0))  /* exec failed */
    {
        errno = err;
        pid = -1;
    }
    if (pid == -1) FILE_SetDosError();
    close( fd[0] );
    return pid;
}


static inline DWORD append_string( void **ptr, const WCHAR *str )
{
    DWORD len = strlenW( str );
    memcpy( *ptr, str, len * sizeof(WCHAR) );
    *ptr = (WCHAR *)*ptr + len;
    return len * sizeof(WCHAR);
}

/***********************************************************************
 *           create_startup_info
 */
static startup_info_t *create_startup_info( LPCWSTR filename, LPCWSTR cmdline,
                                            LPCWSTR cur_dir, LPWSTR env, DWORD flags,
                                            const STARTUPINFOW *startup, DWORD *info_size )
{
    const RTL_USER_PROCESS_PARAMETERS *cur_params;
    startup_info_t *info;
    DWORD size;
    void *ptr;
    UNICODE_STRING newdir;
    WCHAR imagepath[MAX_PATH];
    HANDLE hstdin, hstdout, hstderr;

    if(!GetLongPathNameW( filename, imagepath, MAX_PATH ))
        lstrcpynW( imagepath, filename, MAX_PATH );
    if(!GetFullPathNameW( imagepath, MAX_PATH, imagepath, NULL ))
        lstrcpynW( imagepath, filename, MAX_PATH );

    cur_params = NtCurrentTeb()->Peb->ProcessParameters;

    newdir.Buffer = NULL;
    if (cur_dir)
    {
        if (RtlDosPathNameToNtPathName_U( cur_dir, &newdir, NULL, NULL ))
            cur_dir = newdir.Buffer + 4;  /* skip \??\ prefix */
        else
            cur_dir = NULL;
    }
    if (!cur_dir)
    {
        if (NtCurrentTeb()->Tib.SubSystemTib)  /* FIXME: hack */
            cur_dir = ((WIN16_SUBSYSTEM_TIB *)NtCurrentTeb()->Tib.SubSystemTib)->curdir.DosPath.Buffer;
        else
            cur_dir = cur_params->CurrentDirectory.DosPath.Buffer;
    }

    size = sizeof(*info);
    size += strlenW( cur_dir ) * sizeof(WCHAR);
    size += cur_params->DllPath.Length;
    size += strlenW( imagepath ) * sizeof(WCHAR);
    size += strlenW( cmdline ) * sizeof(WCHAR);
    if (startup->lpTitle) size += strlenW( startup->lpTitle ) * sizeof(WCHAR);
    if (startup->lpDesktop) size += strlenW( startup->lpDesktop ) * sizeof(WCHAR);
    /* FIXME: shellinfo */
    if (startup->lpReserved2 && startup->cbReserved2) size += startup->cbReserved2;
    size = (size + 1) & ~1;
    *info_size = size;

    if (!(info = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, size ))) goto done;

    info->console_flags = cur_params->ConsoleFlags;
    if (flags & CREATE_NEW_PROCESS_GROUP) info->console_flags = 1;
    if (flags & CREATE_NEW_CONSOLE) info->console = (obj_handle_t)1;  /* FIXME: cf. kernel_main.c */

    if (startup->dwFlags & STARTF_USESTDHANDLES)
    {
        hstdin  = startup->hStdInput;
        hstdout = startup->hStdOutput;
        hstderr = startup->hStdError;
    }
    else
    {
        hstdin  = GetStdHandle( STD_INPUT_HANDLE );
        hstdout = GetStdHandle( STD_OUTPUT_HANDLE );
        hstderr = GetStdHandle( STD_ERROR_HANDLE );
    }
    info->hstdin  = wine_server_obj_handle( hstdin );
    info->hstdout = wine_server_obj_handle( hstdout );
    info->hstderr = wine_server_obj_handle( hstderr );
    if ((flags & (CREATE_NEW_CONSOLE | DETACHED_PROCESS)) != 0)
    {
        /* this is temporary (for console handles). We have no way to control that the handle is invalid in child process otherwise */
        if (is_console_handle(hstdin))  info->hstdin  = wine_server_obj_handle( INVALID_HANDLE_VALUE );
        if (is_console_handle(hstdout)) info->hstdout = wine_server_obj_handle( INVALID_HANDLE_VALUE );
        if (is_console_handle(hstderr)) info->hstderr = wine_server_obj_handle( INVALID_HANDLE_VALUE );
    }
    else
    {
        if (is_console_handle(hstdin))  info->hstdin  = console_handle_unmap(hstdin);
        if (is_console_handle(hstdout)) info->hstdout = console_handle_unmap(hstdout);
        if (is_console_handle(hstderr)) info->hstderr = console_handle_unmap(hstderr);
    }

    info->x         = startup->dwX;
    info->y         = startup->dwY;
    info->xsize     = startup->dwXSize;
    info->ysize     = startup->dwYSize;
    info->xchars    = startup->dwXCountChars;
    info->ychars    = startup->dwYCountChars;
    info->attribute = startup->dwFillAttribute;
    info->flags     = startup->dwFlags;
    info->show      = startup->wShowWindow;

    ptr = info + 1;
    info->curdir_len = append_string( &ptr, cur_dir );
    info->dllpath_len = cur_params->DllPath.Length;
    memcpy( ptr, cur_params->DllPath.Buffer, cur_params->DllPath.Length );
    ptr = (char *)ptr + cur_params->DllPath.Length;
    info->imagepath_len = append_string( &ptr, imagepath );
    info->cmdline_len = append_string( &ptr, cmdline );
    if (startup->lpTitle) info->title_len = append_string( &ptr, startup->lpTitle );
    if (startup->lpDesktop) info->desktop_len = append_string( &ptr, startup->lpDesktop );
    if (startup->lpReserved2 && startup->cbReserved2)
    {
        info->runtime_len = startup->cbReserved2;
        memcpy( ptr, startup->lpReserved2, startup->cbReserved2 );
    }

done:
    RtlFreeUnicodeString( &newdir );
    return info;
}


/***********************************************************************
 *           create_process
 *
 * Create a new process. If hFile is a valid handle we have an exe
 * file, otherwise it is a Winelib app.
 */
static BOOL create_process( HANDLE hFile, LPCWSTR filename, LPWSTR cmd_line, LPWSTR env,
                            LPCWSTR cur_dir, LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa,
                            BOOL inherit, DWORD flags, LPSTARTUPINFOW startup,
                            LPPROCESS_INFORMATION info, LPCSTR unixdir,
                            void *res_start, void *res_end, DWORD binary_type, int exec_only )
{
    BOOL ret, success = FALSE;
    HANDLE process_info;
    WCHAR *env_end;
    char *winedebug = NULL;
    char **argv;
    startup_info_t *startup_info;
    DWORD startup_info_size;
    int socketfd[2], stdin_fd = -1, stdout_fd = -1;
    pid_t pid;
    int err;

    if (sizeof(void *) == sizeof(int) && !is_wow64 && (binary_type & BINARY_FLAG_64BIT))
    {
        ERR( "starting 64-bit process %s not supported on this platform\n", debugstr_w(filename) );
        SetLastError( ERROR_BAD_EXE_FORMAT );
        return FALSE;
    }

    RtlAcquirePebLock();

    if (!(startup_info = create_startup_info( filename, cmd_line, cur_dir, env, flags, startup,
                                              &startup_info_size )))
    {
        RtlReleasePebLock();
        return FALSE;
    }
    if (!env) env = NtCurrentTeb()->Peb->ProcessParameters->Environment;
    env_end = env;
    while (*env_end)
    {
        static const WCHAR WINEDEBUG[] = {'W','I','N','E','D','E','B','U','G','=',0};
        if (!winedebug && !strncmpW( env_end, WINEDEBUG, sizeof(WINEDEBUG)/sizeof(WCHAR) - 1 ))
        {
            DWORD len = WideCharToMultiByte( CP_UNIXCP, 0, env_end, -1, NULL, 0, NULL, NULL );
            if ((winedebug = HeapAlloc( GetProcessHeap(), 0, len )))
                WideCharToMultiByte( CP_UNIXCP, 0, env_end, -1, winedebug, len, NULL, NULL );
        }
        env_end += strlenW(env_end) + 1;
    }
    env_end++;

    /* create the socket for the new process */

    if (socketpair( PF_UNIX, SOCK_STREAM, 0, socketfd ) == -1)
    {
        RtlReleasePebLock();
        HeapFree( GetProcessHeap(), 0, winedebug );
        HeapFree( GetProcessHeap(), 0, startup_info );
        SetLastError( ERROR_TOO_MANY_OPEN_FILES );
        return FALSE;
    }
    wine_server_send_fd( socketfd[1] );
    close( socketfd[1] );

    /* create the process on the server side */

    SERVER_START_REQ( new_process )
    {
        req->inherit_all    = inherit;
        req->create_flags   = flags;
        req->socket_fd      = socketfd[1];
        req->exe_file       = wine_server_obj_handle( hFile );
        req->process_access = PROCESS_ALL_ACCESS;
        req->process_attr   = (psa && (psa->nLength >= sizeof(*psa)) && psa->bInheritHandle) ? OBJ_INHERIT : 0;
        req->thread_access  = THREAD_ALL_ACCESS;
        req->thread_attr    = (tsa && (tsa->nLength >= sizeof(*tsa)) && tsa->bInheritHandle) ? OBJ_INHERIT : 0;
        req->info_size      = startup_info_size;

        wine_server_add_data( req, startup_info, startup_info_size );
        wine_server_add_data( req, env, (env_end - env) * sizeof(WCHAR) );
        if ((ret = !wine_server_call_err( req )))
        {
            info->dwProcessId = (DWORD)reply->pid;
            info->dwThreadId  = (DWORD)reply->tid;
            info->hProcess    = wine_server_ptr_handle( reply->phandle );
            info->hThread     = wine_server_ptr_handle( reply->thandle );
        }
        process_info = wine_server_ptr_handle( reply->info );
    }
    SERVER_END_REQ;

    RtlReleasePebLock();
    if (!ret)
    {
        close( socketfd[0] );
        HeapFree( GetProcessHeap(), 0, startup_info );
        HeapFree( GetProcessHeap(), 0, winedebug );
        return FALSE;
    }

    if (!(flags & (CREATE_NEW_CONSOLE | DETACHED_PROCESS)))
    {
        if (startup_info->hstdin)
            wine_server_handle_to_fd( wine_server_ptr_handle(startup_info->hstdin),
                                      FILE_READ_DATA, &stdin_fd, NULL );
        if (startup_info->hstdout)
            wine_server_handle_to_fd( wine_server_ptr_handle(startup_info->hstdout),
                                      FILE_WRITE_DATA, &stdout_fd, NULL );
    }
    HeapFree( GetProcessHeap(), 0, startup_info );

    /* create the child process */
    argv = build_argv( cmd_line, 1 );

    if (exec_only || !(pid = fork()))  /* child */
    {
        char preloader_reserve[64], socket_env[64];

        if (flags & (CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE | DETACHED_PROCESS))
        {
            if (!(pid = fork()))
            {
                int fd = open( "/dev/null", O_RDWR );
                setsid();
                /* close stdin and stdout */
                if (fd != -1)
                {
                    dup2( fd, 0 );
                    dup2( fd, 1 );
                    close( fd );
                }
            }
            else if (pid != -1) _exit(0);  /* parent */
        }
        else
        {
            if (stdin_fd != -1) dup2( stdin_fd, 0 );
            if (stdout_fd != -1) dup2( stdout_fd, 1 );
        }

        if (stdin_fd != -1) close( stdin_fd );
        if (stdout_fd != -1) close( stdout_fd );

        /* Reset signals that we previously set to SIG_IGN */
        signal( SIGPIPE, SIG_DFL );
        signal( SIGCHLD, SIG_DFL );

        sprintf( socket_env, "WINESERVERSOCKET=%u", socketfd[0] );
        sprintf( preloader_reserve, "WINEPRELOADRESERVE=%lx-%lx",
                 (unsigned long)res_start, (unsigned long)res_end );

        putenv( preloader_reserve );
        putenv( socket_env );
        if (winedebug) putenv( winedebug );
        if (unixdir) chdir(unixdir);

        if (argv) wine_exec_wine_binary( NULL, argv, getenv("WINELOADER") );
        _exit(1);
    }

    /* this is the parent */

    if (stdin_fd != -1) close( stdin_fd );
    if (stdout_fd != -1) close( stdout_fd );
    close( socketfd[0] );
    HeapFree( GetProcessHeap(), 0, argv );
    HeapFree( GetProcessHeap(), 0, winedebug );
    if (pid == -1)
    {
        FILE_SetDosError();
        goto error;
    }

    /* wait for the new process info to be ready */

    WaitForSingleObject( process_info, INFINITE );
    SERVER_START_REQ( get_new_process_info )
    {
        req->info = wine_server_obj_handle( process_info );
        wine_server_call( req );
        success = reply->success;
        err = reply->exit_code;
    }
    SERVER_END_REQ;

    if (!success)
    {
        SetLastError( err ? err : ERROR_INTERNAL_ERROR );
        goto error;
    }
    CloseHandle( process_info );
    return success;

error:
    CloseHandle( process_info );
    CloseHandle( info->hProcess );
    CloseHandle( info->hThread );
    info->hProcess = info->hThread = 0;
    info->dwProcessId = info->dwThreadId = 0;
    return FALSE;
}


/***********************************************************************
 *           create_vdm_process
 *
 * Create a new VDM process for a 16-bit or DOS application.
 */
static BOOL create_vdm_process( LPCWSTR filename, LPWSTR cmd_line, LPWSTR env, LPCWSTR cur_dir,
                                LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa,
                                BOOL inherit, DWORD flags, LPSTARTUPINFOW startup,
                                LPPROCESS_INFORMATION info, LPCSTR unixdir,
                                DWORD binary_type, int exec_only )
{
    static const WCHAR argsW[] = {'%','s',' ','-','-','a','p','p','-','n','a','m','e',' ','"','%','s','"',' ','%','s',0};

    BOOL ret;
    LPWSTR new_cmd_line = HeapAlloc( GetProcessHeap(), 0,
                                     (strlenW(filename) + strlenW(cmd_line) + 30) * sizeof(WCHAR) );

    if (!new_cmd_line)
    {
        SetLastError( ERROR_OUTOFMEMORY );
        return FALSE;
    }
    sprintfW( new_cmd_line, argsW, winevdmW, filename, cmd_line );
    ret = create_process( 0, winevdmW, new_cmd_line, env, cur_dir, psa, tsa, inherit,
                          flags, startup, info, unixdir, NULL, NULL, binary_type, exec_only );
    HeapFree( GetProcessHeap(), 0, new_cmd_line );
    return ret;
}


/***********************************************************************
 *           create_cmd_process
 *
 * Create a new cmd shell process for a .BAT file.
 */
static BOOL create_cmd_process( LPCWSTR filename, LPWSTR cmd_line, LPVOID env, LPCWSTR cur_dir,
                                LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa,
                                BOOL inherit, DWORD flags, LPSTARTUPINFOW startup,
                                LPPROCESS_INFORMATION info )

{
    static const WCHAR comspecW[] = {'C','O','M','S','P','E','C',0};
    static const WCHAR slashcW[] = {' ','/','c',' ',0};
    WCHAR comspec[MAX_PATH];
    WCHAR *newcmdline;
    BOOL ret;

    if (!GetEnvironmentVariableW( comspecW, comspec, sizeof(comspec)/sizeof(WCHAR) ))
        return FALSE;
    if (!(newcmdline = HeapAlloc( GetProcessHeap(), 0,
                                  (strlenW(comspec) + 4 + strlenW(cmd_line) + 1) * sizeof(WCHAR))))
        return FALSE;

    strcpyW( newcmdline, comspec );
    strcatW( newcmdline, slashcW );
    strcatW( newcmdline, cmd_line );
    ret = CreateProcessW( comspec, newcmdline, psa, tsa, inherit,
                          flags, env, cur_dir, startup, info );
    HeapFree( GetProcessHeap(), 0, newcmdline );
    return ret;
}


/*************************************************************************
 *               get_file_name
 *
 * Helper for CreateProcess: retrieve the file name to load from the
 * app name and command line. Store the file name in buffer, and
 * return a possibly modified command line.
 * Also returns a handle to the opened file if it's a Windows binary.
 */
static LPWSTR get_file_name( LPCWSTR appname, LPWSTR cmdline, LPWSTR buffer,
                             int buflen, HANDLE *handle )
{
    static const WCHAR quotesW[] = {'"','%','s','"',0};

    WCHAR *name, *pos, *ret = NULL;
    const WCHAR *p;
    BOOL got_space;

    /* if we have an app name, everything is easy */

    if (appname)
    {
        /* use the unmodified app name as file name */
        lstrcpynW( buffer, appname, buflen );
        *handle = open_exe_file( buffer );
        if (!(ret = cmdline) || !cmdline[0])
        {
            /* no command-line, create one */
            if ((ret = HeapAlloc( GetProcessHeap(), 0, (strlenW(appname) + 3) * sizeof(WCHAR) )))
                sprintfW( ret, quotesW, appname );
        }
        return ret;
    }

    /* first check for a quoted file name */

    if ((cmdline[0] == '"') && ((p = strchrW( cmdline + 1, '"' ))))
    {
        int len = p - cmdline - 1;
        /* extract the quoted portion as file name */
        if (!(name = HeapAlloc( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) ))) return NULL;
        memcpy( name, cmdline + 1, len * sizeof(WCHAR) );
        name[len] = 0;

        if (find_exe_file( name, buffer, buflen, handle ))
            ret = cmdline;  /* no change necessary */
        goto done;
    }

    /* now try the command-line word by word */

    if (!(name = HeapAlloc( GetProcessHeap(), 0, (strlenW(cmdline) + 1) * sizeof(WCHAR) )))
        return NULL;
    pos = name;
    p = cmdline;
    got_space = FALSE;

    while (*p)
    {
        do *pos++ = *p++; while (*p && *p != ' ' && *p != '\t');
        *pos = 0;
        if (find_exe_file( name, buffer, buflen, handle ))
        {
            ret = cmdline;
            break;
        }
        if (*p) got_space = TRUE;
    }

    if (ret && got_space)  /* now build a new command-line with quotes */
    {
        if (!(ret = HeapAlloc( GetProcessHeap(), 0, (strlenW(cmdline) + 3) * sizeof(WCHAR) )))
            goto done;
        sprintfW( ret, quotesW, name );
        strcatW( ret, p );
    }
    else if (!ret) SetLastError( ERROR_FILE_NOT_FOUND );

 done:
    HeapFree( GetProcessHeap(), 0, name );
    return ret;
}


/**********************************************************************
 *       CreateProcessA          (KERNEL32.@)
 */
BOOL WINAPI CreateProcessA( LPCSTR app_name, LPSTR cmd_line, LPSECURITY_ATTRIBUTES process_attr,
                            LPSECURITY_ATTRIBUTES thread_attr, BOOL inherit,
                            DWORD flags, LPVOID env, LPCSTR cur_dir,
                            LPSTARTUPINFOA startup_info, LPPROCESS_INFORMATION info )
{
    BOOL ret = FALSE;
    WCHAR *app_nameW = NULL, *cmd_lineW = NULL, *cur_dirW = NULL;
    UNICODE_STRING desktopW, titleW;
    STARTUPINFOW infoW;

    desktopW.Buffer = NULL;
    titleW.Buffer = NULL;
    if (app_name && !(app_nameW = FILE_name_AtoW( app_name, TRUE ))) goto done;
    if (cmd_line && !(cmd_lineW = FILE_name_AtoW( cmd_line, TRUE ))) goto done;
    if (cur_dir && !(cur_dirW = FILE_name_AtoW( cur_dir, TRUE ))) goto done;

    if (startup_info->lpDesktop) RtlCreateUnicodeStringFromAsciiz( &desktopW, startup_info->lpDesktop );
    if (startup_info->lpTitle) RtlCreateUnicodeStringFromAsciiz( &titleW, startup_info->lpTitle );

    memcpy( &infoW, startup_info, sizeof(infoW) );
    infoW.lpDesktop = desktopW.Buffer;
    infoW.lpTitle = titleW.Buffer;

    if (startup_info->lpReserved)
      FIXME("StartupInfo.lpReserved is used, please report (%s)\n",
            debugstr_a(startup_info->lpReserved));

    ret = CreateProcessW( app_nameW, cmd_lineW, process_attr, thread_attr,
                          inherit, flags, env, cur_dirW, &infoW, info );
done:
    HeapFree( GetProcessHeap(), 0, app_nameW );
    HeapFree( GetProcessHeap(), 0, cmd_lineW );
    HeapFree( GetProcessHeap(), 0, cur_dirW );
    RtlFreeUnicodeString( &desktopW );
    RtlFreeUnicodeString( &titleW );
    return ret;
}


/**********************************************************************
 *       CreateProcessW          (KERNEL32.@)
 */
BOOL WINAPI CreateProcessW( LPCWSTR app_name, LPWSTR cmd_line, LPSECURITY_ATTRIBUTES process_attr,
                            LPSECURITY_ATTRIBUTES thread_attr, BOOL inherit, DWORD flags,
                            LPVOID env, LPCWSTR cur_dir, LPSTARTUPINFOW startup_info,
                            LPPROCESS_INFORMATION info )
{
    BOOL retv = FALSE;
    HANDLE hFile = 0;
    char *unixdir = NULL;
    WCHAR name[MAX_PATH];
    WCHAR *tidy_cmdline, *p, *envW = env;
    void *res_start, *res_end;
    DWORD binary_type;

    /* Process the AppName and/or CmdLine to get module name and path */

    TRACE("app %s cmdline %s\n", debugstr_w(app_name), debugstr_w(cmd_line) );

    if (!(tidy_cmdline = get_file_name( app_name, cmd_line, name, sizeof(name)/sizeof(WCHAR), &hFile )))
        return FALSE;
    if (hFile == INVALID_HANDLE_VALUE) goto done;

    /* Warn if unsupported features are used */

    if (flags & (IDLE_PRIORITY_CLASS | HIGH_PRIORITY_CLASS | REALTIME_PRIORITY_CLASS |
                 CREATE_NEW_PROCESS_GROUP | CREATE_SEPARATE_WOW_VDM | CREATE_SHARED_WOW_VDM |
                 CREATE_DEFAULT_ERROR_MODE | CREATE_NO_WINDOW |
                 PROFILE_USER | PROFILE_KERNEL | PROFILE_SERVER))
        WARN("(%s,...): ignoring some flags in %x\n", debugstr_w(name), flags);

    if (cur_dir)
    {
        if (!(unixdir = wine_get_unix_file_name( cur_dir )))
        {
            SetLastError(ERROR_DIRECTORY);
            goto done;
        }
    }
    else
    {
        WCHAR buf[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, buf)) unixdir = wine_get_unix_file_name( buf );
    }

    if (env && !(flags & CREATE_UNICODE_ENVIRONMENT))  /* convert environment to unicode */
    {
        char *p = env;
        DWORD lenW;

        while (*p) p += strlen(p) + 1;
        p++;  /* final null */
        lenW = MultiByteToWideChar( CP_ACP, 0, env, p - (char*)env, NULL, 0 );
        envW = HeapAlloc( GetProcessHeap(), 0, lenW * sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, env, p - (char*)env, envW, lenW );
        flags |= CREATE_UNICODE_ENVIRONMENT;
    }

    info->hThread = info->hProcess = 0;
    info->dwProcessId = info->dwThreadId = 0;

    /* Determine executable type */

    if (!hFile)  /* builtin exe */
    {
        TRACE( "starting %s as Winelib app\n", debugstr_w(name) );
        retv = create_process( 0, name, tidy_cmdline, envW, cur_dir, process_attr, thread_attr,
                               inherit, flags, startup_info, info, unixdir, NULL, NULL,
                               BINARY_UNIX_LIB, FALSE );
        goto done;
    }

    binary_type = MODULE_GetBinaryType( hFile, &res_start, &res_end );
    if (binary_type & BINARY_FLAG_DLL)
    {
        TRACE( "not starting %s since it is a dll\n", debugstr_w(name) );
        SetLastError( ERROR_BAD_EXE_FORMAT );
    }
    else switch (binary_type & BINARY_TYPE_MASK)
    {
    case BINARY_PE:
        TRACE( "starting %s as Win32 binary (%p-%p)\n", debugstr_w(name), res_start, res_end );
        retv = create_process( hFile, name, tidy_cmdline, envW, cur_dir, process_attr, thread_attr,
                               inherit, flags, startup_info, info, unixdir,
                               res_start, res_end, binary_type, FALSE );
        break;
    case BINARY_OS216:
    case BINARY_WIN16:
    case BINARY_DOS:
        TRACE( "starting %s as Win16/DOS binary\n", debugstr_w(name) );
        retv = create_vdm_process( name, tidy_cmdline, envW, cur_dir, process_attr, thread_attr,
                                   inherit, flags, startup_info, info, unixdir, binary_type, FALSE );
        break;
    case BINARY_UNIX_LIB:
        TRACE( "%s is a Unix library, starting as Winelib app\n", debugstr_w(name) );
        retv = create_process( hFile, name, tidy_cmdline, envW, cur_dir, process_attr, thread_attr,
                               inherit, flags, startup_info, info, unixdir,
                               NULL, NULL, binary_type, FALSE );
        break;
    case BINARY_UNKNOWN:
        /* check for .com or .bat extension */
        if ((p = strrchrW( name, '.' )))
        {
            if (!strcmpiW( p, comW ) || !strcmpiW( p, pifW ))
            {
                TRACE( "starting %s as DOS binary\n", debugstr_w(name) );
                retv = create_vdm_process( name, tidy_cmdline, envW, cur_dir, process_attr, thread_attr,
                                           inherit, flags, startup_info, info, unixdir,
                                           binary_type, FALSE );
                break;
            }
            if (!strcmpiW( p, batW ) || !strcmpiW( p, cmdW ) )
            {
                TRACE( "starting %s as batch binary\n", debugstr_w(name) );
                retv = create_cmd_process( name, tidy_cmdline, envW, cur_dir, process_attr, thread_attr,
                                           inherit, flags, startup_info, info );
                break;
            }
        }
        /* fall through */
    case BINARY_UNIX_EXE:
        {
            /* unknown file, try as unix executable */
            char *unix_name;

            TRACE( "starting %s as Unix binary\n", debugstr_w(name) );

            if ((unix_name = wine_get_unix_file_name( name )))
            {
                retv = (fork_and_exec( unix_name, tidy_cmdline, envW, unixdir, flags, startup_info ) != -1);
                HeapFree( GetProcessHeap(), 0, unix_name );
            }
        }
        break;
    }
    CloseHandle( hFile );

 done:
    if (tidy_cmdline != cmd_line) HeapFree( GetProcessHeap(), 0, tidy_cmdline );
    if (envW != env) HeapFree( GetProcessHeap(), 0, envW );
    HeapFree( GetProcessHeap(), 0, unixdir );
    if (retv)
        TRACE( "started process pid %04x tid %04x\n", info->dwProcessId, info->dwThreadId );
    return retv;
}


/**********************************************************************
 *       exec_process
 */
static void exec_process( LPCWSTR name )
{
    HANDLE hFile;
    WCHAR *p;
    void *res_start, *res_end;
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION info;
    DWORD binary_type;

    hFile = open_exe_file( name );
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return;

    memset( &startup_info, 0, sizeof(startup_info) );
    startup_info.cb = sizeof(startup_info);

    /* Determine executable type */

    binary_type = MODULE_GetBinaryType( hFile, &res_start, &res_end );
    if (binary_type & BINARY_FLAG_DLL) return;
    switch (binary_type & BINARY_TYPE_MASK)
    {
    case BINARY_PE:
        TRACE( "starting %s as Win32 binary (%p-%p)\n", debugstr_w(name), res_start, res_end );
        create_process( hFile, name, GetCommandLineW(), NULL, NULL, NULL, NULL,
                        FALSE, 0, &startup_info, &info, NULL, res_start, res_end, binary_type, TRUE );
        break;
    case BINARY_UNIX_LIB:
        TRACE( "%s is a Unix library, starting as Winelib app\n", debugstr_w(name) );
        create_process( hFile, name, GetCommandLineW(), NULL, NULL, NULL, NULL,
                        FALSE, 0, &startup_info, &info, NULL, NULL, NULL, binary_type, TRUE );
        break;
    case BINARY_UNKNOWN:
        /* check for .com or .pif extension */
        if (!(p = strrchrW( name, '.' ))) break;
        if (strcmpiW( p, comW ) && strcmpiW( p, pifW )) break;
        /* fall through */
    case BINARY_OS216:
    case BINARY_WIN16:
    case BINARY_DOS:
        TRACE( "starting %s as Win16/DOS binary\n", debugstr_w(name) );
        create_vdm_process( name, GetCommandLineW(), NULL, NULL, NULL, NULL,
                            FALSE, 0, &startup_info, &info, NULL, binary_type, TRUE );
        break;
    default:
        break;
    }
    CloseHandle( hFile );
}


/***********************************************************************
 *           wait_input_idle
 *
 * Wrapper to call WaitForInputIdle USER function
 */
typedef DWORD (WINAPI *WaitForInputIdle_ptr)( HANDLE hProcess, DWORD dwTimeOut );

static DWORD wait_input_idle( HANDLE process, DWORD timeout )
{
    HMODULE mod = GetModuleHandleA( "user32.dll" );
    if (mod)
    {
        WaitForInputIdle_ptr ptr = (WaitForInputIdle_ptr)GetProcAddress( mod, "WaitForInputIdle" );
        if (ptr) return ptr( process, timeout );
    }
    return 0;
}


/***********************************************************************
 *           WinExec   (KERNEL32.@)
 */
UINT WINAPI WinExec( LPCSTR lpCmdLine, UINT nCmdShow )
{
    PROCESS_INFORMATION info;
    STARTUPINFOA startup;
    char *cmdline;
    UINT ret;

    memset( &startup, 0, sizeof(startup) );
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = nCmdShow;

    /* cmdline needs to be writable for CreateProcess */
    if (!(cmdline = HeapAlloc( GetProcessHeap(), 0, strlen(lpCmdLine)+1 ))) return 0;
    strcpy( cmdline, lpCmdLine );

    if (CreateProcessA( NULL, cmdline, NULL, NULL, FALSE,
                        0, NULL, NULL, &startup, &info ))
    {
        /* Give 30 seconds to the app to come up */
        if (wait_input_idle( info.hProcess, 30000 ) == WAIT_FAILED)
            WARN("WaitForInputIdle failed: Error %d\n", GetLastError() );
        ret = 33;
        /* Close off the handles */
        CloseHandle( info.hThread );
        CloseHandle( info.hProcess );
    }
    else if ((ret = GetLastError()) >= 32)
    {
        FIXME("Strange error set by CreateProcess: %d\n", ret );
        ret = 11;
    }
    HeapFree( GetProcessHeap(), 0, cmdline );
    return ret;
}


/**********************************************************************
 *	    LoadModule    (KERNEL32.@)
 */
HINSTANCE WINAPI LoadModule( LPCSTR name, LPVOID paramBlock )
{
    LOADPARMS32 *params = paramBlock;
    PROCESS_INFORMATION info;
    STARTUPINFOA startup;
    HINSTANCE hInstance;
    LPSTR cmdline, p;
    char filename[MAX_PATH];
    BYTE len;

    if (!name) return (HINSTANCE)ERROR_FILE_NOT_FOUND;

    if (!SearchPathA( NULL, name, ".exe", sizeof(filename), filename, NULL ) &&
        !SearchPathA( NULL, name, NULL, sizeof(filename), filename, NULL ))
        return ULongToHandle(GetLastError());

    len = (BYTE)params->lpCmdLine[0];
    if (!(cmdline = HeapAlloc( GetProcessHeap(), 0, strlen(filename) + len + 2 )))
        return (HINSTANCE)ERROR_NOT_ENOUGH_MEMORY;

    strcpy( cmdline, filename );
    p = cmdline + strlen(cmdline);
    *p++ = ' ';
    memcpy( p, params->lpCmdLine + 1, len );
    p[len] = 0;

    memset( &startup, 0, sizeof(startup) );
    startup.cb = sizeof(startup);
    if (params->lpCmdShow)
    {
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = ((WORD *)params->lpCmdShow)[1];
    }

    if (CreateProcessA( filename, cmdline, NULL, NULL, FALSE, 0,
                        params->lpEnvAddress, NULL, &startup, &info ))
    {
        /* Give 30 seconds to the app to come up */
        if (wait_input_idle( info.hProcess, 30000 ) == WAIT_FAILED)
            WARN("WaitForInputIdle failed: Error %d\n", GetLastError() );
        hInstance = (HINSTANCE)33;
        /* Close off the handles */
        CloseHandle( info.hThread );
        CloseHandle( info.hProcess );
    }
    else if ((hInstance = ULongToHandle(GetLastError())) >= (HINSTANCE)32)
    {
        FIXME("Strange error set by CreateProcess: %p\n", hInstance );
        hInstance = (HINSTANCE)11;
    }

    HeapFree( GetProcessHeap(), 0, cmdline );
    return hInstance;
}


/******************************************************************************
 *           TerminateProcess   (KERNEL32.@)
 *
 * Terminates a process.
 *
 * PARAMS
 *  handle    [I] Process to terminate.
 *  exit_code [I] Exit code.
 *
 * RETURNS
 *  Success: TRUE.
 *  Failure: FALSE, check GetLastError().
 */
BOOL WINAPI TerminateProcess( HANDLE handle, DWORD exit_code )
{
    NTSTATUS status = NtTerminateProcess( handle, exit_code );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}

/***********************************************************************
 *           ExitProcess   (KERNEL32.@)
 *
 * Exits the current process.
 *
 * PARAMS
 *  status [I] Status code to exit with.
 *
 * RETURNS
 *  Nothing.
 */
#ifdef __i386__
__ASM_STDCALL_FUNC( ExitProcess, 4, /* Shrinker depend on this particular ExitProcess implementation */
                   "pushl %ebp\n\t"
                   ".byte 0x8B, 0xEC\n\t" /* movl %esp, %ebp */
                   ".byte 0x6A, 0x00\n\t" /* pushl $0 */
                   ".byte 0x68, 0x00, 0x00, 0x00, 0x00\n\t" /* pushl $0 - 4 bytes immediate */
                   "pushl 8(%ebp)\n\t"
                   "call " __ASM_NAME("process_ExitProcess") __ASM_STDCALL(4) "\n\t"
                   "leave\n\t"
                   "ret $4" )

void WINAPI process_ExitProcess( DWORD status )
{
    LdrShutdownProcess();
    NtTerminateProcess(GetCurrentProcess(), status);
    exit(status);
}

#else

void WINAPI ExitProcess( DWORD status )
{
    LdrShutdownProcess();
    NtTerminateProcess(GetCurrentProcess(), status);
    exit(status);
}

#endif

/***********************************************************************
 * GetExitCodeProcess           [KERNEL32.@]
 *
 * Gets termination status of specified process.
 *
 * PARAMS
 *   hProcess   [in]  Handle to the process.
 *   lpExitCode [out] Address to receive termination status.
 *
 * RETURNS
 *   Success: TRUE
 *   Failure: FALSE
 */
BOOL WINAPI GetExitCodeProcess( HANDLE hProcess, LPDWORD lpExitCode )
{
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION pbi;

    status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi,
                                       sizeof(pbi), NULL);
    if (status == STATUS_SUCCESS)
    {
        if (lpExitCode) *lpExitCode = pbi.ExitStatus;
        return TRUE;
    }
    SetLastError( RtlNtStatusToDosError(status) );
    return FALSE;
}


/***********************************************************************
 *           SetErrorMode   (KERNEL32.@)
 */
UINT WINAPI SetErrorMode( UINT mode )
{
    UINT old = process_error_mode;
    process_error_mode = mode;
    return old;
}

/***********************************************************************
 *           GetErrorMode   (KERNEL32.@)
 */
UINT WINAPI GetErrorMode( void )
{
    return process_error_mode;
}

/**********************************************************************
 * TlsAlloc             [KERNEL32.@]
 *
 * Allocates a thread local storage index.
 *
 * RETURNS
 *    Success: TLS index.
 *    Failure: 0xFFFFFFFF
 */
DWORD WINAPI TlsAlloc( void )
{
    DWORD index;
    PEB * const peb = NtCurrentTeb()->Peb;

    RtlAcquirePebLock();
    index = RtlFindClearBitsAndSet( peb->TlsBitmap, 1, 0 );
    if (index != ~0U) NtCurrentTeb()->TlsSlots[index] = 0; /* clear the value */
    else
    {
        index = RtlFindClearBitsAndSet( peb->TlsExpansionBitmap, 1, 0 );
        if (index != ~0U)
        {
            if (!NtCurrentTeb()->TlsExpansionSlots &&
                !(NtCurrentTeb()->TlsExpansionSlots = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                         8 * sizeof(peb->TlsExpansionBitmapBits) * sizeof(void*) )))
            {
                RtlClearBits( peb->TlsExpansionBitmap, index, 1 );
                index = ~0U;
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
            }
            else
            {
                NtCurrentTeb()->TlsExpansionSlots[index] = 0; /* clear the value */
                index += TLS_MINIMUM_AVAILABLE;
            }
        }
        else SetLastError( ERROR_NO_MORE_ITEMS );
    }
    RtlReleasePebLock();
    return index;
}


/**********************************************************************
 * TlsFree              [KERNEL32.@]
 *
 * Releases a thread local storage index, making it available for reuse.
 *
 * PARAMS
 *    index [in] TLS index to free.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI TlsFree( DWORD index )
{
    BOOL ret;

    RtlAcquirePebLock();
    if (index >= TLS_MINIMUM_AVAILABLE)
    {
        ret = RtlAreBitsSet( NtCurrentTeb()->Peb->TlsExpansionBitmap, index - TLS_MINIMUM_AVAILABLE, 1 );
        if (ret) RtlClearBits( NtCurrentTeb()->Peb->TlsExpansionBitmap, index - TLS_MINIMUM_AVAILABLE, 1 );
    }
    else
    {
        ret = RtlAreBitsSet( NtCurrentTeb()->Peb->TlsBitmap, index, 1 );
        if (ret) RtlClearBits( NtCurrentTeb()->Peb->TlsBitmap, index, 1 );
    }
    if (ret) NtSetInformationThread( GetCurrentThread(), ThreadZeroTlsCell, &index, sizeof(index) );
    else SetLastError( ERROR_INVALID_PARAMETER );
    RtlReleasePebLock();
    return TRUE;
}


/**********************************************************************
 * TlsGetValue          [KERNEL32.@]
 *
 * Gets value in a thread's TLS slot.
 *
 * PARAMS
 *    index [in] TLS index to retrieve value for.
 *
 * RETURNS
 *    Success: Value stored in calling thread's TLS slot for index.
 *    Failure: 0 and GetLastError() returns NO_ERROR.
 */
LPVOID WINAPI TlsGetValue( DWORD index )
{
    LPVOID ret;

    if (index < TLS_MINIMUM_AVAILABLE)
    {
        ret = NtCurrentTeb()->TlsSlots[index];
    }
    else
    {
        index -= TLS_MINIMUM_AVAILABLE;
        if (index >= 8 * sizeof(NtCurrentTeb()->Peb->TlsExpansionBitmapBits))
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return NULL;
        }
        if (!NtCurrentTeb()->TlsExpansionSlots) ret = NULL;
        else ret = NtCurrentTeb()->TlsExpansionSlots[index];
    }
    SetLastError( ERROR_SUCCESS );
    return ret;
}


/**********************************************************************
 * TlsSetValue          [KERNEL32.@]
 *
 * Stores a value in the thread's TLS slot.
 *
 * PARAMS
 *    index [in] TLS index to set value for.
 *    value [in] Value to be stored.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI TlsSetValue( DWORD index, LPVOID value )
{
    if (index < TLS_MINIMUM_AVAILABLE)
    {
        NtCurrentTeb()->TlsSlots[index] = value;
    }
    else
    {
        index -= TLS_MINIMUM_AVAILABLE;
        if (index >= 8 * sizeof(NtCurrentTeb()->Peb->TlsExpansionBitmapBits))
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return FALSE;
        }
        if (!NtCurrentTeb()->TlsExpansionSlots &&
            !(NtCurrentTeb()->TlsExpansionSlots = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                         8 * sizeof(NtCurrentTeb()->Peb->TlsExpansionBitmapBits) * sizeof(void*) )))
        {
            SetLastError( ERROR_NOT_ENOUGH_MEMORY );
            return FALSE;
        }
        NtCurrentTeb()->TlsExpansionSlots[index] = value;
    }
    return TRUE;
}


/***********************************************************************
 *           GetProcessFlags    (KERNEL32.@)
 */
DWORD WINAPI GetProcessFlags( DWORD processid )
{
    IMAGE_NT_HEADERS *nt;
    DWORD flags = 0;

    if (processid && processid != GetCurrentProcessId()) return 0;

    if ((nt = RtlImageNtHeader( NtCurrentTeb()->Peb->ImageBaseAddress )))
    {
        if (nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
            flags |= PDB32_CONSOLE_PROC;
    }
    if (!AreFileApisANSI()) flags |= PDB32_FILE_APIS_OEM;
    if (IsDebuggerPresent()) flags |= PDB32_DEBUGGED;
    return flags;
}


/***********************************************************************
 *           GetProcessDword    (KERNEL.485)
 *           GetProcessDword    (KERNEL32.18)
 * 'Of course you cannot directly access Windows internal structures'
 */
DWORD WINAPI GetProcessDword( DWORD dwProcessID, INT offset )
{
    DWORD               x, y;
    STARTUPINFOW        siw;

    TRACE("(%d, %d)\n", dwProcessID, offset );

    if (dwProcessID && dwProcessID != GetCurrentProcessId())
    {
        ERR("%d: process %x not accessible\n", offset, dwProcessID);
        return 0;
    }

    switch ( offset )
    {
    case GPD_APP_COMPAT_FLAGS:
        return GetAppCompatFlags16(0);
    case GPD_LOAD_DONE_EVENT:
        return 0;
    case GPD_HINSTANCE16:
        return GetTaskDS16();
    case GPD_WINDOWS_VERSION:
        return GetExeVersion16();
    case GPD_THDB:
        return (DWORD_PTR)NtCurrentTeb() - 0x10 /* FIXME */;
    case GPD_PDB:
        return (DWORD_PTR)NtCurrentTeb()->Peb; /* FIXME: truncating a pointer */
    case GPD_STARTF_SHELLDATA: /* return stdoutput handle from startupinfo ??? */
        GetStartupInfoW(&siw);
        return HandleToULong(siw.hStdOutput);
    case GPD_STARTF_HOTKEY: /* return stdinput handle from startupinfo ??? */
        GetStartupInfoW(&siw);
        return HandleToULong(siw.hStdInput);
    case GPD_STARTF_SHOWWINDOW:
        GetStartupInfoW(&siw);
        return siw.wShowWindow;
    case GPD_STARTF_SIZE:
        GetStartupInfoW(&siw);
        x = siw.dwXSize;
        if ( (INT)x == CW_USEDEFAULT ) x = CW_USEDEFAULT16;
        y = siw.dwYSize;
        if ( (INT)y == CW_USEDEFAULT ) y = CW_USEDEFAULT16;
        return MAKELONG( x, y );
    case GPD_STARTF_POSITION:
        GetStartupInfoW(&siw);
        x = siw.dwX;
        if ( (INT)x == CW_USEDEFAULT ) x = CW_USEDEFAULT16;
        y = siw.dwY;
        if ( (INT)y == CW_USEDEFAULT ) y = CW_USEDEFAULT16;
        return MAKELONG( x, y );
    case GPD_STARTF_FLAGS:
        GetStartupInfoW(&siw);
        return siw.dwFlags;
    case GPD_PARENT:
        return 0;
    case GPD_FLAGS:
        return GetProcessFlags(0);
    case GPD_USERDATA:
        return process_dword;
    default:
        ERR("Unknown offset %d\n", offset );
        return 0;
    }
}

/***********************************************************************
 *           SetProcessDword    (KERNEL.484)
 * 'Of course you cannot directly access Windows internal structures'
 */
void WINAPI SetProcessDword( DWORD dwProcessID, INT offset, DWORD value )
{
    TRACE("(%d, %d)\n", dwProcessID, offset );

    if (dwProcessID && dwProcessID != GetCurrentProcessId())
    {
        ERR("%d: process %x not accessible\n", offset, dwProcessID);
        return;
    }

    switch ( offset )
    {
    case GPD_APP_COMPAT_FLAGS:
    case GPD_LOAD_DONE_EVENT:
    case GPD_HINSTANCE16:
    case GPD_WINDOWS_VERSION:
    case GPD_THDB:
    case GPD_PDB:
    case GPD_STARTF_SHELLDATA:
    case GPD_STARTF_HOTKEY:
    case GPD_STARTF_SHOWWINDOW:
    case GPD_STARTF_SIZE:
    case GPD_STARTF_POSITION:
    case GPD_STARTF_FLAGS:
    case GPD_PARENT:
    case GPD_FLAGS:
        ERR("Not allowed to modify offset %d\n", offset );
        break;
    case GPD_USERDATA:
        process_dword = value;
        break;
    default:
        ERR("Unknown offset %d\n", offset );
        break;
    }
}


/***********************************************************************
 *           ExitProcess   (KERNEL.466)
 */
void WINAPI ExitProcess16( WORD status )
{
    DWORD count;
    ReleaseThunkLock( &count );
    ExitProcess( status );
}


/*********************************************************************
 *           OpenProcess   (KERNEL32.@)
 *
 * Opens a handle to a process.
 *
 * PARAMS
 *  access  [I] Desired access rights assigned to the returned handle.
 *  inherit [I] Determines whether or not child processes will inherit the handle.
 *  id      [I] Process identifier of the process to get a handle to.
 *
 * RETURNS
 *  Success: Valid handle to the specified process.
 *  Failure: NULL, check GetLastError().
 */
HANDLE WINAPI OpenProcess( DWORD access, BOOL inherit, DWORD id )
{
    NTSTATUS            status;
    HANDLE              handle;
    OBJECT_ATTRIBUTES   attr;
    CLIENT_ID           cid;

    cid.UniqueProcess = ULongToHandle(id);
    cid.UniqueThread = 0; /* FIXME ? */

    attr.Length = sizeof(OBJECT_ATTRIBUTES);
    attr.RootDirectory = NULL;
    attr.Attributes = inherit ? OBJ_INHERIT : 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    attr.ObjectName = NULL;

    if (GetVersion() & 0x80000000) access = PROCESS_ALL_ACCESS;

    status = NtOpenProcess(&handle, access, &attr, &cid);
    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return NULL;
    }
    return handle;
}


/*********************************************************************
 *           MapProcessHandle   (KERNEL.483)
 *           GetProcessId       (KERNEL32.@)
 *
 * Gets the a unique identifier of a process.
 *
 * PARAMS
 *  hProcess [I] Handle to the process.
 *
 * RETURNS
 *  Success: TRUE.
 *  Failure: FALSE, check GetLastError().
 *
 * NOTES
 *
 * The identifier is unique only on the machine and only until the process
 * exits (including system shutdown).
 */
DWORD WINAPI GetProcessId( HANDLE hProcess )
{
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION pbi;

    status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi,
                                       sizeof(pbi), NULL);
    if (status == STATUS_SUCCESS) return pbi.UniqueProcessId;
    SetLastError( RtlNtStatusToDosError(status) );
    return 0;
}


/*********************************************************************
 *           CloseW32Handle (KERNEL.474)
 *           CloseHandle    (KERNEL32.@)
 *
 * Closes a handle.
 *
 * PARAMS
 *  handle [I] Handle to close.
 *
 * RETURNS
 *  Success: TRUE.
 *  Failure: FALSE, check GetLastError().
 */
BOOL WINAPI CloseHandle( HANDLE handle )
{
    NTSTATUS status;

    /* stdio handles need special treatment */
    if ((handle == (HANDLE)STD_INPUT_HANDLE) ||
        (handle == (HANDLE)STD_OUTPUT_HANDLE) ||
        (handle == (HANDLE)STD_ERROR_HANDLE))
        handle = GetStdHandle( HandleToULong(handle) );

    if (is_console_handle(handle))
        return CloseConsoleHandle(handle);

    status = NtClose( handle );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/*********************************************************************
 *           GetHandleInformation   (KERNEL32.@)
 */
BOOL WINAPI GetHandleInformation( HANDLE handle, LPDWORD flags )
{
    OBJECT_DATA_INFORMATION info;
    NTSTATUS status = NtQueryObject( handle, ObjectDataInformation, &info, sizeof(info), NULL );

    if (status) SetLastError( RtlNtStatusToDosError(status) );
    else if (flags)
    {
        *flags = 0;
        if (info.InheritHandle) *flags |= HANDLE_FLAG_INHERIT;
        if (info.ProtectFromClose) *flags |= HANDLE_FLAG_PROTECT_FROM_CLOSE;
    }
    return !status;
}


/*********************************************************************
 *           SetHandleInformation   (KERNEL32.@)
 */
BOOL WINAPI SetHandleInformation( HANDLE handle, DWORD mask, DWORD flags )
{
    OBJECT_DATA_INFORMATION info;
    NTSTATUS status;

    /* if not setting both fields, retrieve current value first */
    if ((mask & (HANDLE_FLAG_INHERIT | HANDLE_FLAG_PROTECT_FROM_CLOSE)) !=
        (HANDLE_FLAG_INHERIT | HANDLE_FLAG_PROTECT_FROM_CLOSE))
    {
        if ((status = NtQueryObject( handle, ObjectDataInformation, &info, sizeof(info), NULL )))
        {
            SetLastError( RtlNtStatusToDosError(status) );
            return FALSE;
        }
    }
    if (mask & HANDLE_FLAG_INHERIT)
        info.InheritHandle = (flags & HANDLE_FLAG_INHERIT) != 0;
    if (mask & HANDLE_FLAG_PROTECT_FROM_CLOSE)
        info.ProtectFromClose = (flags & HANDLE_FLAG_PROTECT_FROM_CLOSE) != 0;

    status = NtSetInformationObject( handle, ObjectDataInformation, &info, sizeof(info) );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/*********************************************************************
 *           DuplicateHandle   (KERNEL32.@)
 */
BOOL WINAPI DuplicateHandle( HANDLE source_process, HANDLE source,
                             HANDLE dest_process, HANDLE *dest,
                             DWORD access, BOOL inherit, DWORD options )
{
    NTSTATUS status;

    if (is_console_handle(source))
    {
        /* FIXME: this test is not sufficient, we need to test process ids, not handles */
        if (source_process != dest_process ||
            source_process != GetCurrentProcess())
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        *dest = DuplicateConsoleHandle( source, access, inherit, options );
        return (*dest != INVALID_HANDLE_VALUE);
    }
    status = NtDuplicateObject( source_process, source, dest_process, dest,
                                access, inherit ? OBJ_INHERIT : 0, options );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/***********************************************************************
 *           ConvertToGlobalHandle   (KERNEL.476)
 *           ConvertToGlobalHandle  (KERNEL32.@)
 */
HANDLE WINAPI ConvertToGlobalHandle(HANDLE hSrc)
{
    HANDLE ret = INVALID_HANDLE_VALUE;
    DuplicateHandle( GetCurrentProcess(), hSrc, GetCurrentProcess(), &ret, 0, FALSE,
                     DUP_HANDLE_MAKE_GLOBAL | DUP_HANDLE_SAME_ACCESS | DUP_HANDLE_CLOSE_SOURCE );
    return ret;
}


/***********************************************************************
 *           SetHandleContext   (KERNEL32.@)
 */
BOOL WINAPI SetHandleContext(HANDLE hnd,DWORD context)
{
    FIXME("(%p,%d), stub. In case this got called by WSOCK32/WS2_32: "
          "the external WINSOCK DLLs won't work with WINE, don't use them.\n",hnd,context);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


/***********************************************************************
 *           GetHandleContext   (KERNEL32.@)
 */
DWORD WINAPI GetHandleContext(HANDLE hnd)
{
    FIXME("(%p), stub. In case this got called by WSOCK32/WS2_32: "
          "the external WINSOCK DLLs won't work with WINE, don't use them.\n",hnd);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}


/***********************************************************************
 *           CreateSocketHandle   (KERNEL32.@)
 */
HANDLE WINAPI CreateSocketHandle(void)
{
    FIXME("(), stub. In case this got called by WSOCK32/WS2_32: "
          "the external WINSOCK DLLs won't work with WINE, don't use them.\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return INVALID_HANDLE_VALUE;
}


/***********************************************************************
 *           SetPriorityClass   (KERNEL32.@)
 */
BOOL WINAPI SetPriorityClass( HANDLE hprocess, DWORD priorityclass )
{
    NTSTATUS                    status;
    PROCESS_PRIORITY_CLASS      ppc;

    ppc.Foreground = FALSE;
    switch (priorityclass)
    {
    case IDLE_PRIORITY_CLASS:
        ppc.PriorityClass = PROCESS_PRIOCLASS_IDLE; break;
    case BELOW_NORMAL_PRIORITY_CLASS:
        ppc.PriorityClass = PROCESS_PRIOCLASS_BELOW_NORMAL; break;
    case NORMAL_PRIORITY_CLASS:
        ppc.PriorityClass = PROCESS_PRIOCLASS_NORMAL; break;
    case ABOVE_NORMAL_PRIORITY_CLASS:
        ppc.PriorityClass = PROCESS_PRIOCLASS_ABOVE_NORMAL; break;
    case HIGH_PRIORITY_CLASS:
        ppc.PriorityClass = PROCESS_PRIOCLASS_HIGH; break;
    case REALTIME_PRIORITY_CLASS:
        ppc.PriorityClass = PROCESS_PRIOCLASS_REALTIME; break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    status = NtSetInformationProcess(hprocess, ProcessPriorityClass,
                                     &ppc, sizeof(ppc));

    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return FALSE;
    }
    return TRUE;
}


/***********************************************************************
 *           GetPriorityClass   (KERNEL32.@)
 */
DWORD WINAPI GetPriorityClass(HANDLE hProcess)
{
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION pbi;

    status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi,
                                       sizeof(pbi), NULL);
    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return 0;
    }
    switch (pbi.BasePriority)
    {
    case PROCESS_PRIOCLASS_IDLE: return IDLE_PRIORITY_CLASS;
    case PROCESS_PRIOCLASS_BELOW_NORMAL: return BELOW_NORMAL_PRIORITY_CLASS;
    case PROCESS_PRIOCLASS_NORMAL: return NORMAL_PRIORITY_CLASS;
    case PROCESS_PRIOCLASS_ABOVE_NORMAL: return ABOVE_NORMAL_PRIORITY_CLASS;
    case PROCESS_PRIOCLASS_HIGH: return HIGH_PRIORITY_CLASS;
    case PROCESS_PRIOCLASS_REALTIME: return REALTIME_PRIORITY_CLASS;
    }
    SetLastError( ERROR_INVALID_PARAMETER );
    return 0;
}


/***********************************************************************
 *          SetProcessAffinityMask   (KERNEL32.@)
 */
BOOL WINAPI SetProcessAffinityMask( HANDLE hProcess, DWORD_PTR affmask )
{
    NTSTATUS status;

    status = NtSetInformationProcess(hProcess, ProcessAffinityMask,
                                     &affmask, sizeof(DWORD_PTR));
    if (status)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return FALSE;
    }
    return TRUE;
}


/**********************************************************************
 *          GetProcessAffinityMask    (KERNEL32.@)
 */
BOOL WINAPI GetProcessAffinityMask( HANDLE hProcess,
                                    PDWORD_PTR lpProcessAffinityMask,
                                    PDWORD_PTR lpSystemAffinityMask )
{
    PROCESS_BASIC_INFORMATION   pbi;
    NTSTATUS                    status;

    status = NtQueryInformationProcess(hProcess,
                                       ProcessBasicInformation,
                                       &pbi, sizeof(pbi), NULL);
    if (status)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return FALSE;
    }
    if (lpProcessAffinityMask) *lpProcessAffinityMask = pbi.AffinityMask;
    if (lpSystemAffinityMask)  *lpSystemAffinityMask = (1 << NtCurrentTeb()->Peb->NumberOfProcessors) - 1;
    return TRUE;
}


/***********************************************************************
 *           GetProcessVersion    (KERNEL32.@)
 */
DWORD WINAPI GetProcessVersion( DWORD pid )
{
    HANDLE process;
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION pbi;
    SIZE_T count;
    PEB peb;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS nt;
    DWORD ver = 0;

    if (!pid || pid == GetCurrentProcessId())
    {
        IMAGE_NT_HEADERS *nt;

        if ((nt = RtlImageNtHeader( NtCurrentTeb()->Peb->ImageBaseAddress )))
            return ((nt->OptionalHeader.MajorSubsystemVersion << 16) |
                    nt->OptionalHeader.MinorSubsystemVersion);
        return 0;
    }

    process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process) return 0;

    status = NtQueryInformationProcess(process, ProcessBasicInformation, &pbi, sizeof(pbi), NULL);
    if (status) goto err;

    status = NtReadVirtualMemory(process, pbi.PebBaseAddress, &peb, sizeof(peb), &count);
    if (status || count != sizeof(peb)) goto err;

    memset(&dos, 0, sizeof(dos));
    status = NtReadVirtualMemory(process, peb.ImageBaseAddress, &dos, sizeof(dos), &count);
    if (status || count != sizeof(dos)) goto err;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) goto err;

    memset(&nt, 0, sizeof(nt));
    status = NtReadVirtualMemory(process, (char *)peb.ImageBaseAddress + dos.e_lfanew, &nt, sizeof(nt), &count);
    if (status || count != sizeof(nt)) goto err;
    if (nt.Signature != IMAGE_NT_SIGNATURE) goto err;

    ver = MAKELONG(nt.OptionalHeader.MinorSubsystemVersion, nt.OptionalHeader.MajorSubsystemVersion);

err:
    CloseHandle(process);

    if (status != STATUS_SUCCESS)
        SetLastError(RtlNtStatusToDosError(status));

    return ver;
}


/***********************************************************************
 *		SetProcessWorkingSetSize	[KERNEL32.@]
 * Sets the min/max working set sizes for a specified process.
 *
 * PARAMS
 *    hProcess [I] Handle to the process of interest
 *    minset   [I] Specifies minimum working set size
 *    maxset   [I] Specifies maximum working set size
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI SetProcessWorkingSetSize(HANDLE hProcess, SIZE_T minset,
                                     SIZE_T maxset)
{
    WARN("(%p,%ld,%ld): stub - harmless\n",hProcess,minset,maxset);
    if(( minset == (SIZE_T)-1) && (maxset == (SIZE_T)-1)) {
        /* Trim the working set to zero */
        /* Swap the process out of physical RAM */
    }
    return TRUE;
}

/***********************************************************************
 *           GetProcessWorkingSetSize    (KERNEL32.@)
 */
BOOL WINAPI GetProcessWorkingSetSize(HANDLE hProcess, PSIZE_T minset,
                                     PSIZE_T maxset)
{
    FIXME("(%p,%p,%p): stub\n",hProcess,minset,maxset);
    /* 32 MB working set size */
    if (minset) *minset = 32*1024*1024;
    if (maxset) *maxset = 32*1024*1024;
    return TRUE;
}


/***********************************************************************
 *           SetProcessShutdownParameters    (KERNEL32.@)
 */
BOOL WINAPI SetProcessShutdownParameters(DWORD level, DWORD flags)
{
    FIXME("(%08x, %08x): partial stub.\n", level, flags);
    shutdown_flags = flags;
    shutdown_priority = level;
    return TRUE;
}


/***********************************************************************
 * GetProcessShutdownParameters                 (KERNEL32.@)
 *
 */
BOOL WINAPI GetProcessShutdownParameters( LPDWORD lpdwLevel, LPDWORD lpdwFlags )
{
    *lpdwLevel = shutdown_priority;
    *lpdwFlags = shutdown_flags;
    return TRUE;
}


/***********************************************************************
 *           GetProcessPriorityBoost    (KERNEL32.@)
 */
BOOL WINAPI GetProcessPriorityBoost(HANDLE hprocess,PBOOL pDisablePriorityBoost)
{
    FIXME("(%p,%p): semi-stub\n", hprocess, pDisablePriorityBoost);
    
    /* Report that no boost is present.. */
    *pDisablePriorityBoost = FALSE;
    
    return TRUE;
}

/***********************************************************************
 *           SetProcessPriorityBoost    (KERNEL32.@)
 */
BOOL WINAPI SetProcessPriorityBoost(HANDLE hprocess,BOOL disableboost)
{
    FIXME("(%p,%d): stub\n",hprocess,disableboost);
    /* Say we can do it. I doubt the program will notice that we don't. */
    return TRUE;
}


/***********************************************************************
 *		ReadProcessMemory (KERNEL32.@)
 */
BOOL WINAPI ReadProcessMemory( HANDLE process, LPCVOID addr, LPVOID buffer, SIZE_T size,
                               SIZE_T *bytes_read )
{
    NTSTATUS status = NtReadVirtualMemory( process, addr, buffer, size, bytes_read );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/***********************************************************************
 *           WriteProcessMemory    		(KERNEL32.@)
 */
BOOL WINAPI WriteProcessMemory( HANDLE process, LPVOID addr, LPCVOID buffer, SIZE_T size,
                                SIZE_T *bytes_written )
{
    NTSTATUS status = NtWriteVirtualMemory( process, addr, buffer, size, bytes_written );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/****************************************************************************
 *		FlushInstructionCache (KERNEL32.@)
 */
BOOL WINAPI FlushInstructionCache(HANDLE hProcess, LPCVOID lpBaseAddress, SIZE_T dwSize)
{
    NTSTATUS status;
    status = NtFlushInstructionCache( hProcess, lpBaseAddress, dwSize );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/******************************************************************
 *		GetProcessIoCounters (KERNEL32.@)
 */
BOOL WINAPI GetProcessIoCounters(HANDLE hProcess, PIO_COUNTERS ioc)
{
    NTSTATUS    status;

    status = NtQueryInformationProcess(hProcess, ProcessIoCounters, 
                                       ioc, sizeof(*ioc), NULL);
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}

/******************************************************************
 *		GetProcessHandleCount (KERNEL32.@)
 */
BOOL WINAPI GetProcessHandleCount(HANDLE hProcess, DWORD *cnt)
{
    NTSTATUS status;

    status = NtQueryInformationProcess(hProcess, ProcessHandleCount,
                                       cnt, sizeof(*cnt), NULL);
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}

/******************************************************************
 *		QueryFullProcessImageNameA (KERNEL32.@)
 */
BOOL WINAPI QueryFullProcessImageNameA(HANDLE hProcess, DWORD dwFlags, LPSTR lpExeName, PDWORD pdwSize)
{
    BOOL retval;
    DWORD pdwSizeW = *pdwSize;
    LPWSTR lpExeNameW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, *pdwSize * sizeof(WCHAR));

    retval = QueryFullProcessImageNameW(hProcess, dwFlags, lpExeNameW, &pdwSizeW);

    if(retval)
        retval = (0 != WideCharToMultiByte(CP_ACP, 0, lpExeNameW, -1,
                               lpExeName, *pdwSize, NULL, NULL));
    if(retval)
        *pdwSize = strlen(lpExeName);

    HeapFree(GetProcessHeap(), 0, lpExeNameW);
    return retval;
}

/******************************************************************
 *		QueryFullProcessImageNameW (KERNEL32.@)
 */
BOOL WINAPI QueryFullProcessImageNameW(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD pdwSize)
{
    BYTE buffer[sizeof(UNICODE_STRING) + MAX_PATH*sizeof(WCHAR)];  /* this buffer should be enough */
    UNICODE_STRING *dynamic_buffer = NULL;
    UNICODE_STRING nt_path;
    UNICODE_STRING *result = NULL;
    NTSTATUS status;
    DWORD needed;

    RtlInitUnicodeStringEx(&nt_path, NULL);
    /* FIXME: On Windows, ProcessImageFileName return an NT path. We rely that it being a DOS path,
     * as this is on Wine. */
    status = NtQueryInformationProcess(hProcess, ProcessImageFileName, buffer,
                                       sizeof(buffer) - sizeof(WCHAR), &needed);
    if (status == STATUS_INFO_LENGTH_MISMATCH)
    {
        dynamic_buffer = HeapAlloc(GetProcessHeap(), 0, needed + sizeof(WCHAR));
        status = NtQueryInformationProcess(hProcess, ProcessImageFileName, (LPBYTE)dynamic_buffer, needed, &needed);
        result = dynamic_buffer;
    }
    else
        result = (PUNICODE_STRING)buffer;

    if (status) goto cleanup;

    if (dwFlags & PROCESS_NAME_NATIVE)
    {
        result->Buffer[result->Length / sizeof(WCHAR)] = 0;
        if (!RtlDosPathNameToNtPathName_U(result->Buffer, &nt_path, NULL, NULL))
        {
            status = STATUS_OBJECT_PATH_NOT_FOUND;
            goto cleanup;
        }
        result = &nt_path;
    }

    if (result->Length/sizeof(WCHAR) + 1 > *pdwSize)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    *pdwSize = result->Length/sizeof(WCHAR);
    memcpy( lpExeName, result->Buffer, result->Length );
    lpExeName[*pdwSize] = 0;

cleanup:
    HeapFree(GetProcessHeap(), 0, dynamic_buffer);
    RtlFreeUnicodeString(&nt_path);
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}

/***********************************************************************
 * ProcessIdToSessionId   (KERNEL32.@)
 * This function is available on Terminal Server 4SP4 and Windows 2000
 */
BOOL WINAPI ProcessIdToSessionId( DWORD procid, DWORD *sessionid_ptr )
{
    /* According to MSDN, if the calling process is not in a terminal
     * services environment, then the sessionid returned is zero.
     */
    *sessionid_ptr = 0;
    return TRUE;
}


/***********************************************************************
 *		RegisterServiceProcess (KERNEL.491)
 *		RegisterServiceProcess (KERNEL32.@)
 *
 * A service process calls this function to ensure that it continues to run
 * even after a user logged off.
 */
DWORD WINAPI RegisterServiceProcess(DWORD dwProcessId, DWORD dwType)
{
    /* I don't think that Wine needs to do anything in this function */
    return 1; /* success */
}


/**********************************************************************
 *           IsWow64Process         (KERNEL32.@)
 */
BOOL WINAPI IsWow64Process(HANDLE hProcess, PBOOL Wow64Process)
{
    ULONG pbi;
    NTSTATUS status;

    status = NtQueryInformationProcess( hProcess, ProcessWow64Information, &pbi, sizeof(pbi), NULL );

    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError( status ) );
        return FALSE;
    }
    *Wow64Process = (pbi != 0);
    return TRUE;
}


/***********************************************************************
 *           GetCurrentProcess   (KERNEL32.@)
 *
 * Get a handle to the current process.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  A handle representing the current process.
 */
#undef GetCurrentProcess
HANDLE WINAPI GetCurrentProcess(void)
{
    return (HANDLE)~(ULONG_PTR)0;
}

/***********************************************************************
 *           CmdBatNotification   (KERNEL32.@)
 *
 * Notifies the system that a batch file has started or finished.
 *
 * PARAMS
 *  bBatchRunning [I]  TRUE if a batch file has started or 
 *                     FALSE if a batch file has finished executing.
 *
 * RETURNS
 *  Unknown.
 */
BOOL WINAPI CmdBatNotification( BOOL bBatchRunning )
{
    FIXME("%d\n", bBatchRunning);
    return FALSE;
}


/***********************************************************************
 *           RegisterApplicationRestart       (KERNEL32.@)
 */
HRESULT WINAPI RegisterApplicationRestart(PCWSTR pwzCommandLine, DWORD dwFlags)
{
    FIXME("(%s,%d)\n", debugstr_w(pwzCommandLine), dwFlags);

    return S_OK;
}

/**********************************************************************
 *           WTSGetActiveConsoleSessionId     (KERNEL32.@)
 */
DWORD WINAPI WTSGetActiveConsoleSessionId(void)
{
    FIXME("stub\n");
    return 0;
}
