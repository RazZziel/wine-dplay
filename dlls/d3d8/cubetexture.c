/*
 * IDirect3DCubeTexture8 implementation
 *
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
#include "d3d8_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d8);

/* IDirect3DCubeTexture8 IUnknown parts follow: */
static HRESULT WINAPI IDirect3DCubeTexture8Impl_QueryInterface(LPDIRECT3DCUBETEXTURE8 iface, REFIID riid, LPVOID *ppobj) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DResource8)
        || IsEqualGUID(riid, &IID_IDirect3DBaseTexture8)
        || IsEqualGUID(riid, &IID_IDirect3DCubeTexture8)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return S_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    *ppobj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI IDirect3DCubeTexture8Impl_AddRef(LPDIRECT3DCUBETEXTURE8 iface) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %d\n", This, ref - 1);

    return ref;
}

static ULONG WINAPI IDirect3DCubeTexture8Impl_Release(LPDIRECT3DCUBETEXTURE8 iface) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %d\n", This, ref);

    if (ref == 0) {
        TRACE("Releasing child %p\n", This->wineD3DCubeTexture);

        wined3d_mutex_lock();
        IWineD3DCubeTexture_Destroy(This->wineD3DCubeTexture, D3D8CB_DestroySurface);
        wined3d_mutex_unlock();

        IUnknown_Release(This->parentDevice);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DCubeTexture8 IDirect3DResource8 Interface follow: */
static HRESULT WINAPI IDirect3DCubeTexture8Impl_GetDevice(LPDIRECT3DCUBETEXTURE8 iface, IDirect3DDevice8 **ppDevice) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
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

static HRESULT WINAPI IDirect3DCubeTexture8Impl_SetPrivateData(LPDIRECT3DCUBETEXTURE8 iface, REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_SetPrivateData(This->wineD3DCubeTexture,refguid,pData,SizeOfData,Flags);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture8Impl_GetPrivateData(LPDIRECT3DCUBETEXTURE8 iface, REFGUID refguid, void *pData, DWORD *pSizeOfData) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_GetPrivateData(This->wineD3DCubeTexture,refguid,pData,pSizeOfData);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture8Impl_FreePrivateData(LPDIRECT3DCUBETEXTURE8 iface, REFGUID refguid) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_FreePrivateData(This->wineD3DCubeTexture,refguid);
    wined3d_mutex_unlock();

    return hr;
}

static DWORD WINAPI IDirect3DCubeTexture8Impl_SetPriority(LPDIRECT3DCUBETEXTURE8 iface, DWORD PriorityNew) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    DWORD ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret = IWineD3DCubeTexture_SetPriority(This->wineD3DCubeTexture, PriorityNew);
    wined3d_mutex_unlock();

    return ret;
}

static DWORD WINAPI IDirect3DCubeTexture8Impl_GetPriority(LPDIRECT3DCUBETEXTURE8 iface) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    DWORD ret;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    ret =  IWineD3DCubeTexture_GetPriority(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();

    return ret;
}

static void WINAPI IDirect3DCubeTexture8Impl_PreLoad(LPDIRECT3DCUBETEXTURE8 iface) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    IWineD3DCubeTexture_PreLoad(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();
}

static D3DRESOURCETYPE WINAPI IDirect3DCubeTexture8Impl_GetType(LPDIRECT3DCUBETEXTURE8 iface) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    D3DRESOURCETYPE type;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    type = IWineD3DCubeTexture_GetType(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();

    return type;
}

/* IDirect3DCubeTexture8 IDirect3DBaseTexture8 Interface follow: */
static DWORD WINAPI IDirect3DCubeTexture8Impl_SetLOD(LPDIRECT3DCUBETEXTURE8 iface, DWORD LODNew) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    DWORD lod;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    lod = IWineD3DCubeTexture_SetLOD(This->wineD3DCubeTexture, LODNew);
    wined3d_mutex_unlock();

    return lod;
}

