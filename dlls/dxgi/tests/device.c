/*
 * Copyright 2008 Henri Verbeet for CodeWeavers
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
#include "initguid.h"
#include "d3d10.h"
#include "wine/test.h"

HRESULT WINAPI DXGID3D10CreateDevice(HMODULE d3d10core, IDXGIFactory *factory,
        IDXGIAdapter *adapter, UINT flags, DWORD arg5, void **device);

static IDXGIDevice *create_device(HMODULE d3d10core)
{
    IDXGIDevice *dxgi_device = NULL;
    IDXGIFactory *factory = NULL;
    IDXGIAdapter *adapter = NULL;
    IUnknown *device = NULL;
    HRESULT hr;

    hr = CreateDXGIFactory(&IID_IDXGIFactory, (void *)&factory);
    if (FAILED(hr)) goto cleanup;

    hr = IDXGIFactory_EnumAdapters(factory, 0, &adapter);
    ok(SUCCEEDED(hr), "EnumAdapters failed, hr %#x\n", hr);
    if (FAILED(hr)) goto cleanup;

    hr = DXGID3D10CreateDevice(d3d10core, factory, adapter, 0, 0, (void **)&device);
    if (FAILED(hr))
    {
        HMODULE d3d10ref;

        trace("Failed to create a HW device, trying REF\n");
        IDXGIAdapter_Release(adapter);
        adapter = NULL;

        d3d10ref = LoadLibraryA("d3d10ref.dll");
        if (!d3d10ref)
        {
            trace("d3d10ref.dll not available, unable to create a REF device\n");
            goto cleanup;
        }

        hr = IDXGIFactory_CreateSoftwareAdapter(factory, d3d10ref, &adapter);
        FreeLibrary(d3d10ref);
        ok(SUCCEEDED(hr), "CreateSoftwareAdapter failed, hr %#x\n", hr);
        if (FAILED(hr)) goto cleanup;

        hr = DXGID3D10CreateDevice(d3d10core, factory, adapter, 0, 0, (void **)&device);
        ok(SUCCEEDED(hr), "Failed to create a REF device, hr %#x\n", hr);
        if (FAILED(hr)) goto cleanup;
    }

    hr = IUnknown_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);
    ok(SUCCEEDED(hr), "Created device does not implement IDXGIDevice\n");
    IUnknown_Release(device);

cleanup:
    if (adapter) IDXGIAdapter_Release(adapter);
    if (factory) IDXGIFactory_Release(factory);

    return dxgi_device;
}

static void test_device_interfaces(IDXGIDevice *device)
{
    IUnknown *obj;
    HRESULT hr;

    if (SUCCEEDED(hr = IDXGIDevice_QueryInterface(device, &IID_IUnknown, (void **)&obj)))
        IUnknown_Release(obj);
    ok(SUCCEEDED(hr), "IDXGIDevice does not implement IUnknown\n");

    if (SUCCEEDED(hr = IDXGIDevice_QueryInterface(device, &IID_IDXGIObject, (void **)&obj)))
        IUnknown_Release(obj);
    ok(SUCCEEDED(hr), "IDXGIDevice does not implement IDXGIObject\n");

    if (SUCCEEDED(hr = IDXGIDevice_QueryInterface(device, &IID_IDXGIDevice, (void **)&obj)))
        IUnknown_Release(obj);
    ok(SUCCEEDED(hr), "IDXGIDevice does not implement IDXGIDevice\n");

    if (SUCCEEDED(hr = IDXGIDevice_QueryInterface(device, &IID_ID3D10Device, (void **)&obj)))
        IUnknown_Release(obj);
    ok(SUCCEEDED(hr), "IDXGIDevice does not implement ID3D10Device\n");
}

static void test_create_surface(IDXGIDevice *device)
{
    ID3D10Texture2D *texture;
    IDXGISurface *surface;
    DXGI_SURFACE_DESC desc;
    HRESULT hr;

    desc.Width = 512;
    desc.Height = 512;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    hr = IDXGIDevice_CreateSurface(device, &desc, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &surface);
    ok(SUCCEEDED(hr), "Failed to create a dxgi surface, hr %#x\n", hr);

    hr = IDXGISurface_QueryInterface(surface, &IID_ID3D10Texture2D, (void **)&texture);
    ok(SUCCEEDED(hr), "Surface should implement ID3D10Texture2D\n");
    if (SUCCEEDED(hr)) ID3D10Texture2D_Release(texture);

    IDXGISurface_Release(surface);
}

START_TEST(device)
{
    HMODULE d3d10core = LoadLibraryA("d3d10core.dll");
    IDXGIDevice *device;
    ULONG refcount;

    if (!d3d10core)
    {
        win_skip("d3d10core.dll not available, skipping tests\n");
        return;
    }

    device = create_device(d3d10core);
    if (!device)
    {
        skip("Failed to create device, skipping tests\n");
        FreeLibrary(d3d10core);
        return;
    }

    test_device_interfaces(device);
    test_create_surface(device);

    refcount = IDXGIDevice_Release(device);
    ok(!refcount, "Device has %u references left\n", refcount);
    FreeLibrary(d3d10core);
}
