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

    DP_MSG_ExpectReply( This, &data, DPMSG_DEFAULT_WAIT_TIME, DPMSGCMD_REQUESTPLAYERREPLY,
                        &lpMsg, &dwMsgSize );
  }

  /* Need to examine the data and extract the new player id */
  if( SUCCEEDED(hr) )
  {
    LPCDPSP_MSG_REQUESTPLAYERREPLY lpcReply;

    lpcReply = lpMsg;

    *lpdpidAllocatedId = lpcReply->ID;

    TRACE( "Received reply for id = 0x%08x\n", lpcReply->ID );

    HeapFree( GetProcessHeap(), 0, lpMsg );
  }

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
                                DPMSG_WAIT_60_SECS,
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

  /* Setup for receipt */
  hMsgReceipt = DP_MSG_BuildAndLinkReplyStruct( This, &replyStructList,
                                                wReplyCommandId );

  TRACE( "Sending msg and expecting cmd %u in reply within %u ticks\n",
         wReplyCommandId, dwWaitTime );
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

#if 0
  if( wCommandId == DPMSGCMD_ADDFORWARDREQUEST )
  {
    DebugBreak();
  }
#endif

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
    ERR( "No receipt event set - only expecting in reply mode\n" );
    DebugBreak();
  }
}
