/*
 * Direct3D wine internal public interface file
 *
 * Copyright 2002-2003 The wine-d3d team
 * Copyright 2002-2003 Raphael Junqueira
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __WINE_WINED3D_INTERFACE_H
#define __WINE_WINED3D_INTERFACE_H

#if !defined( __WINE_CONFIG_H )
# error You must include config.h to use this header
#endif

#if !defined( __WINE_D3D8_H ) && !defined( __WINE_D3D9_H )
# error You must include d3d8.h or d3d9.h header to use this header
#endif

/*****************************************************************
 * THIS FILE MUST NOT CONTAIN X11 or MESA DEFINES 
 * PLEASE USE wine/wined3d_gl.h INSTEAD
 */


/*****************************************************************************
 * WineD3D Structures to be used when d3d8 and d3d9 are incompatible
 */
typedef struct _WINED3DADAPTER_IDENTIFIER {
    char           *Driver;
    char           *Description;
    char           *DeviceName;
    LARGE_INTEGER  *DriverVersion; 
    DWORD          *VendorId;
    DWORD          *DeviceId;
    DWORD          *SubSysId;
    DWORD          *Revision;
    GUID           *DeviceIdentifier;
    DWORD          *WHQLLevel;
} WINED3DADAPTER_IDENTIFIER;

typedef struct _WINED3DPRESENT_PARAMETERS {
    UINT                *BackBufferWidth;
    UINT                *BackBufferHeight;
    D3DFORMAT           *BackBufferFormat;
    UINT                *BackBufferCount;
    D3DMULTISAMPLE_TYPE *MultiSampleType;
    DWORD               *MultiSampleQuality;
    D3DSWAPEFFECT       *SwapEffect;
    HWND                *hDeviceWindow;
    BOOL                *Windowed;
    BOOL                *EnableAutoDepthStencil;
    D3DFORMAT           *AutoDepthStencilFormat;
    DWORD               *Flags;
    UINT                *FullScreen_RefreshRateInHz;
    UINT                *PresentationInterval;
} WINED3DPRESENT_PARAMETERS;

/* The following have differing names, but actually are the same layout */
#if defined( __WINE_D3D8_H )
 #define WINED3DLIGHT           D3DLIGHT8
 #define WINED3DCLIPSTATUS      D3DCLIPSTATUS8
 #define WINED3DMATERIAL        D3DMATERIAL8
 #define WINED3DVIEWPORT        D3DVIEWPORT8
#else
 #define WINED3DLIGHT           D3DLIGHT9
 #define WINED3DCLIPSTATUS      D3DCLIPSTATUS9
 #define WINED3DMATERIAL        D3DMATERIAL9
 #define WINED3DVIEWPORT        D3DVIEWPORT9
#endif

typedef struct IWineD3D               IWineD3D;
typedef struct IWineD3DDevice         IWineD3DDevice;
typedef struct IWineD3DResource       IWineD3DResource;
typedef struct IWineD3DVertexBuffer   IWineD3DVertexBuffer;
typedef struct IWineD3DIndexBuffer    IWineD3DIndexBuffer;
typedef struct IWineD3DBaseTexture    IWineD3DBaseTexture;
typedef struct IWineD3DStateBlock     IWineD3DStateBlock;

/*****************************************************************************
 * WineD3D interface 
 */

