/* This contains the implementation of the interface Service
 * Providers require to communicate with Direct Play
 *
 * Copyright 2000 Peter Hunnisett
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

#include <string.h>
#include "winerror.h"
#include "wine/debug.h"

#include "dpinit.h"
#include "wine/dplaysp.h"
#include "dplay_global.h"
#include "name_server.h"
#include "dplayx_messages.h"

#include "dplayx_global.h" /* FIXME: For global hack */

/* FIXME: Need to add interface locking inside procedures */

WINE_DEFAULT_DEBUG_CHANNEL(dplay);

/* Prototypes */
static BOOL DPSP_CreateIUnknown( LPVOID lpSP );
static BOOL DPSP_DestroyIUnknown( LPVOID lpSP );
static BOOL DPSP_CreateDirectPlaySP( LPVOID lpSP, IDirectPlay2Impl* dp );
static BOOL DPSP_DestroyDirectPlaySP( LPVOID lpSP );

/* Predefine the interface */
typedef struct IDirectPlaySPImpl IDirectPlaySPImpl;

typedef struct tagDirectPlaySPIUnknownData
{
  LONG              ulObjRef;
  CRITICAL_SECTION  DPSP_lock;
} DirectPlaySPIUnknownData;

typedef struct tagDirectPlaySPData
{
  LPVOID lpSpRemoteData;
  DWORD  dwSpRemoteDataSize; /* Size of data pointed to by lpSpRemoteData */

  LPVOID lpSpLocalData;
  DWORD  dwSpLocalDataSize; /* Size of data pointed to by lpSpLocalData */

  IDirectPlay2Impl* dplay; /* FIXME: This should perhaps be iface not impl */

} DirectPlaySPData;

#define DPSP_IMPL_FIELDS \
   LONG ulInterfaceRef; \
   DirectPlaySPIUnknownData* unk; \
   DirectPlaySPData* sp;

struct IDirectPlaySPImpl
{
  const IDirectPlaySPVtbl *lpVtbl;
  DPSP_IMPL_FIELDS
};

/* Forward declaration of virtual tables */
static const IDirectPlaySPVtbl directPlaySPVT;

/* This structure is passed to the DP object for safe keeping */
typedef struct tagDP_SPPLAYERDATA
{
  LPVOID lpPlayerLocalData;
  DWORD  dwPlayerLocalDataSize;

  LPVOID lpPlayerRemoteData;
  DWORD  dwPlayerRemoteDataSize;
} DP_SPPLAYERDATA, *LPDP_SPPLAYERDATA;

/* Create the SP interface */
HRESULT DPSP_CreateInterface( REFIID riid, LPVOID* ppvObj, IDirectPlay2Impl* dp )
{
  TRACE( " for %s\n", debugstr_guid( riid ) );

  *ppvObj = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                       sizeof( IDirectPlaySPImpl ) );

  if( *ppvObj == NULL )
  {
    return DPERR_OUTOFMEMORY;
  }

  if( IsEqualGUID( &IID_IDirectPlaySP, riid ) )
  {
    IDirectPlaySPImpl *This = *ppvObj;
    This->lpVtbl = &directPlaySPVT;
  }
  else
  {
    /* Unsupported interface */
    HeapFree( GetProcessHeap(), 0, *ppvObj );
    *ppvObj = NULL;

    return E_NOINTERFACE;
  }

  /* Initialize it */
  if( DPSP_CreateIUnknown( *ppvObj ) &&
      DPSP_CreateDirectPlaySP( *ppvObj, dp )
    )
  {
    IDirectPlaySP_AddRef( (LPDIRECTPLAYSP)*ppvObj );
    return S_OK;
  }

  /* Initialize failed, destroy it */
  DPSP_DestroyDirectPlaySP( *ppvObj );
  DPSP_DestroyIUnknown( *ppvObj );

  HeapFree( GetProcessHeap(), 0, *ppvObj );
  *ppvObj = NULL;

  return DPERR_NOMEMORY;
}

