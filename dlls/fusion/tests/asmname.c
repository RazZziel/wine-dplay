/*
 * Copyright 2008 James Hawkins
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

#define COBJMACROS

#include <stdio.h>

#include <windows.h>
#include <mscoree.h>
#include <fusion.h>
#include <corerror.h>

#include "wine/test.h"

/* ok-like statement which takes two unicode strings or one unicode and one ANSI string as arguments */
static CHAR string1[MAX_PATH];

#define ok_aw(aString, wString) \
    WideCharToMultiByte(CP_ACP, 0, wString, -1, string1, MAX_PATH, NULL, NULL); \
    if (lstrcmpA(string1, aString) != 0) \
        ok(0, "Expected \"%s\", got \"%s\"\n", aString, string1);

static HRESULT (WINAPI *pCreateAssemblyNameObject)(LPASSEMBLYNAME *ppAssemblyNameObj,
                                                   LPCWSTR szAssemblyName, DWORD dwFlags,
                                                   LPVOID pvReserved);
static HRESULT (WINAPI *pLoadLibraryShim)(LPCWSTR szDllName, LPCWSTR szVersion,
                                          LPVOID pvReserved, HMODULE *phModDll);

static BOOL init_functionpointers(void)
{
    HRESULT hr;
    HMODULE hfusion;
    HMODULE hmscoree;

    static const WCHAR szFusion[] = {'f','u','s','i','o','n','.','d','l','l',0};

    hmscoree = LoadLibraryA("mscoree.dll");
    if (!hmscoree)
        return FALSE;

    pLoadLibraryShim = (void *)GetProcAddress(hmscoree, "LoadLibraryShim");
    if (!pLoadLibraryShim)
    {
        FreeLibrary(hmscoree);
        return FALSE;
    }

    hr = pLoadLibraryShim(szFusion, NULL, NULL, &hfusion);
    if (FAILED(hr))
        return FALSE;

    pCreateAssemblyNameObject = (void *)GetProcAddress(hfusion, "CreateAssemblyNameObject");
    if (!pCreateAssemblyNameObject)
        return FALSE;

    return TRUE;
}

typedef struct _tagASMPROP_RES
{
    HRESULT hr;
    CHAR val[MAX_PATH];
    DWORD size;
} ASMPROP_RES;

static const ASMPROP_RES defaults[ASM_NAME_MAX_PARAMS] =
{
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_FALSE, "", MAX_PATH},
    {S_FALSE, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0}
};

static const ASMPROP_RES emptyname[ASM_NAME_MAX_PARAMS] =
{
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 2},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_FALSE, "", MAX_PATH},
    {S_FALSE, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0}
};

static const ASMPROP_RES winename[ASM_NAME_MAX_PARAMS] =
{
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "wine", 10},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_FALSE, "", MAX_PATH},
    {S_FALSE, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0}
};

static const ASMPROP_RES vername[ASM_NAME_MAX_PARAMS] =
{
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "wine", 10},
    {S_OK, "\x01", 2},
    {S_OK, "\x02", 2},
    {S_OK, "\x03", 2},
    {S_OK, "\x04", 2},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_FALSE, "", MAX_PATH},
    {S_FALSE, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0}
};

static const ASMPROP_RES badvername[ASM_NAME_MAX_PARAMS] =
{
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "wine", 10},
    {S_OK, "\x01", 2},
    {S_OK, "\x05", 2},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_FALSE, "", MAX_PATH},
    {S_FALSE, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0}
};

static const ASMPROP_RES neutralname[ASM_NAME_MAX_PARAMS] =
{
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "wine", 10},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 2},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_FALSE, "", MAX_PATH},
    {S_FALSE, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0}
};

static const ASMPROP_RES enname[ASM_NAME_MAX_PARAMS] =
{
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "wine", 10},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "en", 6},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_FALSE, "", MAX_PATH},
    {S_FALSE, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0}
};

static const ASMPROP_RES pubkeyname[ASM_NAME_MAX_PARAMS] =
{
    {S_OK, "", 0},
    {S_OK, "\x12\x34\x56\x78\x90\xab\xcd\xef", 8},
    {S_OK, "", 0},
    {S_OK, "wine", 10},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", MAX_PATH},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0},
    {S_OK, "", 0}
};

static inline void to_widechar(LPWSTR dest, LPCSTR src)
{
    MultiByteToWideChar(CP_ACP, 0, src, -1, dest, MAX_PATH);
}

static inline void to_multibyte(LPSTR dest, LPWSTR src)
{
    WideCharToMultiByte(CP_ACP, 0, src, -1, dest, MAX_PATH, NULL, NULL);
}

