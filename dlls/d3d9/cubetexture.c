/*
 * IDirect3DCubeTexture9 implementation
 *
 * Copyright 2002-2005 Jason Edmeades
 * Copyright 2002-2005 Raphael Junqueira
 * Copyright 2005 Oliver Stieber
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
#include "d3d9_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d9);


/* IDirect3DCubeTexture9 IUnknown parts follow: */
static HRESULT WINAPI IDirect3DCubeTexture9Impl_QueryInterface(LPDIRECT3DCUBETEXTURE9 iface, REFIID riid, LPVOID* ppobj) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DResource9)
        || IsEqualGUID(riid, &IID_IDirect3DBaseTexture9)
        || IsEqualGUID(riid, &IID_IDirect3DCubeTexture9)) {
        IDirect3DCubeTexture9_AddRef(iface);
        *ppobj = This;
        return S_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    *ppobj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI IDirect3DCubeTexture9Impl_AddRef(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %d\n", This, ref - 1);

    return ref;
}

static ULONG WINAPI IDirect3DCubeTexture9Impl_Release(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %d\n", This, ref);

    if (ref == 0) {
        TRACE("Releasing child %p\n", This->wineD3DCubeTexture);

        wined3d_mutex_lock();
        IWineD3DCubeTexture_Destroy(This->wineD3DCubeTexture, D3D9CB_DestroySurface);
        IDirect3DDevice9Ex_Release(This->parentDevice);
        wined3d_mutex_unlock();

        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DCubeTexture9 IDirect3DResource9 Interface follow: */
static HRESULT WINAPI IDirect3DCubeTexture9Impl_GetDevice(LPDIRECT3DCUBETEXTURE9 iface, IDirect3DDevice9** ppDevice) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    IWineD3DDevice *wined3d_device;
    HRESULT hr;
    TRACE("(%p) Relay\n" , This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_GetDevice(This->wineD3DCubeTexture, &wined3d_device);
    if (SUCCEEDED(hr))
    {
        IWineD3DDevice_GetParent(wined3d_device, (IUnknown **)ppDevice);
        IWineD3DDevice_Release(wined3d_device);
    }
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture9Impl_SetPrivateData(LPDIRECT3DCUBETEXTURE9 iface, REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_SetPrivateData(This->wineD3DCubeTexture,refguid,pData,SizeOfData,Flags);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture9Impl_GetPrivateData(LPDIRECT3DCUBETEXTURE9 iface, REFGUID refguid, void* pData, DWORD* pSizeOfData) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_GetPrivateData(This->wineD3DCubeTexture,refguid,pData,pSizeOfData);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture9Impl_FreePrivateData(LPDIRECT3DCUBETEXTURE9 iface, REFGUID refguid) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_FreePrivateData(This->wineD3DCubeTexture,refguid);
    wined3d_mutex_unlock();

    return hr;
}

static DWORD WINAPI IDirect3DCubeTexture9Impl_SetPriority(LPDIRECT3DCUBETEXTURE9 iface, DWORD PriorityNew) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    DWORD ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret = IWineD3DCubeTexture_SetPriority(This->wineD3DCubeTexture, PriorityNew);
    wined3d_mutex_unlock();

    return ret;
}

static DWORD WINAPI IDirect3DCubeTexture9Impl_GetPriority(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    DWORD ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret = IWineD3DCubeTexture_GetPriority(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();

    return ret;
}

static void WINAPI IDirect3DCubeTexture9Impl_PreLoad(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    IWineD3DCubeTexture_PreLoad(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();
}

static D3DRESOURCETYPE WINAPI IDirect3DCubeTexture9Impl_GetType(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    D3DRESOURCETYPE ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret = IWineD3DCubeTexture_GetType(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();

    return ret;
}

/* IDirect3DCubeTexture9 IDirect3DBaseTexture9 Interface follow: */
static DWORD WINAPI IDirect3DCubeTexture9Impl_SetLOD(LPDIRECT3DCUBETEXTURE9 iface, DWORD LODNew) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    DWORD ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret = IWineD3DCubeTexture_SetLOD(This->wineD3DCubeTexture, LODNew);
    wined3d_mutex_unlock();

    return ret;
}

static DWORD WINAPI IDirect3DCubeTexture9Impl_GetLOD(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    DWORD ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret = IWineD3DCubeTexture_GetLOD(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();

    return ret;
}

static DWORD WINAPI IDirect3DCubeTexture9Impl_GetLevelCount(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    DWORD ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret = IWineD3DCubeTexture_GetLevelCount(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();

    return ret;
}

static HRESULT WINAPI IDirect3DCubeTexture9Impl_SetAutoGenFilterType(LPDIRECT3DCUBETEXTURE9 iface, D3DTEXTUREFILTERTYPE FilterType) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_SetAutoGenFilterType(This->wineD3DCubeTexture, (WINED3DTEXTUREFILTERTYPE) FilterType);
    wined3d_mutex_unlock();

    return hr;
}

static D3DTEXTUREFILTERTYPE WINAPI IDirect3DCubeTexture9Impl_GetAutoGenFilterType(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    D3DTEXTUREFILTERTYPE ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret = (D3DTEXTUREFILTERTYPE) IWineD3DCubeTexture_GetAutoGenFilterType(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();

    return ret;
}

static void WINAPI IDirect3DCubeTexture9Impl_GenerateMipSubLevels(LPDIRECT3DCUBETEXTURE9 iface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    IWineD3DCubeTexture_GenerateMipSubLevels(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();
}

/* IDirect3DCubeTexture9 Interface follow: */
static HRESULT WINAPI IDirect3DCubeTexture9Impl_GetLevelDesc(LPDIRECT3DCUBETEXTURE9 iface, UINT Level, D3DSURFACE_DESC* pDesc) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    WINED3DSURFACE_DESC wined3ddesc;
    HRESULT hr;

    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_GetLevelDesc(This->wineD3DCubeTexture, Level, &wined3ddesc);
    wined3d_mutex_unlock();

    if (SUCCEEDED(hr))
    {
        pDesc->Format = d3dformat_from_wined3dformat(wined3ddesc.format);
        pDesc->Type = wined3ddesc.resource_type;
        pDesc->Usage = wined3ddesc.usage;
        pDesc->Pool = wined3ddesc.pool;
        pDesc->MultiSampleType = wined3ddesc.multisample_type;
        pDesc->MultiSampleQuality = wined3ddesc.multisample_quality;
        pDesc->Width = wined3ddesc.width;
        pDesc->Height = wined3ddesc.height;
    }

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture9Impl_GetCubeMapSurface(LPDIRECT3DCUBETEXTURE9 iface, D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface9** ppCubeMapSurface) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    HRESULT hrc = D3D_OK;
    IWineD3DSurface *mySurface = NULL;

    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hrc = IWineD3DCubeTexture_GetCubeMapSurface(This->wineD3DCubeTexture, (WINED3DCUBEMAP_FACES) FaceType, Level, &mySurface);
    if (hrc == D3D_OK && NULL != ppCubeMapSurface) {
       IWineD3DCubeTexture_GetParent(mySurface, (IUnknown **)ppCubeMapSurface);
       IWineD3DCubeTexture_Release(mySurface);
    }
    wined3d_mutex_unlock();

    return hrc;
}

static HRESULT WINAPI IDirect3DCubeTexture9Impl_LockRect(LPDIRECT3DCUBETEXTURE9 iface, D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_LockRect(This->wineD3DCubeTexture, (WINED3DCUBEMAP_FACES) FaceType, Level, (WINED3DLOCKED_RECT *) pLockedRect, pRect, Flags);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture9Impl_UnlockRect(LPDIRECT3DCUBETEXTURE9 iface, D3DCUBEMAP_FACES FaceType, UINT Level) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_UnlockRect(This->wineD3DCubeTexture, (WINED3DCUBEMAP_FACES) FaceType, Level);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT  WINAPI IDirect3DCubeTexture9Impl_AddDirtyRect(LPDIRECT3DCUBETEXTURE9 iface, D3DCUBEMAP_FACES FaceType, CONST RECT* pDirtyRect) {
    IDirect3DCubeTexture9Impl *This = (IDirect3DCubeTexture9Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_AddDirtyRect(This->wineD3DCubeTexture, (WINED3DCUBEMAP_FACES) FaceType, pDirtyRect);
    wined3d_mutex_unlock();

    return hr;
}


static const IDirect3DCubeTexture9Vtbl Direct3DCubeTexture9_Vtbl =
{
    /* IUnknown */
    IDirect3DCubeTexture9Impl_QueryInterface,
    IDirect3DCubeTexture9Impl_AddRef,
    IDirect3DCubeTexture9Impl_Release,
    /* IDirect3DResource9 */
    IDirect3DCubeTexture9Impl_GetDevice,
    IDirect3DCubeTexture9Impl_SetPrivateData,
    IDirect3DCubeTexture9Impl_GetPrivateData,
    IDirect3DCubeTexture9Impl_FreePrivateData,
    IDirect3DCubeTexture9Impl_SetPriority,
    IDirect3DCubeTexture9Impl_GetPriority,
    IDirect3DCubeTexture9Impl_PreLoad,
    IDirect3DCubeTexture9Impl_GetType,
    /* IDirect3DBaseTexture9 */
    IDirect3DCubeTexture9Impl_SetLOD,
    IDirect3DCubeTexture9Impl_GetLOD,
    IDirect3DCubeTexture9Impl_GetLevelCount,
    IDirect3DCubeTexture9Impl_SetAutoGenFilterType,
    IDirect3DCubeTexture9Impl_GetAutoGenFilterType,
    IDirect3DCubeTexture9Impl_GenerateMipSubLevels,
    IDirect3DCubeTexture9Impl_GetLevelDesc,
    IDirect3DCubeTexture9Impl_GetCubeMapSurface,
    IDirect3DCubeTexture9Impl_LockRect,
    IDirect3DCubeTexture9Impl_UnlockRect,
    IDirect3DCubeTexture9Impl_AddDirtyRect
};




/* IDirect3DDevice9 IDirect3DCubeTexture9 Methods follow: */
HRESULT  WINAPI  IDirect3DDevice9Impl_CreateCubeTexture(LPDIRECT3DDEVICE9EX iface,
                                                        UINT EdgeLength, UINT Levels, DWORD Usage,
                                                        D3DFORMAT Format, D3DPOOL Pool,
                                                        IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) {

    IDirect3DCubeTexture9Impl *object;
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hr = D3D_OK;

    TRACE("(%p) : ELen(%d) Lvl(%d) Usage(%d) fmt(%u), Pool(%d)  Shared(%p)\n", This, EdgeLength, Levels, Usage, Format, Pool, pSharedHandle);

    /* Allocate the storage for the device */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));

    if (NULL == object) {
        ERR("(%p) allocation of CubeTexture failed\n", This);
        return D3DERR_OUTOFVIDEOMEMORY;
    }
    object->lpVtbl = &Direct3DCubeTexture9_Vtbl;
    object->ref = 1;

    wined3d_mutex_lock();
    hr = IWineD3DDevice_CreateCubeTexture(This->WineD3DDevice, EdgeLength, Levels, Usage,
            wined3dformat_from_d3dformat(Format), Pool, &object->wineD3DCubeTexture, (IUnknown *)object);
    wined3d_mutex_unlock();

    if (hr != D3D_OK){

        /* free up object */
        WARN("(%p) call to IWineD3DDevice_CreateCubeTexture failed\n", This);
        HeapFree(GetProcessHeap(), 0, object);
    } else {
        IDirect3DDevice9Ex_AddRef(iface);
        object->parentDevice = iface;
        *ppCubeTexture = (LPDIRECT3DCUBETEXTURE9) object;
        TRACE("(%p) : Created cube texture %p\n", This, object);
    }

    TRACE("(%p) returning %p\n",This, *ppCubeTexture);
    return hr;
}
