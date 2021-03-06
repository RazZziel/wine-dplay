/* Direct Play 2,3,4 Implementation
 *
 * Copyright 1998,1999,2000,2001 - Peter Hunnisett
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

#include <stdarg.h>
#include <string.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winerror.h"
#include "winbase.h"
#include "winnt.h"
#include "winreg.h"
#include "winnls.h"
#include "wine/unicode.h"
#include "wine/debug.h"

#include "dpinit.h"
#include "dplayx_global.h"
#include "name_server.h"
#include "dplayx_queue.h"
#include "wine/dplaysp.h"
#include "dplay_global.h"

WINE_DEFAULT_DEBUG_CHANNEL(dplay);

/* FIXME: Should this be externed? */
extern HRESULT DPL_CreateCompoundAddress
( LPCDPCOMPOUNDADDRESSELEMENT lpElements, DWORD dwElementCount,
  LPVOID lpAddress, LPDWORD lpdwAddressSize, BOOL bAnsiInterface );


/* Local function prototypes */
static BOOL DP_CopyDPNAMEStruct( LPDPNAME lpDst, const DPNAME *lpSrc, BOOL bAnsi );
static void DP_SetPlayerData( lpPlayerData lpPData, DWORD dwFlags,
                              LPVOID lpData, DWORD dwDataSize );

static lpGroupData DP_CreateGroup( IDirectPlay2AImpl* iface, const DPID *lpid,
                                   const DPNAME *lpName, DWORD dwFlags,
                                   DPID idParent, BOOL bAnsi );
static void DP_SetGroupData( lpGroupData lpGData, DWORD dwFlags,
                             LPVOID lpData, DWORD dwDataSize );
static void DP_DeleteDPNameStruct( LPDPNAME lpDPName );
static void DP_DeletePlayer( IDirectPlay2Impl* This, DPID dpid );
static BOOL CALLBACK cbDeletePlayerFromAllGroups( DPID dpId,
                                                  DWORD dwPlayerType,
                                                  LPCDPNAME lpName,
                                                  DWORD dwFlags,
                                                  LPVOID lpContext );
static lpGroupData DP_FindAnyGroup( IDirectPlay2AImpl* This, DPID dpid );
static BOOL CALLBACK cbRemoveGroupOrPlayer( DPID dpId, DWORD dwPlayerType,
                                            LPCDPNAME lpName, DWORD dwFlags,
                                            LPVOID lpContext );
static void DP_DeleteGroup( IDirectPlay2Impl* This, DPID dpid );

/* Forward declarations of virtual tables */
static const IDirectPlay2Vtbl directPlay2AVT;
static const IDirectPlay3Vtbl directPlay3AVT;
static const IDirectPlay4Vtbl directPlay4AVT;

static const IDirectPlay2Vtbl directPlay2WVT;
static const IDirectPlay3Vtbl directPlay3WVT;
static const IDirectPlay4Vtbl directPlay4WVT;

/* Helper methods for player/group interfaces */
static HRESULT DP_IF_DeletePlayerFromGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup,
            DPID idPlayer, BOOL bAnsi );
static HRESULT DP_IF_CreatePlayer
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, LPDPID lpidPlayer,
            LPDPNAME lpPlayerName, HANDLE hEvent, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_DestroyGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup, BOOL bAnsi );
static HRESULT DP_IF_DestroyPlayer
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idPlayer, BOOL bAnsi );
static HRESULT DP_IF_EnumGroupPlayers
          ( IDirectPlay2Impl* This, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_EnumGroups
          ( IDirectPlay2Impl* This, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_EnumPlayers
          ( IDirectPlay2Impl* This, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_GetGroupData
          ( IDirectPlay2Impl* This, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_GetGroupName
          ( IDirectPlay2Impl* This, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize, BOOL bAnsi );
static HRESULT DP_IF_GetPlayerData
          ( IDirectPlay2Impl* This, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_GetPlayerName
          ( IDirectPlay2Impl* This, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize, BOOL bAnsi );
static HRESULT DP_IF_SetGroupName
          ( IDirectPlay2Impl* This, DPID idGroup, LPDPNAME lpGroupName,
            DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_SetPlayerData
          ( IDirectPlay2Impl* This, DPID idPlayer, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_SetPlayerName
          ( IDirectPlay2Impl* This, DPID idPlayer, LPDPNAME lpPlayerName,
            DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_AddGroupToGroup
          ( IDirectPlay3Impl* This, DPID idParentGroup, DPID idGroup );
static HRESULT DP_IF_CreateGroup
          ( IDirectPlay2AImpl* This, LPVOID lpMsgHdr, LPDPID lpidGroup,
            LPDPNAME lpGroupName, LPVOID lpData, DWORD dwDataSize,
            DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_CreateGroupInGroup
          ( IDirectPlay3Impl* This, LPVOID lpMsgHdr, DPID idParentGroup,
            LPDPID lpidGroup, LPDPNAME lpGroupName, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_AddPlayerToGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup,
            DPID idPlayer, BOOL bAnsi );
static HRESULT DP_IF_DeleteGroupFromGroup
          ( IDirectPlay3Impl* This, DPID idParentGroup, DPID idGroup );
static HRESULT DP_SecureOpen
          ( IDirectPlay2Impl* This, LPCDPSESSIONDESC2 lpsd, DWORD dwFlags,
            LPCDPSECURITYDESC lpSecurity, LPCDPCREDENTIALS lpCredentials,
            BOOL bAnsi );
static HRESULT DP_SendEx
          ( IDirectPlay2Impl* This, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID, BOOL bAnsi );
static HRESULT DP_IF_Receive
          ( IDirectPlay2Impl* This, LPDPID lpidFrom, LPDPID lpidTo,
            DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize, BOOL bAnsi );
static HRESULT DP_IF_GetMessageQueue
          ( IDirectPlay4Impl* This, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes, BOOL bAnsi );
static HRESULT DP_SP_SendEx
          ( IDirectPlay2Impl* This, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID );
static HRESULT DP_IF_SetGroupData
          ( IDirectPlay2Impl* This, DPID idGroup, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_GetPlayerCaps
          ( IDirectPlay2Impl* This, DPID idPlayer, LPDPCAPS lpDPCaps,
            DWORD dwFlags );
static HRESULT DP_IF_Close( IDirectPlay2Impl* This, BOOL bAnsi );
static HRESULT DP_IF_CancelMessage
          ( IDirectPlay4Impl* This, DWORD dwMsgID, DWORD dwFlags,
            DWORD dwMinPriority, DWORD dwMaxPriority, BOOL bAnsi );
static HRESULT DP_IF_EnumGroupsInGroup
          ( IDirectPlay3AImpl* This, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_GetGroupParent
          ( IDirectPlay3AImpl* This, DPID idGroup, LPDPID lpidGroup,
            BOOL bAnsi );
static HRESULT DP_IF_GetCaps
          ( IDirectPlay2Impl* This, LPDPCAPS lpDPCaps, DWORD dwFlags );
static HRESULT DP_IF_EnumSessions
          ( IDirectPlay2Impl* This, LPDPSESSIONDESC2 lpsd, DWORD dwTimeout,
            LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_InitializeConnection
          ( IDirectPlay3Impl* This, LPVOID lpConnection, DWORD dwFlags, BOOL bAnsi );
static BOOL CALLBACK cbDPCreateEnumConnections( LPCGUID lpguidSP,
    LPVOID lpConnection, DWORD dwConnectionSize, LPCDPNAME lpName,
    DWORD dwFlags, LPVOID lpContext );
static BOOL DP_BuildCompoundAddr( GUID guidDataType, LPGUID lpcSpGuid,
                                  LPVOID* lplpAddrBuf, LPDWORD lpdwBufSize );



static inline DPID DP_NextObjectId(void);
static DPID DP_GetRemoteNextObjectId(void);

static DWORD DP_CalcSessionDescSize( LPCDPSESSIONDESC2 lpSessDesc, BOOL bAnsi );
static void DP_CopySessionDesc( LPDPSESSIONDESC2 destSessionDesc,
                                LPCDPSESSIONDESC2 srcSessDesc, BOOL bAnsi );


static HMODULE DP_LoadSP( LPCGUID lpcGuid, LPSPINITDATA lpSpData, LPBOOL lpbIsDpSp );
static HRESULT DP_InitializeDPSP( IDirectPlay3Impl* This, HMODULE hServiceProvider );
static HRESULT DP_InitializeDPLSP( IDirectPlay3Impl* This, HMODULE hServiceProvider );






#define DPID_NOPARENT_GROUP 0 /* Magic number to indicate no parent of group */
#define DPID_SYSTEM_GROUP DPID_NOPARENT_GROUP /* If system group is supported
                                                 we don't have to change much */
#define DPID_NAME_SERVER 0x19a9d65b  /* Don't ask me why */

/* Strip out dwFlag values which cannot be sent in the CREATEGROUP msg */
#define DPMSG_CREATEGROUP_DWFLAGS(x) ( (x) & DPGROUP_HIDDEN )

/* Strip out all dwFlags values for CREATEPLAYER msg */
#define DPMSG_CREATEPLAYER_DWFLAGS(x) 0

static LONG kludgePlayerGroupId = 1000;

/* ------------------------------------------------------------------ */


static BOOL DP_CreateIUnknown( LPVOID lpDP )
{
  IDirectPlay2AImpl *This = lpDP;

  This->unk = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->unk) ) );
  if ( This->unk == NULL )
  {
    return FALSE;
  }

  InitializeCriticalSection( &This->unk->DP_lock );
  This->unk->DP_lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": IDirectPlay2AImpl*->DirectPlayIUnknownData*->DP_lock");

  return TRUE;
}

static BOOL DP_DestroyIUnknown( LPVOID lpDP )
{
  IDirectPlay2AImpl *This = lpDP;

  This->unk->DP_lock.DebugInfo->Spare[0] = 0;
  DeleteCriticalSection( &This->unk->DP_lock );
  HeapFree( GetProcessHeap(), 0, This->unk );

  return TRUE;
}

static BOOL DP_CreateDirectPlay2( LPVOID lpDP )
{
  IDirectPlay2AImpl *This = lpDP;

  This->dp2 = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->dp2) ) );
  if ( This->dp2 == NULL )
  {
    return FALSE;
  }

  This->dp2->bConnectionOpen = FALSE;

  This->dp2->hEnumSessionThread = INVALID_HANDLE_VALUE;
  This->dp2->dwEnumSessionLock = 0;

  This->dp2->bHostInterface = FALSE;

  DPQ_INIT(This->dp2->receiveMsgs);
  DPQ_INIT(This->dp2->sendMsgs);
  DPQ_INIT(This->dp2->replysExpected);

  if( !NS_InitializeSessionCache( &This->dp2->lpNameServerData ) )
  {
    /* FIXME: Memory leak */
    return FALSE;
  }

  /* Provide an initial session desc with nothing in it */
  This->dp2->lpSessionDesc = HeapAlloc( GetProcessHeap(),
                                        HEAP_ZERO_MEMORY,
                                        sizeof( *This->dp2->lpSessionDesc ) );
  if( This->dp2->lpSessionDesc == NULL )
  {
    /* FIXME: Memory leak */
    return FALSE;
  }
  This->dp2->lpSessionDesc->dwSize = sizeof( *This->dp2->lpSessionDesc );

  /* We are emulating a dp 6 implementation */
  This->dp2->spData.dwSPVersion = DPSP_MAJORVERSION;

  This->dp2->spData.lpCB = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                      sizeof( *This->dp2->spData.lpCB ) );
  This->dp2->spData.lpCB->dwSize = sizeof( *This->dp2->spData.lpCB );
  This->dp2->spData.lpCB->dwVersion = DPSP_MAJORVERSION;

  /* This is the pointer to the service provider */
  if( FAILED( DPSP_CreateInterface( &IID_IDirectPlaySP,
                                    (LPVOID*)&This->dp2->spData.lpISP, This ) )
    )
  {
    /* FIXME: Memory leak */
    return FALSE;
  }

  /* Setup lobby provider information */
  This->dp2->dplspData.dwSPVersion = DPSP_MAJORVERSION;
  This->dp2->dplspData.lpCB = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                         sizeof( *This->dp2->dplspData.lpCB ) );
  This->dp2->dplspData.lpCB->dwSize = sizeof(  *This->dp2->dplspData.lpCB );

  if( FAILED( DPLSP_CreateInterface( &IID_IDPLobbySP,
                                     (LPVOID*)&This->dp2->dplspData.lpISP, This ) )
    )
  {
    /* FIXME: Memory leak */
    return FALSE;
  }

  return TRUE;
}

/* Definition of the global function in dplayx_queue.h. #
 * FIXME: Would it be better to have a dplayx_queue.c for this function? */
DPQ_DECL_DELETECB( cbDeleteElemFromHeap, LPVOID )
{
  HeapFree( GetProcessHeap(), 0, elem );
}

static BOOL DP_DestroyDirectPlay2( LPVOID lpDP )
{
  IDirectPlay2AImpl *This = lpDP;

  if( This->dp2->hEnumSessionThread != INVALID_HANDLE_VALUE )
  {
    TerminateThread( This->dp2->hEnumSessionThread, 0 );
    CloseHandle( This->dp2->hEnumSessionThread );
  }

  /* Finish with the SP - have it shutdown */
  if( This->dp2->spData.lpCB->ShutdownEx )
  {
    DPSP_SHUTDOWNDATA data;

    TRACE( "Calling SP ShutdownEx\n" );

    data.lpISP = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->ShutdownEx)( &data );
  }
  else if (This->dp2->spData.lpCB->Shutdown ) /* obsolete interface */
  {
    TRACE( "Calling obsolete SP Shutdown\n" );
    (*This->dp2->spData.lpCB->Shutdown)();
  }

  /* Unload the SP (if it exists) */
  if( This->dp2->hServiceProvider != 0 )
  {
    FreeLibrary( This->dp2->hServiceProvider );
  }

  /* Unload the Lobby Provider (if it exists) */
  if( This->dp2->hDPLobbyProvider != 0 )
  {
    FreeLibrary( This->dp2->hDPLobbyProvider );
  }

  /* FIXME: Need to delete receive and send msgs queue contents */

  NS_DeleteSessionCache( This->dp2->lpNameServerData );

  HeapFree( GetProcessHeap(), 0, This->dp2->lpSessionDesc );

  IDirectPlaySP_Release( This->dp2->spData.lpISP );

  /* Delete the contents */
  HeapFree( GetProcessHeap(), 0, This->dp2 );

  return TRUE;
}

static BOOL DP_CreateDirectPlay3( LPVOID lpDP )
{
  IDirectPlay3AImpl *This = lpDP;

  This->dp3 = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->dp3) ) );
  if ( This->dp3 == NULL )
  {
    return FALSE;
  }

  return TRUE;
}

static BOOL DP_DestroyDirectPlay3( LPVOID lpDP )
{
  IDirectPlay3AImpl *This = lpDP;

  /* Delete the contents */
  HeapFree( GetProcessHeap(), 0, This->dp3 );

  return TRUE;
}

static BOOL DP_CreateDirectPlay4( LPVOID lpDP )
{
  IDirectPlay4AImpl *This = lpDP;

  This->dp4 = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->dp4) ) );
  if ( This->dp4 == NULL )
  {
    return FALSE;
  }

  return TRUE;
}

static BOOL DP_DestroyDirectPlay4( LPVOID lpDP )
{
  IDirectPlay3AImpl *This = lpDP;

  /* Delete the contents */
  HeapFree( GetProcessHeap(), 0, This->dp4 );

  return TRUE;
}


/* Create a new interface */
HRESULT DP_CreateInterface
         ( REFIID riid, LPVOID* ppvObj )
{
  TRACE( " for %s\n", debugstr_guid( riid ) );

  *ppvObj = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                       sizeof( IDirectPlay2Impl ) );

  if( *ppvObj == NULL )
  {
    return DPERR_OUTOFMEMORY;
  }

  if( IsEqualGUID( &IID_IDirectPlay2, riid ) )
  {
    IDirectPlay2Impl *This = *ppvObj;
    This->lpVtbl = &directPlay2WVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay2A, riid ) )
  {
    IDirectPlay2AImpl *This = *ppvObj;
    This->lpVtbl = &directPlay2AVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay3, riid ) )
  {
    IDirectPlay3Impl *This = *ppvObj;
    This->lpVtbl = &directPlay3WVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay3A, riid ) )
  {
    IDirectPlay3AImpl *This = *ppvObj;
    This->lpVtbl = &directPlay3AVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay4, riid ) )
  {
    IDirectPlay4Impl *This = *ppvObj;
    This->lpVtbl = &directPlay4WVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay4A, riid ) )
  {
    IDirectPlay4AImpl *This = *ppvObj;
    This->lpVtbl = &directPlay4AVT;
  }
  else
  {
    /* Unsupported interface */
    HeapFree( GetProcessHeap(), 0, *ppvObj );
    *ppvObj = NULL;

    return E_NOINTERFACE;
  }

  /* Initialize it */
  if ( DP_CreateIUnknown( *ppvObj ) &&
       DP_CreateDirectPlay2( *ppvObj ) &&
       DP_CreateDirectPlay3( *ppvObj ) &&
       DP_CreateDirectPlay4( *ppvObj )
     )
  {
    IDirectPlayX_AddRef( (LPDIRECTPLAY2A)*ppvObj );

    return S_OK;
  }

  /* Initialize failed, destroy it */
  DP_DestroyDirectPlay4( *ppvObj );
  DP_DestroyDirectPlay3( *ppvObj );
  DP_DestroyDirectPlay2( *ppvObj );
  DP_DestroyIUnknown( *ppvObj );

  HeapFree( GetProcessHeap(), 0, *ppvObj );

  *ppvObj = NULL;
  return DPERR_NOMEMORY;
}


/* Direct Play methods */

/* Shared between all dplay types */
static HRESULT WINAPI DP_QueryInterface
         ( LPDIRECTPLAY2 iface, REFIID riid, LPVOID* ppvObj )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  TRACE("(%p)->(%s,%p)\n", This, debugstr_guid( riid ), ppvObj );

  *ppvObj = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                       sizeof( *This ) );

  if( *ppvObj == NULL )
  {
    return DPERR_OUTOFMEMORY;
  }

  CopyMemory( *ppvObj, This, sizeof( *This )  );
  (*(IDirectPlay2Impl**)ppvObj)->ulInterfaceRef = 0;

  if( IsEqualGUID( &IID_IDirectPlay2, riid ) )
  {
    IDirectPlay2Impl *This = *ppvObj;
    This->lpVtbl = &directPlay2WVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay2A, riid ) )
  {
    IDirectPlay2AImpl *This = *ppvObj;
    This->lpVtbl = &directPlay2AVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay3, riid ) )
  {
    IDirectPlay3Impl *This = *ppvObj;
    This->lpVtbl = &directPlay3WVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay3A, riid ) )
  {
    IDirectPlay3AImpl *This = *ppvObj;
    This->lpVtbl = &directPlay3AVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay4, riid ) )
  {
    IDirectPlay4Impl *This = *ppvObj;
    This->lpVtbl = &directPlay4WVT;
  }
  else if( IsEqualGUID( &IID_IDirectPlay4A, riid ) )
  {
    IDirectPlay4AImpl *This = *ppvObj;
    This->lpVtbl = &directPlay4AVT;
  }
  else
  {
    /* Unsupported interface */
    HeapFree( GetProcessHeap(), 0, *ppvObj );
    *ppvObj = NULL;

    return E_NOINTERFACE;
  }

  IDirectPlayX_AddRef( (LPDIRECTPLAY2)*ppvObj );

  return S_OK;
}

/* Shared between all dplay types */
static ULONG WINAPI DP_AddRef
         ( LPDIRECTPLAY3 iface )
{
  ULONG ulInterfaceRefCount, ulObjRefCount;
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;

  ulObjRefCount       = InterlockedIncrement( &This->unk->ulObjRef );
  ulInterfaceRefCount = InterlockedIncrement( &This->ulInterfaceRef );

  TRACE( "ref count incremented to %u:%u for %p\n",
         ulInterfaceRefCount, ulObjRefCount, This );

  return ulObjRefCount;
}

static ULONG WINAPI DP_Release
( LPDIRECTPLAY3 iface )
{
  ULONG ulInterfaceRefCount, ulObjRefCount;

  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;

  ulObjRefCount       = InterlockedDecrement( &This->unk->ulObjRef );
  ulInterfaceRefCount = InterlockedDecrement( &This->ulInterfaceRef );

  TRACE( "ref count decremented to %u:%u for %p\n",
         ulInterfaceRefCount, ulObjRefCount, This );

  /* Deallocate if this is the last reference to the object */
  if( ulObjRefCount == 0 )
  {
     /* If we're destroying the object, this must be the last ref
        of the last interface */
     DP_DestroyDirectPlay4( This );
     DP_DestroyDirectPlay3( This );
     DP_DestroyDirectPlay2( This );
     DP_DestroyIUnknown( This );
  }

  /* Deallocate the interface */
  if( ulInterfaceRefCount == 0 )
  {
    HeapFree( GetProcessHeap(), 0, This );
  }

  return ulObjRefCount;
}

static inline DPID DP_NextObjectId(void)
{
  return (DPID)InterlockedIncrement( &kludgePlayerGroupId );
}