static BOOL DPSP_CreateIUnknown( LPVOID lpSP )
{
  IDirectPlaySPImpl *This = lpSP;

  This->unk = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->unk) ) );

  if ( This->unk == NULL )
  {
    return FALSE;
  }

  InitializeCriticalSection( &This->unk->DPSP_lock );
  This->unk->DPSP_lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": IDirectPlaySPImpl*->DirectPlaySPIUnknownData*->DPSP_lock");

  return TRUE;
}

static BOOL DPSP_DestroyIUnknown( LPVOID lpSP )
{
  IDirectPlaySPImpl *This = lpSP;

  This->unk->DPSP_lock.DebugInfo->Spare[0] = 0;
  DeleteCriticalSection( &This->unk->DPSP_lock );
  HeapFree( GetProcessHeap(), 0, This->unk );

  return TRUE;
}


static BOOL DPSP_CreateDirectPlaySP( LPVOID lpSP, IDirectPlay2Impl* dp )
{
  IDirectPlaySPImpl *This = lpSP;

  This->sp = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->sp) ) );

  if ( This->sp == NULL )
  {
    return FALSE;
  }

  This->sp->dplay = dp;

  /* Normally we should be keeping a reference, but since only the dplay
   * interface that created us can destroy us, we do not keep a reference
   * to it (ie we'd be stuck with always having one reference to the dplay
   * object, and hence us, around).
   * NOTE: The dp object does reference count us.
   *
   * FIXME: This is a kludge to get around a problem where a queryinterface
   *        is used to get a new interface and then is closed. We will then
   *        reference garbage. However, with this we will never deallocate
   *        the interface we store. The correct fix is to require all
   *        DP internal interfaces to use the This->dp2 interface which
   *        should be changed to This->dp
   */
  IDirectPlayX_AddRef( (LPDIRECTPLAY2)dp );

  return TRUE;
}

static BOOL DPSP_DestroyDirectPlaySP( LPVOID lpSP )
{
  IDirectPlaySPImpl *This = lpSP;

  /* Normally we should be keeping a reference, but since only the dplay
   * interface that created us can destroy us, we do not keep a reference
   * to it (ie we'd be stuck with always having one reference to the dplay
   * object, and hence us, around).
   * NOTE: The dp object does reference count us.
   */
  /*IDirectPlayX_Release( (LPDIRECTPLAY2)This->sp->dplay ); */

  HeapFree( GetProcessHeap(), 0, This->sp->lpSpRemoteData );
  HeapFree( GetProcessHeap(), 0, This->sp->lpSpLocalData );

  /* FIXME: Need to delete player queue */

  HeapFree( GetProcessHeap(), 0, This->sp );
  return TRUE;
}

/* Interface implementation */

static HRESULT WINAPI DPSP_QueryInterface
( LPDIRECTPLAYSP iface,
  REFIID riid,
  LPVOID* ppvObj )
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;
  TRACE("(%p)->(%s,%p)\n", This, debugstr_guid( riid ), ppvObj );

  *ppvObj = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                       sizeof( *This ) );

  if( *ppvObj == NULL )
  {
    return DPERR_OUTOFMEMORY;
  }

  CopyMemory( *ppvObj, This, sizeof( *This )  );
  (*(IDirectPlaySPImpl**)ppvObj)->ulInterfaceRef = 0;

  if( IsEqualGUID( &IID_IDirectPlaySP, riid ) )
  {
    IDirectPlaySPImpl *This = *ppvObj;
    This->lpVtbl = &directPlaySPVT;
  }
  else
  {
    /* Unsupported interface */
    HeapFree( GetProcessHeap(), 0, *ppvObj );
    *ppvObj = NULL;

    return E_NOINTERFACE;
  }

  IDirectPlaySP_AddRef( (LPDIRECTPLAYSP)*ppvObj );

  return S_OK;
}

static ULONG WINAPI DPSP_AddRef
( LPDIRECTPLAYSP iface )
{
  ULONG ulInterfaceRefCount, ulObjRefCount;
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  ulObjRefCount       = InterlockedIncrement( &This->unk->ulObjRef );
  ulInterfaceRefCount = InterlockedIncrement( &This->ulInterfaceRef );

  TRACE( "ref count incremented to %u:%u for %p\n",
         ulInterfaceRefCount, ulObjRefCount, This );

  return ulObjRefCount;
}