static void test_assembly_name_props_line(IAssemblyName *name,
                                          const ASMPROP_RES *vals, int line)
{
    HRESULT hr;
    DWORD i, size;
    WCHAR expect[MAX_PATH];
    WCHAR str[MAX_PATH];
    CHAR val[MAX_PATH];

    for (i = 0; i < ASM_NAME_MAX_PARAMS; i++)
    {
        to_widechar(expect, vals[i].val);

        size = MAX_PATH;
        ZeroMemory(str, MAX_PATH);
        hr = IAssemblyName_GetProperty(name, i, str, &size);
        to_multibyte(val, str);

        ok(hr == vals[i].hr ||
           broken(i >= ASM_NAME_CONFIG_MASK && hr == E_INVALIDARG) || /* .NET 1.1 */
           broken(i >= ASM_NAME_FILE_MAJOR_VERSION && hr == E_INVALIDARG), /* .NET 1.0 */
           "%d: prop %d: Expected %08x, got %08x\n", line, i, vals[i].hr, hr);
        if (hr != E_INVALIDARG)
        {
            if (i == ASM_NAME_PUBLIC_KEY_TOKEN)
                ok(!memcmp(vals[i].val, str, size), "Expected a correct ASM_NAME_PUBLIC_KEY_TOKEN\n");
            else
                ok(!lstrcmpA(vals[i].val, val), "%d: prop %d: Expected \"%s\", got \"%s\"\n", line, i, vals[i].val, val);
            ok(size == vals[i].size, "%d: prop %d: Expected %d, got %d\n", line, i, vals[i].size, size);
        }
    }
}

#define test_assembly_name_props(name, vals) \
    test_assembly_name_props_line(name, vals, __LINE__);