/* *lplpReply will be non NULL iff there is something to reply */
HRESULT DP_HandleMessage( IDirectPlay2Impl* This, LPCVOID lpcMessageBody,
                          DWORD  dwMessageBodySize, LPCVOID lpcMessageHeader,
                          WORD wCommandId, WORD wVersion,
                          LPVOID* lplpReply, LPDWORD lpdwMsgSize )
{
  TRACE( "(%p)->(%p,0x%08x,%p,0x%x,%u)\n",
         This, lpcMessageBody, dwMessageBodySize, lpcMessageHeader, wCommandId,
         wVersion );

  switch( wCommandId )
  {
    /* Name server needs to handle this request */
    case DPMSGCMD_ENUMSESSIONS:
    {
      /* Reply expected */
      NS_ReplyToEnumSessionsRequest( lpcMessageBody, lplpReply, lpdwMsgSize, This );

      break;
    }

    /* Name server needs to handle this request */
    case DPMSGCMD_ENUMSESSIONSREPLY:
    {
      /* No reply expected */
      NS_AddRemoteComputerAsNameServer( lpcMessageHeader,
                                        This->dp2->spData.dwSPHeaderSize,
                                        lpcMessageBody,
                                        This->dp2->lpNameServerData );
      break;
    }

    case DPMSGCMD_REQUESTPLAYERID:
    {
      LPDPSP_MSG_REQUESTPLAYERID lpcMsg = lpcMessageBody;
      LPDPSP_MSG_REQUESTPLAYERREPLY lpReply;

      *lpdwMsgSize = This->dp2->spData.dwSPHeaderSize + sizeof( *lpReply );

      *lplpReply = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, *lpdwMsgSize );

      lpReply = (LPDPSP_MSG_REQUESTPLAYERREPLY)( (LPBYTE)(*lplpReply) +
                                                 This->dp2->spData.dwSPHeaderSize );

      lpReply->envelope.dwMagic    = DPMSG_SIGNATURE;
      lpReply->envelope.wCommandId = DPMSGCMD_REQUESTPLAYERREPLY;
      lpReply->envelope.wVersion   = DX61AVERSION;

      if ( lpcMsg->Flags & DPLAYI_PLAYER_SYSPLAYER )
      {
        /* Request to join the game */

        if ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_SECURESERVER )
        {
          FIXME( "Fill lpReply->SecDesc with Game->SSPIProvider\n" );
        }
        FIXME( "Fill lpReply->CAPIProvider with Game->CAPIProvider\n" );
        FIXME( "Fill lpReply->SecDesc->dwEncryptionAlgorithm with Game->EncryptionAlgorithm\n" );

        /* Player is not local anymore */
        lpcMsg->Flags ^= DPLAYI_PLAYER_PLAYERLOCAL;

        lpReply->ID = DP_NextObjectId();
        lpReply->Result = DP_IF_CreatePlayer( This, lpcMessageHeader,
                                              &lpReply->ID, NULL, 0, NULL, 0,
                                              lpcMsg->Flags, TRUE/*TODO*/ );
        lpReply->Result = S_OK;
      }
      else
      {
        /* Request to to add a normal player from an
         * existing member of the session */

        if ( ( This->dp2->lpSessionDesc->dwCurrentPlayers
               == This->dp2->lpSessionDesc->dwMaxPlayers ) ||
             ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_NEWPLAYERSDISABLED ) )
        {
          lpReply->Result = DPERR_NONEWPLAYERS;
        }
        else
        {
          lpReply->ID = DP_NextObjectId();
          lpReply->Result = S_OK;
        }
      }

      break;
    }

    case DPMSGCMD_CREATEPLAYER:
    {
      LPDPSP_MSG_CREATEPLAYER lpcMsg = lpcMessageBody;
      LPDPLAYI_PACKEDPLAYER lpPackedPlayer =
        (LPDPLAYI_PACKEDPLAYER)(((LPBYTE) lpcMsg) + lpcMsg->CreateOffset);
      PACKEDPLAYERDATA packedPlayerData;

      DP_MSG_ParsePackedPlayer( This,
                                (LPBYTE) lpPackedPlayer,
                                &packedPlayerData,
                                FALSE,
                                TRUE/*TODO*/ );

      return DP_CreatePlayer( This, lpPackedPlayer->PlayerID,
                              &packedPlayerData.name,
                              lpPackedPlayer->Flags,
                              packedPlayerData.lpPlayerData,
                              packedPlayerData.dwPlayerDataSize,
                              NULL, TRUE/*TODO*/, NULL );

      /* TODO send msg to upper layer */
      break;
    }

    case DPMSGCMD_PACKET:
    {
      LPCDPSP_MSG_PACKET lpcMsg = lpcMessageBody;
      LPDPSP_MSG_ENVELOPE packetData;

      /* TODO: We have to wait for all the messages in the sequence and
       *       assemble them in a bigger message. */
      if ( lpcMsg->TotalPackets > 1 )
      {
        FIXME( "TODO: Message belongs to a sequence of %d, implement assembly\n",
               lpcMsg->TotalPackets );
        return DPERR_GENERIC;
      }

      /* For now, for simplicity we'll just decapsulate the embedded
       * message and work with it. */
      packetData = (LPVOID)(lpcMsg + 1);

      TRACE( "Processing embedded message with envelope (0x%08x, 0x%08x, %d)\n",
             packetData->dwMagic, packetData->wCommandId, packetData->wVersion );

      return DP_HandleMessage( This,
                               packetData,
                               lpcMsg->DataSize,
                               lpcMessageHeader,
                               packetData->wCommandId,
                               packetData->wVersion,
                               lplpReply, lpdwMsgSize );
      break;
    }

    case DPMSGCMD_REQUESTPLAYERREPLY:
    {
      DP_MSG_ReplyReceived( This, wCommandId, lpcMessageHeader,
                            lpcMessageBody, dwMessageBodySize );
      break;
    }

    case DPMSGCMD_ADDFORWARDREQUEST:
    {
      /*LPCDPSP_MSG_ADDFORWARDREQUEST lpcMsg =
        (LPCDPSP_MSG_ADDFORWARDREQUEST) lpcMessageBody;*/

      /* TODO: Send ADDFORWARD messages to all the players to populate
       *       their name tables.
       *       Start NametablePopulation timer and wait for ADDFORWARDACKs */
      FIXME( "Spread name table population messages\n" );

      /* TODO remember to set local addr somewhere */
      /*      call NS_SetLocalAddr with a SOCKADDR_IN */

      FIXME( "This should happen after we received all the DPMSGCMD_ADDFORWARDACKs\n" );
      DP_MSG_ReplyToEnumPlayersRequest( This, lplpReply, lpdwMsgSize );

      break;
    }

    case DPMSGCMD_ADDFORWARDREPLY:
    {
      DP_MSG_ReplyReceived( This, wCommandId, lpcMessageHeader,
                            lpcMessageBody, dwMessageBodySize );
      break;
    }

    case DPMSGCMD_ENUMPLAYERSREPLY:
    case DPMSGCMD_SUPERENUMPLAYERSREPLY:
    {
      /* If we're joining a session, when we receive this
       * command we were waiting for a ADDFORWARDREPLY */
      if ( !DP_MSG_ReplyReceived( This, DPMSGCMD_ADDFORWARDREPLY,
                                  lpcMessageHeader, lpcMessageBody,
                                  dwMessageBodySize ) )
      {
        /* If we were not joining a session, check if we are
         * waiting for an enumeration of players or groups */
        DP_MSG_ReplyReceived( This, wCommandId, lpcMessageHeader,
                              lpcMessageBody, dwMessageBodySize );
      }
      break;
    }

    case DPMSGCMD_ADDFORWARDACK:
    {
      /* When we receive an ADDFORWARDACK for each of the ADDFORWARDs
       * we've sent, send a SUPERENUMPLAYERSREPLY back to the peer
       * that sent the ADDFORWARDREQUEST */
      /* TODO: We'll skip this for now and just send the SUPERENUMPLAYERSREPLY
       *       right away when we get a ADDFORWARDREQUEST */
      break;
    }

    default:
    {
      FIXME( "Unknown wCommandId 0x%08x. Ignoring message\n", wCommandId );
      return DPERR_GENERIC;
    }
  }

  return DP_OK;
}


static HRESULT DP_IF_AddPlayerToGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup,
            DPID idPlayer, BOOL bAnsi )
{
  lpGroupData  lpGData;
  lpPlayerList lpPList;
  lpPlayerList lpNewPList;

  TRACE( "(%p)->(%p,0x%08x,0x%08x,%u)\n",
         This, lpMsgHdr, idGroup, idPlayer, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  /* Find the group */
  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Find the player */
  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  /* Create a player list (ie "shortcut" ) */
  lpNewPList = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpNewPList ) );
  if( lpNewPList == NULL )
  {
    return DPERR_CANTADDPLAYER;
  }

  /* Add the shortcut */
  lpPList->lpPData->uRef++;
  lpNewPList->lpPData = lpPList->lpPData;

  /* Add the player to the list of players for this group */
  DPQ_INSERT(lpGData->players,lpNewPList,players);

  /* Let the SP know that we've added a player to the group */
  if( This->dp2->spData.lpCB->AddPlayerToGroup )
  {
    DPSP_ADDPLAYERTOGROUPDATA data;

    TRACE( "Calling SP AddPlayerToGroup\n" );

    data.idPlayer = idPlayer;
    data.idGroup  = idGroup;
    data.lpISP    = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->AddPlayerToGroup)( &data );
  }

  /* Inform all other peers of the addition of player to the group. If there are
   * no peers keep this event quiet.
   * Also, if this event was the result of another machine sending it to us,
   * don't bother rebroadcasting it.
   */
  if( ( lpMsgHdr == NULL ) &&
      This->dp2->lpSessionDesc &&
      ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_MULTICASTSERVER ) )
  {
    DPMSG_ADDPLAYERTOGROUP msg;
    msg.dwType = DPSYS_ADDPLAYERTOGROUP;

    msg.dpIdGroup  = idGroup;
    msg.dpIdPlayer = idPlayer;

    /* FIXME: Correct to just use send effectively? */
    /* FIXME: Should size include data w/ message or just message "header" */
    /* FIXME: Check return code */
    DP_SendEx( This, DPID_SERVERPLAYER, DPID_ALLPLAYERS, 0, &msg, sizeof( msg ),               0, 0, NULL, NULL, bAnsi );
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_AddPlayerToGroup
          ( LPDIRECTPLAY2A iface, DPID idGroup, DPID idPlayer )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_AddPlayerToGroup( This, NULL, idGroup, idPlayer, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_AddPlayerToGroup
          ( LPDIRECTPLAY2 iface, DPID idGroup, DPID idPlayer )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_AddPlayerToGroup( This, NULL, idGroup, idPlayer, FALSE );
}

static HRESULT DP_IF_Close( IDirectPlay2Impl* This, BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  TRACE("(%p)->(%u)\n", This, bAnsi );

  /* FIXME: Need to find a new host I assume (how?) */
  /* FIXME: Need to destroy all local groups */
  /* FIXME: Need to migrate all remotely visible players to the new host */

  /* Invoke the SP callback to inform of session close */
  if( This->dp2->spData.lpCB->CloseEx )
  {
    DPSP_CLOSEDATA data;

    TRACE( "Calling SP CloseEx\n" );

    data.lpISP = This->dp2->spData.lpISP;

    hr = (*This->dp2->spData.lpCB->CloseEx)( &data );

  }
  else if ( This->dp2->spData.lpCB->Close ) /* Try obsolete version */
  {
    TRACE( "Calling SP Close (obsolete interface)\n" );

    hr = (*This->dp2->spData.lpCB->Close)();
  }

  if ( !FAILED(hr) )
  {
    This->dp2->bConnectionOpen = FALSE;
  }

  return hr;
}

static HRESULT WINAPI DirectPlay2AImpl_Close
          ( LPDIRECTPLAY2A iface )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_Close( This, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_Close
          ( LPDIRECTPLAY2 iface )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_Close( This, FALSE );
}

static
lpGroupData DP_CreateGroup( IDirectPlay2AImpl* This, const DPID *lpid,
                            const DPNAME *lpName, DWORD dwFlags,
                            DPID idParent, BOOL bAnsi )
{
  lpGroupData lpGData;

  /* Allocate the new space and add to end of high level group list */
  lpGData = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpGData ) );

  if( lpGData == NULL )
  {
    return NULL;
  }

  DPQ_INIT(lpGData->groups);
  DPQ_INIT(lpGData->players);

  /* Set the desired player ID - no sanity checking to see if it exists */
  lpGData->dpid = *lpid;

  DP_CopyDPNAMEStruct( &lpGData->name, lpName, bAnsi );

  /* FIXME: Should we check that the parent exists? */
  lpGData->parent  = idParent;

  /* FIXME: Should we validate the dwFlags? */
  lpGData->dwFlags = dwFlags;

  TRACE( "Created group id 0x%08x\n", *lpid );

  return lpGData;
}

/* This method assumes that all links to it are already deleted */
static void
DP_DeleteGroup( IDirectPlay2Impl* This, DPID dpid )
{
  lpGroupList lpGList;

  TRACE( "(%p)->(0x%08x)\n", This, dpid );

  DPQ_REMOVE_ENTRY( This->dp2->lpSysGroup->groups, groups, lpGData->dpid, ==, dpid, lpGList );

  if( lpGList == NULL )
  {
    ERR( "DPID 0x%08x not found\n", dpid );
    return;
  }

  if( --(lpGList->lpGData->uRef) )
  {
    FIXME( "Why is this not the last reference to group?\n" );
    DebugBreak();
  }

  /* Delete player */
  DP_DeleteDPNameStruct( &lpGList->lpGData->name );
  HeapFree( GetProcessHeap(), 0, lpGList->lpGData );

  /* Remove and Delete Player List object */
  HeapFree( GetProcessHeap(), 0, lpGList );

}

static lpGroupData DP_FindAnyGroup( IDirectPlay2AImpl* This, DPID dpid )
{
  lpGroupList lpGroups;

  TRACE( "(%p)->(0x%08x)\n", This, dpid );

  if( dpid == DPID_SYSTEM_GROUP )
  {
    return This->dp2->lpSysGroup;
  }
  else
  {
    DPQ_FIND_ENTRY( This->dp2->lpSysGroup->groups, groups, lpGData->dpid, ==, dpid, lpGroups );
  }

  if( lpGroups == NULL )
  {
    return NULL;
  }

  return lpGroups->lpGData;
}

static HRESULT DP_IF_CreateGroup
          ( IDirectPlay2AImpl* This, LPVOID lpMsgHdr, LPDPID lpidGroup,
            LPDPNAME lpGroupName, LPVOID lpData, DWORD dwDataSize,
            DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData lpGData;

  TRACE( "(%p)->(%p,%p,%p,%p,0x%08x,0x%08x,%u)\n",
         This, lpMsgHdr, lpidGroup, lpGroupName, lpData, dwDataSize,
         dwFlags, bAnsi );

  /* If the name is not specified, we must provide one */
  if( DPID_UNKNOWN == *lpidGroup )
  {
    /* If we are the name server, we decide on the group ids. If not, we
     * must ask for one before attempting a creation.
     */
    if( This->dp2->bHostInterface )
    {
      *lpidGroup = DP_NextObjectId();
    }
    else
    {
      *lpidGroup = DP_GetRemoteNextObjectId();
    }
  }

  lpGData = DP_CreateGroup( This, lpidGroup, lpGroupName, dwFlags,
                            DPID_NOPARENT_GROUP, bAnsi );

  if( lpGData == NULL )
  {
    return DPERR_CANTADDPLAYER; /* yes player not group */
  }

  if( DPID_SYSTEM_GROUP == *lpidGroup )
  {
    This->dp2->lpSysGroup = lpGData;
    TRACE( "Inserting system group\n" );
  }
  else
  {
    /* Insert into the system group */
    lpGroupList lpGroup = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpGroup ) );
    lpGroup->lpGData = lpGData;

    DPQ_INSERT( This->dp2->lpSysGroup->groups, lpGroup, groups );
  }

  /* Something is now referencing this data */
  lpGData->uRef++;

  /* Set all the important stuff for the group */
  DP_SetGroupData( lpGData, DPSET_REMOTE, lpData, dwDataSize );

  /* FIXME: We should only create the system group if GetCaps returns
   *        DPCAPS_GROUPOPTIMIZED.
   */

  /* Let the SP know that we've created this group */
  if( This->dp2->spData.lpCB->CreateGroup )
  {
    DPSP_CREATEGROUPDATA data;
    DWORD dwCreateFlags = 0;

    TRACE( "Calling SP CreateGroup\n" );

    if( *lpidGroup == DPID_NOPARENT_GROUP )
      dwCreateFlags |= DPLAYI_GROUP_SYSGROUP;

    if( lpMsgHdr == NULL )
      dwCreateFlags |= DPLAYI_PLAYER_PLAYERLOCAL;

    if( dwFlags & DPGROUP_HIDDEN )
      dwCreateFlags |= DPLAYI_GROUP_HIDDEN;

    data.idGroup           = *lpidGroup;
    data.dwFlags           = dwCreateFlags;
    data.lpSPMessageHeader = lpMsgHdr;
    data.lpISP             = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->CreateGroup)( &data );
  }

  /* Inform all other peers of the creation of a new group. If there are
   * no peers keep this event quiet.
   * Also if this message was sent to us, don't rebroadcast.
   */
  if( ( lpMsgHdr == NULL ) &&
      This->dp2->lpSessionDesc &&
      ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_MULTICASTSERVER ) )
  {
    DPMSG_CREATEPLAYERORGROUP msg;
    msg.dwType = DPSYS_CREATEPLAYERORGROUP;

    msg.dwPlayerType     = DPPLAYERTYPE_GROUP;
    msg.dpId             = *lpidGroup;
    msg.dwCurrentPlayers = 0; /* FIXME: Incorrect? */
    msg.lpData           = lpData;
    msg.dwDataSize       = dwDataSize;
    msg.dpnName          = *lpGroupName;
    msg.dpIdParent       = DPID_NOPARENT_GROUP;
    msg.dwFlags          = DPMSG_CREATEGROUP_DWFLAGS( dwFlags );

    /* FIXME: Correct to just use send effectively? */
    /* FIXME: Should size include data w/ message or just message "header" */
    /* FIXME: Check return code */
    DP_SendEx( This, DPID_SERVERPLAYER, DPID_ALLPLAYERS, 0, &msg, sizeof( msg ),
               0, 0, NULL, NULL, bAnsi );
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_CreateGroup
          ( LPDIRECTPLAY2A iface, LPDPID lpidGroup, LPDPNAME lpGroupName,
            LPVOID lpData, DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( lpidGroup == NULL ||
      !This->dp2->bConnectionOpen ||
      dwDataSize >= MAXDWORD ||
      ( lpData == NULL && dwDataSize != 0 ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  *lpidGroup = DPID_UNKNOWN;

  return DP_IF_CreateGroup( This, NULL, lpidGroup,
                            lpGroupName, lpData, dwDataSize, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_CreateGroup
          ( LPDIRECTPLAY2 iface, LPDPID lpidGroup, LPDPNAME lpGroupName,
            LPVOID lpData, DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( lpidGroup == NULL ||
      !This->dp2->bConnectionOpen ||
      dwDataSize >= MAXDWORD ||
      ( lpData == NULL && dwDataSize != 0 ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  *lpidGroup = DPID_UNKNOWN;

  return DP_IF_CreateGroup( This, NULL, lpidGroup,
                            lpGroupName, lpData, dwDataSize, dwFlags, FALSE );
}


static void
DP_SetGroupData( lpGroupData lpGData, DWORD dwFlags,
                 LPVOID lpData, DWORD dwDataSize )
{
  /* Clear out the data with this player */
  if( dwFlags & DPSET_LOCAL )
  {
    if ( lpGData->dwLocalDataSize != 0 )
    {
      HeapFree( GetProcessHeap(), 0, lpGData->lpLocalData );
      lpGData->lpLocalData = NULL;
      lpGData->dwLocalDataSize = 0;
    }
  }
  else
  {
    if( lpGData->dwRemoteDataSize != 0 )
    {
      HeapFree( GetProcessHeap(), 0, lpGData->lpRemoteData );
      lpGData->lpRemoteData = NULL;
      lpGData->dwRemoteDataSize = 0;
    }
  }

  /* Reallocate for new data */
  if( lpData != NULL )
  {
    LPVOID lpNewData = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                  sizeof( dwDataSize ) );
    CopyMemory( lpNewData, lpData, dwDataSize );

    if( dwFlags & DPSET_LOCAL )
    {
      lpGData->lpLocalData     = lpData;
      lpGData->dwLocalDataSize = dwDataSize;
    }
    else
    {
      lpGData->lpRemoteData     = lpNewData;
      lpGData->dwRemoteDataSize = dwDataSize;
    }
  }

}


HRESULT DP_CreatePlayer( IDirectPlay2Impl* This, DPID idPlayer,
                         LPDPNAME lpName, DWORD dwFlags,
                         LPVOID lpData, DWORD dwDataSize,
                         HANDLE hEvent, BOOL bAnsi,
                         lpPlayerData* lplpPlayer )
{
  /* Create the storage for a new player and insert
   * it in the list of players. */

  lpPlayerList lpPList = NULL;
  lpPlayerData lpPlayer;
  HRESULT hr;

  TRACE( "(%p)->(0x%08x,%p,0x%08x,%p,%d,%p,%u,%p)\n",
         This, idPlayer, lpName, dwFlags, lpData,
         dwDataSize, hEvent, bAnsi, lplpPlayer );

  /* Verify that we don't already have this player */
  DPQ_FIND_ENTRY( This->dp2->lpSysGroup->players, players,
                  lpPData->dpid, ==, idPlayer, lpPList );
  if ( lpPList != NULL )
  {
    hr = DPERR_CANTCREATEPLAYER;
    goto end;
  }

  /* Allocate the storage for the player and associate it with list element */
  lpPlayer = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpPlayer ) );
  if( lpPlayer == NULL )
  {
    hr = DPERR_CANTCREATEPLAYER;
    goto end;
  }

  if ( lplpPlayer != NULL )
  {
    *lplpPlayer = lpPlayer;
  }

  lpPlayer->dpid = idPlayer;
  lpPlayer->dwFlags = dwFlags;

  DP_CopyDPNAMEStruct( &lpPlayer->name, lpName, bAnsi );

  /* If we were given an event handle, duplicate it */
  if( hEvent != 0 )
  {
    if( !DuplicateHandle( GetCurrentProcess(), hEvent,
                          GetCurrentProcess(), &lpPlayer->hEvent,
                          0, FALSE, DUPLICATE_SAME_ACCESS ) )
    {
      ERR( "Can't duplicate player msg handle %p\n", hEvent );
      hr = DPERR_CANTCREATEPLAYER;
      goto end;
    }
  }

  /* Set player data */
  if ( lpData != NULL )
  {
    DP_SetPlayerData( lpPlayer, DPSET_REMOTE, lpData, dwDataSize );
  }

  /* Initialize the SP data section */
  lpPlayer->lpSPPlayerData = DPSP_CreateSPPlayerData();

  if( ~dwFlags & DPLAYI_PLAYER_SYSPLAYER )
  {
    This->dp2->lpSessionDesc->dwCurrentPlayers++;
  }

  /* Create the list object and link it in */
  lpPList = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpPList ) );
  if( lpPList == NULL )
  {
    hr = DPERR_CANTADDPLAYER;
    goto end;
  }

  lpPlayer->uRef = 1;
  lpPList->lpPData = lpPlayer;

  /* Add the player to the system group */
  DPQ_INSERT( This->dp2->lpSysGroup->players, lpPList, players );

  /* Quick hack */
  if ( ~dwFlags & DPLAYI_PLAYER_PLAYERLOCAL ) {
    lpPlayerList lpPList;
    LPDPMSG lpMsg =
      HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct DPMSG) );
    LPDPMSG_CREATEPLAYERORGROUP msg =
      HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DPMSG_CREATEPLAYERORGROUP) );

    msg->dwType            = DPSYS_CREATEPLAYERORGROUP;
    msg->dwPlayerType      = DPPLAYERTYPE_PLAYER;
    msg->dpId              = idPlayer;
    msg->dwCurrentPlayers  = This->dp2->lpSessionDesc->dwCurrentPlayers;
    msg->lpData            = NULL;/*TODO*/
    msg->dwDataSize        = 0;/*TODO*/
    msg->dpIdParent        = DPID_NOPARENT_GROUP;
    msg->dwFlags           = DPMSG_CREATEPLAYER_DWFLAGS( dwFlags );
    if ( lpName )
    {
      msg->dpnName         = *lpName;
    }

    lpMsg->msg = (LPDPMSG_GENERIC) msg;
    lpMsg->dwMsgSize = sizeof(LPDPMSG_CREATEPLAYERORGROUP);
    DPQ_INSERT( This->dp2->receiveMsgs, lpMsg, msgs );

    if ( (lpPList = DPQ_FIRST( This->dp2->lpSysGroup->players )) )
    {
      do
      {
        if ( ( lpPList->lpPData->dwFlags & DPLAYI_PLAYER_PLAYERLOCAL ) &&
             lpPList->lpPData->hEvent )
        {
          SetEvent( lpPList->lpPData->hEvent );
        }
      }
      while( (lpPList = DPQ_NEXT( lpPList->players  )) );
    }
  }

end:
  if ( FAILED(hr) )
  {
    DP_DeletePlayer( This, idPlayer );
  }
  return hr;
}

/* Delete the contents of the DPNAME struct */
static void
DP_DeleteDPNameStruct( LPDPNAME lpDPName )
{
  HeapFree( GetProcessHeap(), HEAP_ZERO_MEMORY, lpDPName->u1.lpszShortNameA );
  HeapFree( GetProcessHeap(), HEAP_ZERO_MEMORY, lpDPName->u2.lpszLongNameA );
}