static ULONG WINAPI DPSP_Release
( LPDIRECTPLAYSP iface )
{
  ULONG ulInterfaceRefCount, ulObjRefCount;
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  ulObjRefCount       = InterlockedDecrement( &This->unk->ulObjRef );
  ulInterfaceRefCount = InterlockedDecrement( &This->ulInterfaceRef );

  TRACE( "ref count decremented to %u:%u for %p\n",
         ulInterfaceRefCount, ulObjRefCount, This );

  /* Deallocate if this is the last reference to the object */
  if( ulObjRefCount == 0 )
  {
     DPSP_DestroyDirectPlaySP( This );
     DPSP_DestroyIUnknown( This );
  }

  if( ulInterfaceRefCount == 0 )
  {
    HeapFree( GetProcessHeap(), 0, This );
  }

  return ulInterfaceRefCount;
}

static HRESULT WINAPI IDirectPlaySPImpl_AddMRUEntry
( LPDIRECTPLAYSP iface,
  LPCWSTR lpSection,
  LPCWSTR lpKey,
  LPCVOID lpData,
  DWORD   dwDataSize,
  DWORD   dwMaxEntries
)
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  /* Should be able to call the comctl32 undocumented MRU routines.
     I suspect that the interface works appropriately */
  FIXME( "(%p)->(%p,%p%p,0x%08x,0x%08x): stub\n",
         This, lpSection, lpKey, lpData, dwDataSize, dwMaxEntries );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlaySPImpl_CreateAddress
( LPDIRECTPLAYSP iface,
  REFGUID guidSP,
  REFGUID guidDataType,
  LPCVOID lpData,
  DWORD   dwDataSize,
  LPVOID  lpAddress,
  LPDWORD lpdwAddressSize
)
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  FIXME( "(%p)->(%s,%s,%p,0x%08x,%p,%p): stub\n",
         This, debugstr_guid(guidSP), debugstr_guid(guidDataType),
         lpData, dwDataSize, lpAddress, lpdwAddressSize );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlaySPImpl_EnumAddress
( LPDIRECTPLAYSP iface,
  LPDPENUMADDRESSCALLBACK lpEnumAddressCallback,
  LPCVOID lpAddress,
  DWORD dwAddressSize,
  LPVOID lpContext
)
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  TRACE( "(%p)->(%p,%p,0x%08x,%p)\n",
         This, lpEnumAddressCallback, lpAddress, dwAddressSize, lpContext );

  DPL_EnumAddress( lpEnumAddressCallback, lpAddress, dwAddressSize, lpContext );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlaySPImpl_EnumMRUEntries
( LPDIRECTPLAYSP iface,
  LPCWSTR lpSection,
  LPCWSTR lpKey,
  LPENUMMRUCALLBACK lpEnumMRUCallback,
  LPVOID lpContext
)
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  /* Should be able to call the comctl32 undocumented MRU routines.
     I suspect that the interface works appropriately */
  FIXME( "(%p)->(%p,%p,%p,%p,): stub\n",
         This, lpSection, lpKey, lpEnumMRUCallback, lpContext );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlaySPImpl_GetPlayerFlags
( LPDIRECTPLAYSP iface,
  DPID idPlayer,
  LPDWORD lpdwPlayerFlags
)
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  FIXME( "(%p)->(0x%08x,%p): stub\n",
         This, idPlayer, lpdwPlayerFlags );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlaySPImpl_GetSPPlayerData