static DWORD WINAPI IDirect3DCubeTexture8Impl_GetLOD(LPDIRECT3DCUBETEXTURE8 iface) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    DWORD lod;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    lod = IWineD3DCubeTexture_GetLOD((LPDIRECT3DBASETEXTURE8) This);
    wined3d_mutex_unlock();

    return lod;
}

static DWORD WINAPI IDirect3DCubeTexture8Impl_GetLevelCount(LPDIRECT3DCUBETEXTURE8 iface) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    DWORD cnt;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    cnt = IWineD3DCubeTexture_GetLevelCount(This->wineD3DCubeTexture);
    wined3d_mutex_unlock();

    return cnt;
}

/* IDirect3DCubeTexture8 Interface follow: */
static HRESULT WINAPI IDirect3DCubeTexture8Impl_GetLevelDesc(LPDIRECT3DCUBETEXTURE8 iface, UINT Level, D3DSURFACE_DESC *pDesc) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    HRESULT hr;
    WINED3DSURFACE_DESC    wined3ddesc;

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
        pDesc->Size = wined3ddesc.size;
        pDesc->MultiSampleType = wined3ddesc.multisample_type;
        pDesc->Width = wined3ddesc.width;
        pDesc->Height = wined3ddesc.height;
    }

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture8Impl_GetCubeMapSurface(LPDIRECT3DCUBETEXTURE8 iface, D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface8 **ppCubeMapSurface) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
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

static HRESULT WINAPI IDirect3DCubeTexture8Impl_LockRect(LPDIRECT3DCUBETEXTURE8 iface, D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT *pRect, DWORD Flags) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_LockRect(This->wineD3DCubeTexture, (WINED3DCUBEMAP_FACES) FaceType, Level, (WINED3DLOCKED_RECT *) pLockedRect, pRect, Flags);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture8Impl_UnlockRect(LPDIRECT3DCUBETEXTURE8 iface, D3DCUBEMAP_FACES FaceType, UINT Level) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_UnlockRect(This->wineD3DCubeTexture, (WINED3DCUBEMAP_FACES) FaceType, Level);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DCubeTexture8Impl_AddDirtyRect(LPDIRECT3DCUBETEXTURE8 iface, D3DCUBEMAP_FACES FaceType, CONST RECT *pDirtyRect) {
    IDirect3DCubeTexture8Impl *This = (IDirect3DCubeTexture8Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DCubeTexture_AddDirtyRect(This->wineD3DCubeTexture, (WINED3DCUBEMAP_FACES) FaceType, pDirtyRect);
    wined3d_mutex_unlock();

    return hr;
}


const IDirect3DCubeTexture8Vtbl Direct3DCubeTexture8_Vtbl =
{
    /* IUnknown */
    IDirect3DCubeTexture8Impl_QueryInterface,
    IDirect3DCubeTexture8Impl_AddRef,
    IDirect3DCubeTexture8Impl_Release,
    /* IDirect3DResource8 */
    IDirect3DCubeTexture8Impl_GetDevice,
    IDirect3DCubeTexture8Impl_SetPrivateData,
    IDirect3DCubeTexture8Impl_GetPrivateData,
    IDirect3DCubeTexture8Impl_FreePrivateData,
    IDirect3DCubeTexture8Impl_SetPriority,
    IDirect3DCubeTexture8Impl_GetPriority,
    IDirect3DCubeTexture8Impl_PreLoad,
    IDirect3DCubeTexture8Impl_GetType,
    /* IDirect3DBaseTexture8 */
    IDirect3DCubeTexture8Impl_SetLOD,
    IDirect3DCubeTexture8Impl_GetLOD,
    IDirect3DCubeTexture8Impl_GetLevelCount,
    /* IDirect3DCubeTexture8 */
    IDirect3DCubeTexture8Impl_GetLevelDesc,
    IDirect3DCubeTexture8Impl_GetCubeMapSurface,
    IDirect3DCubeTexture8Impl_LockRect,
    IDirect3DCubeTexture8Impl_UnlockRect,
    IDirect3DCubeTexture8Impl_AddDirtyRect
};
