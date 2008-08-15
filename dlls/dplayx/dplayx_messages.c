/* DirectPlay & DirectPlayLobby messaging implementation
 *
 * Copyright 2000,2001 - Peter Hunnisett
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
 *
 * NOTES
 *  o Messaging interface required for both DirectPlay and DirectPlayLobby.
 */

#include <stdarg.h>
#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "winnls.h"

#include "dplayx_messages.h"
#include "dplay_global.h"
#include "dplayx_global.h"
#include "name_server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dplay);

typedef struct tagMSGTHREADINFO
{
  HANDLE hStart;
  HANDLE hDeath;
  HANDLE hSettingRead;
  HANDLE hNotifyEvent;
} MSGTHREADINFO, *LPMSGTHREADINFO;

static DWORD CALLBACK DPL_MSG_ThreadMain( LPVOID lpContext );
static LPVOID DP_MSG_ExpectReply( IDirectPlay2AImpl* This, LPDPSP_SENDDATA data,
                                  DWORD dwWaitTime, WORD wReplyCommandId,
                                  LPVOID* lplpReplyMsg, LPDWORD lpdwMsgBodySize );
static DWORD DP_MSG_FillSuperPackedPlayer( IDirectPlay2Impl* lpDP,
                                           LPDPLAYI_SUPERPACKEDPLAYER lpPackedPlayer,
                                           LPVOID lpData,
                                           BOOL bIsGroup,
                                           BOOL bAnsi );
static DWORD DP_MSG_FillPackedPlayer( IDirectPlay2Impl* lpDP,
                                      LPDPLAYI_PACKEDPLAYER lpPackedPlayer,
                                      LPVOID lpData,
                                      BOOL bIsGroup,
                                      BOOL bAnsi );
DWORD DP_CopyString( LPVOID destination, LPVOID source, BOOL bAnsi );


/* Create the message reception thread to allow the application to receive
 * asynchronous message reception
 */
DWORD CreateLobbyMessageReceptionThread( HANDLE hNotifyEvent, HANDLE hStart,
                                         HANDLE hDeath, HANDLE hConnRead )
{
  DWORD           dwMsgThreadId;
  LPMSGTHREADINFO lpThreadInfo;

  lpThreadInfo = HeapAlloc( GetProcessHeap(), 0, sizeof( *lpThreadInfo ) );
  if( lpThreadInfo == NULL )
  {
    return 0;
  }

  /* The notify event may or may not exist. Depends if async comm or not */
  if( hNotifyEvent &&
      !DuplicateHandle( GetCurrentProcess(), hNotifyEvent,
                        GetCurrentProcess(), &lpThreadInfo->hNotifyEvent,
                        0, FALSE, DUPLICATE_SAME_ACCESS ) )
  {
    ERR( "Unable to duplicate event handle\n" );
    goto error;
  }

  /* These 3 handles don't need to be duplicated because we don't keep a
   * reference to them where they're created. They're created specifically
   * for the message thread
   */
  lpThreadInfo->hStart       = hStart;
  lpThreadInfo->hDeath       = hDeath;
  lpThreadInfo->hSettingRead = hConnRead;

  if( !CreateThread( NULL,                  /* Security attribs */
                     0,                     /* Stack */
                     DPL_MSG_ThreadMain,    /* Msg reception function */
                     lpThreadInfo,          /* Msg reception func parameter */
                     0,                     /* Flags */
                     &dwMsgThreadId         /* Updated with thread id */
                   )
    )
  {
    ERR( "Unable to create msg thread\n" );
    goto error;
  }

  /* FIXME: Should I be closing the handle to the thread or does that
            terminate the thread? */

  return dwMsgThreadId;

error:

  HeapFree( GetProcessHeap(), 0, lpThreadInfo );

  return 0;
}