( LPDIRECTPLAYSP iface,
  DPID idPlayer,
  LPVOID* lplpData,
  LPDWORD lpdwDataSize,
  DWORD dwFlags
)
{
  HRESULT hr;
  LPDP_SPPLAYERDATA lpPlayerData;
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  TRACE( "(%p)->(0x%08x,%p,%p,0x%08x)\n",
         This, idPlayer, lplpData, lpdwDataSize, dwFlags );

  hr = DP_GetSPPlayerData( This->sp->dplay, idPlayer, (LPVOID*)&lpPlayerData );

  if( FAILED(hr) )
  {
    ERR( "Couldn't get player data: %s\n", DPLAYX_HresultToString(hr) );
    return hr;
  }

  if( dwFlags == DPSET_LOCAL )
  {
    *lplpData     = lpPlayerData->lpPlayerLocalData;
    *lpdwDataSize = lpPlayerData->dwPlayerLocalDataSize;
  }
  else if( dwFlags == DPSET_REMOTE )
  {
    *lplpData     = lpPlayerData->lpPlayerRemoteData;
    *lpdwDataSize = lpPlayerData->dwPlayerRemoteDataSize;
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlaySPImpl_HandleMessage
( LPDIRECTPLAYSP iface,
  LPVOID lpMessageBody,
  DWORD  dwMessageBodySize,
  LPVOID lpMessageHeader
)
{
  LPDPSP_MSG_ENVELOPE lpMsg = lpMessageBody;
  DPSP_REPLYDATA data;
  HRESULT hr;

  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  TRACE( "(%p)->(%p,0x%08x,%p)\n",
         This, lpMessageBody, dwMessageBodySize, lpMessageHeader );

  TRACE( "Incoming message has envelope of 0x%08x, 0x%x, %u\n",
         lpMsg->dwMagic, lpMsg->wCommandId, lpMsg->wVersion );

  if( lpMsg->dwMagic != DPMSG_SIGNATURE )
  {
    ERR( "Unknown magic 0x%08x!\n", lpMsg->dwMagic );
    return DPERR_GENERIC;
  }

  /* Pass everything else to Direct Play */
  data.lpMessage     = NULL;
  data.dwMessageSize = 0;

  /* Pass this message to the dplay interface to handle */
  hr = DP_HandleMessage( This->sp->dplay, lpMessageBody, dwMessageBodySize,
                         lpMessageHeader, lpMsg->wCommandId, lpMsg->wVersion,
                         &data.lpMessage, &data.dwMessageSize );
  if( FAILED(hr) )
  {
    ERR( "Command processing failed %s\n", DPLAYX_HresultToString(hr) );
  }

  /* Do we want a reply? */
  if( data.lpMessage != NULL )
  {
    data.lpSPMessageHeader = lpMessageHeader;
    data.idNameServer      = 0;
    data.lpISP             = iface;

    hr = (This->sp->dplay->dp2->spData.lpCB->Reply)( &data );

    if( FAILED(hr) )
    {
      ERR( "Reply failed %s\n", DPLAYX_HresultToString(hr) );
    }
  }

  return hr;
}

static HRESULT WINAPI IDirectPlaySPImpl_SetSPPlayerData
( LPDIRECTPLAYSP iface,
  DPID idPlayer,
  LPVOID lpData,
  DWORD dwDataSize,
  DWORD dwFlags
)
{
  HRESULT           hr;
  LPDP_SPPLAYERDATA lpPlayerEntry;
  LPVOID            lpPlayerData;

  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  TRACE( "(%p)->(0x%08x,%p,0x%08x,0x%08x)\n",
         This, idPlayer, lpData, dwDataSize, dwFlags );

  hr = DP_GetSPPlayerData( This->sp->dplay, idPlayer, (LPVOID*)&lpPlayerEntry );
  if( FAILED(hr) )
  {
    /* Player must not exist */
    return DPERR_INVALIDPLAYER;
  }

  lpPlayerData = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwDataSize );
  CopyMemory( lpPlayerData, lpData, dwDataSize );

  /* If we have data already allocated, free it and replace it */
  if( dwFlags == DPSET_LOCAL )
  {
    HeapFree( GetProcessHeap(), 0, lpPlayerEntry->lpPlayerLocalData );
    lpPlayerEntry->lpPlayerLocalData = lpPlayerData;
    lpPlayerEntry->dwPlayerLocalDataSize = dwDataSize;
  }
  else if( dwFlags == DPSET_REMOTE )
  {
    HeapFree( GetProcessHeap(), 0, lpPlayerEntry->lpPlayerRemoteData );
    lpPlayerEntry->lpPlayerRemoteData = lpPlayerData;
    lpPlayerEntry->dwPlayerRemoteDataSize = dwDataSize;
  }

  hr = DP_SetSPPlayerData( This->sp->dplay, idPlayer, lpPlayerEntry );

  return hr;
}

static HRESULT WINAPI IDirectPlaySPImpl_CreateCompoundAddress
( LPDIRECTPLAYSP iface,
  LPCDPCOMPOUNDADDRESSELEMENT lpElements,
  DWORD dwElementCount,
  LPVOID lpAddress,
  LPDWORD lpdwAddressSize
)
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  FIXME( "(%p)->(%p,0x%08x,%p,%p): stub\n",
         This, lpElements, dwElementCount, lpAddress, lpdwAddressSize );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlaySPImpl_GetSPData
( LPDIRECTPLAYSP iface,
  LPVOID* lplpData,
  LPDWORD lpdwDataSize,
  DWORD dwFlags
)
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  TRACE( "(%p)->(%p,%p,0x%08x)\n",
         This, lplpData, lpdwDataSize, dwFlags );

  if( dwFlags == DPSET_REMOTE )
  {
    *lpdwDataSize = This->sp->dwSpRemoteDataSize;
    *lplpData     = This->sp->lpSpRemoteData;
  }
  else if( dwFlags == DPSET_LOCAL )
  {
    *lpdwDataSize = This->sp->dwSpLocalDataSize;
    *lplpData     = This->sp->lpSpLocalData;
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlaySPImpl_SetSPData
( LPDIRECTPLAYSP iface,
  LPVOID lpData,
  DWORD dwDataSize,
  DWORD dwFlags
)
{
  LPVOID lpSpData;

  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  TRACE( "(%p)->(%p,%d,0x%08x)\n",
         This, lpData, dwDataSize, dwFlags );

  lpSpData = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwDataSize );
  CopyMemory( lpSpData, lpData, dwDataSize );

  /* If we have data already allocated, free it and replace it */
  if( dwFlags == DPSET_REMOTE )
  {
    HeapFree( GetProcessHeap(), 0, This->sp->lpSpRemoteData );
    This->sp->dwSpRemoteDataSize = dwDataSize;
    This->sp->lpSpRemoteData = lpSpData;
  }
  else if ( dwFlags == DPSET_LOCAL )
  {
    HeapFree( GetProcessHeap(), 0, This->sp->lpSpLocalData );
    This->sp->lpSpLocalData     = lpSpData;
    This->sp->dwSpLocalDataSize = dwDataSize;
  }

  return DP_OK;
}

static VOID WINAPI IDirectPlaySPImpl_SendComplete
( LPDIRECTPLAYSP iface,
  LPVOID unknownA,
  DWORD unknownB
)
{
  IDirectPlaySPImpl *This = (IDirectPlaySPImpl *)iface;

  FIXME( "(%p)->(%p,0x%08x): stub\n",
         This, unknownA, unknownB );
}

static const IDirectPlaySPVtbl directPlaySPVT =
{

  DPSP_QueryInterface,
  DPSP_AddRef,
  DPSP_Release,

  IDirectPlaySPImpl_AddMRUEntry,
  IDirectPlaySPImpl_CreateAddress,
  IDirectPlaySPImpl_EnumAddress,
  IDirectPlaySPImpl_EnumMRUEntries,
  IDirectPlaySPImpl_GetPlayerFlags,
  IDirectPlaySPImpl_GetSPPlayerData,
  IDirectPlaySPImpl_HandleMessage,
  IDirectPlaySPImpl_SetSPPlayerData,
  IDirectPlaySPImpl_CreateCompoundAddress,
  IDirectPlaySPImpl_GetSPData,
  IDirectPlaySPImpl_SetSPData,
  IDirectPlaySPImpl_SendComplete
};


/* DP external interfaces to call into DPSP interface */

/* Allocate the structure */
LPVOID DPSP_CreateSPPlayerData(void)
{
  TRACE( "Creating SPPlayer data struct\n" );
  return HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                    sizeof( DP_SPPLAYERDATA ) );
}