static void test_CreateAssemblyNameObject(void)
{
    IAssemblyName *name;
    WCHAR str[MAX_PATH];
    WCHAR namestr[MAX_PATH];
    DWORD size, hi, lo;
    HRESULT hr;

    static const WCHAR empty[] = {0};

    /* NULL ppAssemblyNameObj */
    to_widechar(namestr, "wine.dll");
    hr = pCreateAssemblyNameObject(NULL, namestr, 0, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    /* NULL szAssemblyName, CANOF_PARSE_DISPLAY_NAME */
    name = (IAssemblyName *)0xdeadbeef;
    hr = pCreateAssemblyNameObject(&name, NULL, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);
    ok(name == (IAssemblyName *)0xdeadbeef, "Expected 0xdeadbeef, got %p\n", name);

    /* empty szAssemblyName, CANOF_PARSE_DISPLAY_NAME */
    name = (IAssemblyName *)0xdeadbeef;
    hr = pCreateAssemblyNameObject(&name, empty, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);
    ok(name == (IAssemblyName *)0xdeadbeef, "Expected 0xdeadbeef, got %p\n", name);

    /* check the contents of the AssemblyName for default values */

    /* NULL szAssemblyName */
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, NULL, CANOF_SET_DEFAULT_VALUES, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == E_INVALIDARG), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(lstrlenW(str) == 0, "Expected empty name\n");
    ok(size == 0, "Expected 0, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0, "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    if (hr == S_OK)
        win_skip(".NET 1.x doesn't handle ASM_NAME_PROCESSOR_ID_ARRAY"
                 " and ASM_NAME_OSINFO_ARRAY correctly\n");
    else
        test_assembly_name_props(name, defaults);

    IAssemblyName_Release(name);

    /* empty szAssemblyName */
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, empty, CANOF_SET_DEFAULT_VALUES, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(lstrlenW(str) == 0, "Expected empty name\n");
    ok(size == 1, "Expected 1, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0, "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    if (hr == S_OK)
        win_skip(".NET 1.x doesn't handle ASM_NAME_PROCESSOR_ID_ARRAY"
                 " and ASM_NAME_OSINFO_ARRAY correctly\n");
    else
        test_assembly_name_props(name, emptyname);

    IAssemblyName_Release(name);

    /* 'wine' */
    to_widechar(namestr, "wine");
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_SET_DEFAULT_VALUES, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0, "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    if (hr == S_OK)
        win_skip(".NET 1.x doesn't handle ASM_NAME_PROCESSOR_ID_ARRAY"
                 " and ASM_NAME_OSINFO_ARRAY correctly\n");
    else
        test_assembly_name_props(name, winename);

    IAssemblyName_Release(name);

    /* check the contents of the AssemblyName with parsing */

    /* 'wine' */
    to_widechar(namestr, "wine");
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0, "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    test_assembly_name_props(name, winename);

    IAssemblyName_Release(name);

    /* 'wine, Version=1.2.3.4' */
    to_widechar(namestr, "wine, Version=1.2.3.4");
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine, Version=1.2.3.4", str);
    ok(size == 22, "Expected 22, got %d\n", size);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(hi == 0x00010002, "Expected 0x00010002, got %08x\n", hi);
    ok(lo == 0x00030004, "Expected 0x00030004, got %08x\n", lo);

    test_assembly_name_props(name, vername);

    IAssemblyName_Release(name);

    /* Version isn't of the form 1.x.x.x */
    to_widechar(namestr, "wine, Version=1.5");
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine, Version=1.5", str);
    ok(size == 18, "Expected 18, got %d\n", size);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0 ||
       broken(hi == 0x10005), /* .NET 1.x */
       "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    test_assembly_name_props(name, badvername);

    IAssemblyName_Release(name);

    /* 'wine, Culture=neutral' */
    to_widechar(namestr, "wine, Culture=neutral");
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine, Culture=neutral", str);
    ok(size == 22, "Expected 22, got %d\n", size);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0, "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    test_assembly_name_props(name, neutralname);

    IAssemblyName_Release(name);

    /* 'wine, Culture=en' */
    to_widechar(namestr, "wine, Culture=en");
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine, Culture=en", str);
    ok(size == 17, "Expected 17, got %d\n", size);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0, "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    test_assembly_name_props(name, enname);

    IAssemblyName_Release(name);

    /* 'wine, PublicKeyToken=1234567890abcdef' */
    to_widechar(namestr, "wine, PublicKeyToken=1234567890abcdef");
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine, PublicKeyToken=1234567890abcdef", str);
    ok(size == 38, "Expected 38, got %d\n", size);

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0, "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    test_assembly_name_props(name, pubkeyname);

    IAssemblyName_Release(name);

    /* invalid property */
    to_widechar(namestr, "wine, BadProp=42");
    name = NULL;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(name != NULL, "Expected non-NULL name\n");

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetDisplayName(name, str, &size, ASM_DISPLAYF_FULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    todo_wine
    {
        ok_aw("wine", str);
        ok(size == 5, "Expected 5, got %d\n", size);
    }

    size = MAX_PATH;
    str[0] = '\0';
    hr = IAssemblyName_GetName(name, &size, str);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_aw("wine", str);
    ok(size == 5, "Expected 5, got %d\n", size);

    hi = 0xbeefcace;
    lo = 0xcafebabe;
    hr = IAssemblyName_GetVersion(name, &hi, &lo);
    ok(hr == FUSION_E_INVALID_NAME ||
       broken(hr == S_OK), /* .NET 1.x */
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(hi == 0, "Expected 0, got %08x\n", hi);
    ok(lo == 0, "Expected 0, got %08x\n", lo);

    test_assembly_name_props(name, winename);

    IAssemblyName_Release(name);

    /* PublicKeyToken is not 16 chars long */
    to_widechar(namestr, "wine, PublicKeyToken=567890abcdef");
    name = (IAssemblyName *)0xdeadbeef;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    if (hr == S_OK && name != (IAssemblyName *)0xdeadbeef)
    {
        win_skip(".NET 1.x doesn't check PublicKeyToken correctly\n");
        IAssemblyName_Release(name);
        return;
    }
    ok(hr == FUSION_E_INVALID_NAME,
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(name == (IAssemblyName *)0xdeadbeef, "Expected 0xdeadbeef, got %p\n", name);

    /* PublicKeyToken contains invalid chars */
    to_widechar(namestr, "wine, PublicKeyToken=1234567890ghijkl");
    name = (IAssemblyName *)0xdeadbeef;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == FUSION_E_INVALID_NAME,
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(name == (IAssemblyName *)0xdeadbeef, "Expected 0xdeadbeef, got %p\n", name);

    /* no comma separator */
    to_widechar(namestr, "wine PublicKeyToken=1234567890abcdef");
    name = (IAssemblyName *)0xdeadbeef;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    todo_wine
    {
        ok(hr == FUSION_E_INVALID_NAME,
           "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
        ok(name == (IAssemblyName *)0xdeadbeef, "Expected 0xdeadbeef, got %p\n", name);
    }

    /* no '=' */
    to_widechar(namestr, "wine, PublicKeyToken");
    name = (IAssemblyName *)0xdeadbeef;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == FUSION_E_INVALID_NAME,
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(name == (IAssemblyName *)0xdeadbeef, "Expected 0xdeadbeef, got %p\n", name);

    /* no value */
    to_widechar(namestr, "wine, PublicKeyToken=");
    name = (IAssemblyName *)0xdeadbeef;
    hr = pCreateAssemblyNameObject(&name, namestr, CANOF_PARSE_DISPLAY_NAME, NULL);
    ok(hr == FUSION_E_INVALID_NAME,
       "Expected FUSION_E_INVALID_NAME, got %08x\n", hr);
    ok(name == (IAssemblyName *)0xdeadbeef, "Expected 0xdeadbeef, got %p\n", name);
}

START_TEST(asmname)
{
    if (!init_functionpointers())
    {
        win_skip("fusion.dll not available\n");
        return;
    }

    test_CreateAssemblyNameObject();
}