static DWORD CALLBACK DPL_MSG_ThreadMain( LPVOID lpContext )
{
  LPMSGTHREADINFO lpThreadInfo = lpContext;
  DWORD dwWaitResult;

  TRACE( "Msg thread created. Waiting on app startup\n" );

  /* Wait to ensure that the lobby application is started w/ 1 min timeout */
  dwWaitResult = WaitForSingleObject( lpThreadInfo->hStart, 10000 /* 10 sec */ );
  if( dwWaitResult == WAIT_TIMEOUT )
  {
    FIXME( "Should signal app/wait creation failure (0x%08x)\n", dwWaitResult );
    goto end_of_thread;
  }

  /* Close this handle as it's not needed anymore */
  CloseHandle( lpThreadInfo->hStart );
  lpThreadInfo->hStart = 0;

  /* Wait until the lobby knows what it is */
  dwWaitResult = WaitForSingleObject( lpThreadInfo->hSettingRead, INFINITE );
  if( dwWaitResult == WAIT_TIMEOUT )
  {
    ERR( "App Read connection setting timeout fail (0x%08x)\n", dwWaitResult );
  }

  /* Close this handle as it's not needed anymore */
  CloseHandle( lpThreadInfo->hSettingRead );
  lpThreadInfo->hSettingRead = 0;

  TRACE( "App created && initialized starting main message reception loop\n" );

  for ( ;; )
  {
    MSG lobbyMsg;
    GetMessageW( &lobbyMsg, 0, 0, 0 );
  }

end_of_thread:
  TRACE( "Msg thread exiting!\n" );
  HeapFree( GetProcessHeap(), 0, lpThreadInfo );

  return 0;
}

/* DP messaging stuff */
static HANDLE DP_MSG_BuildAndLinkReplyStruct( IDirectPlay2Impl* This,
                                              LPDP_MSG_REPLY_STRUCT_LIST lpReplyStructList,
                                              WORD wReplyCommandId );
static LPVOID DP_MSG_CleanReplyStruct( LPDP_MSG_REPLY_STRUCT_LIST lpReplyStructList,
                                       LPVOID* lplpReplyMsg, LPDWORD lpdwMsgBodySize );


static
HANDLE DP_MSG_BuildAndLinkReplyStruct( IDirectPlay2Impl* This,
                                       LPDP_MSG_REPLY_STRUCT_LIST lpReplyStructList, WORD wReplyCommandId )
{
  lpReplyStructList->replyExpected.hReceipt       = CreateEventW( NULL, FALSE, FALSE, NULL );
  lpReplyStructList->replyExpected.wExpectedReply = wReplyCommandId;
  lpReplyStructList->replyExpected.lpReplyMsg     = NULL;
  lpReplyStructList->replyExpected.dwMsgBodySize  = 0;

  /* Insert into the message queue while locked */
  EnterCriticalSection( &This->unk->DP_lock );
    DPQ_INSERT( This->dp2->replysExpected, lpReplyStructList, replysExpected );
  LeaveCriticalSection( &This->unk->DP_lock );

  return lpReplyStructList->replyExpected.hReceipt;
}

static
LPVOID DP_MSG_CleanReplyStruct( LPDP_MSG_REPLY_STRUCT_LIST lpReplyStructList,
                                LPVOID* lplpReplyMsg, LPDWORD lpdwMsgBodySize  )
{
  CloseHandle( lpReplyStructList->replyExpected.hReceipt );

  *lplpReplyMsg    = lpReplyStructList->replyExpected.lpReplyMsg;
  *lpdwMsgBodySize = lpReplyStructList->replyExpected.dwMsgBodySize;

  return lpReplyStructList->replyExpected.lpReplyMsg;
}