/* This method assumes that all links to it are already deleted */
static void
DP_DeletePlayer( IDirectPlay2Impl* This, DPID dpid )
{
  lpPlayerList lpPList;

  TRACE( "(%p)->(0x%08x)\n", This, dpid );

  DPQ_REMOVE_ENTRY( This->dp2->lpSysGroup->players, players, lpPData->dpid, ==, dpid, lpPList );

  if( lpPList == NULL )
  {
    ERR( "DPID 0x%08x not found\n", dpid );
    return;
  }

  /* Verify that this is the last reference to the data */
  if( --(lpPList->lpPData->uRef) )
  {
    FIXME( "Why is this not the last reference to player?\n" );
    DebugBreak();
  }

  /* Delete player */
  DP_DeleteDPNameStruct( &lpPList->lpPData->name );

  CloseHandle( lpPList->lpPData->hEvent );
  HeapFree( GetProcessHeap(), 0, lpPList->lpPData );

  /* Delete Player List object */
  HeapFree( GetProcessHeap(), 0, lpPList );
}

lpPlayerList DP_FindPlayer( IDirectPlay2AImpl* This, DPID dpid )
{
  lpPlayerList lpPlayers;

  TRACE( "(%p)->(0x%08x)\n", This, dpid );

  if(This->dp2->lpSysGroup == NULL)
    return NULL;

  DPQ_FIND_ENTRY( This->dp2->lpSysGroup->players, players, lpPData->dpid, ==, dpid, lpPlayers );

  return lpPlayers;
}

/* Basic area for Dst must already be allocated */
static BOOL DP_CopyDPNAMEStruct( LPDPNAME lpDst, const DPNAME *lpSrc, BOOL bAnsi )
{
  if( lpSrc == NULL )
  {
    ZeroMemory( lpDst, sizeof( *lpDst ) );
    lpDst->dwSize = sizeof( *lpDst );
    return TRUE;
  }

  if( lpSrc->dwSize != sizeof( *lpSrc) )
  {
    return FALSE;
  }

  /* Delete any existing pointers */
  HeapFree( GetProcessHeap(), 0, lpDst->u1.lpszShortNameA );
  HeapFree( GetProcessHeap(), 0, lpDst->u2.lpszLongNameA );

  /* Copy as required */
  CopyMemory( lpDst, lpSrc, lpSrc->dwSize );

  if( bAnsi )
  {
    if( lpSrc->u1.lpszShortNameA )
    {
        lpDst->u1.lpszShortNameA = HeapAlloc( GetProcessHeap(), 0,
                                             strlen(lpSrc->u1.lpszShortNameA)+1 );
        strcpy( lpDst->u1.lpszShortNameA, lpSrc->u1.lpszShortNameA );
    }
    else
    {
        lpDst->u1.lpszShortNameA = NULL;
    }
    if( lpSrc->u2.lpszLongNameA )
    {
        lpDst->u2.lpszLongNameA = HeapAlloc( GetProcessHeap(), 0,
                                              strlen(lpSrc->u2.lpszLongNameA)+1 );
        strcpy( lpDst->u2.lpszLongNameA, lpSrc->u2.lpszLongNameA );
    }
    else
    {
        lpDst->u2.lpszLongNameA = NULL;
    }
  }
  else
  {
    if( lpSrc->u1.lpszShortNameA )
    {
        lpDst->u1.lpszShortName = HeapAlloc( GetProcessHeap(), 0,
                                              (strlenW(lpSrc->u1.lpszShortName)+1)*sizeof(WCHAR) );
        strcpyW( lpDst->u1.lpszShortName, lpSrc->u1.lpszShortName );
    }
    else
    {
        lpDst->u1.lpszShortNameA = NULL;
    }
    if( lpSrc->u2.lpszLongNameA )
    {
        lpDst->u2.lpszLongName = HeapAlloc( GetProcessHeap(), 0,
                                             (strlenW(lpSrc->u2.lpszLongName)+1)*sizeof(WCHAR) );
        strcpyW( lpDst->u2.lpszLongName, lpSrc->u2.lpszLongName );
    }
    else
    {
        lpDst->u2.lpszLongNameA = NULL;
    }
  }

  return TRUE;
}

static void
DP_SetPlayerData( lpPlayerData lpPData, DWORD dwFlags,
                  LPVOID lpData, DWORD dwDataSize )
{
  /* Clear out the data with this player */
  if( dwFlags & DPSET_LOCAL )
  {
    if ( lpPData->dwLocalDataSize != 0 )
    {
      HeapFree( GetProcessHeap(), 0, lpPData->lpLocalData );
      lpPData->lpLocalData = NULL;
      lpPData->dwLocalDataSize = 0;
    }
  }
  else
  {
    if( lpPData->dwRemoteDataSize != 0 )
    {
      HeapFree( GetProcessHeap(), 0, lpPData->lpRemoteData );
      lpPData->lpRemoteData = NULL;
      lpPData->dwRemoteDataSize = 0;
    }
  }

  /* Reallocate for new data */
  if( lpData != NULL )
  {
    LPVOID lpNewData = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                  sizeof( dwDataSize ) );
    CopyMemory( lpNewData, lpData, dwDataSize );

    if( dwFlags & DPSET_LOCAL )
    {
      lpPData->lpLocalData     = lpData;
      lpPData->dwLocalDataSize = dwDataSize;
    }
    else
    {
      lpPData->lpRemoteData     = lpNewData;
      lpPData->dwRemoteDataSize = dwDataSize;
    }
  }

}

static HRESULT DP_IF_CreatePlayer
( IDirectPlay2Impl* This,
  LPVOID lpMsgHdr, /* NULL for local creation, non NULL for remote creation */
  LPDPID lpidPlayer,
  LPDPNAME lpPlayerName,
  HANDLE hEvent,
  LPVOID lpData,
  DWORD dwDataSize,
  DWORD dwFlags,
  BOOL bAnsi )
{
  HRESULT hr = DP_OK;
  lpPlayerData lpPData;

  TRACE( "(%p)->(%p,%p,%p,%p,0x%08x,0x%08x,%u)\n",
         This, lpidPlayer, lpPlayerName, hEvent, lpData,
         dwDataSize, dwFlags, bAnsi );

  if( dwFlags == 0 )
  {
    dwFlags = DPPLAYER_SPECTATOR;
  }

  if( lpidPlayer == NULL )
  {
    return DPERR_INVALIDPARAMS;
  }


  /* Determine the creation flags for the player. These will be passed
   * to the name server if requesting a player id and to the SP when
   * informing it of the player creation
   */
  {
    if( dwFlags & DPPLAYER_SERVERPLAYER )
    {
      if( *lpidPlayer == DPID_SERVERPLAYER )
      {
        /* Server player for the host interface */
        dwFlags |= DPLAYI_PLAYER_APPSERVER;
      }
      else if( *lpidPlayer == DPID_NAME_SERVER )
      {
        /* Name server - master of everything */
        dwFlags |= (DPLAYI_PLAYER_NAMESRVR|DPLAYI_PLAYER_SYSPLAYER);
      }
      else
      {
        /* Server player for a non host interface */
        dwFlags |= DPLAYI_PLAYER_SYSPLAYER;
      }
    }

    if( lpMsgHdr == NULL )
      dwFlags |= DPLAYI_PLAYER_PLAYERLOCAL;
  }

  /* If the name is not specified, we must provide one */
  if( *lpidPlayer == DPID_UNKNOWN )
  {
    /* If we are the session master, we dish out the group/player ids */
    if( This->dp2->bHostInterface )
    {
      *lpidPlayer = DP_NextObjectId();
    }
    else
    {
      hr = DP_MSG_SendRequestPlayerId( This, dwFlags & 0x000000FF, lpidPlayer );

      if( FAILED(hr) )
      {
        ERR( "Request for ID failed: %s\n", DPLAYX_HresultToString( hr ) );
        return hr;
      }
    }
  }

  hr = DP_CreatePlayer( This, *lpidPlayer, lpPlayerName, dwFlags,
                        lpData, dwDataSize, hEvent, bAnsi,
                        &lpPData );
  if( FAILED(hr) )
  {
    return hr;
  }

  /* Let the SP know that we've created this player */
  if( This->dp2->spData.lpCB->CreatePlayer )
  {
    DPSP_CREATEPLAYERDATA data;

    data.idPlayer          = *lpidPlayer;
    data.dwFlags           = dwFlags;
    data.lpSPMessageHeader = lpMsgHdr;
    data.lpISP             = This->dp2->spData.lpISP;

    TRACE( "Calling SP CreatePlayer 0x%08x: dwFlags: 0x%08x lpMsgHdr: %p\n",
           *lpidPlayer, data.dwFlags, data.lpSPMessageHeader );

    hr = (*This->dp2->spData.lpCB->CreatePlayer)( &data );
  }

  if( FAILED(hr) )
  {
    ERR( "Failed to create player with sp: %s\n", DPLAYX_HresultToString(hr) );
    return hr;
  }

  /* Now let the SP know that this player is a member of the system group */
  if( This->dp2->spData.lpCB->AddPlayerToGroup )
  {
    DPSP_ADDPLAYERTOGROUPDATA data;

    data.idPlayer = *lpidPlayer;
    data.idGroup  = DPID_SYSTEM_GROUP;
    data.lpISP    = This->dp2->spData.lpISP;

    TRACE( "Calling SP AddPlayerToGroup (sys group)\n" );

    hr = (*This->dp2->spData.lpCB->AddPlayerToGroup)( &data );
  }

  if( FAILED(hr) )
  {
    ERR( "Failed to add player to sys group with sp: %s\n",
         DPLAYX_HresultToString(hr) );
    return hr;
  }


  if ( ( ! This->dp2->bHostInterface ) && ( lpMsgHdr == NULL ) )
  {
     if ( dwFlags & DPLAYI_PLAYER_SYSPLAYER )
     {
       /* Let the name server know about the creation of this player,
        * and  reeceive the name table */
       hr = DP_MSG_ForwardPlayerCreation( This, *lpidPlayer);
     }
     else
     {
       /* Inform all other peers of the creation of a new player.
        * Also, if this was a remote event, no need to rebroadcast it. */
       hr = DP_MSG_SendCreatePlayer( This, lpPData );
     }
  }

  return hr;
}

static HRESULT WINAPI DirectPlay2AImpl_CreatePlayer
          ( LPDIRECTPLAY2A iface, LPDPID lpidPlayer, LPDPNAME lpPlayerName,
            HANDLE hEvent, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( lpidPlayer == NULL || !This->dp2->bConnectionOpen )
  {
    return DPERR_INVALIDPARAMS;
  }

  if ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_NEWPLAYERSDISABLED )
  {
    return DPERR_CANTCREATEPLAYER;
  }

  if( dwFlags & DPPLAYER_SERVERPLAYER )
  {
    *lpidPlayer = DPID_SERVERPLAYER;
  }
  else
  {
    *lpidPlayer = DPID_UNKNOWN;
  }

  return DP_IF_CreatePlayer( This, NULL, lpidPlayer, lpPlayerName, hEvent,
                           lpData, dwDataSize, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_CreatePlayer
          ( LPDIRECTPLAY2 iface, LPDPID lpidPlayer, LPDPNAME lpPlayerName,
            HANDLE hEvent, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( lpidPlayer == NULL || !This->dp2->bConnectionOpen )
  {
    return DPERR_INVALIDPARAMS;
  }

  if ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_NEWPLAYERSDISABLED )
  {
    return DPERR_CANTCREATEPLAYER;
  }

  if( dwFlags & DPPLAYER_SERVERPLAYER )
  {
    *lpidPlayer = DPID_SERVERPLAYER;
  }
  else
  {
    *lpidPlayer = DPID_UNKNOWN;
  }

  return DP_IF_CreatePlayer( This, NULL, lpidPlayer, lpPlayerName, hEvent,
                           lpData, dwDataSize, dwFlags, FALSE );
}

static DPID DP_GetRemoteNextObjectId(void)
{
  FIXME( ":stub\n" );

  /* Hack solution */
  return DP_NextObjectId();
}

static HRESULT DP_IF_DeletePlayerFromGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup,
            DPID idPlayer, BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  lpGroupData  lpGData;
  lpPlayerList lpPList;

  TRACE( "(%p)->(%p,0x%08x,0x%08x,%u)\n",
         This, lpMsgHdr, idGroup, idPlayer, bAnsi );

  /* Find the group */
  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Find the player */
  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  /* Remove the player shortcut from the group */
  DPQ_REMOVE_ENTRY( lpGData->players, players, lpPData->dpid, ==, idPlayer, lpPList );

  if( lpPList == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  /* One less reference */
  lpPList->lpPData->uRef--;

  /* Delete the Player List element */
  HeapFree( GetProcessHeap(), 0, lpPList );

  /* Inform the SP if they care */
  if( This->dp2->spData.lpCB->RemovePlayerFromGroup )
  {
    DPSP_REMOVEPLAYERFROMGROUPDATA data;

    TRACE( "Calling SP RemovePlayerFromGroup\n" );

    data.idPlayer = idPlayer;
    data.idGroup  = idGroup;
    data.lpISP    = This->dp2->spData.lpISP;

    hr = (*This->dp2->spData.lpCB->RemovePlayerFromGroup)( &data );
  }

  /* Need to send a DELETEPLAYERFROMGROUP message */
  FIXME( "Need to send a message\n" );

  return hr;
}

static HRESULT WINAPI DirectPlay2AImpl_DeletePlayerFromGroup
          ( LPDIRECTPLAY2A iface, DPID idGroup, DPID idPlayer )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_DeletePlayerFromGroup( This, NULL, idGroup, idPlayer, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_DeletePlayerFromGroup
          ( LPDIRECTPLAY2 iface, DPID idGroup, DPID idPlayer )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_DeletePlayerFromGroup( This, NULL, idGroup, idPlayer, FALSE );
}

typedef struct _DPRGOPContext
{
  IDirectPlay3Impl* This;
  BOOL              bAnsi;
  DPID              idGroup;
} DPRGOPContext, *lpDPRGOPContext;

static BOOL CALLBACK
cbRemoveGroupOrPlayer(
    DPID            dpId,
    DWORD           dwPlayerType,
    LPCDPNAME       lpName,
    DWORD           dwFlags,
    LPVOID          lpContext )
{
  lpDPRGOPContext lpCtxt = (lpDPRGOPContext)lpContext;

  TRACE( "Removing element:0x%08x (type:0x%08x) from element:0x%08x\n",
           dpId, dwPlayerType, lpCtxt->idGroup );

  if( dwPlayerType == DPPLAYERTYPE_GROUP )
  {
    if( FAILED( DP_IF_DeleteGroupFromGroup( lpCtxt->This, lpCtxt->idGroup,
                                            dpId )
              )
      )
    {
      ERR( "Unable to delete group 0x%08x from group 0x%08x\n",
             dpId, lpCtxt->idGroup );
    }
  }
  else
  {
    if( FAILED( DP_IF_DeletePlayerFromGroup( (IDirectPlay2Impl*)lpCtxt->This,
                                             NULL, lpCtxt->idGroup,
                                             dpId, lpCtxt->bAnsi )
              )
      )
    {
      ERR( "Unable to delete player 0x%08x from grp 0x%08x\n",
             dpId, lpCtxt->idGroup );
    }
  }

  return TRUE; /* Continue enumeration */
}

static HRESULT DP_IF_DestroyGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup, BOOL bAnsi )
{
  lpGroupData lpGData;
  DPRGOPContext context;

  FIXME( "(%p)->(%p,0x%08x,%u): semi stub\n",
         This, lpMsgHdr, idGroup, bAnsi );

  /* Find the group */
  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER; /* yes player */
  }

  context.This    = (IDirectPlay3Impl*)This;
  context.bAnsi   = bAnsi;
  context.idGroup = idGroup;

  /* Remove all players that this group has */
  DP_IF_EnumGroupPlayers( This, idGroup, NULL,
                          cbRemoveGroupOrPlayer, &context, 0, bAnsi );

  /* Remove all links to groups that this group has since this is dp3 */
  DP_IF_EnumGroupsInGroup( (IDirectPlay3Impl*)This, idGroup, NULL,
                           cbRemoveGroupOrPlayer, (LPVOID)&context, 0, bAnsi );

  /* Remove this group from the parent group - if it has one */
  if( ( idGroup != DPID_SYSTEM_GROUP ) &&
      ( lpGData->parent != DPID_SYSTEM_GROUP )
    )
  {
    DP_IF_DeleteGroupFromGroup( (IDirectPlay3Impl*)This, lpGData->parent,
                                idGroup );
  }

  /* Now delete this group data and list from the system group */
  DP_DeleteGroup( This, idGroup );

  /* Let the SP know that we've destroyed this group */
  if( This->dp2->spData.lpCB->DeleteGroup )
  {
    DPSP_DELETEGROUPDATA data;

    FIXME( "data.dwFlags is incorrect\n" );

    data.idGroup = idGroup;
    data.dwFlags = 0;
    data.lpISP   = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->DeleteGroup)( &data );
  }

  FIXME( "Send out a DESTORYPLAYERORGROUP message\n" );

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_DestroyGroup
          ( LPDIRECTPLAY2A iface, DPID idGroup )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_DestroyGroup( This, NULL, idGroup, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_DestroyGroup
          ( LPDIRECTPLAY2 iface, DPID idGroup )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_DestroyGroup( This, NULL, idGroup, FALSE );
}

typedef struct _DPFAGContext
{
  IDirectPlay2Impl* This;
  DPID              idPlayer;
  BOOL              bAnsi;
} DPFAGContext, *lpDPFAGContext;