#define INTERFACE IWineD3D
DECLARE_INTERFACE_(IWineD3D,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3D methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD_(UINT,GetAdapterCount)(THIS) PURE;
    STDMETHOD(RegisterSoftwareDevice)(THIS_ void * pInitializeFunction) PURE;
    STDMETHOD_(HMONITOR,GetAdapterMonitor)(THIS_ UINT Adapter) PURE;
    STDMETHOD_(UINT,GetAdapterModeCount)(THIS_ UINT Adapter, D3DFORMAT Format) PURE;
    STDMETHOD(EnumAdapterModes)(THIS_ UINT  Adapter, UINT  Mode, D3DFORMAT Format, D3DDISPLAYMODE * pMode) PURE;
    STDMETHOD(GetAdapterDisplayMode)(THIS_ UINT  Adapter, D3DDISPLAYMODE * pMode) PURE;
    STDMETHOD(GetAdapterIdentifier)(THIS_ UINT Adapter, DWORD Flags, WINED3DADAPTER_IDENTIFIER* pIdentifier) PURE;
    STDMETHOD(CheckDeviceMultiSampleType)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType, D3DFORMAT  SurfaceFormat, BOOL  Windowed, D3DMULTISAMPLE_TYPE  MultiSampleType, DWORD *pQuality) PURE;
    STDMETHOD(CheckDepthStencilMatch)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType, D3DFORMAT  AdapterFormat, D3DFORMAT  RenderTargetFormat, D3DFORMAT  DepthStencilFormat) PURE;
    STDMETHOD(CheckDeviceType)(THIS_ UINT  Adapter, D3DDEVTYPE  CheckType, D3DFORMAT  DisplayFormat, D3DFORMAT  BackBufferFormat, BOOL  Windowed) PURE;
    STDMETHOD(CheckDeviceFormat)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType, D3DFORMAT  AdapterFormat, DWORD  Usage, D3DRESOURCETYPE  RType, D3DFORMAT  CheckFormat) PURE;
    STDMETHOD(CheckDeviceFormatConversion)(THIS_ UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) PURE;
    STDMETHOD(GetDeviceCaps)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType, void * pCaps) PURE;
    STDMETHOD(CreateDevice)(THIS_ UINT  Adapter, D3DDEVTYPE  DeviceType,HWND  hFocusWindow, DWORD  BehaviorFlags, WINED3DPRESENT_PARAMETERS * pPresentationParameters, IWineD3DDevice ** ppReturnedDeviceInterface, IUnknown *parent) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3D_QueryInterface(p,a,b)                    (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3D_AddRef(p)                                (p)->lpVtbl->AddRef(p)
#define IWineD3D_Release(p)                               (p)->lpVtbl->Release(p)
/*** IWineD3D methods ***/
#define IWineD3D_GetParent(p,a)                           (p)->lpVtbl->GetParent(p,a)
#define IWineD3D_GetAdapterCount(p)                       (p)->lpVtbl->GetAdapterCount(p)
#define IWineD3D_RegisterSoftwareDevice(p,a)              (p)->lpVtbl->RegisterSoftwareDevice(p,a)
#define IWineD3D_GetAdapterMonitor(p,a)                   (p)->lpVtbl->GetAdapterMonitor(p,a)
#define IWineD3D_GetAdapterModeCount(p,a,b)               (p)->lpVtbl->GetAdapterModeCount(p,a,b)
#define IWineD3D_EnumAdapterModes(p,a,b,c,d)              (p)->lpVtbl->EnumAdapterModes(p,a,b,c,d)
#define IWineD3D_GetAdapterDisplayMode(p,a,b)             (p)->lpVtbl->GetAdapterDisplayMode(p,a,b)
#define IWineD3D_GetAdapterIdentifier(p,a,b,c)            (p)->lpVtbl->GetAdapterIdentifier(p,a,b,c)
#define IWineD3D_CheckDeviceMultiSampleType(p,a,b,c,d,e,f) (p)->lpVtbl->CheckDeviceMultiSampleType(p,a,b,c,d,e,f)
#define IWineD3D_CheckDepthStencilMatch(p,a,b,c,d,e)      (p)->lpVtbl->CheckDepthStencilMatch(p,a,b,c,d,e)
#define IWineD3D_CheckDeviceType(p,a,b,c,d,e)             (p)->lpVtbl->CheckDeviceType(p,a,b,c,d,e)
#define IWineD3D_CheckDeviceFormat(p,a,b,c,d,e,f)         (p)->lpVtbl->CheckDeviceFormat(p,a,b,c,d,e,f)
#define IWineD3D_CheckDeviceFormatConversion(p,a,b,c,d)   (p)->lpVtbl->CheckDeviceFormatConversion(p,a,b,c,d)
#define IWineD3D_GetDeviceCaps(p,a,b,c)                   (p)->lpVtbl->GetDeviceCaps(p,a,b,c)
#define IWineD3D_CreateDevice(p,a,b,c,d,e,f,g)            (p)->lpVtbl->CreateDevice(p,a,b,c,d,e,f,g)
#endif

/* Define the main WineD3D entrypoint */
IWineD3D* WINAPI WineDirect3DCreate(UINT SDKVersion, UINT dxVersion, IUnknown *parent);

/*****************************************************************************
 * WineD3DDevice interface 
 */
#define INTERFACE IWineD3DDevice
DECLARE_INTERFACE_(IWineD3DDevice,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3D methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(CreateVertexBuffer)(THIS_ UINT  Length,DWORD  Usage,DWORD  FVF,D3DPOOL  Pool,IWineD3DVertexBuffer **ppVertexBuffer, HANDLE *sharedHandle, IUnknown *parent) PURE;
    STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IWineD3DIndexBuffer** ppIndexBuffer, HANDLE* pSharedHandle, IUnknown *parent) PURE;
    STDMETHOD(CreateStateBlock)(THIS_ D3DSTATEBLOCKTYPE Type, IWineD3DStateBlock **ppStateBlock, IUnknown *parent) PURE;
    STDMETHOD(SetFVF)(THIS_ DWORD  fvf) PURE;
    STDMETHOD(GetFVF)(THIS_ DWORD * pfvf) PURE;
    STDMETHOD(SetStreamSource)(THIS_ UINT  StreamNumber,IWineD3DVertexBuffer * pStreamData,UINT Offset,UINT  Stride) PURE;
    STDMETHOD(GetStreamSource)(THIS_ UINT  StreamNumber,IWineD3DVertexBuffer ** ppStreamData,UINT *pOffset, UINT * pStride) PURE;
    STDMETHOD(SetTransform)(THIS_ D3DTRANSFORMSTATETYPE  State,CONST D3DMATRIX * pMatrix) PURE;
    STDMETHOD(GetTransform)(THIS_ D3DTRANSFORMSTATETYPE  State,D3DMATRIX * pMatrix) PURE;
    STDMETHOD(MultiplyTransform)(THIS_ D3DTRANSFORMSTATETYPE  State, CONST D3DMATRIX * pMatrix) PURE;
    STDMETHOD(SetLight)(THIS_ DWORD  Index,CONST WINED3DLIGHT * pLight) PURE;
    STDMETHOD(GetLight)(THIS_ DWORD  Index,WINED3DLIGHT * pLight) PURE;
    STDMETHOD(SetLightEnable)(THIS_ DWORD  Index,BOOL  Enable) PURE;
    STDMETHOD(GetLightEnable)(THIS_ DWORD  Index,BOOL * pEnable) PURE;
    STDMETHOD(SetClipPlane)(THIS_ DWORD  Index,CONST float * pPlane) PURE;
    STDMETHOD(GetClipPlane)(THIS_ DWORD  Index,float * pPlane) PURE;
    STDMETHOD(SetClipStatus)(THIS_ CONST WINED3DCLIPSTATUS * pClipStatus) PURE;
    STDMETHOD(GetClipStatus)(THIS_ WINED3DCLIPSTATUS * pClipStatus) PURE;
    STDMETHOD(SetMaterial)(THIS_ CONST WINED3DMATERIAL * pMaterial) PURE;
    STDMETHOD(GetMaterial)(THIS_ WINED3DMATERIAL *pMaterial) PURE;
    STDMETHOD(SetIndices)(THIS_ IWineD3DIndexBuffer * pIndexData,UINT  BaseVertexIndex) PURE;
    STDMETHOD(GetIndices)(THIS_ IWineD3DIndexBuffer ** ppIndexData,UINT * pBaseVertexIndex) PURE;
    STDMETHOD(SetViewport)(THIS_ CONST WINED3DVIEWPORT * pViewport) PURE;
    STDMETHOD(GetViewport)(THIS_ WINED3DVIEWPORT * pViewport) PURE;
    STDMETHOD(SetRenderState)(THIS_ D3DRENDERSTATETYPE  State,DWORD  Value) PURE;
    STDMETHOD(GetRenderState)(THIS_ D3DRENDERSTATETYPE  State,DWORD * pValue) PURE;
    STDMETHOD(SetTextureStageState)(THIS_ DWORD  Stage,D3DTEXTURESTAGESTATETYPE  Type,DWORD  Value) PURE;
    STDMETHOD(GetTextureStageState)(THIS_ DWORD  Stage,D3DTEXTURESTAGESTATETYPE  Type,DWORD * pValue) PURE;
    STDMETHOD(BeginScene)(THIS) PURE;
    STDMETHOD(EndScene)(THIS) PURE;
    STDMETHOD(Present)(THIS_ CONST RECT * pSourceRect,CONST RECT * pDestRect,HWND  hDestWindowOverride,CONST RGNDATA * pDirtyRegion) PURE;
    STDMETHOD(Clear)(THIS_ DWORD  Count,CONST D3DRECT * pRects,DWORD  Flags,D3DCOLOR  Color,float  Z,DWORD  Stencil) PURE;
    STDMETHOD(DrawPrimitive)(THIS_ D3DPRIMITIVETYPE  PrimitiveType,UINT  StartVertex,UINT  PrimitiveCount) PURE;
    STDMETHOD(DrawIndexedPrimitive)(THIS_ D3DPRIMITIVETYPE  PrimitiveType,INT baseVIdx, UINT  minIndex,UINT  NumVertices,UINT  startIndex,UINT  primCount) PURE;
    STDMETHOD(DrawPrimitiveUP)(THIS_ D3DPRIMITIVETYPE  PrimitiveType,UINT  PrimitiveCount,CONST void * pVertexStreamZeroData,UINT  VertexStreamZeroStride) PURE;
    STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ D3DPRIMITIVETYPE  PrimitiveType,UINT  MinVertexIndex,UINT  NumVertexIndices,UINT  PrimitiveCount,CONST void * pIndexData,D3DFORMAT  IndexDataFormat,CONST void * pVertexStreamZeroData,UINT  VertexStreamZeroStride) PURE;
    /*** Internal use IWineD3D methods ***/
    STDMETHOD_(void, SetupTextureStates)(THIS_ DWORD Stage, DWORD Flags);


};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DDevice_QueryInterface(p,a,b)                    (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DDevice_AddRef(p)                                (p)->lpVtbl->AddRef(p)
#define IWineD3DDevice_Release(p)                               (p)->lpVtbl->Release(p)
/*** IWineD3DDevice methods ***/
#define IWineD3DDevice_GetParent(p,a)                           (p)->lpVtbl->GetParent(p,a)
#define IWineD3DDevice_CreateVertexBuffer(p,a,b,c,d,e,f,g)      (p)->lpVtbl->CreateVertexBuffer(p,a,b,c,d,e,f,g)
#define IWineD3DDevice_CreateIndexBuffer(p,a,b,c,d,e,f,g)       (p)->lpVtbl->CreateIndexBuffer(p,a,b,c,d,e,f,g)
#define IWineD3DDevice_CreateStateBlock(p,a,b,c)                (p)->lpVtbl->CreateStateBlock(p,a,b,c)
#define IWineD3DDevice_SetFVF(p,a)                              (p)->lpVtbl->SetFVF(p,a)
#define IWineD3DDevice_GetFVF(p,a)                              (p)->lpVtbl->GetFVF(p,a)
#define IWineD3DDevice_SetStreamSource(p,a,b,c,d)               (p)->lpVtbl->SetStreamSource(p,a,b,c,d)
#define IWineD3DDevice_GetStreamSource(p,a,b,c,d)               (p)->lpVtbl->GetStreamSource(p,a,b,c,d)
#define IWineD3DDevice_SetTransform(p,a,b)                      (p)->lpVtbl->SetTransform(p,a,b)
#define IWineD3DDevice_GetTransform(p,a,b)                      (p)->lpVtbl->GetTransform(p,a,b)
#define IWineD3DDevice_MultiplyTransform(p,a,b)                 (p)->lpVtbl->MultiplyTransform(p,a,b)
#define IWineD3DDevice_SetLight(p,a,b)                          (p)->lpVtbl->SetLight(p,a,b)
#define IWineD3DDevice_GetLight(p,a,b)                          (p)->lpVtbl->GetLight(p,a,b)
#define IWineD3DDevice_SetLightEnable(p,a,b)                    (p)->lpVtbl->SetLightEnable(p,a,b)
#define IWineD3DDevice_GetLightEnable(p,a,b)                    (p)->lpVtbl->GetLightEnable(p,a,b)
#define IWineD3DDevice_SetClipPlane(p,a,b)                      (p)->lpVtbl->SetClipPlane(p,a,b)
#define IWineD3DDevice_GetClipPlane(p,a,b)                      (p)->lpVtbl->GetClipPlane(p,a,b)
#define IWineD3DDevice_SetClipStatus(p,a)                       (p)->lpVtbl->SetClipStatus(p,a)
#define IWineD3DDevice_GetClipStatus(p,a)                       (p)->lpVtbl->GetClipStatus(p,a)
#define IWineD3DDevice_SetMaterial(p,a)                         (p)->lpVtbl->SetMaterial(p,a)
#define IWineD3DDevice_GetMaterial(p,a)                         (p)->lpVtbl->GetMaterial(p,a)
#define IWineD3DDevice_SetIndices(p,a,b)                        (p)->lpVtbl->SetIndices(p,a,b)
#define IWineD3DDevice_GetIndices(p,a,b)                        (p)->lpVtbl->GetIndices(p,a,b)
#define IWineD3DDevice_SetViewport(p,a)                         (p)->lpVtbl->SetViewport(p,a)
#define IWineD3DDevice_GetViewport(p,a)                         (p)->lpVtbl->GetViewport(p,a)
#define IWineD3DDevice_SetRenderState(p,a,b)                    (p)->lpVtbl->SetRenderState(p,a,b)
#define IWineD3DDevice_GetRenderState(p,a,b)                    (p)->lpVtbl->GetRenderState(p,a,b)
#define IWineD3DDevice_SetTextureStageState(p,a,b,c)            (p)->lpVtbl->SetTextureStageState(p,a,b,c)
#define IWineD3DDevice_GetTextureStageState(p,a,b,c)            (p)->lpVtbl->GetTextureStageState(p,a,b,c)
#define IWineD3DDevice_BeginScene(p)                            (p)->lpVtbl->BeginScene(p)
#define IWineD3DDevice_EndScene(p)                              (p)->lpVtbl->EndScene(p)
#define IWineD3DDevice_Present(p,a,b,c,d)                       (p)->lpVtbl->Present(p,a,b,c,d)
#define IWineD3DDevice_Clear(p,a,b,c,d,e,f)                     (p)->lpVtbl->Clear(p,a,b,c,d,e,f)
#define IWineD3DDevice_DrawPrimitive(p,a,b,c)                   (p)->lpVtbl->DrawPrimitive(p,a,b,c)
#define IWineD3DDevice_DrawIndexedPrimitive(p,a,b,c,d,e,f)      (p)->lpVtbl->DrawIndexedPrimitive(p,a,b,c,d,e,f)
#define IWineD3DDevice_DrawPrimitiveUP(p,a,b,c,d)               (p)->lpVtbl->DrawPrimitiveUP(p,a,b,c,d)
#define IWineD3DDevice_DrawIndexedPrimitiveUP(p,a,b,c,d,e,f,g,h) (p)->lpVtbl->DrawIndexedPrimitiveUP(p,a,b,c,d,e,f,g,h)
#define IWineD3DDevice_SetupTextureStates(p,a,b)                (p)->lpVtbl->SetupTextureStates(p,a,b)
#endif

/*****************************************************************************
 * WineD3DResource interface 
 */
#define INTERFACE IWineD3DResource
DECLARE_INTERFACE_(IWineD3DResource,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DResource_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DResource_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DResource_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DResource methods ***/
#define IWineD3DResource_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DResource_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DResource_SetPrivateData(p,a,b,c,d)    (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DResource_GetPrivateData(p,a,b,c)      (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DResource_FreePrivateData(p,a)         (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DResource_SetPriority(p,a)             (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DResource_GetPriority(p)               (p)->lpVtbl->GetPriority(p)
#define IWineD3DResource_PreLoad(p)                   (p)->lpVtbl->PreLoad(p)
#define IWineD3DResource_GetType(p)                   (p)->lpVtbl->GetType(p)
#endif

/*****************************************************************************
 * WineD3DVertexBuffer interface 
 */
#define INTERFACE IWineD3DVertexBuffer
DECLARE_INTERFACE_(IWineD3DVertexBuffer,IWineD3DResource)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DVertexBuffer methods ***/
    STDMETHOD(Lock)(THIS_ UINT  OffsetToLock, UINT  SizeToLock, BYTE ** ppbData, DWORD  Flags) PURE;
    STDMETHOD(Unlock)(THIS) PURE;
    STDMETHOD(GetDesc)(THIS_ D3DVERTEXBUFFER_DESC  * pDesc) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DVertexBuffer_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DVertexBuffer_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DVertexBuffer_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DResource methods ***/
#define IWineD3DVertexBuffer_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DVertexBuffer_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DVertexBuffer_SetPrivateData(p,a,b,c,d)    (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DVertexBuffer_GetPrivateData(p,a,b,c)      (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DVertexBuffer_FreePrivateData(p,a)         (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DVertexBuffer_SetPriority(p,a)             (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DVertexBuffer_GetPriority(p)               (p)->lpVtbl->GetPriority(p)
#define IWineD3DVertexBuffer_PreLoad(p)                   (p)->lpVtbl->PreLoad(p)
#define IWineD3DVertexBuffer_GetType(p)                   (p)->lpVtbl->GetType(p)
/*** IWineD3DVertexBuffer methods ***/
#define IWineD3DVertexBuffer_Lock(p,a,b,c,d)              (p)->lpVtbl->Lock(p,a,b,c,d)
#define IWineD3DVertexBuffer_Unlock(p)                    (p)->lpVtbl->Unlock(p)
#define IWineD3DVertexBuffer_GetDesc(p,a)                 (p)->lpVtbl->GetDesc(p,a)
#endif

/*****************************************************************************
 * WineD3DIndexBuffer interface 
 */
#define INTERFACE IWineD3DIndexBuffer
DECLARE_INTERFACE_(IWineD3DIndexBuffer,IWineD3DResource)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DIndexBuffer methods ***/
    STDMETHOD(Lock)(THIS_ UINT  OffsetToLock, UINT  SizeToLock, BYTE ** ppbData, DWORD  Flags) PURE;
    STDMETHOD(Unlock)(THIS) PURE;
    STDMETHOD(GetDesc)(THIS_ D3DINDEXBUFFER_DESC  * pDesc) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DIndexBuffer_QueryInterface(p,a,b)        (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DIndexBuffer_AddRef(p)                    (p)->lpVtbl->AddRef(p)
#define IWineD3DIndexBuffer_Release(p)                   (p)->lpVtbl->Release(p)
/*** IWineD3DResource methods ***/
#define IWineD3DIndexBuffer_GetParent(p,a)               (p)->lpVtbl->GetParent(p,a)
#define IWineD3DIndexBuffer_GetDevice(p,a)               (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DIndexBuffer_SetPrivateData(p,a,b,c,d)    (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DIndexBuffer_GetPrivateData(p,a,b,c)      (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DIndexBuffer_FreePrivateData(p,a)         (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DIndexBuffer_SetPriority(p,a)             (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DIndexBuffer_GetPriority(p)               (p)->lpVtbl->GetPriority(p)
#define IWineD3DIndexBuffer_PreLoad(p)                   (p)->lpVtbl->PreLoad(p)
#define IWineD3DIndexBuffer_GetType(p)                   (p)->lpVtbl->GetType(p)
/*** IWineD3DIndexBuffer methods ***/
#define IWineD3DIndexBuffer_Lock(p,a,b,c,d)              (p)->lpVtbl->Lock(p,a,b,c,d)
#define IWineD3DIndexBuffer_Unlock(p)                    (p)->lpVtbl->Unlock(p)
#define IWineD3DIndexBuffer_GetDesc(p,a)                 (p)->lpVtbl->GetDesc(p,a)
#endif

/*****************************************************************************
 * IWineD3DBaseTexture interface
 *   Note at d3d8 this does NOT extend Resource, but at d3d9 it does
 *     since most functions are common anyway, it makes sense to extend it
 */
#define INTERFACE IWineD3DBaseTexture
DECLARE_INTERFACE_(IWineD3DBaseTexture,IWineD3DResource)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DResource methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(GetDevice)(THIS_ IWineD3DDevice ** ppDevice) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID  refguid, void * pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID  refguid) PURE;
    STDMETHOD_(DWORD,SetPriority)(THIS_ DWORD  PriorityNew) PURE;
    STDMETHOD_(DWORD,GetPriority)(THIS) PURE;
    STDMETHOD_(void,PreLoad)(THIS) PURE;
    STDMETHOD_(D3DRESOURCETYPE,GetType)(THIS) PURE;
    /*** IWineD3DBaseTexture methods ***/
    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew) PURE;
    STDMETHOD_(DWORD, GetLOD)(THIS) PURE;
    STDMETHOD_(DWORD, GetLevelCount)(THIS) PURE;
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType) PURE;
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS) PURE;
    STDMETHOD_(void, GenerateMipSubLevels)(THIS) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DBaseTexture_QueryInterface(p,a,b)      (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DBaseTexture_AddRef(p)                  (p)->lpVtbl->AddRef(p)
#define IWineD3DBaseTexture_Release(p)                 (p)->lpVtbl->Release(p)
/*** IWineD3DBaseTexture methods: IDirect3DResource9 ***/
#define IWineD3DBaseTexture_GetParent(p,a)             (p)->lpVtbl->GetParent(p,a)
#define IWineD3DBaseTexture_GetDevice(p,a)             (p)->lpVtbl->GetDevice(p,a)
#define IWineD3DBaseTexture_SetPrivateData(p,a,b,c,d)  (p)->lpVtbl->SetPrivateData(p,a,b,c,d)
#define IWineD3DBaseTexture_GetPrivateData(p,a,b,c)    (p)->lpVtbl->GetPrivateData(p,a,b,c)
#define IWineD3DBaseTexture_FreePrivateData(p,a)       (p)->lpVtbl->FreePrivateData(p,a)
#define IWineD3DBaseTexture_SetPriority(p,a)           (p)->lpVtbl->SetPriority(p,a)
#define IWineD3DBaseTexture_GetPriority(p)             (p)->lpVtbl->GetPriority(p)
#define IWineD3DBaseTexture_PreLoad(p)                 (p)->lpVtbl->PreLoad(p)
#define IWineD3DBaseTexture_GetType(p)                 (p)->lpVtbl->GetType(p)
/*** IWineD3DBaseTexture methods ***/
#define IWineD3DBaseTexture_SetLOD(p,a)                (p)->lpVtbl->SetLOD(p,a)
#define IWineD3DBaseTexture_GetLOD(p)                  (p)->lpVtbl->GetLOD(p)
#define IWineD3DBaseTexture_GetLevelCount(p)           (p)->lpVtbl->GetLevelCount(p)
#define IWineD3DBaseTexture_SetAutoGenFilterType(p,a)  (p)->lpVtbl->SetAutoGenFilterType(p,a)
#define IWineD3DBaseTexture_GetAutoGenFilterType(p)    (p)->lpVtbl->GetAutoGenFilterType(p)
#define IWineD3DBaseTexture_GenerateMipSubLevels(p)    (p)->lpVtbl->GenerateMipSubLevels(p)
#endif

/*****************************************************************************
 * WineD3DStateBlock interface 
 */
#define INTERFACE IWineD3DStateBlock
DECLARE_INTERFACE_(IWineD3DStateBlock,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IWineD3DStateBlock methods ***/
    STDMETHOD(GetParent)(THIS_ IUnknown **pParent) PURE;
    STDMETHOD(InitStartupStateBlock)(THIS) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
/*** IUnknown methods ***/
#define IWineD3DStateBlock_QueryInterface(p,a,b)                (p)->lpVtbl->QueryInterface(p,a,b)
#define IWineD3DStateBlock_AddRef(p)                            (p)->lpVtbl->AddRef(p)
#define IWineD3DStateBlock_Release(p)                           (p)->lpVtbl->Release(p)
/*** IWineD3DStateBlock methods ***/
#define IWineD3DStateBlock_GetParent(p,a)                       (p)->lpVtbl->GetParent(p,a)
#define IWineD3DStateBlock_InitStartupStateBlock(p)             (p)->lpVtbl->InitStartupStateBlock(p)
#endif


#if 0 /* FIXME: During porting in from d3d8 - the following will be used */
/*****************************************************************
 * Some defines
 */

/* Device caps */
#define MAX_PALETTES      256
#define MAX_STREAMS       16
#define MAX_CLIPPLANES    D3DMAXUSERCLIPPLANES
#define MAX_LEVELS        256

/* Other useful values */
#define HIGHEST_RENDER_STATE 174
#define HIGHEST_TEXTURE_STATE 29
#define HIGHEST_TRANSFORMSTATE 512
#define D3DSBT_RECORDED 0xfffffffe

#define D3D_VSHADER_MAX_CONSTANTS 96
#define D3D_PSHADER_MAX_CONSTANTS 32

/*****************************************************************
 * Some includes
 */

#include "wine/wined3d_gl.h"
#include "wine/wined3d_types.h"

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>

/*****************************************************************
 * Some defines
 */

typedef struct IDirect3DImpl IDirect3DImpl;
typedef struct IDirect3DBaseTextureImpl IDirect3DBaseTextureImpl;
typedef struct IDirect3DVolumeTextureImpl IDirect3DVolumeTextureImpl;
typedef struct IDirect3DDeviceImpl IDirect3DDeviceImpl;
typedef struct IDirect3DTextureImpl IDirect3DTextureImpl;
typedef struct IDirect3DCubeTextureImpl IDirect3DCubeTextureImpl;
typedef struct IDirect3DIndexBufferImpl IDirect3DIndexBufferImpl;
typedef struct IDirect3DSurfaceImpl IDirect3DSurfaceImpl;
typedef struct IDirect3DSwapChainImpl IDirect3DSwapChainImpl;
typedef struct IDirect3DResourceImpl IDirect3DResourceImpl;
typedef struct IDirect3DVolumeImpl IDirect3DVolumeImpl;
typedef struct IDirect3DVertexBufferImpl IDirect3DVertexBufferImpl;
typedef struct IDirect3DStateBlockImpl IDirect3DStateBlockImpl;
typedef struct IDirect3DVertexShaderImpl IDirect3DVertexShaderImpl;
typedef struct IDirect3DPixelShaderImpl IDirect3DPixelShaderImpl;
typedef struct IDirect3DVertexShaderDeclarationImpl IDirect3DVertexShaderDeclarationImpl;


/*************************************************************
 * d3dcore interfaces defs
 */

/** Vertex Shader API */
extern HRESULT WINAPI IDirect3DVertexShaderImpl_ParseProgram(IDirect3DVertexShaderImpl* This, CONST DWORD* pFunction);
extern HRESULT WINAPI IDirect3DVertexShaderImpl_GetFunction(IDirect3DVertexShaderImpl* This, VOID* pData, UINT* pSizeOfData);
extern HRESULT WINAPI IDirect3DVertexShaderImpl_SetConstantB(IDirect3DVertexShaderImpl* This, UINT StartRegister, CONST BOOL*  pConstantData, UINT BoolCount);
extern HRESULT WINAPI IDirect3DVertexShaderImpl_SetConstantI(IDirect3DVertexShaderImpl* This, UINT StartRegister, CONST INT*   pConstantData, UINT Vector4iCount);
extern HRESULT WINAPI IDirect3DVertexShaderImpl_SetConstantF(IDirect3DVertexShaderImpl* This, UINT StartRegister, CONST FLOAT* pConstantData, UINT Vector4fCount);
extern HRESULT WINAPI IDirect3DVertexShaderImpl_GetConstantB(IDirect3DVertexShaderImpl* This, UINT StartRegister, BOOL*  pConstantData, UINT BoolCount);
extern HRESULT WINAPI IDirect3DVertexShaderImpl_GetConstantI(IDirect3DVertexShaderImpl* This, UINT StartRegister, INT*   pConstantData, UINT Vector4iCount);
extern HRESULT WINAPI IDirect3DVertexShaderImpl_GetConstantF(IDirect3DVertexShaderImpl* This, UINT StartRegister, FLOAT* pConstantData, UINT Vector4fCount);
/* internal Interfaces */
extern DWORD   WINAPI IDirect3DVertexShaderImpl_GetVersion(IDirect3DVertexShaderImpl* This);
extern HRESULT WINAPI IDirect3DVertexShaderImpl_ExecuteSW(IDirect3DVertexShaderImpl* This, VSHADERINPUTDATA* input, VSHADEROUTPUTDATA* output);

#endif /* Temporary #if 0 */


#endif