HRESULT DP_MSG_SendRequestPlayerId( IDirectPlay2AImpl* This, DWORD dwFlags,
                                    LPDPID lpdpidAllocatedId )
{
  LPVOID                     lpMsg;
  LPDPSP_MSG_REQUESTPLAYERID lpMsgBody;
  DWORD                      dwMsgSize;
  HRESULT                    hr = DP_OK;

  dwMsgSize = This->dp2->spData.dwSPHeaderSize + sizeof( *lpMsgBody );

  lpMsg = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwMsgSize );

  lpMsgBody = (LPDPSP_MSG_REQUESTPLAYERID)( (LPBYTE)lpMsg +
                                            This->dp2->spData.dwSPHeaderSize );

  /* Compose dplay message envelope */
  lpMsgBody->envelope.dwMagic    = DPMSG_SIGNATURE;
  lpMsgBody->envelope.wCommandId = DPMSGCMD_REQUESTPLAYERID;
  lpMsgBody->envelope.wVersion   = DX61AVERSION;

  /* Compose the body of the message */
  lpMsgBody->Flags = dwFlags;

  /* Send the message */
  {
    DPSP_SENDDATA data;

    data.dwFlags        = DPSEND_GUARANTEED;
    data.idPlayerTo     = 0; /* Name server */
    data.idPlayerFrom   = 0; /* Sending from DP */
    data.lpMessage      = lpMsg;
    data.dwMessageSize  = dwMsgSize;
    data.bSystemMessage = TRUE; /* Allow reply to be sent */
    data.lpISP          = This->dp2->spData.lpISP;

    TRACE( "Asking for player id w/ Flags 0x%08x\n", lpMsgBody->Flags );

    lpMsg = DP_MSG_ExpectReply( This, &data,
                                DPMSG_RELIABLE_API_TIMER,
                                DPMSGCMD_REQUESTPLAYERREPLY,
                                &lpMsg, &dwMsgSize );
  }

  /* Examine reply */
  if( lpMsg )
  {
    *lpdpidAllocatedId = ((LPCDPSP_MSG_REQUESTPLAYERREPLY) lpMsg)->ID;
    TRACE( "Received new id 0x%08x\n", *lpdpidAllocatedId );
  }
  else
  {
    ERR( "Didn't receive reply\n" );
    hr = DPERR_GENERIC;
  }

  HeapFree( GetProcessHeap(), 0, lpMsg );
  return hr;
}

HRESULT DP_MSG_ForwardPlayerCreation( IDirectPlay2AImpl* This, DPID dpidServer )
{
  LPVOID                       lpMsg;
  LPDPSP_MSG_ADDFORWARDREQUEST lpMsgBody;
  DWORD                        dwMsgSize;
  HRESULT                      hr = DP_OK;

  dwMsgSize = This->dp2->spData.dwSPHeaderSize + sizeof( *lpMsgBody );

  lpMsg = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwMsgSize );

  lpMsgBody = (LPDPSP_MSG_ADDFORWARDREQUEST)( (LPBYTE)lpMsg +
                                              This->dp2->spData.dwSPHeaderSize );

  /* Compose dplay message envelope */
  lpMsgBody->envelope.dwMagic    = DPMSG_SIGNATURE;
  lpMsgBody->envelope.wCommandId = DPMSGCMD_ADDFORWARDREQUEST;
  lpMsgBody->envelope.wVersion   = DX61AVERSION;

  /* Compose body of message */
  FIXME( "TODO\n" );

  /* Send the message */
  {
    DPSP_SENDDATA data;

    data.dwFlags        = DPSEND_GUARANTEED;
    data.idPlayerTo     = 0; /* Name server */
    data.idPlayerFrom   = dpidServer; /* Sending from session server */
    data.lpMessage      = lpMsg;
    data.dwMessageSize  = dwMsgSize;
    data.bSystemMessage = TRUE; /* Allow reply to be sent */
    data.lpISP          = This->dp2->spData.lpISP;

    TRACE( "Sending forward player request with 0x%08x\n", dpidServer );

    lpMsg = DP_MSG_ExpectReply( This, &data,
                                DPMSG_RELIABLE_API_TIMER,
                                DPMSGCMD_ADDFORWARD,
                                &lpMsg, &dwMsgSize );
  }

  /* Need to examine the data and extract the new player id */
  if( lpMsg != NULL )
  {
    FIXME( "Name Table reply received: stub\n" );
  }

  return hr;
}

/* Queue up a structure indicating that we want a reply of type wReplyCommandId. DPlay does
 * not seem to offer any way of uniquely differentiating between replies of the same type
 * relative to the request sent. There is an implicit assumption that there will be no
 * ordering issues on sends and receives from the opposite machine. No wonder MS is not
 * a networking company.
 */