static HRESULT DP_IF_DestroyPlayer
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idPlayer, BOOL bAnsi )
{
  DPFAGContext cbContext;

  FIXME( "(%p)->(%p,0x%08x,%u): semi stub\n",
         This, lpMsgHdr, idPlayer, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( DP_FindPlayer( This, idPlayer ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  /* FIXME: If the player is remote, we must be the host to delete this */

  cbContext.This     = This;
  cbContext.idPlayer = idPlayer;
  cbContext.bAnsi    = bAnsi;

  /* Find each group and call DeletePlayerFromGroup if the player is a
     member of the group */
  DP_IF_EnumGroups( This, NULL, cbDeletePlayerFromAllGroups,
                    &cbContext, DPENUMGROUPS_ALL, bAnsi );

  /* Now delete player and player list from the sys group */
  DP_DeletePlayer( This, idPlayer );

  /* Let the SP know that we've destroyed this group */
  if( This->dp2->spData.lpCB->DeletePlayer )
  {
    DPSP_DELETEPLAYERDATA data;

    FIXME( "data.dwFlags is incorrect\n" );

    data.idPlayer = idPlayer;
    data.dwFlags = 0;
    data.lpISP   = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->DeletePlayer)( &data );
  }

  FIXME( "Send a DELETEPLAYERORGROUP msg\n" );

  return DP_OK;
}

static BOOL CALLBACK
cbDeletePlayerFromAllGroups(
    DPID            dpId,
    DWORD           dwPlayerType,
    LPCDPNAME       lpName,
    DWORD           dwFlags,
    LPVOID          lpContext )
{
  lpDPFAGContext lpCtxt = (lpDPFAGContext)lpContext;

  if( dwPlayerType == DPPLAYERTYPE_GROUP )
  {
    DP_IF_DeletePlayerFromGroup( lpCtxt->This, NULL, dpId, lpCtxt->idPlayer,
                                 lpCtxt->bAnsi );

    /* Enumerate all groups in this group since this will normally only
     * be called for top level groups
     */
    DP_IF_EnumGroupsInGroup( (IDirectPlay3Impl*)lpCtxt->This,
                             dpId, NULL,
                             cbDeletePlayerFromAllGroups,
                             lpContext, DPENUMGROUPS_ALL,
                             lpCtxt->bAnsi );

  }
  else
  {
    ERR( "Group callback has dwPlayerType = 0x%08x\n", dwPlayerType );
  }

  return TRUE;
}

static HRESULT WINAPI DirectPlay2AImpl_DestroyPlayer
          ( LPDIRECTPLAY2A iface, DPID idPlayer )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_DestroyPlayer( This, NULL, idPlayer, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_DestroyPlayer
          ( LPDIRECTPLAY2 iface, DPID idPlayer )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_DestroyPlayer( This, NULL, idPlayer, FALSE );
}

static HRESULT DP_IF_EnumGroupPlayers
          ( IDirectPlay2Impl* This, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData  lpGData;
  lpPlayerList lpPList;

  FIXME("(%p)->(0x%08x,%p,%p,%p,0x%08x,%u): semi stub\n",
          This, idGroup, lpguidInstance, lpEnumPlayersCallback2,
          lpContext, dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( !This->dp2->bConnectionOpen )
  {
    return DPERR_NOSESSIONS;
  }

  if( ( lpEnumPlayersCallback2 == NULL ) ||
      ( ( dwFlags & DPENUMPLAYERS_SESSION ) && ( lpguidInstance == NULL ) ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Find the group */
  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  if( DPQ_IS_EMPTY( lpGData->players ) )
  {
    return DP_OK;
  }

  lpPList = DPQ_FIRST( lpGData->players );

  /* Walk the players in this group */
  for( ;; )
  {
    /* We do not enum any system players as they are of no
     * consequence to the end user.
     */
    if( lpPList->lpPData->dwFlags & DPLAYI_PLAYER_SYSPLAYER )
    {

      /* FIXME: Need to add stuff for dwFlags checking */

      if( !lpEnumPlayersCallback2( lpPList->lpPData->dpid, DPPLAYERTYPE_PLAYER,
                                   &lpPList->lpPData->name,
                                   lpPList->lpPData->dwFlags,
                                   lpContext )
        )
      {
        /* User requested break */
        return DP_OK;
      }
    }

    if( DPQ_IS_ENDOFLIST( lpPList->players ) )
    {
      break;
    }

    lpPList = DPQ_NEXT( lpPList->players );
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_EnumGroupPlayers
          ( LPDIRECTPLAY2A iface, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumGroupPlayers( This, idGroup, lpguidInstance,
                               lpEnumPlayersCallback2, lpContext,
                               dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_EnumGroupPlayers
          ( LPDIRECTPLAY2 iface, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumGroupPlayers( This, idGroup, lpguidInstance,
                               lpEnumPlayersCallback2, lpContext,
                               dwFlags, FALSE );
}

/* NOTE: This only enumerates top level groups (created with CreateGroup) */
static HRESULT DP_IF_EnumGroups
          ( IDirectPlay2Impl* This, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi )
{
  return DP_IF_EnumGroupsInGroup( (IDirectPlay3Impl*)This,
                                  DPID_SYSTEM_GROUP, lpguidInstance,
                                  lpEnumPlayersCallback2, lpContext,
                                  dwFlags, bAnsi );
}

static HRESULT WINAPI DirectPlay2AImpl_EnumGroups
          ( LPDIRECTPLAY2A iface, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumGroups( This, lpguidInstance, lpEnumPlayersCallback2,
                         lpContext, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_EnumGroups
          ( LPDIRECTPLAY2 iface, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumGroups( This, lpguidInstance, lpEnumPlayersCallback2,
                         lpContext, dwFlags, FALSE );
}

static HRESULT DP_IF_EnumPlayers
          ( IDirectPlay2Impl* This, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi )
{
  return DP_IF_EnumGroupPlayers( This, DPID_SYSTEM_GROUP, lpguidInstance,
                                 lpEnumPlayersCallback2, lpContext,
                                 dwFlags, bAnsi );
}

static HRESULT WINAPI DirectPlay2AImpl_EnumPlayers
          ( LPDIRECTPLAY2A iface, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumPlayers( This, lpguidInstance, lpEnumPlayersCallback2,
                          lpContext, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_EnumPlayers
          ( LPDIRECTPLAY2 iface, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumPlayers( This, lpguidInstance, lpEnumPlayersCallback2,
                          lpContext, dwFlags, FALSE );
}

/* This function should call the registered callback function that the user
   passed into EnumSessions for each entry available.
 */
static void DP_InvokeEnumSessionCallbacks
       ( LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
         LPVOID lpNSInfo,
         DWORD dwTimeout,
         LPVOID lpContext )
{
  LPDPSESSIONDESC2 lpSessionDesc;

  FIXME( ": not checking for conditions\n" );

  /* Not sure if this should be pruning but it's convenient */
  NS_PruneSessionCache( lpNSInfo );

  NS_ResetSessionEnumeration( lpNSInfo );

  /* Enumerate all sessions */
  /* FIXME: Need to indicate ANSI */
  while( (lpSessionDesc = NS_WalkSessions( lpNSInfo ) ) != NULL )
  {
    TRACE( "EnumSessionsCallback2 invoked\n" );
    if( !lpEnumSessionsCallback2( lpSessionDesc, &dwTimeout, 0, lpContext ) )
    {
      return;
    }
  }

  /* Invoke one last time to indicate that there is no more to come */
  lpEnumSessionsCallback2( NULL, &dwTimeout, DPESC_TIMEDOUT, lpContext );
}

static DWORD CALLBACK DP_EnumSessionsSendAsyncRequestThread( LPVOID lpContext )
{
  EnumSessionAsyncCallbackData* data = lpContext;
  HANDLE hSuicideRequest = data->hSuicideRequest;
  DWORD dwTimeout = data->dwTimeout;

  TRACE( "Thread started with timeout = 0x%08x\n", dwTimeout );

  for( ;; )
  {
    HRESULT hr;

    /* Sleep up to dwTimeout waiting for request to terminate thread */
    if( WaitForSingleObject( hSuicideRequest, dwTimeout ) == WAIT_OBJECT_0 )
    {
      TRACE( "Thread terminating on terminate request\n" );
      break;
    }

    /* Now resend the enum request */
    hr = NS_SendSessionRequestBroadcast( &data->requestGuid,
                                         data->dwEnumSessionFlags,
                                         data->lpSpData );

    if( FAILED(hr) )
    {
      ERR( "Enum broadcase request failed: %s\n", DPLAYX_HresultToString(hr) );
      /* FIXME: Should we kill this thread? How to inform the main thread? */
    }

  }

  TRACE( "Thread terminating\n" );

  /* Clean up the thread data */
  CloseHandle( hSuicideRequest );
  HeapFree( GetProcessHeap(), 0, lpContext );

  /* FIXME: Need to have some notification to main app thread that this is
   *        dead. It would serve two purposes. 1) allow sync on termination
   *        so that we don't actually send something to ourselves when we
   *        become name server (race condition) and 2) so that if we die
   *        abnormally something else will be able to tell.
   */

  return 1;
}

static void DP_KillEnumSessionThread( IDirectPlay2Impl* This )
{
  /* Does a thread exist? If so we were doing an async enum session */
  if( This->dp2->hEnumSessionThread != INVALID_HANDLE_VALUE )
  {
    TRACE( "Killing EnumSession thread %p\n",
           This->dp2->hEnumSessionThread );

    /* Request that the thread kill itself nicely */
    SetEvent( This->dp2->hKillEnumSessionThreadEvent );
    CloseHandle( This->dp2->hKillEnumSessionThreadEvent );

    /* We no longer need to know about the thread */
    CloseHandle( This->dp2->hEnumSessionThread );

    This->dp2->hEnumSessionThread = INVALID_HANDLE_VALUE;
  }
}

static HRESULT DP_IF_EnumSessions
          ( IDirectPlay2Impl* This, LPDPSESSIONDESC2 lpsd, DWORD dwTimeout,
            LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  TRACE( "(%p)->(%p,0x%08x,%p,%p,0x%08x,%u)\n",
         This, lpsd, dwTimeout, lpEnumSessionsCallback2, lpContext, dwFlags,
         bAnsi );
  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( (lpsd == NULL) || (lpsd->dwSize != sizeof(DPSESSIONDESC2)) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Can't enumerate if the session is already open */
  if( This->dp2->bConnectionOpen )
  {
    return DPERR_GENERIC;
  }

#if 1
  /* The loading of a lobby provider _seems_ to require a backdoor loading
   * of the service provider to also associate with this DP object. This is
   * because the app doesn't seem to have to call EnumConnections and
   * InitializeConnection for the SP before calling this method. As such
   * we'll do their dirty work for them with a quick hack so as to always
   * load the TCP/IP service provider.
   *
   * The correct solution would seem to involve creating a dialog box which
   * contains the possible SPs. These dialog boxes most likely follow SDK
   * examples.
   */
   if( This->dp2->bDPLSPInitialized && !This->dp2->bSPInitialized )
   {
     LPVOID lpConnection;
     DWORD  dwSize;

     WARN( "Hack providing TCP/IP SP for lobby provider activated\n" );

     if( !DP_BuildCompoundAddr( DPAID_ServiceProvider, (LPGUID)&DPSPGUID_TCPIP,
                                &lpConnection, &dwSize ) )
     {
       ERR( "Can't build compound addr\n" );
       return DPERR_GENERIC;
     }

     hr = DP_IF_InitializeConnection( (IDirectPlay3Impl*)This, lpConnection,
                                      0, bAnsi );
     if( FAILED(hr) )
     {
       return hr;
     }

     /* Free up the address buffer */
     HeapFree( GetProcessHeap(), 0, lpConnection );

     /* The SP is now initialized */
     This->dp2->bSPInitialized = TRUE;
   }
#endif


  /* Use the service provider default? */
  if( dwTimeout == 0 )
  {
    DPCAPS spCaps;
    spCaps.dwSize = sizeof( spCaps );

    DP_IF_GetCaps( This, &spCaps, 0 );
    dwTimeout = spCaps.dwTimeout;

    /* The service provider doesn't provide one either! */
    if( dwTimeout == 0 )
    {
      /* Provide the TCP/IP default */
      dwTimeout = DPMSG_WAIT_5_SECS;
    }
  }

  if( dwFlags & DPENUMSESSIONS_STOPASYNC )
  {
    DP_KillEnumSessionThread( This );
    return hr;
  }

  if( ( dwFlags & DPENUMSESSIONS_ASYNC ) )
  {
    /* Enumerate everything presently in the local session cache */
    DP_InvokeEnumSessionCallbacks( lpEnumSessionsCallback2,
                                   This->dp2->lpNameServerData, dwTimeout,
                                   lpContext );

    if( This->dp2->dwEnumSessionLock != 0 )
      return DPERR_CONNECTING;

    /* See if we've already created a thread to service this interface */
    if( This->dp2->hEnumSessionThread == INVALID_HANDLE_VALUE )
    {
      DWORD dwThreadId;
      This->dp2->dwEnumSessionLock++;

      /* Send the first enum request inline since the user may cancel a dialog
       * if one is presented. Also, may also have a connecting return code.
       */
      hr = NS_SendSessionRequestBroadcast( &lpsd->guidApplication,
                                           dwFlags, &This->dp2->spData );

      if( SUCCEEDED(hr) )
      {
        EnumSessionAsyncCallbackData* lpData
          = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpData ) );
        /* FIXME: need to kill the thread on object deletion */
        lpData->lpSpData  = &This->dp2->spData;

        lpData->requestGuid = lpsd->guidApplication;
        lpData->dwEnumSessionFlags = dwFlags;
        lpData->dwTimeout = dwTimeout;

        This->dp2->hKillEnumSessionThreadEvent =
          CreateEventW( NULL, TRUE, FALSE, NULL );

        if( !DuplicateHandle( GetCurrentProcess(),
                              This->dp2->hKillEnumSessionThreadEvent,
                              GetCurrentProcess(),
                              &lpData->hSuicideRequest,
                              0, FALSE, DUPLICATE_SAME_ACCESS )
          )
        {
          ERR( "Can't duplicate thread killing handle\n" );
        }

        TRACE( ": creating EnumSessionsRequest thread\n" );

        This->dp2->hEnumSessionThread = CreateThread( NULL,
                                                      0,
                                                      DP_EnumSessionsSendAsyncRequestThread,
                                                      lpData,
                                                      0,
                                                      &dwThreadId );
      }
      This->dp2->dwEnumSessionLock--;
    }
  }
  else
  {
    /* Invalidate the session cache for the interface */
    NS_InvalidateSessionCache( This->dp2->lpNameServerData );

    /* Send the broadcast for session enumeration */
    hr = NS_SendSessionRequestBroadcast( &lpsd->guidApplication,
                                         dwFlags,
                                         &This->dp2->spData );


    SleepEx( dwTimeout, FALSE );

    DP_InvokeEnumSessionCallbacks( lpEnumSessionsCallback2,
                                   This->dp2->lpNameServerData, dwTimeout,
                                   lpContext );
  }

  return hr;
}

static HRESULT WINAPI DirectPlay2AImpl_EnumSessions
          ( LPDIRECTPLAY2A iface, LPDPSESSIONDESC2 lpsd, DWORD dwTimeout,
            LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumSessions( This, lpsd, dwTimeout, lpEnumSessionsCallback2,
                             lpContext, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_EnumSessions
          ( LPDIRECTPLAY2 iface, LPDPSESSIONDESC2 lpsd, DWORD dwTimeout,
            LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumSessions( This, lpsd, dwTimeout, lpEnumSessionsCallback2,
                             lpContext, dwFlags, FALSE );
}

static HRESULT DP_IF_GetPlayerCaps
          ( IDirectPlay2Impl* This, DPID idPlayer, LPDPCAPS lpDPCaps,
            DWORD dwFlags )
{
  HRESULT hr;
  DPSP_GETCAPSDATA data;

  TRACE("(%p)->(0x%08x,%p,0x%08x)\n", This, idPlayer, lpDPCaps, dwFlags);

  if ( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if ( lpDPCaps->dwSize != sizeof(DPCAPS) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Query the service provider */
  data.idPlayer = idPlayer;
  data.dwFlags  = dwFlags;
  data.lpCaps   = lpDPCaps;
  data.lpISP    = This->dp2->spData.lpISP;

  hr = (*This->dp2->spData.lpCB->GetCaps)( &data );

  /* Substract size of DirectPlay header */
  data.lpCaps->dwMaxBufferSize -= sizeof(DPSP_MSG_ENVELOPE);

  return hr;
}

static HRESULT DP_IF_GetCaps
          ( IDirectPlay2Impl* This, LPDPCAPS lpDPCaps, DWORD dwFlags )
{
  return DP_IF_GetPlayerCaps( This, DPID_ALLPLAYERS, lpDPCaps, dwFlags );
}

static HRESULT WINAPI DirectPlay2AImpl_GetCaps
          ( LPDIRECTPLAY2A iface, LPDPCAPS lpDPCaps, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetCaps( This, lpDPCaps, dwFlags );
}

static HRESULT WINAPI DirectPlay2WImpl_GetCaps
          ( LPDIRECTPLAY2 iface, LPDPCAPS lpDPCaps, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetCaps( This, lpDPCaps, dwFlags );
}

static HRESULT DP_IF_GetGroupData
          ( IDirectPlay2Impl* This, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize, DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData lpGData;
  DWORD dwRequiredBufferSize;
  LPVOID lpCopyDataFrom;

  TRACE( "(%p)->(0x%08x,%p,%p,0x%08x,%u)\n",
         This, idGroup, lpData, lpdwDataSize, dwFlags, bAnsi );

  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* How much buffer is required? */
  if( dwFlags & DPSET_LOCAL )
  {
    dwRequiredBufferSize = lpGData->dwLocalDataSize;
    lpCopyDataFrom       = lpGData->lpLocalData;
  }
  else
  {
    dwRequiredBufferSize = lpGData->dwRemoteDataSize;
    lpCopyDataFrom       = lpGData->lpRemoteData;
  }

  /* Is the user requesting to know how big a buffer is required? */
  if( ( lpData == NULL ) ||
      ( *lpdwDataSize < dwRequiredBufferSize )
    )
  {
    *lpdwDataSize = dwRequiredBufferSize;
    return DPERR_BUFFERTOOSMALL;
  }

  CopyMemory( lpData, lpCopyDataFrom, dwRequiredBufferSize );

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_GetGroupData
          ( LPDIRECTPLAY2A iface, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetGroupData( This, idGroup, lpData, lpdwDataSize,
                           dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetGroupData
          ( LPDIRECTPLAY2 iface, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetGroupData( This, idGroup, lpData, lpdwDataSize,
                           dwFlags, FALSE );
}

static HRESULT DP_IF_GetGroupName
          ( IDirectPlay2Impl* This, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize, BOOL bAnsi )
{
  lpGroupData lpGData;
  LPDPNAME    lpName = lpData;
  DWORD       dwRequiredDataSize;

  FIXME("(%p)->(0x%08x,%p,%p,%u) ANSI ignored\n",
          This, idGroup, lpData, lpdwDataSize, bAnsi );

  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  dwRequiredDataSize = lpGData->name.dwSize;

  if( lpGData->name.u1.lpszShortNameA )
  {
    dwRequiredDataSize += strlen( lpGData->name.u1.lpszShortNameA ) + 1;
  }

  if( lpGData->name.u2.lpszLongNameA )
  {
    dwRequiredDataSize += strlen( lpGData->name.u2.lpszLongNameA ) + 1;
  }

  if( ( lpData == NULL ) ||
      ( *lpdwDataSize < dwRequiredDataSize )
    )
  {
    *lpdwDataSize = dwRequiredDataSize;
    return DPERR_BUFFERTOOSMALL;
  }

  /* Copy the structure */
  CopyMemory( lpName, &lpGData->name, lpGData->name.dwSize );

  if( lpGData->name.u1.lpszShortNameA )
  {
    strcpy( ((char*)lpName)+lpGData->name.dwSize,
            lpGData->name.u1.lpszShortNameA );
  }
  else
  {
    lpName->u1.lpszShortNameA = NULL;
  }

  if( lpGData->name.u1.lpszShortNameA )
  {
    strcpy( ((char*)lpName)+lpGData->name.dwSize,
            lpGData->name.u2.lpszLongNameA );
  }
  else
  {
    lpName->u2.lpszLongNameA = NULL;
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_GetGroupName
          ( LPDIRECTPLAY2A iface, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetGroupName( This, idGroup, lpData, lpdwDataSize, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetGroupName
          ( LPDIRECTPLAY2 iface, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetGroupName( This, idGroup, lpData, lpdwDataSize, FALSE );
}

static HRESULT DP_IF_GetMessageCount
          ( IDirectPlay2Impl* This, DPID idPlayer,
            LPDWORD lpdwCount, BOOL bAnsi )
{
  FIXME("(%p)->(0x%08x,%p,%u): stub\n", This, idPlayer, lpdwCount, bAnsi );
  return DP_IF_GetMessageQueue( (IDirectPlay4Impl*)This, 0, idPlayer,
                                DPMESSAGEQUEUE_RECEIVE, lpdwCount, NULL,
                                bAnsi );
}

static HRESULT WINAPI DirectPlay2AImpl_GetMessageCount
          ( LPDIRECTPLAY2A iface, DPID idPlayer, LPDWORD lpdwCount )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetMessageCount( This, idPlayer, lpdwCount, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetMessageCount
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPDWORD lpdwCount )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetMessageCount( This, idPlayer, lpdwCount, FALSE );
}

static HRESULT WINAPI DirectPlay2AImpl_GetPlayerAddress
          ( LPDIRECTPLAY2A iface, DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  FIXME("(%p)->(0x%08x,%p,%p): stub\n", This, idPlayer, lpData, lpdwDataSize );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay2WImpl_GetPlayerAddress
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  FIXME("(%p)->(0x%08x,%p,%p): stub\n", This, idPlayer, lpData, lpdwDataSize );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_GetPlayerCaps
          ( LPDIRECTPLAY2A iface, DPID idPlayer, LPDPCAPS lpPlayerCaps,
            DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetPlayerCaps( This, idPlayer, lpPlayerCaps, dwFlags );
}

static HRESULT WINAPI DirectPlay2WImpl_GetPlayerCaps
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPDPCAPS lpPlayerCaps,
            DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetPlayerCaps( This, idPlayer, lpPlayerCaps, dwFlags );
}

static HRESULT DP_IF_GetPlayerData
          ( IDirectPlay2Impl* This, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize, DWORD dwFlags, BOOL bAnsi )
{
  lpPlayerList lpPList;
  DWORD dwRequiredBufferSize;
  LPVOID lpCopyDataFrom;

  TRACE( "(%p)->(0x%08x,%p,%p,0x%08x,%u)\n",
         This, idPlayer, lpData, lpdwDataSize, dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  if( lpdwDataSize == NULL )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* How much buffer is required? */
  if( dwFlags & DPSET_LOCAL )
  {
    dwRequiredBufferSize = lpPList->lpPData->dwLocalDataSize;
    lpCopyDataFrom       = lpPList->lpPData->lpLocalData;
  }
  else
  {
    dwRequiredBufferSize = lpPList->lpPData->dwRemoteDataSize;
    lpCopyDataFrom       = lpPList->lpPData->lpRemoteData;
  }

  /* Is the user requesting to know how big a buffer is required? */
  if( ( lpData == NULL ) ||
      ( *lpdwDataSize < dwRequiredBufferSize )
    )
  {
    *lpdwDataSize = dwRequiredBufferSize;
    return DPERR_BUFFERTOOSMALL;
  }

  CopyMemory( lpData, lpCopyDataFrom, dwRequiredBufferSize );

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_GetPlayerData
          ( LPDIRECTPLAY2A iface, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetPlayerData( This, idPlayer, lpData, lpdwDataSize,
                            dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetPlayerData
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetPlayerData( This, idPlayer, lpData, lpdwDataSize,
                            dwFlags, FALSE );
}

static HRESULT DP_IF_GetPlayerName
          ( IDirectPlay2Impl* This, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize, BOOL bAnsi )
{
  lpPlayerList lpPList;
  LPDPNAME    lpName = lpData;
  DWORD       dwRequiredDataSize;

  FIXME( "(%p)->(0x%08x,%p,%p,%u): ANSI\n",
         This, idPlayer, lpData, lpdwDataSize, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  dwRequiredDataSize = lpPList->lpPData->name.dwSize;

  if( lpPList->lpPData->name.u1.lpszShortNameA )
  {
    dwRequiredDataSize += strlen( lpPList->lpPData->name.u1.lpszShortNameA ) + 1;
  }

  if( lpPList->lpPData->name.u2.lpszLongNameA )
  {
    dwRequiredDataSize += strlen( lpPList->lpPData->name.u2.lpszLongNameA ) + 1;
  }

  if( ( lpData == NULL ) ||
      ( *lpdwDataSize < dwRequiredDataSize )
    )
  {
    *lpdwDataSize = dwRequiredDataSize;
    return DPERR_BUFFERTOOSMALL;
  }

  /* Copy the structure */
  CopyMemory( lpName, &lpPList->lpPData->name, lpPList->lpPData->name.dwSize );

  if( lpPList->lpPData->name.u1.lpszShortNameA )
  {
    strcpy( ((char*)lpName)+lpPList->lpPData->name.dwSize,
            lpPList->lpPData->name.u1.lpszShortNameA );
  }
  else
  {
    lpName->u1.lpszShortNameA = NULL;
  }

  if( lpPList->lpPData->name.u2.lpszLongNameA )
  {
    strcpy( ((char*)lpName)+lpPList->lpPData->name.dwSize,
            lpPList->lpPData->name.u2.lpszLongNameA );
  }
  else
  {
    lpName->u2.lpszLongNameA = NULL;
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_GetPlayerName
          ( LPDIRECTPLAY2A iface, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetPlayerName( This, idPlayer, lpData, lpdwDataSize, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetPlayerName
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetPlayerName( This, idPlayer, lpData, lpdwDataSize, FALSE );
}

static HRESULT DP_GetSessionDesc
          ( IDirectPlay2Impl* This, LPVOID lpData, LPDWORD lpdwDataSize,
            BOOL bAnsi )
{
  DWORD dwRequiredSize;

  TRACE( "(%p)->(%p,%p,%u)\n", This, lpData, lpdwDataSize, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( !This->dp2->bConnectionOpen )
  {
    return DPERR_NOSESSIONS;
  }

  if( ( lpdwDataSize == NULL ) || ( *lpdwDataSize >= MAXDWORD ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* FIXME: Get from This->dp2->lpSessionDesc */
  dwRequiredSize = DP_CalcSessionDescSize( This->dp2->lpSessionDesc, bAnsi );

  if ( ( lpData == NULL ) ||
       ( *lpdwDataSize < dwRequiredSize )
     )
  {
    *lpdwDataSize = dwRequiredSize;
    return DPERR_BUFFERTOOSMALL;
  }

  DP_CopySessionDesc( lpData, This->dp2->lpSessionDesc, bAnsi );

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_GetSessionDesc
          ( LPDIRECTPLAY2A iface, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_GetSessionDesc( This, lpData, lpdwDataSize, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetSessionDesc
          ( LPDIRECTPLAY2 iface, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_GetSessionDesc( This, lpData, lpdwDataSize, TRUE );
}

/* Intended only for COM compatibility. Always returns an error. */
static HRESULT WINAPI DirectPlay2AImpl_Initialize
          ( LPDIRECTPLAY2A iface, LPGUID lpGUID )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  TRACE("(%p)->(%p): stub\n", This, lpGUID );
  return DPERR_ALREADYINITIALIZED;
}

/* Intended only for COM compatibility. Always returns an error. */
static HRESULT WINAPI DirectPlay2WImpl_Initialize
          ( LPDIRECTPLAY2 iface, LPGUID lpGUID )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  TRACE("(%p)->(%p): stub\n", This, lpGUID );
  return DPERR_ALREADYINITIALIZED;
}


static HRESULT DP_SecureOpen
          ( IDirectPlay2Impl* This, LPCDPSESSIONDESC2 lpsd, DWORD dwFlags,
            LPCDPSECURITYDESC lpSecurity, LPCDPCREDENTIALS lpCredentials,
            BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  FIXME( "(%p)->(%p,0x%08x,%p,%p): partial stub\n",
         This, lpsd, dwFlags, lpSecurity, lpCredentials );

  if( ( lpsd == NULL ) ||
      ( lpsd->dwSize != sizeof(DPSESSIONDESC2) ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  if( This->dp2->bConnectionOpen )
  {
    TRACE( ": rejecting already open connection.\n" );
    return DPERR_ALREADYINITIALIZED;
  }

  /* If we're enumerating, kill the thread */
  DP_KillEnumSessionThread( This );

  if( dwFlags & DPOPEN_JOIN )
  {
    LPDPSESSIONDESC2 current = NULL;
    NS_ResetSessionEnumeration( This->dp2->lpNameServerData );
    while( ( current = NS_WalkSessions( This->dp2->lpNameServerData ) ) )
    {
      if ( IsEqualGUID( &lpsd->guidInstance, &current->guidInstance ) )
        break;
    }
    if ( current == NULL )
      return DPERR_NOSESSIONS;
  }
  else if( dwFlags & DPOPEN_CREATE )
  {
    /* Rightoo - this computer is the host and the local computer needs to be
       the name server so that others can join this session */
    NS_SetLocalComputerAsNameServer( lpsd, This->dp2->lpNameServerData );

    This->dp2->bHostInterface = TRUE;

    hr = DP_SetSessionDesc( This, lpsd, 0, TRUE, bAnsi );
    if( FAILED( hr ) )
    {
      ERR( "Unable to set session desc: %s\n", DPLAYX_HresultToString( hr ) );
      return hr;
    }
  }

  /* Invoke the conditional callback for the service provider */
  if( This->dp2->spData.lpCB->Open )
  {
    DPSP_OPENDATA data;

    FIXME( "Not all data fields are correct. Need new parameter\n" );

    data.bCreate           = (dwFlags & DPOPEN_CREATE ) ? TRUE : FALSE;
    data.lpSPMessageHeader = (dwFlags & DPOPEN_CREATE ) ? NULL
                                                        : NS_GetNSAddr( This->dp2->lpNameServerData );
    data.lpISP             = This->dp2->spData.lpISP;
    data.bReturnStatus     = (dwFlags & DPOPEN_RETURNSTATUS) ? TRUE : FALSE;
    data.dwOpenFlags       = dwFlags;
    data.dwSessionFlags    = This->dp2->lpSessionDesc->dwFlags;

    hr = (*This->dp2->spData.lpCB->Open)(&data);
    if( FAILED( hr ) )
    {
      ERR( "Unable to open session: %s\n", DPLAYX_HresultToString( hr ) );
      return hr;
    }
  }

  {
    /* Create the system group of which everything is a part of */
    DPID systemGroup = DPID_SYSTEM_GROUP;

    hr = DP_IF_CreateGroup( This, NULL, &systemGroup, NULL,
                            NULL, 0, 0, TRUE );

  }

  if( dwFlags & DPOPEN_JOIN )
  {
    DPID dpidServerId = DPID_UNKNOWN;

    /* Create the server player for this interface. This way we can receive
     * messages for this session.
     */
    /* FIXME: I suppose that we should be setting an event for a receive
     *        type of thing. That way the messaging thread could know to wake
     *        up. DPlay would then trigger the hEvent for the player the
     *        message is directed to.
     */
    hr = DP_IF_CreatePlayer( This, NULL, &dpidServerId, NULL, 0, NULL,
                             0,
                             DPPLAYER_SERVERPLAYER | DPPLAYER_LOCAL , bAnsi );

  }
  else if( dwFlags & DPOPEN_CREATE )
  {
    DPID dpidNameServerId = DPID_NAME_SERVER;

    hr = DP_IF_CreatePlayer( This, NULL, &dpidNameServerId, NULL, 0, NULL,
                             0, DPPLAYER_SERVERPLAYER, bAnsi );
  }

  if( FAILED(hr) )
  {
    ERR( "Couldn't create name server/system player: %s\n",
         DPLAYX_HresultToString(hr) );
  }
  else
  {
    This->dp2->bConnectionOpen = TRUE;
  }

  return hr;
}

static HRESULT WINAPI DirectPlay2AImpl_Open
          ( LPDIRECTPLAY2A iface, LPDPSESSIONDESC2 lpsd, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  TRACE("(%p)->(%p,0x%08x)\n", This, lpsd, dwFlags );
  return DP_SecureOpen( This, lpsd, dwFlags, NULL, NULL, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_Open
          ( LPDIRECTPLAY2 iface, LPDPSESSIONDESC2 lpsd, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  TRACE("(%p)->(%p,0x%08x)\n", This, lpsd, dwFlags );
  return DP_SecureOpen( This, lpsd, dwFlags, NULL, NULL, FALSE );
}

static HRESULT DP_IF_Receive
          ( IDirectPlay2Impl* This, LPDPID lpidFrom, LPDPID lpidTo,
            DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize, BOOL bAnsi )
{
  LPDPMSG lpMsg = NULL;

  FIXME( "(%p)->(%p,%p,0x%08x,%p,%p,%u): stub\n",
         This, lpidFrom, lpidTo, dwFlags, lpData, lpdwDataSize, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if ( ( lpdwDataSize == NULL ) ||
       ( ( dwFlags & DPRECEIVE_FROMPLAYER ) && ( lpidFrom == NULL ) ) ||
       ( ( dwFlags & DPRECEIVE_TOPLAYER ) && ( lpidTo == NULL ) ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  if( dwFlags == 0 )
  {
    dwFlags = DPRECEIVE_ALL;
  }

  if( dwFlags & DPRECEIVE_ALL )
  {
    lpMsg = DPQ_FIRST( This->dp2->receiveMsgs );
  }
  else
  {
    if ( (lpMsg = DPQ_FIRST( This->dp2->receiveMsgs )) )
    {
      do
      {
        if ( ( ( dwFlags & DPRECEIVE_FROMPLAYER ) &&
               ( dwFlags & DPRECEIVE_TOPLAYER )   &&
               ( lpMsg->idFrom == *lpidFrom )     &&
               ( lpMsg->idTo == *lpidTo ) )          || /* From & To */
             ( ( dwFlags & DPRECEIVE_FROMPLAYER ) &&
               ( lpMsg->idFrom == *lpidFrom ) )      || /* From */
             ( ( dwFlags & DPRECEIVE_TOPLAYER )   &&
               ( lpMsg->idTo == *lpidTo ) ) )           /* To */
        {
          break;
        }
      }
      while( (lpMsg = DPQ_NEXT( lpMsg->msgs )) );
    }
  }

  if( lpMsg == NULL )
  {
    return DPERR_NOMESSAGES;
  }

  /* Buffer size check */
  if ( ( lpData == NULL ) ||
       ( *lpdwDataSize < lpMsg->dwMsgSize ) )
  {
    *lpdwDataSize = lpMsg->dwMsgSize;
    return DPERR_BUFFERTOOSMALL;
  }

  /* Copy into the provided buffer */
  if (lpData) CopyMemory( lpData, lpMsg->msg, *lpdwDataSize );

  /* Set players */
  if ( lpidFrom )
  {
    *lpidFrom = lpMsg->idFrom;
  }
  if ( lpidTo )
  {
    *lpidTo = lpMsg->idTo;
  }

  /* Remove message from queue */
  if( !( dwFlags & DPRECEIVE_PEEK ) )
  {
    HeapFree( GetProcessHeap(), 0, lpMsg->msg );
    DPQ_REMOVE( This->dp2->receiveMsgs, lpMsg, msgs );
    HeapFree( GetProcessHeap(), 0, lpMsg );
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_Receive
          ( LPDIRECTPLAY2A iface, LPDPID lpidFrom, LPDPID lpidTo,
            DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_Receive( This, lpidFrom, lpidTo, dwFlags,
                        lpData, lpdwDataSize, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_Receive
          ( LPDIRECTPLAY2 iface, LPDPID lpidFrom, LPDPID lpidTo,
            DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_Receive( This, lpidFrom, lpidTo, dwFlags,
                        lpData, lpdwDataSize, FALSE );
}

static HRESULT WINAPI DirectPlay2AImpl_Send
          ( LPDIRECTPLAY2A iface, DPID idFrom, DPID idTo, DWORD dwFlags, LPVOID lpData, DWORD dwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_SendEx( This, idFrom, idTo, dwFlags, lpData, dwDataSize,
                    0, 0, NULL, NULL, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_Send
          ( LPDIRECTPLAY2 iface, DPID idFrom, DPID idTo, DWORD dwFlags, LPVOID lpData, DWORD dwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_SendEx( This, idFrom, idTo, dwFlags, lpData, dwDataSize,
                    0, 0, NULL, NULL, FALSE );
}

static HRESULT DP_IF_SetGroupData
          ( IDirectPlay2Impl* This, DPID idGroup, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData lpGData;

  TRACE( "(%p)->(0x%08x,%p,0x%08x,0x%08x,%u)\n",
         This, idGroup, lpData, dwDataSize, dwFlags, bAnsi );

  /* Parameter check */
  if( ( lpData == NULL ) &&
      ( dwDataSize != 0 )
    )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Find the pointer to the data for this player */
  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDOBJECT;
  }

  if( !(dwFlags & DPSET_LOCAL) )
  {
    FIXME( "Was this group created by this interface?\n" );
    /* FIXME: If this is a remote update need to allow it but not
     *        send a message.
     */
  }

  DP_SetGroupData( lpGData, dwFlags, lpData, dwDataSize );

  /* FIXME: Only send a message if this group is local to the session otherwise
   * it will have been rejected above
   */
  if( !(dwFlags & DPSET_LOCAL) )
  {
    FIXME( "Send msg?\n" );
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_SetGroupData
          ( LPDIRECTPLAY2A iface, DPID idGroup, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetGroupData( This, idGroup, lpData, dwDataSize, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_SetGroupData
          ( LPDIRECTPLAY2 iface, DPID idGroup, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetGroupData( This, idGroup, lpData, dwDataSize, dwFlags, FALSE );
}

static HRESULT DP_IF_SetGroupName
          ( IDirectPlay2Impl* This, DPID idGroup, LPDPNAME lpGroupName,
            DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData lpGData;

  TRACE( "(%p)->(0x%08x,%p,0x%08x,%u)\n", This, idGroup,
         lpGroupName, dwFlags, bAnsi );

  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  DP_CopyDPNAMEStruct( &lpGData->name, lpGroupName, bAnsi );

  /* Should send a DPMSG_SETPLAYERORGROUPNAME message */
  FIXME( "Message not sent and dwFlags ignored\n" );

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_SetGroupName
          ( LPDIRECTPLAY2A iface, DPID idGroup, LPDPNAME lpGroupName,
            DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetGroupName( This, idGroup, lpGroupName, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_SetGroupName
          ( LPDIRECTPLAY2 iface, DPID idGroup, LPDPNAME lpGroupName,
            DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetGroupName( This, idGroup, lpGroupName, dwFlags, FALSE );
}

static HRESULT DP_IF_SetPlayerData
          ( IDirectPlay2Impl* This, DPID idPlayer, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi )
{
  lpPlayerList lpPList;

  TRACE( "(%p)->(0x%08x,%p,0x%08x,0x%08x,%u)\n",
         This, idPlayer, lpData, dwDataSize, dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  /* Parameter check */
  if( ( ( lpData == NULL ) && ( dwDataSize != 0 ) ) ||
      ( dwDataSize >= MAXDWORD ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Find the pointer to the data for this player */
  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  if( !(dwFlags & DPSET_LOCAL) )
  {
    FIXME( "Was this group created by this interface?\n" );
    /* FIXME: If this is a remote update need to allow it but not
     *        send a message.
     */
  }

  DP_SetPlayerData( lpPList->lpPData, dwFlags, lpData, dwDataSize );

  if( !(dwFlags & DPSET_LOCAL) )
  {
    FIXME( "Send msg?\n" );
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_SetPlayerData
          ( LPDIRECTPLAY2A iface, DPID idPlayer, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetPlayerData( This, idPlayer, lpData, dwDataSize,
                              dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_SetPlayerData
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetPlayerData( This, idPlayer, lpData, dwDataSize,
                              dwFlags, FALSE );
}

static HRESULT DP_IF_SetPlayerName
          ( IDirectPlay2Impl* This, DPID idPlayer, LPDPNAME lpPlayerName,
            DWORD dwFlags, BOOL bAnsi )
{
  lpPlayerList lpPList;

  TRACE( "(%p)->(0x%08x,%p,0x%08x,%u)\n",
         This, idPlayer, lpPlayerName, dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  DP_CopyDPNAMEStruct( &lpPList->lpPData->name, lpPlayerName, bAnsi );

  /* Should send a DPMSG_SETPLAYERORGROUPNAME message */
  FIXME( "Message not sent and dwFlags ignored\n" );

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_SetPlayerName
          ( LPDIRECTPLAY2A iface, DPID idPlayer, LPDPNAME lpPlayerName,
            DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetPlayerName( This, idPlayer, lpPlayerName, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_SetPlayerName
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPDPNAME lpPlayerName,
            DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetPlayerName( This, idPlayer, lpPlayerName, dwFlags, FALSE );
}

HRESULT DP_SetSessionDesc
          ( IDirectPlay2Impl* This, LPCDPSESSIONDESC2 lpSessDesc,
            DWORD dwFlags, BOOL bInitial, BOOL bAnsi  )
{
  DWORD            dwRequiredSize;
  LPDPSESSIONDESC2 lpTempSessDesc;

  TRACE( "(%p)->(%p,0x%08x,%u,%u)\n",
         This, lpSessDesc, dwFlags, bInitial, bAnsi );

  /* Illegal combinations of flags */
  if ( ( lpSessDesc->dwFlags & DPSESSION_MIGRATEHOST ) &&
       ( lpSessDesc->dwFlags & ( DPSESSION_CLIENTSERVER |
                                 DPSESSION_MULTICASTSERVER |
                                 DPSESSION_SECURESERVER ) ) )
  {
    return DPERR_INVALIDFLAGS;
  }

  /* FIXME: Copy into This->dp2->lpSessionDesc */
  dwRequiredSize = DP_CalcSessionDescSize( lpSessDesc, bAnsi );
  lpTempSessDesc = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwRequiredSize );

  if( lpTempSessDesc == NULL )
  {
    return DPERR_OUTOFMEMORY;
  }

  /* Free the old */
  HeapFree( GetProcessHeap(), 0, This->dp2->lpSessionDesc );

  This->dp2->lpSessionDesc = lpTempSessDesc;
  /* Set the new */
  DP_CopySessionDesc( This->dp2->lpSessionDesc, lpSessDesc, bAnsi );
  if( bInitial )
  {
    /*Initializing session GUID*/
    CoCreateGuid( &(This->dp2->lpSessionDesc->guidInstance) );
  }
  /* If this is an external invocation of the interface, we should be
   * letting everyone know that things have changed. Otherwise this is
   * just an initialization and it doesn't need to be propagated.
   */
  if( !bInitial )
  {
    FIXME( "Need to send a DPMSG_SETSESSIONDESC msg to everyone\n" );
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay2AImpl_SetSessionDesc
          ( LPDIRECTPLAY2A iface, LPDPSESSIONDESC2 lpSessDesc, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( !This->dp2->bConnectionOpen )
  {
    return DPERR_NOSESSIONS;
  }

  if( dwFlags || (lpSessDesc == NULL) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Only the host is allowed to update the session desc */
  if( !This->dp2->bHostInterface )
  {
    return DPERR_ACCESSDENIED;
  }

  return DP_SetSessionDesc( This, lpSessDesc, dwFlags, FALSE, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_SetSessionDesc
          ( LPDIRECTPLAY2 iface, LPDPSESSIONDESC2 lpSessDesc, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( !This->dp2->bConnectionOpen )
  {
    return DPERR_NOSESSIONS;
  }

  if( dwFlags || (lpSessDesc == NULL) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Illegal combinations of flags */
  if ( ( lpSessDesc->dwFlags & DPSESSION_MIGRATEHOST ) &&
       ( lpSessDesc->dwFlags & ( DPSESSION_CLIENTSERVER |
                                 DPSESSION_MULTICASTSERVER |
                                 DPSESSION_SECURESERVER ) ) )
  {
    return DPERR_INVALIDFLAGS;
  }

  /* Only the host is allowed to update the session desc */
  if( !This->dp2->bHostInterface )
  {
    return DPERR_ACCESSDENIED;
  }

  return DP_SetSessionDesc( This, lpSessDesc, dwFlags, FALSE, TRUE );
}

/* FIXME: See about merging some of this stuff with dplayx_global.c stuff */
static DWORD DP_CalcSessionDescSize( LPCDPSESSIONDESC2 lpSessDesc, BOOL bAnsi )
{
  DWORD dwSize = 0;

  if( lpSessDesc == NULL )
  {
    /* Hmmm..don't need any size? */
    ERR( "NULL lpSessDesc\n" );
    return dwSize;
  }

  dwSize += sizeof( *lpSessDesc );

  if( bAnsi )
  {
    if( lpSessDesc->u1.lpszSessionNameA )
    {
      dwSize += lstrlenA( lpSessDesc->u1.lpszSessionNameA ) + 1;
    }

    if( lpSessDesc->u2.lpszPasswordA )
    {
      dwSize += lstrlenA( lpSessDesc->u2.lpszPasswordA ) + 1;
    }
  }
  else /* UNICODE */
  {
    if( lpSessDesc->u1.lpszSessionName )
    {
      dwSize += sizeof( WCHAR ) *
        ( lstrlenW( lpSessDesc->u1.lpszSessionName ) + 1 );
    }

    if( lpSessDesc->u2.lpszPassword )
    {
      dwSize += sizeof( WCHAR ) *
        ( lstrlenW( lpSessDesc->u2.lpszPassword ) + 1 );
    }
  }

  return dwSize;
}

/* Assumes that contiguous buffers are already allocated. */
static void DP_CopySessionDesc( LPDPSESSIONDESC2 lpSessionDest,
                                LPCDPSESSIONDESC2 lpSessionSrc, BOOL bAnsi )
{
  BYTE* lpStartOfFreeSpace;

  if( lpSessionDest == NULL )
  {
    ERR( "NULL lpSessionDest\n" );
    return;
  }

  CopyMemory( lpSessionDest, lpSessionSrc, sizeof( *lpSessionSrc ) );

  lpStartOfFreeSpace = ((BYTE*)lpSessionDest) + sizeof( *lpSessionSrc );

  if( bAnsi )
  {
    if( lpSessionSrc->u1.lpszSessionNameA )
    {
      lstrcpyA( (LPSTR)lpStartOfFreeSpace,
                lpSessionDest->u1.lpszSessionNameA );
      lpSessionDest->u1.lpszSessionNameA = (LPSTR)lpStartOfFreeSpace;
      lpStartOfFreeSpace +=
        lstrlenA( lpSessionDest->u1.lpszSessionNameA ) + 1;
    }

    if( lpSessionSrc->u2.lpszPasswordA )
    {
      lstrcpyA( (LPSTR)lpStartOfFreeSpace,
                lpSessionDest->u2.lpszPasswordA );
      lpSessionDest->u2.lpszPasswordA = (LPSTR)lpStartOfFreeSpace;
      lpStartOfFreeSpace +=
        lstrlenA( lpSessionDest->u2.lpszPasswordA ) + 1;
    }
  }
  else /* UNICODE */
  {
    if( lpSessionSrc->u1.lpszSessionName )
    {
      lstrcpyW( (LPWSTR)lpStartOfFreeSpace,
                lpSessionDest->u1.lpszSessionName );
      lpSessionDest->u1.lpszSessionName = (LPWSTR)lpStartOfFreeSpace;
      lpStartOfFreeSpace += sizeof(WCHAR) *
        ( lstrlenW( lpSessionDest->u1.lpszSessionName ) + 1 );
    }

    if( lpSessionSrc->u2.lpszPassword )
    {
      lstrcpyW( (LPWSTR)lpStartOfFreeSpace,
                lpSessionDest->u2.lpszPassword );
      lpSessionDest->u2.lpszPassword = (LPWSTR)lpStartOfFreeSpace;
      lpStartOfFreeSpace += sizeof(WCHAR) *
        ( lstrlenW( lpSessionDest->u2.lpszPassword ) + 1 );
    }
  }
}


static HRESULT DP_IF_AddGroupToGroup
          ( IDirectPlay3Impl* This, DPID idParentGroup, DPID idGroup )
{
  lpGroupData lpGData;
  lpGroupList lpNewGList;

  TRACE( "(%p)->(0x%08x,0x%08x)\n", This, idParentGroup, idGroup );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idParentGroup ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  if( ( lpGData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Create a player list (ie "shortcut" ) */
  lpNewGList = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpNewGList ) );
  if( lpNewGList == NULL )
  {
    return DPERR_CANTADDPLAYER;
  }

  /* Add the shortcut */
  lpGData->uRef++;
  lpNewGList->lpGData = lpGData;

  /* Add the player to the list of players for this group */
  DPQ_INSERT( lpGData->groups, lpNewGList, groups );

  /* Send a ADDGROUPTOGROUP message */
  FIXME( "Not sending message\n" );

  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_AddGroupToGroup
          ( LPDIRECTPLAY3A iface, DPID idParentGroup, DPID idGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_AddGroupToGroup( This, idParentGroup, idGroup );
}

static HRESULT WINAPI DirectPlay3WImpl_AddGroupToGroup
          ( LPDIRECTPLAY3 iface, DPID idParentGroup, DPID idGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_AddGroupToGroup( This, idParentGroup, idGroup );
}

static HRESULT DP_IF_CreateGroupInGroup
          ( IDirectPlay3Impl* This, LPVOID lpMsgHdr, DPID idParentGroup,
            LPDPID lpidGroup, LPDPNAME lpGroupName, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData lpGParentData;
  lpGroupList lpGList;
  lpGroupData lpGData;

  TRACE( "(%p)->(0x%08x,%p,%p,%p,0x%08x,0x%08x,%u)\n",
         This, idParentGroup, lpidGroup, lpGroupName, lpData,
         dwDataSize, dwFlags, bAnsi );

  /* Verify that the specified parent is valid */
  if( ( lpGParentData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This,
                                         idParentGroup ) ) == NULL
    )
  {
    return DPERR_INVALIDGROUP;
  }

  lpGData = DP_CreateGroup( (IDirectPlay2AImpl*)This, lpidGroup, lpGroupName,
                            dwFlags, idParentGroup, bAnsi );

  if( lpGData == NULL )
  {
    return DPERR_CANTADDPLAYER; /* yes player not group */
  }

  /* Something else is referencing this data */
  lpGData->uRef++;

  DP_SetGroupData( lpGData, DPSET_REMOTE, lpData, dwDataSize );

  /* The list has now been inserted into the interface group list. We now
     need to put a "shortcut" to this group in the parent group */
  lpGList = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpGList ) );
  if( lpGList == NULL )
  {
    FIXME( "Memory leak\n" );
    return DPERR_CANTADDPLAYER; /* yes player not group */
  }

  lpGList->lpGData = lpGData;

  DPQ_INSERT( lpGParentData->groups, lpGList, groups );

  /* Let the SP know that we've created this group */
  if( This->dp2->spData.lpCB->CreateGroup )
  {
    DPSP_CREATEGROUPDATA data;

    TRACE( "Calling SP CreateGroup\n" );

    data.idGroup           = *lpidGroup;
    data.dwFlags           = dwFlags;
    data.lpSPMessageHeader = lpMsgHdr;
    data.lpISP             = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->CreateGroup)( &data );
  }

  /* Inform all other peers of the creation of a new group. If there are
   * no peers keep this quiet.
   */
  if( This->dp2->lpSessionDesc &&
      ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_MULTICASTSERVER ) )
  {
    DPMSG_CREATEPLAYERORGROUP msg;

    msg.dwType = DPSYS_CREATEPLAYERORGROUP;
    msg.dwPlayerType = DPPLAYERTYPE_GROUP;
    msg.dpId = *lpidGroup;
    msg.dwCurrentPlayers = idParentGroup; /* FIXME: Incorrect? */
    msg.lpData = lpData;
    msg.dwDataSize = dwDataSize;
    msg.dpnName = *lpGroupName;

    /* FIXME: Correct to just use send effectively? */
    /* FIXME: Should size include data w/ message or just message "header" */
    /* FIXME: Check return code */
    DP_SendEx( (IDirectPlay2Impl*)This,
               DPID_SERVERPLAYER, DPID_ALLPLAYERS, 0, &msg, sizeof( msg ),
               0, 0, NULL, NULL, bAnsi );
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_CreateGroupInGroup
          ( LPDIRECTPLAY3A iface, DPID idParentGroup, LPDPID lpidGroup,
            LPDPNAME lpGroupName, LPVOID lpData, DWORD dwDataSize,
            DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( lpidGroup == NULL ||
      !This->dp2->bConnectionOpen ||
      dwDataSize >= MAXDWORD ||
      ( lpData == NULL && dwDataSize != 0 ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  *lpidGroup = DPID_UNKNOWN;

  return DP_IF_CreateGroupInGroup( This, NULL, idParentGroup, lpidGroup,
                                   lpGroupName, lpData, dwDataSize, dwFlags,
                                   TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_CreateGroupInGroup
          ( LPDIRECTPLAY3 iface, DPID idParentGroup, LPDPID lpidGroup,
            LPDPNAME lpGroupName, LPVOID lpData, DWORD dwDataSize,
            DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( lpidGroup == NULL ||
      !This->dp2->bConnectionOpen ||
      dwDataSize >= MAXDWORD ||
      ( lpData == NULL && dwDataSize != 0 ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  *lpidGroup = DPID_UNKNOWN;

  return DP_IF_CreateGroupInGroup( This, NULL, idParentGroup, lpidGroup,
                                   lpGroupName, lpData, dwDataSize,
                                   dwFlags, FALSE );
}

static HRESULT DP_IF_DeleteGroupFromGroup
          ( IDirectPlay3Impl* This, DPID idParentGroup, DPID idGroup )
{
  lpGroupList lpGList;
  lpGroupData lpGParentData;

  TRACE("(%p)->(0x%08x,0x%08x)\n", This, idParentGroup, idGroup );

  /* Is the parent group valid? */
  if( ( lpGParentData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idParentGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Remove the group from the parent group queue */
  DPQ_REMOVE_ENTRY( lpGParentData->groups, groups, lpGData->dpid, ==, idGroup, lpGList );

  if( lpGList == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Decrement the ref count */
  lpGList->lpGData->uRef--;

  /* Free up the list item */
  HeapFree( GetProcessHeap(), 0, lpGList );

  /* Should send a DELETEGROUPFROMGROUP message */
  FIXME( "message not sent\n" );

  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_DeleteGroupFromGroup
          ( LPDIRECTPLAY3 iface, DPID idParentGroup, DPID idGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_DeleteGroupFromGroup( This, idParentGroup, idGroup );
}

static HRESULT WINAPI DirectPlay3WImpl_DeleteGroupFromGroup
          ( LPDIRECTPLAY3 iface, DPID idParentGroup, DPID idGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_DeleteGroupFromGroup( This, idParentGroup, idGroup );
}
static BOOL DP_BuildCompoundAddr( GUID guidDataType,
                                  LPGUID lpcSpGuid,
                                  LPVOID* lplpAddrBuf,
                                  LPDWORD lpdwBufSize )
{
  DPCOMPOUNDADDRESSELEMENT dpCompoundAddress;
  HRESULT                  hr;

  dpCompoundAddress.dwDataSize = sizeof( GUID );
  dpCompoundAddress.guidDataType = guidDataType;
  dpCompoundAddress.lpData = lpcSpGuid;

  *lplpAddrBuf = NULL;
  *lpdwBufSize = 0;

  hr = DPL_CreateCompoundAddress( &dpCompoundAddress, 1, *lplpAddrBuf,
                                  lpdwBufSize, TRUE );

  if( hr != DPERR_BUFFERTOOSMALL )
  {
    ERR( "can't get buffer size: %s\n", DPLAYX_HresultToString( hr ) );
    return FALSE;
  }

  /* Now allocate the buffer */
  *lplpAddrBuf = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                            *lpdwBufSize );

  hr = DPL_CreateCompoundAddress( &dpCompoundAddress, 1, *lplpAddrBuf,
                                  lpdwBufSize, TRUE );
  if( FAILED(hr) )
  {
    ERR( "can't create address: %s\n", DPLAYX_HresultToString( hr ) );
    return FALSE;
  }

  return TRUE;
}

static HRESULT WINAPI DP_IF_EnumConnections
          ( LPDIRECTPLAY3A iface, LPCGUID lpguidApplication,
            LPDPENUMCONNECTIONSCALLBACK lpEnumCallback, LPVOID lpContext,
            DWORD dwFlags, BOOL bAnsi )
{
  HKEY hkResult;
  WCHAR searchSubKeySP[] = {
    'S', 'O', 'F', 'T', 'W', 'A', 'R', 'E', '\\',
    'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', '\\',
    'D', 'i', 'r', 'e', 'c', 't', 'P', 'l', 'a', 'y', '\\',
    'S', 'e', 'r', 'v', 'i', 'c', 'e', ' ', 'P', 'r', 'o', 'v', 'i', 'd', 'e', 'r', 's', 0 };
  WCHAR searchSubKeyLP[] = {
    'S', 'O', 'F', 'T', 'W', 'A', 'R', 'E', '\\',
    'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', '\\',
    'D', 'i', 'r', 'e', 'c', 't', 'P', 'l', 'a', 'y', '\\',
    'L', 'o', 'b', 'b', 'y', ' ', 'P', 'r', 'o', 'v', 'i', 'd', 'e', 'r', 's', 0 };
  WCHAR guidDataSubKey[] = { 'G', 'u', 'i', 'd', 0 };
  WCHAR subKeyName[255]; /* 255 is the maximum key size according to MSDN */
  DWORD dwIndex, sizeOfSubKeyName = sizeof(subKeyName) / sizeof(WCHAR);
  FILETIME filetime;

  /* A default dwFlags (0) is backwards compatible -- DPCONNECTION_DIRECTPLAY */
  if( dwFlags == 0 )
  {
    dwFlags = DPCONNECTION_DIRECTPLAY;
  }

  if( ! ( ( dwFlags & DPCONNECTION_DIRECTPLAY ) ||
          ( dwFlags & DPCONNECTION_DIRECTPLAYLOBBY ) ) )
  {
    return DPERR_INVALIDFLAGS;
  }

  if( !lpEnumCallback )
  {
    return DPERR_INVALIDPARAMS;
  }


  /* Need to loop over the service providers in the registry */
  if( RegOpenKeyExW( HKEY_LOCAL_MACHINE,
                     ( dwFlags & DPCONNECTION_DIRECTPLAY ) ? searchSubKeySP
                                                           : searchSubKeyLP,
                     0, KEY_READ, &hkResult ) != ERROR_SUCCESS )
  {
    /* Hmmm. Does this mean that there are no service providers? */
    ERR(": no service providers?\n");
    return DP_OK;
  }


  /* Traverse all the service providers we have available */
  for( dwIndex=0;
       RegEnumKeyExW( hkResult, dwIndex, subKeyName, &sizeOfSubKeyName,
                      NULL, NULL, NULL, &filetime ) != ERROR_NO_MORE_ITEMS;
       ++dwIndex, sizeOfSubKeyName = sizeof(subKeyName) / sizeof(WCHAR) )
  {
    HKEY     hkServiceProvider;
    GUID     serviceProviderGUID;
    WCHAR    guidKeyContent[39];
    DWORD    sizeOfReturnBuffer = sizeof(guidKeyContent);
    DPNAME   dpName;
    BOOL     continueEnumeration = TRUE;

    LPVOID   lpAddressBuffer = NULL;
    DWORD    dwAddressBufferSize = 0;


    TRACE(" this time through: %s\n", debugstr_w(subKeyName) );

    /* Get a handle for this particular service provider */
    if( RegOpenKeyExW( hkResult, subKeyName, 0, KEY_READ,
                       &hkServiceProvider ) != ERROR_SUCCESS )
    {
      ERR(": what the heck is going on?\n" );
      continue;
    }

    if( RegQueryValueExW( hkServiceProvider, guidDataSubKey,
                          NULL, NULL, (LPBYTE)guidKeyContent,
                          &sizeOfReturnBuffer ) != ERROR_SUCCESS )
    {
      ERR(": missing GUID registry data members\n" );
      RegCloseKey(hkServiceProvider);
      continue;
    }
    RegCloseKey(hkServiceProvider);

    CLSIDFromString( guidKeyContent, &serviceProviderGUID );

    /* Fill in the DPNAME struct for the service provider */
    dpName.dwSize = sizeof( dpName );
    dpName.dwFlags = 0;
    if ( bAnsi )
    {
      dpName.u1.lpszShortNameA = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                            sizeOfSubKeyName+1 );
      WideCharToMultiByte( CP_ACP, 0, subKeyName,
                           -1, dpName.u1.lpszShortNameA, -1, 0, 0);
      dpName.u2.lpszLongNameA = NULL;
    }
    else
    {
      dpName.u1.lpszShortName = subKeyName;
      dpName.u2.lpszLongName = NULL;
    }

    /* Create the compound address for the service provider.
     * NOTE: This is a gruesome architectural scar right now.  DP
     *       uses DPL and DPL uses DP.  Nasty stuff. This may be why the
     *       native dll just gets around this little bit by allocating an
     *       80 byte buffer which isn't even filled with a valid compound
     *       address. Oh well. Creating a proper compound address is the
     *       way to go anyways despite this method taking slightly more
     *       heap space and realtime :) */

    if ( DP_BuildCompoundAddr( ( ( dwFlags & DPCONNECTION_DIRECTPLAY )
                                 ? DPAID_ServiceProvider
                                 : DPAID_LobbyProvider ),
                               &serviceProviderGUID,
                               &lpAddressBuffer,
                               &dwAddressBufferSize ) )
    {
      /* The enumeration will return FALSE if we are not to continue */
      continueEnumeration = lpEnumCallback( &serviceProviderGUID, lpAddressBuffer,
                                            dwAddressBufferSize, &dpName,
                                            dwFlags, lpContext );
    }
    else
    {
      ERR( "Couldn't build compound address\n" );
    }

    HeapFree( GetProcessHeap(), 0, lpAddressBuffer );
    if ( bAnsi )
      HeapFree( GetProcessHeap(), 0, dpName.u1.lpszShortNameA );

    if (!continueEnumeration)
      return DP_OK;
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_EnumConnections
          ( LPDIRECTPLAY3A iface, LPCGUID lpguidApplication, LPDPENUMCONNECTIONSCALLBACK lpEnumCallback, LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  TRACE("(%p)->(%p,%p,%p,0x%08x)\n", This, lpguidApplication, lpEnumCallback, lpContext, dwFlags );
  return DP_IF_EnumConnections( iface, lpguidApplication, lpEnumCallback, lpContext, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_EnumConnections
          ( LPDIRECTPLAY3 iface, LPCGUID lpguidApplication, LPDPENUMCONNECTIONSCALLBACK lpEnumCallback, LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  TRACE("(%p)->(%p,%p,%p,0x%08x)\n", This, lpguidApplication, lpEnumCallback, lpContext, dwFlags );
  return DP_IF_EnumConnections( iface, lpguidApplication, lpEnumCallback, lpContext, dwFlags, FALSE );
}

static HRESULT DP_IF_EnumGroupsInGroup
          ( IDirectPlay3AImpl* This, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi )
{
  lpGroupList lpGList;
  lpGroupData lpGData;

  FIXME( "(%p)->(0x%08x,%p,%p,%p,0x%08x,%u): semi stub\n",
         This, idGroup, lpguidInstance, lpEnumPlayersCallback2,
         lpContext, dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( !This->dp2->bConnectionOpen )
  {
    return DPERR_NOSESSIONS;
  }

  if( ( lpEnumPlayersCallback2 == NULL ) ||
      ( ( dwFlags & DPENUMGROUPS_SESSION ) && ( lpguidInstance == NULL ) ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  if( ( lpGData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  if( DPQ_IS_EMPTY( lpGData->groups ) )
  {
    return DP_OK;
  }

  lpGList = DPQ_FIRST( lpGData->groups );

  for( ;; )
  {
    /* FIXME: Should check dwFlags for match here */

    if( !(*lpEnumPlayersCallback2)( lpGList->lpGData->dpid, DPPLAYERTYPE_GROUP,
                                    &lpGList->lpGData->name, dwFlags,
                                    lpContext ) )
    {
      return DP_OK; /* User requested break */
    }

    if( DPQ_IS_ENDOFLIST( lpGList->groups ) )
    {
      break;
    }

    lpGList = DPQ_NEXT( lpGList->groups );

  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_EnumGroupsInGroup
          ( LPDIRECTPLAY3A iface, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2, LPVOID lpContext,
            DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_EnumGroupsInGroup( This, idGroup, lpguidInstance,
                                  lpEnumPlayersCallback2, lpContext, dwFlags,
                                  TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_EnumGroupsInGroup
          ( LPDIRECTPLAY3A iface, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2, LPVOID lpContext,
            DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_EnumGroupsInGroup( This, idGroup, lpguidInstance,
                                  lpEnumPlayersCallback2, lpContext, dwFlags,
                                  FALSE );
}

static HRESULT WINAPI DirectPlay3AImpl_GetGroupConnectionSettings
          ( LPDIRECTPLAY3A iface, DWORD dwFlags, DPID idGroup, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x,%p,%p): stub\n", This, dwFlags, idGroup, lpData, lpdwDataSize );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3WImpl_GetGroupConnectionSettings
          ( LPDIRECTPLAY3 iface, DWORD dwFlags, DPID idGroup, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x,%p,%p): stub\n", This, dwFlags, idGroup, lpData, lpdwDataSize );
  return DP_OK;
}

static BOOL CALLBACK DP_GetSpLpGuidFromCompoundAddress(
    REFGUID         guidDataType,
    DWORD           dwDataSize,
    LPCVOID         lpData,
    LPVOID          lpContext )
{
  /* Looking for the GUID of the provider to load */
  if( ( IsEqualGUID( guidDataType, &DPAID_ServiceProvider ) ) ||
      ( IsEqualGUID( guidDataType, &DPAID_LobbyProvider ) )
    )
  {
    TRACE( "Found SP/LP (%s) %s (data size = 0x%08x)\n",
           debugstr_guid( guidDataType ), debugstr_guid( lpData ), dwDataSize );

    if( dwDataSize != sizeof( GUID ) )
    {
      ERR( "Invalid sp/lp guid size 0x%08x\n", dwDataSize );
    }

    memcpy( lpContext, lpData, dwDataSize );

    /* There shouldn't be more than 1 GUID/compound address */
    return FALSE;
  }

  /* Still waiting for what we want */
  return TRUE;
}


/* Find and perform a LoadLibrary on the requested SP or LP GUID */
static HMODULE DP_LoadSP( LPCGUID lpcGuid, LPSPINITDATA lpSpData, LPBOOL lpbIsDpSp )
{
  UINT i;
  LPCSTR spSubKey         = "SOFTWARE\\Microsoft\\DirectPlay\\Service Providers";
  LPCSTR lpSubKey         = "SOFTWARE\\Microsoft\\DirectPlay\\Lobby Providers";
  LPCSTR guidDataSubKey   = "Guid";
  LPCSTR majVerDataSubKey = "dwReserved1";
  LPCSTR minVerDataSubKey = "dwReserved2";
  LPCSTR pathSubKey       = "Path";

  TRACE( " request to load %s\n", debugstr_guid( lpcGuid ) );

  /* FIXME: Cloned code with a quick hack. */
  for( i=0; i<2; i++ )
  {
    HKEY hkResult;
    LPCSTR searchSubKey;
    char subKeyName[51];
    DWORD dwIndex, sizeOfSubKeyName=50;
    FILETIME filetime;

    (i == 0) ? (searchSubKey = spSubKey ) : (searchSubKey = lpSubKey );
    *lpbIsDpSp = (i == 0) ? TRUE : FALSE;


    /* Need to loop over the service providers in the registry */
    if( RegOpenKeyExA( HKEY_LOCAL_MACHINE, searchSubKey,
                         0, KEY_READ, &hkResult ) != ERROR_SUCCESS )
    {
      /* Hmmm. Does this mean that there are no service providers? */
      ERR(": no service providers?\n");
      return 0;
    }

    /* Traverse all the service providers we have available */
    for( dwIndex=0;
         RegEnumKeyExA( hkResult, dwIndex, subKeyName, &sizeOfSubKeyName,
                        NULL, NULL, NULL, &filetime ) != ERROR_NO_MORE_ITEMS;
         ++dwIndex, sizeOfSubKeyName=51 )
    {

      HKEY     hkServiceProvider;
      GUID     serviceProviderGUID;
      DWORD    returnType, sizeOfReturnBuffer = 255;
      char     returnBuffer[256];
      WCHAR    buff[51];
      DWORD    dwTemp, len;

      TRACE(" this time through: %s\n", subKeyName );

      /* Get a handle for this particular service provider */
      if( RegOpenKeyExA( hkResult, subKeyName, 0, KEY_READ,
                         &hkServiceProvider ) != ERROR_SUCCESS )
      {
         ERR(": what the heck is going on?\n" );
         continue;
      }

      if( RegQueryValueExA( hkServiceProvider, guidDataSubKey,
                            NULL, &returnType, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) != ERROR_SUCCESS )
      {
        ERR(": missing GUID registry data members\n" );
        continue;
      }

      /* FIXME: Check return types to ensure we're interpreting data right */
      MultiByteToWideChar( CP_ACP, 0, returnBuffer, -1, buff, sizeof(buff)/sizeof(WCHAR) );
      CLSIDFromString( buff, &serviceProviderGUID );
      /* FIXME: Have I got a memory leak on the serviceProviderGUID? */

      /* Determine if this is the Service Provider that the user asked for */
      if( !IsEqualGUID( &serviceProviderGUID, lpcGuid ) )
      {
        continue;
      }

      if( i == 0 ) /* DP SP */
      {
        len = MultiByteToWideChar( CP_ACP, 0, subKeyName, -1, NULL, 0 );
        lpSpData->lpszName = HeapAlloc( GetProcessHeap(), 0, len*sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, subKeyName, -1, lpSpData->lpszName, len );
      }

      sizeOfReturnBuffer = 255;

      /* Get dwReserved1 */
      if( RegQueryValueExA( hkServiceProvider, majVerDataSubKey,
                            NULL, &returnType, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) != ERROR_SUCCESS )
      {
         ERR(": missing dwReserved1 registry data members\n") ;
         continue;
      }

      if( i == 0 )
          memcpy( &lpSpData->dwReserved1, returnBuffer, sizeof(lpSpData->dwReserved1) );

      sizeOfReturnBuffer = 255;

      /* Get dwReserved2 */
      if( RegQueryValueExA( hkServiceProvider, minVerDataSubKey,
                            NULL, &returnType, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) != ERROR_SUCCESS )
      {
         ERR(": missing dwReserved1 registry data members\n") ;
         continue;
      }

      if( i == 0 )
          memcpy( &lpSpData->dwReserved2, returnBuffer, sizeof(lpSpData->dwReserved2) );

      sizeOfReturnBuffer = 255;

      /* Get the path for this service provider */
      if( ( dwTemp = RegQueryValueExA( hkServiceProvider, pathSubKey,
                            NULL, NULL, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) ) != ERROR_SUCCESS )
      {
        ERR(": missing PATH registry data members: 0x%08x\n", dwTemp );
        continue;
      }

      TRACE( "Loading %s\n", returnBuffer );
      return LoadLibraryA( returnBuffer );
    }
  }

  return 0;
}

static
HRESULT DP_InitializeDPSP( IDirectPlay3Impl* This, HMODULE hServiceProvider )
{
  HRESULT hr;
  LPDPSP_SPINIT SPInit;

  /* Initialize the service provider by calling SPInit */
  SPInit = (LPDPSP_SPINIT)GetProcAddress( hServiceProvider, "SPInit" );

  if( SPInit == NULL )
  {
    ERR( "Service provider doesn't provide SPInit interface?\n" );
    FreeLibrary( hServiceProvider );
    return DPERR_UNAVAILABLE;
  }

  TRACE( "Calling SPInit (DP SP entry point)\n" );

  hr = (*SPInit)( &This->dp2->spData );

  if( FAILED(hr) )
  {
    ERR( "DP SP Initialization failed: %s\n", DPLAYX_HresultToString(hr) );
    FreeLibrary( hServiceProvider );
    return hr;
  }

  /* FIXME: Need to verify the sanity of the returned callback table
   *        using IsBadCodePtr */
  This->dp2->bSPInitialized = TRUE;

  /* This interface is now initialized as a DP object */
  This->dp2->connectionInitialized = DP_SERVICE_PROVIDER;

  /* Store the handle of the module so that we can unload it later */
  This->dp2->hServiceProvider = hServiceProvider;

  return hr;
}

static
HRESULT DP_InitializeDPLSP( IDirectPlay3Impl* This, HMODULE hLobbyProvider )
{
  HRESULT hr;
  LPSP_INIT DPLSPInit;

  /* Initialize the service provider by calling SPInit */
  DPLSPInit = (LPSP_INIT)GetProcAddress( hLobbyProvider, "DPLSPInit" );

  if( DPLSPInit == NULL )
  {
    ERR( "Service provider doesn't provide DPLSPInit interface?\n" );
    FreeLibrary( hLobbyProvider );
    return DPERR_UNAVAILABLE;
  }

  TRACE( "Calling DPLSPInit (DPL SP entry point)\n" );

  hr = (*DPLSPInit)( &This->dp2->dplspData );

  if( FAILED(hr) )
  {
    ERR( "DPL SP Initialization failed: %s\n", DPLAYX_HresultToString(hr) );
    FreeLibrary( hLobbyProvider );
    return hr;
  }

  /* FIXME: Need to verify the sanity of the returned callback table
   *        using IsBadCodePtr */

  This->dp2->bDPLSPInitialized = TRUE;

  /* This interface is now initialized as a lobby object */
  This->dp2->connectionInitialized = DP_LOBBY_PROVIDER;

  /* Store the handle of the module so that we can unload it later */
  This->dp2->hDPLobbyProvider = hLobbyProvider;

  return hr;
}

static HRESULT DP_IF_InitializeConnection
          ( IDirectPlay3Impl* This, LPVOID lpConnection, DWORD dwFlags, BOOL bAnsi )
{
  HMODULE hServiceProvider;
  HRESULT hr;
  GUID guidSP;
  const DWORD dwAddrSize = 80; /* FIXME: Need to calculate it correctly */
  BOOL bIsDpSp; /* TRUE if Direct Play SP, FALSE if Direct Play Lobby SP */

  TRACE("(%p)->(%p,0x%08x,%u)\n", This, lpConnection, dwFlags, bAnsi );

  if ( lpConnection == NULL )
  {
    return DPERR_INVALIDPARAMS;
  }

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  if( This->dp2->connectionInitialized != NO_PROVIDER )
  {
    return DPERR_ALREADYINITIALIZED;
  }

  /* Find out what the requested SP is and how large this buffer is */
  hr = DPL_EnumAddress( DP_GetSpLpGuidFromCompoundAddress, lpConnection,
                        dwAddrSize, &guidSP );

  if( FAILED(hr) )
  {
    ERR( "Invalid compound address?\n" );
    return DPERR_UNAVAILABLE;
  }

  /* Load the service provider */
  hServiceProvider = DP_LoadSP( &guidSP, &This->dp2->spData, &bIsDpSp );

  if( hServiceProvider == 0 )
  {
    ERR( "Unable to load service provider %s\n", debugstr_guid(&guidSP) );
    return DPERR_UNAVAILABLE;
  }

  if( bIsDpSp )
  {
     /* Fill in what we can of the Service Provider required information.
      * The rest was be done in DP_LoadSP
      */
     This->dp2->spData.lpAddress = lpConnection;
     This->dp2->spData.dwAddressSize = dwAddrSize;
     This->dp2->spData.lpGuid = &guidSP;

     hr = DP_InitializeDPSP( This, hServiceProvider );
  }
  else
  {
     This->dp2->dplspData.lpAddress = lpConnection;

     hr = DP_InitializeDPLSP( This, hServiceProvider );
  }

  if( FAILED(hr) )
  {
    return hr;
  }

  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_InitializeConnection
          ( LPDIRECTPLAY3A iface, LPVOID lpConnection, DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_InitializeConnection( This, lpConnection, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_InitializeConnection
          ( LPDIRECTPLAY3 iface, LPVOID lpConnection, DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_InitializeConnection( This, lpConnection, dwFlags, FALSE );
}

static HRESULT WINAPI DirectPlay3AImpl_SecureOpen
          ( LPDIRECTPLAY3A iface, LPCDPSESSIONDESC2 lpsd, DWORD dwFlags,
            LPCDPSECURITYDESC lpSecurity, LPCDPCREDENTIALS lpCredentials )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface; /* Yes a dp 2 interface */
  return DP_SecureOpen( This, lpsd, dwFlags, lpSecurity, lpCredentials, TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_SecureOpen
          ( LPDIRECTPLAY3 iface, LPCDPSESSIONDESC2 lpsd, DWORD dwFlags,
            LPCDPSECURITYDESC lpSecurity, LPCDPCREDENTIALS lpCredentials )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface; /* Yes a dp 2 interface */
  return DP_SecureOpen( This, lpsd, dwFlags, lpSecurity, lpCredentials, FALSE );
}

static HRESULT WINAPI DirectPlay3AImpl_SendChatMessage
          ( LPDIRECTPLAY3A iface, DPID idFrom, DPID idTo, DWORD dwFlags, LPDPCHAT lpChatMessage )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x,0x%08x,%p): stub\n", This, idFrom, idTo, dwFlags, lpChatMessage );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3WImpl_SendChatMessage
          ( LPDIRECTPLAY3 iface, DPID idFrom, DPID idTo, DWORD dwFlags, LPDPCHAT lpChatMessage )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x,0x%08x,%p): stub\n", This, idFrom, idTo, dwFlags, lpChatMessage );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_SetGroupConnectionSettings
          ( LPDIRECTPLAY3A iface, DWORD dwFlags, DPID idGroup, LPDPLCONNECTION lpConnection )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x,%p): stub\n", This, dwFlags, idGroup, lpConnection );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3WImpl_SetGroupConnectionSettings
          ( LPDIRECTPLAY3 iface, DWORD dwFlags, DPID idGroup, LPDPLCONNECTION lpConnection )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x,%p): stub\n", This, dwFlags, idGroup, lpConnection );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_StartSession
          ( LPDIRECTPLAY3A iface, DWORD dwFlags, DPID idGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x): stub\n", This, dwFlags, idGroup );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3WImpl_StartSession
          ( LPDIRECTPLAY3 iface, DWORD dwFlags, DPID idGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x): stub\n", This, dwFlags, idGroup );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_GetGroupFlags
          ( LPDIRECTPLAY3A iface, DPID idGroup, LPDWORD lpdwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idGroup, lpdwFlags );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3WImpl_GetGroupFlags
          ( LPDIRECTPLAY3 iface, DPID idGroup, LPDWORD lpdwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idGroup, lpdwFlags );
  return DP_OK;
}

static HRESULT DP_IF_GetGroupParent
          ( IDirectPlay3AImpl* This, DPID idGroup, LPDPID lpidGroup,
            BOOL bAnsi )
{
  lpGroupData lpGData;

  TRACE("(%p)->(0x%08x,%p,%u)\n", This, idGroup, lpidGroup, bAnsi );

  if( ( lpGData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  *lpidGroup = lpGData->dpid;

  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_GetGroupParent
          ( LPDIRECTPLAY3A iface, DPID idGroup, LPDPID lpidGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_GetGroupParent( This, idGroup, lpidGroup, TRUE );
}
static HRESULT WINAPI DirectPlay3WImpl_GetGroupParent
          ( LPDIRECTPLAY3 iface, DPID idGroup, LPDPID lpidGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_GetGroupParent( This, idGroup, lpidGroup, FALSE );
}

static HRESULT WINAPI DirectPlay3AImpl_GetPlayerAccount
          ( LPDIRECTPLAY3A iface, DPID idPlayer, DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x,%p,%p): stub\n", This, idPlayer, dwFlags, lpData, lpdwDataSize );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3WImpl_GetPlayerAccount
          ( LPDIRECTPLAY3 iface, DPID idPlayer, DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x,%p,%p): stub\n", This, idPlayer, dwFlags, lpData, lpdwDataSize );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3AImpl_GetPlayerFlags
          ( LPDIRECTPLAY3A iface, DPID idPlayer, LPDWORD lpdwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idPlayer, lpdwFlags );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay3WImpl_GetPlayerFlags
          ( LPDIRECTPLAY3 iface, DPID idPlayer, LPDWORD lpdwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idPlayer, lpdwFlags );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay4AImpl_GetGroupOwner
          ( LPDIRECTPLAY4A iface, DPID idGroup, LPDPID lpidGroupOwner )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idGroup, lpidGroupOwner );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay4WImpl_GetGroupOwner
          ( LPDIRECTPLAY4 iface, DPID idGroup, LPDPID lpidGroupOwner )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idGroup, lpidGroupOwner );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay4AImpl_SetGroupOwner
          ( LPDIRECTPLAY4A iface, DPID idGroup , DPID idGroupOwner )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x): stub\n", This, idGroup, idGroupOwner );
  return DP_OK;
}

static HRESULT WINAPI DirectPlay4WImpl_SetGroupOwner
          ( LPDIRECTPLAY4 iface, DPID idGroup , DPID idGroupOwner )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;
  FIXME("(%p)->(0x%08x,0x%08x): stub\n", This, idGroup, idGroupOwner );
  return DP_OK;
}

static HRESULT DP_SendEx
          ( IDirectPlay2Impl* This, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID, BOOL bAnsi )
{
  lpGroupData  lpGData;
  BOOL         bSendToGroup;

  TRACE( "(%p)->(0x%08x,0x%08x,0x%08x,%p,0x%08x,0x%08x,0x%08x,%p,%p,%u)\n",
         This, idFrom, idTo, dwFlags, lpData, dwDataSize, dwPriority,
         dwTimeout, lpContext, lpdwMsgID, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  /* FIXME: Add parameter checking */
  /* FIXME: First call to this needs to acquire a message id which will be
   *        used for multiple sends
   */

  /* NOTE: Can't send messages to yourself - this will be trapped in receive */

  /* Check idFrom points to a valid player */
  if ( DP_FindPlayer( This, idFrom ) == NULL )
  {
    ERR( "Invalid from player 0x%08x\n", idFrom );
    return DPERR_INVALIDPLAYER;
  }

  /* Check idTo: it can be a player or a group */
  if( idTo == DPID_ALLPLAYERS )
  {
    bSendToGroup = TRUE;
    lpGData = This->dp2->lpSysGroup;
  }
  else if ( DP_FindPlayer( This, idTo ) )
  {
    bSendToGroup = FALSE;
  }
  else if ( ( lpGData = DP_FindAnyGroup( This, idTo ) ) )
  {
    bSendToGroup = TRUE;
  }
  else
  {
    return DPERR_INVALIDPLAYER;
  }

  if ( bSendToGroup )
  {
#if 0
    DPSP_SENDTOGROUPEXDATA data;

    data.lpISP         = This->dp2->spData.lpISP;
    data.dwFlags       = dwFlags;
    data.idGroupTo     = idTo;
    data.idPlayerFrom  = idFrom;
    data.lpSendBuffers = lpData;
    data.cBuffers      = NULL;
    data.dwMessageSize = dwDataSize;
    data.dwPriority    = dwPriority;
    data.dwTimeout     = dwTimeout;
    data.lpDPContext   = lpContext;
    data.lpdwSPMsgID   = lpdwMsgID;

    return (*This->dp2->spData.lpCB->SendToGroupEx)( &data );
#endif
    DPSP_SENDDATA data;
    lpPlayerList lpPList;

    data.dwFlags        = dwFlags;
    data.idPlayerFrom   = idFrom;
    data.lpMessage      = lpData;
    data.dwMessageSize  = dwDataSize;
    data.bSystemMessage = FALSE;
    data.lpISP          = This->dp2->spData.lpISP;

    if ( (lpPList = DPQ_FIRST( lpGData->players )) )
    {
      do
      {
        if ( ~lpPList->lpPData->dwFlags & DPLAYI_PLAYER_PLAYERLOCAL )
        {
          data.idPlayerTo = lpPList->lpPData->dpid;
          (*This->dp2->spData.lpCB->Send)( &data );
        }
      }
      while( (lpPList = DPQ_NEXT( lpPList->players )) );
    }
  }
  else
  {
    DPSP_SENDDATA data;

    data.dwFlags        = dwFlags;
    data.idPlayerFrom   = idFrom;
    data.idPlayerTo     = idTo;
    data.lpMessage      = lpData;
    data.dwMessageSize  = dwDataSize;
    data.bSystemMessage = FALSE;
    data.lpISP          = This->dp2->spData.lpISP;

    return (*This->dp2->spData.lpCB->Send)( &data );
  }

  /* Have the service provider send this message */
  /*wat?
    return DP_SP_SendEx( This, dwFlags, lpData, dwDataSize, dwPriority,
    dwTimeout, lpContext, lpdwMsgID );*/

  return DP_OK;
}


static HRESULT WINAPI DirectPlay4AImpl_SendEx
          ( LPDIRECTPLAY4A iface, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface; /* yes downcast to 2 */
  return DP_SendEx( This, idFrom, idTo, dwFlags, lpData, dwDataSize,
                    dwPriority, dwTimeout, lpContext, lpdwMsgID, TRUE );
}

static HRESULT WINAPI DirectPlay4WImpl_SendEx
          ( LPDIRECTPLAY4 iface, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface; /* yes downcast to 2 */
  return DP_SendEx( This, idFrom, idTo, dwFlags, lpData, dwDataSize,
                    dwPriority, dwTimeout, lpContext, lpdwMsgID, FALSE );
}

static HRESULT DP_SP_SendEx
          ( IDirectPlay2Impl* This, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID )
{
  LPDPMSG lpMElem;

  FIXME( ": stub\n" );

  /* FIXME: This queuing should only be for async messages */

  lpMElem = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpMElem ) );
  lpMElem->msg = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwDataSize );

  CopyMemory( lpMElem->msg, lpData, dwDataSize );

  /* FIXME: Need to queue based on priority */
  DPQ_INSERT( This->dp2->sendMsgs, lpMElem, msgs );

  return DP_OK;
}

static HRESULT DP_IF_GetMessageQueue
          ( IDirectPlay4Impl* This, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes, BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  FIXME( "(%p)->(0x%08x,0x%08x,0x%08x,%p,%p,%u): semi stub\n",
         This, idFrom, idTo, dwFlags, lpdwNumMsgs, lpdwNumBytes, bAnsi );

  /* FIXME: Do we need to do idFrom and idTo sanity checking here? */
  /* FIXME: What about sends which are not immediate? */

  if( This->dp2->spData.lpCB->GetMessageQueue )
  {
    DPSP_GETMESSAGEQUEUEDATA data;

    FIXME( "Calling SP GetMessageQueue - is it right?\n" );

    /* FIXME: None of this is documented :( */

    data.lpISP        = This->dp2->spData.lpISP;
    data.dwFlags      = dwFlags;
    data.idFrom       = idFrom;
    data.idTo         = idTo;
    data.lpdwNumMsgs  = lpdwNumMsgs;
    data.lpdwNumBytes = lpdwNumBytes;

    hr = (*This->dp2->spData.lpCB->GetMessageQueue)( &data );
  }
  else
  {
    FIXME( "No SP for GetMessageQueue - fake some data\n" );
  }

  return hr;
}

static HRESULT WINAPI DirectPlay4AImpl_GetMessageQueue
          ( LPDIRECTPLAY4A iface, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;
  return DP_IF_GetMessageQueue( This, idFrom, idTo, dwFlags, lpdwNumMsgs,
                                lpdwNumBytes, TRUE );
}

static HRESULT WINAPI DirectPlay4WImpl_GetMessageQueue
          ( LPDIRECTPLAY4 iface, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;
  return DP_IF_GetMessageQueue( This, idFrom, idTo, dwFlags, lpdwNumMsgs,
                                lpdwNumBytes, FALSE );
}

static HRESULT DP_IF_CancelMessage
          ( IDirectPlay4Impl* This, DWORD dwMsgID, DWORD dwFlags,
            DWORD dwMinPriority, DWORD dwMaxPriority, BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  FIXME( "(%p)->(0x%08x,0x%08x,%u): semi stub\n",
         This, dwMsgID, dwFlags, bAnsi );

  if( This->dp2->spData.lpCB->Cancel )
  {
    DPSP_CANCELDATA data;

    TRACE( "Calling SP Cancel\n" );

    /* FIXME: Undocumented callback */

    data.lpISP          = This->dp2->spData.lpISP;
    data.dwFlags        = dwFlags;
    data.lprglpvSPMsgID = NULL;
    data.cSPMsgID       = dwMsgID;
    data.dwMinPriority  = dwMinPriority;
    data.dwMaxPriority  = dwMaxPriority;

    hr = (*This->dp2->spData.lpCB->Cancel)( &data );
  }
  else
  {
    FIXME( "SP doesn't implement Cancel\n" );
  }

  return hr;
}

static HRESULT WINAPI DirectPlay4AImpl_CancelMessage
          ( LPDIRECTPLAY4A iface, DWORD dwMsgID, DWORD dwFlags )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  if( dwMsgID == 0 )
  {
    dwFlags |= DPCANCELSEND_ALL;
  }

  return DP_IF_CancelMessage( This, dwMsgID, dwFlags, 0, 0, TRUE );
}

static HRESULT WINAPI DirectPlay4WImpl_CancelMessage
          ( LPDIRECTPLAY4 iface, DWORD dwMsgID, DWORD dwFlags )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  if( dwMsgID == 0 )
  {
    dwFlags |= DPCANCELSEND_ALL;
  }

  return DP_IF_CancelMessage( This, dwMsgID, dwFlags, 0, 0, FALSE );
}

static HRESULT WINAPI DirectPlay4AImpl_CancelPriority
          ( LPDIRECTPLAY4A iface, DWORD dwMinPriority, DWORD dwMaxPriority,
            DWORD dwFlags )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  return DP_IF_CancelMessage( This, 0, DPCANCELSEND_PRIORITY, dwMinPriority,
                              dwMaxPriority, TRUE );
}

static HRESULT WINAPI DirectPlay4WImpl_CancelPriority
          ( LPDIRECTPLAY4 iface, DWORD dwMinPriority, DWORD dwMaxPriority,
            DWORD dwFlags )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  return DP_IF_CancelMessage( This, 0, DPCANCELSEND_PRIORITY, dwMinPriority,
                              dwMaxPriority, FALSE );
}

/* Note: Hack so we can reuse the old functions without compiler warnings */
#if !defined(__STRICT_ANSI__) && defined(__GNUC__)
# define XCAST(fun)     (typeof(directPlay2WVT.fun))
#else
# define XCAST(fun)     (void*)
#endif

static const IDirectPlay2Vtbl directPlay2WVT =
{
  XCAST(QueryInterface)DP_QueryInterface,
  XCAST(AddRef)DP_AddRef,
  XCAST(Release)DP_Release,

  DirectPlay2WImpl_AddPlayerToGroup,
  DirectPlay2WImpl_Close,
  DirectPlay2WImpl_CreateGroup,
  DirectPlay2WImpl_CreatePlayer,
  DirectPlay2WImpl_DeletePlayerFromGroup,
  DirectPlay2WImpl_DestroyGroup,
  DirectPlay2WImpl_DestroyPlayer,
  DirectPlay2WImpl_EnumGroupPlayers,
  DirectPlay2WImpl_EnumGroups,
  DirectPlay2WImpl_EnumPlayers,
  DirectPlay2WImpl_EnumSessions,
  DirectPlay2WImpl_GetCaps,
  DirectPlay2WImpl_GetGroupData,
  DirectPlay2WImpl_GetGroupName,
  DirectPlay2WImpl_GetMessageCount,
  DirectPlay2WImpl_GetPlayerAddress,
  DirectPlay2WImpl_GetPlayerCaps,
  DirectPlay2WImpl_GetPlayerData,
  DirectPlay2WImpl_GetPlayerName,
  DirectPlay2WImpl_GetSessionDesc,
  DirectPlay2WImpl_Initialize,
  DirectPlay2WImpl_Open,
  DirectPlay2WImpl_Receive,
  DirectPlay2WImpl_Send,
  DirectPlay2WImpl_SetGroupData,
  DirectPlay2WImpl_SetGroupName,
  DirectPlay2WImpl_SetPlayerData,
  DirectPlay2WImpl_SetPlayerName,
  DirectPlay2WImpl_SetSessionDesc
};
#undef XCAST

/* Note: Hack so we can reuse the old functions without compiler warnings */
#if !defined(__STRICT_ANSI__) && defined(__GNUC__)
# define XCAST(fun)     (typeof(directPlay2AVT.fun))
#else
# define XCAST(fun)     (void*)
#endif

static const IDirectPlay2Vtbl directPlay2AVT =
{
  XCAST(QueryInterface)DP_QueryInterface,
  XCAST(AddRef)DP_AddRef,
  XCAST(Release)DP_Release,

  DirectPlay2AImpl_AddPlayerToGroup,
  DirectPlay2AImpl_Close,
  DirectPlay2AImpl_CreateGroup,
  DirectPlay2AImpl_CreatePlayer,
  DirectPlay2AImpl_DeletePlayerFromGroup,
  DirectPlay2AImpl_DestroyGroup,
  DirectPlay2AImpl_DestroyPlayer,
  DirectPlay2AImpl_EnumGroupPlayers,
  DirectPlay2AImpl_EnumGroups,
  DirectPlay2AImpl_EnumPlayers,
  DirectPlay2AImpl_EnumSessions,
  DirectPlay2AImpl_GetCaps,
  DirectPlay2AImpl_GetGroupData,
  DirectPlay2AImpl_GetGroupName,
  DirectPlay2AImpl_GetMessageCount,
  DirectPlay2AImpl_GetPlayerAddress,
  DirectPlay2AImpl_GetPlayerCaps,
  DirectPlay2AImpl_GetPlayerData,
  DirectPlay2AImpl_GetPlayerName,
  DirectPlay2AImpl_GetSessionDesc,
  DirectPlay2AImpl_Initialize,
  DirectPlay2AImpl_Open,
  DirectPlay2AImpl_Receive,
  DirectPlay2AImpl_Send,
  DirectPlay2AImpl_SetGroupData,
  DirectPlay2AImpl_SetGroupName,
  DirectPlay2AImpl_SetPlayerData,
  DirectPlay2AImpl_SetPlayerName,
  DirectPlay2AImpl_SetSessionDesc
};
#undef XCAST


/* Note: Hack so we can reuse the old functions without compiler warnings */
#if !defined(__STRICT_ANSI__) && defined(__GNUC__)
# define XCAST(fun)     (typeof(directPlay3AVT.fun))
#else
# define XCAST(fun)     (void*)
#endif

static const IDirectPlay3Vtbl directPlay3AVT =
{
  XCAST(QueryInterface)DP_QueryInterface,
  XCAST(AddRef)DP_AddRef,
  XCAST(Release)DP_Release,

  XCAST(AddPlayerToGroup)DirectPlay2AImpl_AddPlayerToGroup,
  XCAST(Close)DirectPlay2AImpl_Close,
  XCAST(CreateGroup)DirectPlay2AImpl_CreateGroup,
  XCAST(CreatePlayer)DirectPlay2AImpl_CreatePlayer,
  XCAST(DeletePlayerFromGroup)DirectPlay2AImpl_DeletePlayerFromGroup,
  XCAST(DestroyGroup)DirectPlay2AImpl_DestroyGroup,
  XCAST(DestroyPlayer)DirectPlay2AImpl_DestroyPlayer,
  XCAST(EnumGroupPlayers)DirectPlay2AImpl_EnumGroupPlayers,
  XCAST(EnumGroups)DirectPlay2AImpl_EnumGroups,
  XCAST(EnumPlayers)DirectPlay2AImpl_EnumPlayers,
  XCAST(EnumSessions)DirectPlay2AImpl_EnumSessions,
  XCAST(GetCaps)DirectPlay2AImpl_GetCaps,
  XCAST(GetGroupData)DirectPlay2AImpl_GetGroupData,
  XCAST(GetGroupName)DirectPlay2AImpl_GetGroupName,
  XCAST(GetMessageCount)DirectPlay2AImpl_GetMessageCount,
  XCAST(GetPlayerAddress)DirectPlay2AImpl_GetPlayerAddress,
  XCAST(GetPlayerCaps)DirectPlay2AImpl_GetPlayerCaps,
  XCAST(GetPlayerData)DirectPlay2AImpl_GetPlayerData,
  XCAST(GetPlayerName)DirectPlay2AImpl_GetPlayerName,
  XCAST(GetSessionDesc)DirectPlay2AImpl_GetSessionDesc,
  XCAST(Initialize)DirectPlay2AImpl_Initialize,
  XCAST(Open)DirectPlay2AImpl_Open,
  XCAST(Receive)DirectPlay2AImpl_Receive,
  XCAST(Send)DirectPlay2AImpl_Send,
  XCAST(SetGroupData)DirectPlay2AImpl_SetGroupData,
  XCAST(SetGroupName)DirectPlay2AImpl_SetGroupName,
  XCAST(SetPlayerData)DirectPlay2AImpl_SetPlayerData,
  XCAST(SetPlayerName)DirectPlay2AImpl_SetPlayerName,
  XCAST(SetSessionDesc)DirectPlay2AImpl_SetSessionDesc,

  DirectPlay3AImpl_AddGroupToGroup,
  DirectPlay3AImpl_CreateGroupInGroup,
  DirectPlay3AImpl_DeleteGroupFromGroup,
  DirectPlay3AImpl_EnumConnections,
  DirectPlay3AImpl_EnumGroupsInGroup,
  DirectPlay3AImpl_GetGroupConnectionSettings,
  DirectPlay3AImpl_InitializeConnection,
  DirectPlay3AImpl_SecureOpen,
  DirectPlay3AImpl_SendChatMessage,
  DirectPlay3AImpl_SetGroupConnectionSettings,
  DirectPlay3AImpl_StartSession,
  DirectPlay3AImpl_GetGroupFlags,
  DirectPlay3AImpl_GetGroupParent,
  DirectPlay3AImpl_GetPlayerAccount,
  DirectPlay3AImpl_GetPlayerFlags
};
#undef XCAST

/* Note: Hack so we can reuse the old functions without compiler warnings */
#if !defined(__STRICT_ANSI__) && defined(__GNUC__)
# define XCAST(fun)     (typeof(directPlay3WVT.fun))
#else
# define XCAST(fun)     (void*)
#endif
static const IDirectPlay3Vtbl directPlay3WVT =
{
  XCAST(QueryInterface)DP_QueryInterface,
  XCAST(AddRef)DP_AddRef,
  XCAST(Release)DP_Release,

  XCAST(AddPlayerToGroup)DirectPlay2WImpl_AddPlayerToGroup,
  XCAST(Close)DirectPlay2WImpl_Close,
  XCAST(CreateGroup)DirectPlay2WImpl_CreateGroup,
  XCAST(CreatePlayer)DirectPlay2WImpl_CreatePlayer,
  XCAST(DeletePlayerFromGroup)DirectPlay2WImpl_DeletePlayerFromGroup,
  XCAST(DestroyGroup)DirectPlay2WImpl_DestroyGroup,
  XCAST(DestroyPlayer)DirectPlay2WImpl_DestroyPlayer,
  XCAST(EnumGroupPlayers)DirectPlay2WImpl_EnumGroupPlayers,
  XCAST(EnumGroups)DirectPlay2WImpl_EnumGroups,
  XCAST(EnumPlayers)DirectPlay2WImpl_EnumPlayers,
  XCAST(EnumSessions)DirectPlay2WImpl_EnumSessions,
  XCAST(GetCaps)DirectPlay2WImpl_GetCaps,
  XCAST(GetGroupData)DirectPlay2WImpl_GetGroupData,
  XCAST(GetGroupName)DirectPlay2WImpl_GetGroupName,
  XCAST(GetMessageCount)DirectPlay2WImpl_GetMessageCount,
  XCAST(GetPlayerAddress)DirectPlay2WImpl_GetPlayerAddress,
  XCAST(GetPlayerCaps)DirectPlay2WImpl_GetPlayerCaps,
  XCAST(GetPlayerData)DirectPlay2WImpl_GetPlayerData,
  XCAST(GetPlayerName)DirectPlay2WImpl_GetPlayerName,
  XCAST(GetSessionDesc)DirectPlay2WImpl_GetSessionDesc,
  XCAST(Initialize)DirectPlay2WImpl_Initialize,
  XCAST(Open)DirectPlay2WImpl_Open,
  XCAST(Receive)DirectPlay2WImpl_Receive,
  XCAST(Send)DirectPlay2WImpl_Send,
  XCAST(SetGroupData)DirectPlay2WImpl_SetGroupData,
  XCAST(SetGroupName)DirectPlay2WImpl_SetGroupName,
  XCAST(SetPlayerData)DirectPlay2WImpl_SetPlayerData,
  XCAST(SetPlayerName)DirectPlay2WImpl_SetPlayerName,
  XCAST(SetSessionDesc)DirectPlay2WImpl_SetSessionDesc,

  DirectPlay3WImpl_AddGroupToGroup,
  DirectPlay3WImpl_CreateGroupInGroup,
  DirectPlay3WImpl_DeleteGroupFromGroup,
  DirectPlay3WImpl_EnumConnections,
  DirectPlay3WImpl_EnumGroupsInGroup,
  DirectPlay3WImpl_GetGroupConnectionSettings,
  DirectPlay3WImpl_InitializeConnection,
  DirectPlay3WImpl_SecureOpen,
  DirectPlay3WImpl_SendChatMessage,
  DirectPlay3WImpl_SetGroupConnectionSettings,
  DirectPlay3WImpl_StartSession,
  DirectPlay3WImpl_GetGroupFlags,
  DirectPlay3WImpl_GetGroupParent,
  DirectPlay3WImpl_GetPlayerAccount,
  DirectPlay3WImpl_GetPlayerFlags
};
#undef XCAST

/* Note: Hack so we can reuse the old functions without compiler warnings */
#if !defined(__STRICT_ANSI__) && defined(__GNUC__)
# define XCAST(fun)     (typeof(directPlay4WVT.fun))
#else
# define XCAST(fun)     (void*)
#endif
static const IDirectPlay4Vtbl directPlay4WVT =
{
  XCAST(QueryInterface)DP_QueryInterface,
  XCAST(AddRef)DP_AddRef,
  XCAST(Release)DP_Release,

  XCAST(AddPlayerToGroup)DirectPlay2WImpl_AddPlayerToGroup,
  XCAST(Close)DirectPlay2WImpl_Close,
  XCAST(CreateGroup)DirectPlay2WImpl_CreateGroup,
  XCAST(CreatePlayer)DirectPlay2WImpl_CreatePlayer,
  XCAST(DeletePlayerFromGroup)DirectPlay2WImpl_DeletePlayerFromGroup,
  XCAST(DestroyGroup)DirectPlay2WImpl_DestroyGroup,
  XCAST(DestroyPlayer)DirectPlay2WImpl_DestroyPlayer,
  XCAST(EnumGroupPlayers)DirectPlay2WImpl_EnumGroupPlayers,
  XCAST(EnumGroups)DirectPlay2WImpl_EnumGroups,
  XCAST(EnumPlayers)DirectPlay2WImpl_EnumPlayers,
  XCAST(EnumSessions)DirectPlay2WImpl_EnumSessions,
  XCAST(GetCaps)DirectPlay2WImpl_GetCaps,
  XCAST(GetGroupData)DirectPlay2WImpl_GetGroupData,
  XCAST(GetGroupName)DirectPlay2WImpl_GetGroupName,
  XCAST(GetMessageCount)DirectPlay2WImpl_GetMessageCount,
  XCAST(GetPlayerAddress)DirectPlay2WImpl_GetPlayerAddress,
  XCAST(GetPlayerCaps)DirectPlay2WImpl_GetPlayerCaps,
  XCAST(GetPlayerData)DirectPlay2WImpl_GetPlayerData,
  XCAST(GetPlayerName)DirectPlay2WImpl_GetPlayerName,
  XCAST(GetSessionDesc)DirectPlay2WImpl_GetSessionDesc,
  XCAST(Initialize)DirectPlay2WImpl_Initialize,
  XCAST(Open)DirectPlay2WImpl_Open,
  XCAST(Receive)DirectPlay2WImpl_Receive,
  XCAST(Send)DirectPlay2WImpl_Send,
  XCAST(SetGroupData)DirectPlay2WImpl_SetGroupData,
  XCAST(SetGroupName)DirectPlay2WImpl_SetGroupName,
  XCAST(SetPlayerData)DirectPlay2WImpl_SetPlayerData,
  XCAST(SetPlayerName)DirectPlay2WImpl_SetPlayerName,
  XCAST(SetSessionDesc)DirectPlay2WImpl_SetSessionDesc,

  XCAST(AddGroupToGroup)DirectPlay3WImpl_AddGroupToGroup,
  XCAST(CreateGroupInGroup)DirectPlay3WImpl_CreateGroupInGroup,
  XCAST(DeleteGroupFromGroup)DirectPlay3WImpl_DeleteGroupFromGroup,
  XCAST(EnumConnections)DirectPlay3WImpl_EnumConnections,
  XCAST(EnumGroupsInGroup)DirectPlay3WImpl_EnumGroupsInGroup,
  XCAST(GetGroupConnectionSettings)DirectPlay3WImpl_GetGroupConnectionSettings,
  XCAST(InitializeConnection)DirectPlay3WImpl_InitializeConnection,
  XCAST(SecureOpen)DirectPlay3WImpl_SecureOpen,
  XCAST(SendChatMessage)DirectPlay3WImpl_SendChatMessage,
  XCAST(SetGroupConnectionSettings)DirectPlay3WImpl_SetGroupConnectionSettings,
  XCAST(StartSession)DirectPlay3WImpl_StartSession,
  XCAST(GetGroupFlags)DirectPlay3WImpl_GetGroupFlags,
  XCAST(GetGroupParent)DirectPlay3WImpl_GetGroupParent,
  XCAST(GetPlayerAccount)DirectPlay3WImpl_GetPlayerAccount,
  XCAST(GetPlayerFlags)DirectPlay3WImpl_GetPlayerFlags,

  DirectPlay4WImpl_GetGroupOwner,
  DirectPlay4WImpl_SetGroupOwner,
  DirectPlay4WImpl_SendEx,
  DirectPlay4WImpl_GetMessageQueue,
  DirectPlay4WImpl_CancelMessage,
  DirectPlay4WImpl_CancelPriority
};
#undef XCAST


/* Note: Hack so we can reuse the old functions without compiler warnings */
#if !defined(__STRICT_ANSI__) && defined(__GNUC__)
# define XCAST(fun)     (typeof(directPlay4AVT.fun))
#else
# define XCAST(fun)     (void*)
#endif
static const IDirectPlay4Vtbl directPlay4AVT =
{
  XCAST(QueryInterface)DP_QueryInterface,
  XCAST(AddRef)DP_AddRef,
  XCAST(Release)DP_Release,

  XCAST(AddPlayerToGroup)DirectPlay2AImpl_AddPlayerToGroup,
  XCAST(Close)DirectPlay2AImpl_Close,
  XCAST(CreateGroup)DirectPlay2AImpl_CreateGroup,
  XCAST(CreatePlayer)DirectPlay2AImpl_CreatePlayer,
  XCAST(DeletePlayerFromGroup)DirectPlay2AImpl_DeletePlayerFromGroup,
  XCAST(DestroyGroup)DirectPlay2AImpl_DestroyGroup,
  XCAST(DestroyPlayer)DirectPlay2AImpl_DestroyPlayer,
  XCAST(EnumGroupPlayers)DirectPlay2AImpl_EnumGroupPlayers,
  XCAST(EnumGroups)DirectPlay2AImpl_EnumGroups,
  XCAST(EnumPlayers)DirectPlay2AImpl_EnumPlayers,
  XCAST(EnumSessions)DirectPlay2AImpl_EnumSessions,
  XCAST(GetCaps)DirectPlay2AImpl_GetCaps,
  XCAST(GetGroupData)DirectPlay2AImpl_GetGroupData,
  XCAST(GetGroupName)DirectPlay2AImpl_GetGroupName,
  XCAST(GetMessageCount)DirectPlay2AImpl_GetMessageCount,
  XCAST(GetPlayerAddress)DirectPlay2AImpl_GetPlayerAddress,
  XCAST(GetPlayerCaps)DirectPlay2AImpl_GetPlayerCaps,
  XCAST(GetPlayerData)DirectPlay2AImpl_GetPlayerData,
  XCAST(GetPlayerName)DirectPlay2AImpl_GetPlayerName,
  XCAST(GetSessionDesc)DirectPlay2AImpl_GetSessionDesc,
  XCAST(Initialize)DirectPlay2AImpl_Initialize,
  XCAST(Open)DirectPlay2AImpl_Open,
  XCAST(Receive)DirectPlay2AImpl_Receive,
  XCAST(Send)DirectPlay2AImpl_Send,
  XCAST(SetGroupData)DirectPlay2AImpl_SetGroupData,
  XCAST(SetGroupName)DirectPlay2AImpl_SetGroupName,
  XCAST(SetPlayerData)DirectPlay2AImpl_SetPlayerData,
  XCAST(SetPlayerName)DirectPlay2AImpl_SetPlayerName,
  XCAST(SetSessionDesc)DirectPlay2AImpl_SetSessionDesc,

  XCAST(AddGroupToGroup)DirectPlay3AImpl_AddGroupToGroup,
  XCAST(CreateGroupInGroup)DirectPlay3AImpl_CreateGroupInGroup,
  XCAST(DeleteGroupFromGroup)DirectPlay3AImpl_DeleteGroupFromGroup,
  XCAST(EnumConnections)DirectPlay3AImpl_EnumConnections,
  XCAST(EnumGroupsInGroup)DirectPlay3AImpl_EnumGroupsInGroup,
  XCAST(GetGroupConnectionSettings)DirectPlay3AImpl_GetGroupConnectionSettings,
  XCAST(InitializeConnection)DirectPlay3AImpl_InitializeConnection,
  XCAST(SecureOpen)DirectPlay3AImpl_SecureOpen,
  XCAST(SendChatMessage)DirectPlay3AImpl_SendChatMessage,
  XCAST(SetGroupConnectionSettings)DirectPlay3AImpl_SetGroupConnectionSettings,
  XCAST(StartSession)DirectPlay3AImpl_StartSession,
  XCAST(GetGroupFlags)DirectPlay3AImpl_GetGroupFlags,
  XCAST(GetGroupParent)DirectPlay3AImpl_GetGroupParent,
  XCAST(GetPlayerAccount)DirectPlay3AImpl_GetPlayerAccount,
  XCAST(GetPlayerFlags)DirectPlay3AImpl_GetPlayerFlags,

  DirectPlay4AImpl_GetGroupOwner,
  DirectPlay4AImpl_SetGroupOwner,
  DirectPlay4AImpl_SendEx,
  DirectPlay4AImpl_GetMessageQueue,
  DirectPlay4AImpl_CancelMessage,
  DirectPlay4AImpl_CancelPriority
};
#undef XCAST

HRESULT DP_GetSPPlayerData( IDirectPlay2Impl* lpDP,
                            DPID idPlayer,
                            LPVOID* lplpData )
{
  lpPlayerList lpPlayer = DP_FindPlayer( lpDP, idPlayer );

  if( lpPlayer == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  *lplpData = lpPlayer->lpPData->lpSPPlayerData;

  return DP_OK;
}

HRESULT DP_SetSPPlayerData( IDirectPlay2Impl* lpDP,
                            DPID idPlayer,
                            LPVOID lpData )
{
  lpPlayerList lpPlayer = DP_FindPlayer( lpDP, idPlayer );

  if( lpPlayer == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  lpPlayer->lpPData->lpSPPlayerData = lpData;

  return DP_OK;
}

/***************************************************************************
 *  DirectPlayEnumerateAW
 *
 *  The pointer to the structure lpContext will be filled with the
 *  appropriate data for each service offered by the OS. These services are
 *  not necessarily available on this particular machine but are defined
 *  as simple service providers under the "Service Providers" registry key.
 *  This structure is then passed to lpEnumCallback for each of the different
 *  services.
 *
 *  This API is useful only for applications written using DirectX3 or
 *  worse. It is superseded by IDirectPlay3::EnumConnections which also
 *  gives information on the actual connections.
 *
 * defn of a service provider:
 * A dynamic-link library used by DirectPlay to communicate over a network.
 * The service provider contains all the network-specific code required
 * to send and receive messages. Online services and network operators can
 * supply service providers to use specialized hardware, protocols, communications
 * media, and network resources.
 *
 */
static HRESULT DirectPlayEnumerateAW(LPDPENUMDPCALLBACKA lpEnumCallbackA,
                                     LPDPENUMDPCALLBACKW lpEnumCallbackW,
                                     LPVOID lpContext)
{
    HKEY   hkResult;
    static const WCHAR searchSubKey[] = {
	'S', 'O', 'F', 'T', 'W', 'A', 'R', 'E', '\\',
	'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', '\\',
	'D', 'i', 'r', 'e', 'c', 't', 'P', 'l', 'a', 'y', '\\',
	'S', 'e', 'r', 'v', 'i', 'c', 'e', ' ', 'P', 'r', 'o', 'v', 'i', 'd', 'e', 'r', 's', 0 };
    static const WCHAR guidKey[] = { 'G', 'u', 'i', 'd', 0 };
    static const WCHAR descW[] = { 'D', 'e', 's', 'c', 'r', 'i', 'p', 't', 'i', 'o', 'n', 'W', 0 };
    
    DWORD  dwIndex;
    FILETIME filetime;

    char  *descriptionA = NULL;
    DWORD max_sizeOfDescriptionA = 0;
    WCHAR *descriptionW = NULL;
    DWORD max_sizeOfDescriptionW = 0;
    
    if (!lpEnumCallbackA && !lpEnumCallbackW)
    {
	return DPERR_INVALIDPARAMS;
    }
    
    /* Need to loop over the service providers in the registry */
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, searchSubKey,
		      0, KEY_READ, &hkResult) != ERROR_SUCCESS)
    {
	/* Hmmm. Does this mean that there are no service providers? */
	ERR(": no service provider key in the registry - check your Wine installation !!!\n");
	return DPERR_GENERIC;
    }
    
    /* Traverse all the service providers we have available */
    dwIndex = 0;
    while (1)
    {
	WCHAR subKeyName[255]; /* 255 is the maximum key size according to MSDN */
	DWORD sizeOfSubKeyName = sizeof(subKeyName) / sizeof(WCHAR);
	HKEY  hkServiceProvider;
	GUID  serviceProviderGUID;
	WCHAR guidKeyContent[(2 * 16) + 1 + 6 /* This corresponds to '{....-..-..-..-......}' */ ];
	DWORD sizeOfGuidKeyContent = sizeof(guidKeyContent);
	LONG  ret_value;
	
	ret_value = RegEnumKeyExW(hkResult, dwIndex, subKeyName, &sizeOfSubKeyName,
				  NULL, NULL, NULL, &filetime);
	if (ret_value == ERROR_NO_MORE_ITEMS)
	    break;
	else if (ret_value != ERROR_SUCCESS)
	{
	    ERR(": could not enumerate on service provider key.\n");
	    return DPERR_EXCEPTION;
	}
	TRACE(" this time through sub-key %s.\n", debugstr_w(subKeyName));
	
	/* Open the key for this service provider */
	if (RegOpenKeyExW(hkResult, subKeyName, 0, KEY_READ, &hkServiceProvider) != ERROR_SUCCESS)
	{
	    ERR(": could not open registry key for service provider %s.\n", debugstr_w(subKeyName));
	    continue;
	}
	
	/* Get the GUID from the registry */
	if (RegQueryValueExW(hkServiceProvider, guidKey,
			     NULL, NULL, (LPBYTE) guidKeyContent, &sizeOfGuidKeyContent) != ERROR_SUCCESS)
	{
	    ERR(": missing GUID registry data member for service provider %s.\n", debugstr_w(subKeyName));
	    continue;
	}
	if (sizeOfGuidKeyContent != sizeof(guidKeyContent))
	{
	    ERR(": invalid format for the GUID registry data member for service provider %s (%s).\n", debugstr_w(subKeyName), debugstr_w(guidKeyContent));
	    continue;
	}
	CLSIDFromString(guidKeyContent, &serviceProviderGUID );
	
	/* The enumeration will return FALSE if we are not to continue.
	 *
	 * Note: on my windows box, major / minor version is 6 / 0 for all service providers
	 *       and have no relation to any of the two dwReserved1 and dwReserved2 keys.
	 *       I think that it simply means that they are in-line with DirectX 6.0
	 */
	if (lpEnumCallbackA)
	{
	    DWORD sizeOfDescription = 0;
	    
	    /* Note that this is the A case of this function, so use the A variant to get the description string */
	    if (RegQueryValueExA(hkServiceProvider, "DescriptionA",
				 NULL, NULL, NULL, &sizeOfDescription) != ERROR_SUCCESS)
	    {
		ERR(": missing 'DescriptionA' registry data member for service provider %s.\n", debugstr_w(subKeyName));
		continue;
	    }
	    if (sizeOfDescription > max_sizeOfDescriptionA)
	    {
		HeapFree(GetProcessHeap(), 0, descriptionA);
		max_sizeOfDescriptionA = sizeOfDescription;
	    }
	    descriptionA = HeapAlloc(GetProcessHeap(), 0, sizeOfDescription);
	    RegQueryValueExA(hkServiceProvider, "DescriptionA",
			     NULL, NULL, (LPBYTE) descriptionA, &sizeOfDescription);
	    
	    if (!lpEnumCallbackA(&serviceProviderGUID, descriptionA, 6, 0, lpContext))
		goto end;
	}
	else
	{
	    DWORD sizeOfDescription = 0;
	    
	    if (RegQueryValueExW(hkServiceProvider, descW,
				 NULL, NULL, NULL, &sizeOfDescription) != ERROR_SUCCESS)
	    {
		ERR(": missing 'DescriptionW' registry data member for service provider %s.\n", debugstr_w(subKeyName));
		continue;
	    }
	    if (sizeOfDescription > max_sizeOfDescriptionW)
	    {
		HeapFree(GetProcessHeap(), 0, descriptionW);
		max_sizeOfDescriptionW = sizeOfDescription;
	    }
	    descriptionW = HeapAlloc(GetProcessHeap(), 0, sizeOfDescription);
	    RegQueryValueExW(hkServiceProvider, descW,
			     NULL, NULL, (LPBYTE) descriptionW, &sizeOfDescription);

	    if (!lpEnumCallbackW(&serviceProviderGUID, descriptionW, 6, 0, lpContext))
		goto end;
	}
      
      dwIndex++;
  }

 end:
    HeapFree(GetProcessHeap(), 0, descriptionA);
    HeapFree(GetProcessHeap(), 0, descriptionW);
    
    return DP_OK;
}

/***************************************************************************
 *  DirectPlayEnumerate  [DPLAYX.9]
 *  DirectPlayEnumerateA [DPLAYX.2]
 */
HRESULT WINAPI DirectPlayEnumerateA(LPDPENUMDPCALLBACKA lpEnumCallback, LPVOID lpContext )
{
    TRACE("(%p,%p)\n", lpEnumCallback, lpContext);
    
    return DirectPlayEnumerateAW(lpEnumCallback, NULL, lpContext);
}

/***************************************************************************
 *  DirectPlayEnumerateW [DPLAYX.3]
 */
HRESULT WINAPI DirectPlayEnumerateW(LPDPENUMDPCALLBACKW lpEnumCallback, LPVOID lpContext )
{
    TRACE("(%p,%p)\n", lpEnumCallback, lpContext);
    
    return DirectPlayEnumerateAW(NULL, lpEnumCallback, lpContext);
}

typedef struct tagCreateEnum
{
  LPVOID  lpConn;
  LPCGUID lpGuid;
} CreateEnumData, *lpCreateEnumData;

/* Find and copy the matching connection for the SP guid */
static BOOL CALLBACK cbDPCreateEnumConnections(
    LPCGUID     lpguidSP,
    LPVOID      lpConnection,
    DWORD       dwConnectionSize,
    LPCDPNAME   lpName,
    DWORD       dwFlags,
    LPVOID      lpContext)
{
  lpCreateEnumData lpData = (lpCreateEnumData)lpContext;

  if( IsEqualGUID( lpguidSP, lpData->lpGuid ) )
  {
    TRACE( "Found SP entry with guid %s\n", debugstr_guid(lpData->lpGuid) );

    lpData->lpConn = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                dwConnectionSize );
    CopyMemory( lpData->lpConn, lpConnection, dwConnectionSize );

    /* Found the record that we were looking for */
    return FALSE;
  }

  /* Haven't found what were looking for yet */
  return TRUE;
}


/***************************************************************************
 *  DirectPlayCreate [DPLAYX.1]
 *
 */
HRESULT WINAPI DirectPlayCreate
( LPGUID lpGUID, LPDIRECTPLAY *lplpDP, IUnknown *pUnk )
{
  HRESULT hr;
  LPDIRECTPLAY3A lpDP3A;
  CreateEnumData cbData;

  TRACE( "lpGUID=%s lplpDP=%p pUnk=%p\n", debugstr_guid(lpGUID), lplpDP, pUnk );

  if( pUnk != NULL )
  {
    return CLASS_E_NOAGGREGATION;
  }

  if( (lplpDP == NULL) || (lpGUID == NULL) )
  {
    return DPERR_INVALIDPARAMS;
  }


  /* Create an IDirectPlay object. We don't support that so we'll cheat and
     give them an IDirectPlay2A object and hope that doesn't cause problems */
  if( DP_CreateInterface( &IID_IDirectPlay2A, (LPVOID*)lplpDP ) != DP_OK )
  {
    return DPERR_UNAVAILABLE;
  }

  if( IsEqualGUID( &GUID_NULL, lpGUID ) )
  {
    /* The GUID_NULL means don't bind a service provider. Just return the
       interface as is */
    return DP_OK;
  }

  /* Bind the desired service provider since lpGUID is non NULL */
  TRACE( "Service Provider binding for %s\n", debugstr_guid(lpGUID) );

  /* We're going to use a DP3 interface */
  hr = IDirectPlayX_QueryInterface( *lplpDP, &IID_IDirectPlay3A,
                                    (LPVOID*)&lpDP3A );
  if( FAILED(hr) )
  {
    ERR( "Failed to get DP3 interface: %s\n", DPLAYX_HresultToString(hr) );
    return hr;
  }

  cbData.lpConn = NULL;
  cbData.lpGuid = lpGUID;

  /* We were given a service provider, find info about it... */
  hr = IDirectPlayX_EnumConnections( lpDP3A, NULL, cbDPCreateEnumConnections,
                                     &cbData, DPCONNECTION_DIRECTPLAY );
  if( ( FAILED(hr) ) ||
      ( cbData.lpConn == NULL )
    )
  {
    ERR( "Failed to get Enum for SP: %s\n", DPLAYX_HresultToString(hr) );
    IDirectPlayX_Release( lpDP3A );
    return DPERR_UNAVAILABLE;
  }

  /* Initialize the service provider */
  hr = IDirectPlayX_InitializeConnection( lpDP3A, cbData.lpConn, 0 );
  if( FAILED(hr) )
  {
    ERR( "Failed to Initialize SP: %s\n", DPLAYX_HresultToString(hr) );
    HeapFree( GetProcessHeap(), 0, cbData.lpConn );
    IDirectPlayX_Release( lpDP3A );
    return hr;
  }

  /* Release our version of the interface now that we're done with it */
  IDirectPlayX_Release( lpDP3A );
  HeapFree( GetProcessHeap(), 0, cbData.lpConn );

  return DP_OK;
}