static
LPVOID DP_MSG_ExpectReply( IDirectPlay2AImpl* This, LPDPSP_SENDDATA lpData,
                           DWORD dwWaitTime, WORD wReplyCommandId,
                           LPVOID* lplpReplyMsg, LPDWORD lpdwMsgBodySize )
{
  HRESULT                  hr;
  HANDLE                   hMsgReceipt;
  DP_MSG_REPLY_STRUCT_LIST replyStructList;
  DWORD                    dwWaitReturn;
  WORD                     wCommandId;

  /* Setup for receipt */
  hMsgReceipt = DP_MSG_BuildAndLinkReplyStruct( This, &replyStructList,
                                                wReplyCommandId );

  wCommandId = ((LPCDPSP_MSG_ENVELOPE)
                ( ((LPBYTE) lpData->lpMessage) +
                  This->dp2->spData.dwSPHeaderSize ))->wCommandId;
  TRACE( "Sending cmd 0x%x and expecting cmd 0x%x in reply within %u ticks\n",
         wCommandId, wReplyCommandId, dwWaitTime );
  hr = (*This->dp2->spData.lpCB->Send)( lpData );

  if( FAILED(hr) )
  {
    ERR( "Send failed: %s\n", DPLAYX_HresultToString( hr ) );
    return NULL;
  }

  /* The reply message will trigger the hMsgReceipt event effectively switching
   * control back to this thread. See DP_MSG_ReplyReceived.
   */
  dwWaitReturn = WaitForSingleObject( hMsgReceipt, dwWaitTime );
  if( dwWaitReturn != WAIT_OBJECT_0 )
  {
    ERR( "Wait failed 0x%08x\n", dwWaitReturn );
    return NULL;
  }

  /* Clean Up */
  return DP_MSG_CleanReplyStruct( &replyStructList, lplpReplyMsg, lpdwMsgBodySize );
}

/* Determine if there is a matching request for this incoming message and then copy
 * all important data. It is quite silly to have to copy the message, but the documents
 * indicate that a copy is taken. Silly really.
 */
void DP_MSG_ReplyReceived( IDirectPlay2AImpl* This, WORD wCommandId,
                           LPCVOID lpcMsgBody, DWORD dwMsgBodySize )
{
  LPDP_MSG_REPLY_STRUCT_LIST lpReplyList;

  /* Find, and immediately remove (to avoid double triggering), the appropriate entry. Call locked to
   * avoid problems.
   */
  EnterCriticalSection( &This->unk->DP_lock );
    DPQ_REMOVE_ENTRY( This->dp2->replysExpected, replysExpected, replyExpected.wExpectedReply,
                     ==, wCommandId, lpReplyList );
  LeaveCriticalSection( &This->unk->DP_lock );

  if( lpReplyList != NULL )
  {
    lpReplyList->replyExpected.dwMsgBodySize = dwMsgBodySize;
    lpReplyList->replyExpected.lpReplyMsg = HeapAlloc( GetProcessHeap(),
                                                       HEAP_ZERO_MEMORY,
                                                       dwMsgBodySize );
    CopyMemory( lpReplyList->replyExpected.lpReplyMsg,
                lpcMsgBody, dwMsgBodySize );

    /* Signal the thread which sent the message that it has a reply */
    SetEvent( lpReplyList->replyExpected.hReceipt );
  }
  else
  {
    ERR( "No receipt event set for cmd 0x%x, only expecting in reply mode\n",
         wCommandId );
  };
}

DWORD DP_CopyString( LPVOID destination, LPVOID source, BOOL bAnsi )
{
  /* Copies an ASCII string (bAnsi=TRUE) or a wide string (bAnsi=FALSE)
   * from source to the previously allocated destination buffer.
   * The destination string will be always wide.
   * If destination is NULL, doesn't perform the copy but the
   * returned size is still correct.
   *
   * Returns: The size in bytes of the written string. */

  DWORD dwLength;

  if ( source == NULL )
    return 0;

  if ( bAnsi )
  {
    dwLength = MultiByteToWideChar( CP_ACP, 0, (LPSTR) source, -1, NULL, 0 );
    if ( destination )
        MultiByteToWideChar( CP_ACP, 0, (LPSTR) source, -1,
                             (LPWSTR) destination, dwLength );
  }
  else
  {
    dwLength = lstrlenW( (LPWSTR) source ) + 1;
    if ( destination )
        CopyMemory( destination, source, dwLength );
  }

  return dwLength * sizeof(WCHAR);
}

static DWORD DP_MSG_FillSuperPackedPlayer( IDirectPlay2Impl* lpDP,
                                           LPDPLAYI_SUPERPACKEDPLAYER lpPackedPlayer,
                                           LPVOID lpData,
                                           BOOL bIsGroup,
                                           BOOL bAnsi )
{
  /* Fills lpPackedPlayer with the information in lpData.
   * lpData can be a player or a group.
   * If lpPackedPlayer is NULL just returns the size
   * needed to pack the player.
   *
   * Returns: the size of the written data in bytes */

#define PLAYER_OR_GROUP( bIsGroup, lpData, field )        \
  ( bIsGroup                                              \
    ? ((lpGroupData)  lpData)->field                      \
    : ((lpPlayerData) lpData)->field )

  DWORD offset, length, size;
  LPVOID playerSPData;

  if ( lpPackedPlayer == NULL )
  {
    size = sizeof(DPLAYI_SUPERPACKEDPLAYER);    /* Fixed data */
    size += DP_CopyString( NULL,                /* Short name */
                           PLAYER_OR_GROUP( bIsGroup, lpData, name.lpszShortName ),
                           bAnsi );
    size += DP_CopyString( NULL,                /* Long name */
                           PLAYER_OR_GROUP( bIsGroup, lpData, name.lpszShortName ),
                           bAnsi );
    /* Player data length */
    /* Player data */
    /* SP data length */
    /* SP data */

    if ( bIsGroup )
    {
      /* Player count */
      /* Player IDs */
      /* ParentID */
      /* Shortcut ID Count */
      /* Shortcut IDs */
    }
    return size;
  }

  lpPackedPlayer->Size  = sizeof(DPLAYI_SUPERPACKEDPLAYER);

  lpPackedPlayer->Flags = 0x000000FF & PLAYER_OR_GROUP( bIsGroup, lpData, dwFlags );
  lpPackedPlayer->ID    = PLAYER_OR_GROUP( bIsGroup, lpData, dpid );

  if ( lpPackedPlayer->Flags & DPLAYI_PLAYER_SYSPLAYER )
  {
    lpPackedPlayer->VersionOrSystemPlayerID = DX61AVERSION;
  }
  else
  {
    lpPackedPlayer->VersionOrSystemPlayerID =
      DPQ_FIRST( lpDP->dp2->lpSysGroup->players )->lpPData->dpid;
  }

  offset = lpPackedPlayer->Size;

  /* Short name */
  length = DP_CopyString( ((LPBYTE) lpPackedPlayer) + offset,
                          PLAYER_OR_GROUP( bIsGroup, lpData, name.lpszShortName ),
                          bAnsi );
  if ( length )
    lpPackedPlayer->PlayerInfoMask |= SPP_SN;
  offset += length;

  /* Long name */
  length = DP_CopyString( ((LPBYTE) lpPackedPlayer) + offset,
                          PLAYER_OR_GROUP( bIsGroup, lpData, name.lpszLongName ),
                          bAnsi );
  if ( length )
    lpPackedPlayer->PlayerInfoMask |= SPP_LN;
  offset += length;

  /* Player data */
  length = PLAYER_OR_GROUP( bIsGroup, lpData, dwRemoteDataSize);
  if ( length )
  {
    size = spp_get_optimum_size( length );
    lpPackedPlayer->PlayerInfoMask |= spp_size2flags( size, SPP_PD_OFFSET );
    CopyMemory( ((LPBYTE) lpPackedPlayer) + offset, &length, size );
    offset += size;

    CopyMemory( ((LPBYTE) lpPackedPlayer) + offset,
                PLAYER_OR_GROUP( bIsGroup, lpData, lpRemoteData ),
                length );
    offset += length;
  }

  /* Service provider data */
  IDirectPlaySP_GetSPPlayerData( lpDP->dp2->spData.lpISP, lpPackedPlayer->ID,
                                 &playerSPData, &length, DPGET_REMOTE );
  if ( length )
  {
    size = spp_get_optimum_size( length );
    lpPackedPlayer->PlayerInfoMask |= spp_size2flags( size, SPP_SL_OFFSET );
    CopyMemory( ((LPBYTE) lpPackedPlayer) + offset, &length, size );
    offset += size;
    CopyMemory( ((LPBYTE) lpPackedPlayer) + offset, playerSPData, length );
    offset += length;
  }

  if ( bIsGroup )
  {
    /* Player IDs */
    if( !DPQ_IS_EMPTY( ((lpGroupData)lpData)->players ) )
    {
      DWORD player_count, offset_PC;
      lpPlayerList lpPList;

      offset_PC = offset;  /* Space for PlayerCount */
      offset += 4;
      player_count = 0;

      /* PlayerIDs */
      if ( (lpPList = DPQ_FIRST( ((lpGroupData)lpData)->players )) )
      {
        do
        {
          CopyMemory( ((LPBYTE) lpPackedPlayer) + offset,
                      &lpPList->lpPData->dpid,
                      sizeof(DPID) );
          offset += sizeof(DPID);
          player_count++;
        }
        while( (lpPList = DPQ_NEXT( lpPList->players )) );
      }

      /* PlayerCount */
      size = spp_get_optimum_size( player_count );
      lpPackedPlayer->PlayerInfoMask |= spp_size2flags( size, SPP_PC_OFFSET );

      CopyMemory( ((LPBYTE) lpPackedPlayer) + offset_PC, &player_count, size );
    }

    /* ParentID */
    if ( ((lpGroupData)lpData)->parent )
    {
      lpPackedPlayer->PlayerInfoMask |= SPP_PI;
      CopyMemory( ((LPBYTE) lpPackedPlayer) + offset,
                  &((lpGroupData)lpData)->parent,
                  sizeof(DWORD) );
      offset += sizeof(DWORD);
    }

    /* Shortcut IDs */
    /*size = spp_get_optimum_size( ___ );
      lpPackedPlayer->PlayerInfoMask |= spp_size2flags( size, SPP_SC_OFFSET );*/
    FIXME( "TODO: Add shortcut IDs\n" );
  }

  return offset;
}

static DWORD DP_MSG_FillPackedPlayer( IDirectPlay2Impl* lpDP,
                                      LPDPLAYI_PACKEDPLAYER lpPackedPlayer,
                                      LPVOID lpData,
                                      BOOL bIsGroup,
                                      BOOL bAnsi )
{
  /* Fills lpPackedPlayer with the information in lpData.
   * lpData can be a player or a group.
   * If lpPackedPlayer is NULL just returns the size
   * needed to pack the player.
   *
   * Returns: the size of the written data in bytes */

  LPVOID playerSPData;
  DWORD offset;

  if ( lpPackedPlayer == NULL )
  {
    DWORD size;
    size = sizeof(DPLAYI_PACKEDPLAYER);    /* Fixed data */
    size += DP_CopyString( NULL,           /* Short name */
                           PLAYER_OR_GROUP( bIsGroup, lpData, name.lpszShortName ),
                           bAnsi );
    size += DP_CopyString( NULL,           /* Long name */
                           PLAYER_OR_GROUP( bIsGroup, lpData, name.lpszLongName ),
                           bAnsi );
    /* SP data */
    /* Player data */

    if ( bIsGroup )
    {
      /* Player IDs */
    }
    return size;
  }

  lpPackedPlayer->SystemPlayerID  =
    DPQ_FIRST( lpDP->dp2->lpSysGroup->players )->lpPData->dpid;
  lpPackedPlayer->FixedSize       = sizeof(DPLAYI_PACKEDPLAYER);
  lpPackedPlayer->PlayerVersion   = DX61AVERSION;

  lpPackedPlayer->Flags           = 0x000000FF & PLAYER_OR_GROUP( bIsGroup, lpData, dwFlags );
  lpPackedPlayer->PlayerID        = PLAYER_OR_GROUP( bIsGroup, lpData, dpid );
  lpPackedPlayer->ParentID        = ( bIsGroup
                                      ? ((lpGroupData)lpData)->parent
                                      : 0 );

  offset = lpPackedPlayer->FixedSize;

  /* Short name */
  lpPackedPlayer->ShortNameLength =
    DP_CopyString( ((LPBYTE) lpPackedPlayer) + offset,
                   PLAYER_OR_GROUP( bIsGroup, lpData, name.lpszShortName ),
                   bAnsi );
  offset += lpPackedPlayer->ShortNameLength;

  /* Long name */
  lpPackedPlayer->LongNameLength =
    DP_CopyString( ((LPBYTE) lpPackedPlayer) + offset,
                   PLAYER_OR_GROUP( bIsGroup, lpData, name.lpszLongName ),
                   bAnsi );
  offset += lpPackedPlayer->LongNameLength;

  /* Service provider data */
  IDirectPlaySP_GetSPPlayerData( lpDP->dp2->spData.lpISP,
                                 lpPackedPlayer->PlayerID,
                                 &playerSPData,
                                 &lpPackedPlayer->ServiceProviderDataSize,
                                 DPGET_REMOTE );
  if ( lpPackedPlayer->ServiceProviderDataSize )
  {
    CopyMemory( ((LPBYTE) lpPackedPlayer) + offset, playerSPData,
                lpPackedPlayer->ServiceProviderDataSize );
    offset += lpPackedPlayer->ServiceProviderDataSize;
  }

  /* Player data */
  lpPackedPlayer->PlayerDataSize = PLAYER_OR_GROUP( bIsGroup, lpData, dwRemoteDataSize );
  if ( lpPackedPlayer->PlayerDataSize )
  {
    CopyMemory( ((LPBYTE) lpPackedPlayer) + offset,
                PLAYER_OR_GROUP( bIsGroup, lpData, lpRemoteData ),
                lpPackedPlayer->PlayerDataSize );
    offset += lpPackedPlayer->PlayerDataSize;
  }

  if ( bIsGroup )
  {
    lpPlayerList lpPList;

    /* Player IDs */
    lpPackedPlayer->NumberOfPlayers = 0;

    if ( (lpPList = DPQ_FIRST( ((lpGroupData)lpData)->players )) )
    {
      do
      {
        CopyMemory( ((LPBYTE) lpPackedPlayer) + offset,
                    &lpPList->lpPData->dpid,
                    sizeof(DPID) );
        offset += sizeof(DPID);
        lpPackedPlayer->NumberOfPlayers++;
      }
      while( (lpPList = DPQ_NEXT( lpPList->players )) );
    }

  }

  /* Total size */
  lpPackedPlayer->Size = offset;

  return lpPackedPlayer->Size;
}

void DP_MSG_ReplyToEnumPlayersRequest( IDirectPlay2Impl* lpDP,
                                       LPVOID* lplpReply,
                                       LPDWORD lpdwMsgSize )
{
  /* Depending on the type of session, builds either a EnumPlayers
     reply or a SuperEnumPlayers reply, and places it in lplpReply */

  /* TODO: Find size to allocate *lplpReply
   *       Check if we're releasing that memory */

  LPDPSP_MSG_ENUMPLAYERSREPLY lpReply; /* Also valid in the SUPER case */
  lpPlayerList lpPList;
  lpGroupList lpGList;
  DWORD offset;
  BOOL bAnsi = TRUE; /* FIXME: This needs to be in the DPLAY interface */


  *lplpReply = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 1024 );
  lpReply = (LPDPSP_MSG_ENUMPLAYERSREPLY)( (LPBYTE)(*lplpReply) +
                                           lpDP->dp2->spData.dwSPHeaderSize );

  lpReply->envelope.dwMagic  = DPMSG_SIGNATURE;
  lpReply->envelope.wVersion = DX61AVERSION;

  if ( lpDP->dp2->lpSessionDesc->dwFlags & DPSESSION_CLIENTSERVER )
  {
    lpReply->envelope.wCommandId = DPMSGCMD_ENUMPLAYERSREPLY;
  }
  else
  {
    lpReply->envelope.wCommandId = DPMSGCMD_SUPERENUMPLAYERSREPLY;
  }


  /* Session description */
  lpReply->DescriptionOffset = ( sizeof(DPSP_MSG_ENUMPLAYERSREPLY) -
                                 sizeof(DPSESSIONDESC2) );
  CopyMemory( &lpReply->DPSessionDesc, lpDP->dp2->lpSessionDesc,
              sizeof(DPSESSIONDESC2) );

  offset = sizeof(DPSP_MSG_ENUMPLAYERSREPLY);

  /* Session name */
  if ( lpDP->dp2->lpSessionDesc->lpszSessionName )
  {
    lpReply->NameOffset = offset;
    offset += DP_CopyString( ((LPBYTE) lpReply) + offset,
                             lpDP->dp2->lpSessionDesc->lpszSessionName,
                             bAnsi );
  }

  /* Session password */
  if ( lpDP->dp2->lpSessionDesc->lpszPassword )
  {
    lpReply->PasswordOffset = offset;
    offset += DP_CopyString( ((LPBYTE) lpReply) + offset,
                             lpDP->dp2->lpSessionDesc->lpszPassword,
                             bAnsi );
  }

  /* Populate PlayerInfo list */

  /* - Players */
  if ( (lpPList = DPQ_FIRST( lpDP->dp2->lpSysGroup->players )) )
  {
    do
    {
      if ( lpReply->envelope.wCommandId == DPMSGCMD_SUPERENUMPLAYERSREPLY )
      {
        offset += DP_MSG_FillSuperPackedPlayer(
          lpDP, (LPDPLAYI_SUPERPACKEDPLAYER)( ((LPBYTE)lpReply) + offset ),
          lpPList->lpPData, FALSE, bAnsi );
      }
      else
      {
        offset += DP_MSG_FillPackedPlayer(
          lpDP, (LPDPLAYI_PACKEDPLAYER)( ((LPBYTE)lpReply) + offset ),
          lpPList->lpPData, FALSE, bAnsi );
      }

      lpReply->PlayerCount++;
    }
    while( (lpPList = DPQ_NEXT( lpPList->players )) );
  }

  /* - Groups */
  if ( (lpGList = DPQ_FIRST( lpDP->dp2->lpSysGroup->groups )) )
  {
    do
    {
      if ( lpReply->envelope.wCommandId == DPMSGCMD_SUPERENUMPLAYERSREPLY )
      {
        offset += DP_MSG_FillSuperPackedPlayer(
          lpDP, (LPDPLAYI_SUPERPACKEDPLAYER)( ((LPBYTE)lpReply) + offset ),
          lpGList->lpGData, TRUE, bAnsi );
      }
      else
      {
        offset += DP_MSG_FillPackedPlayer(
          lpDP, (LPDPLAYI_PACKEDPLAYER)( ((LPBYTE)lpReply) + offset ),
          lpGList->lpGData, TRUE, bAnsi );
      }

      lpReply->GroupCount++;
    }
    while( (lpGList = DPQ_NEXT( lpGList->groups )) );
  }

  if ( lpReply->envelope.wCommandId == DPMSGCMD_SUPERENUMPLAYERSREPLY )
  {
    /* - Groups with shortcuts */
    FIXME( "TODO: Add shortcut IDs\n" );
  }

  *lpdwMsgSize = lpDP->dp2->spData.dwSPHeaderSize + offset;

}
