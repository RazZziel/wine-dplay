/* Internet TCP/IP and IPX Connection For DirectPlay
 *
 * Copyright 2008 Ismael Barros Barros
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winsock2.h"
#include "dpwsockx_dll.h"
#include "wine/debug.h"
#include "dplay.h"
#include "wine/dplaysp.h"

WINE_DEFAULT_DEBUG_CHANNEL(dplay);


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(0x%p, %d, %p)\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;    /* prefer native version */
    case DLL_PROCESS_ATTACH:
        /* TODO: Initialization */
        DisableThreadLibraryCalls(hinstDLL);
        break;
    case DLL_PROCESS_DETACH:
        break;
    default:
        break;
    }

    return TRUE;
}



static DWORD WINAPI udp_listener_thread( LPVOID lpParameter )
{
    LPDPWS_THREADDATA listener = lpParameter;
    int recvMsgSize;
    char buff[4096];
    SOCKADDR_IN clientAddr;
    int clientAddrSize;

    listener->is_running = TRUE;

    TRACE( "listening on port %d\n", ntohs(listener->addr.sin_port) );

    for ( ;; )
    {
        clientAddrSize = sizeof(clientAddr);

        /* Listen for messages */
        recvMsgSize = recvfrom( listener->sock, buff, sizeof(buff), 0,
                                (LPSOCKADDR) &clientAddr, &clientAddrSize );

        if ( recvMsgSize == 0 )
        {
            TRACE( "recvfrom(): connection closed\n" );
        }
        else if ( recvMsgSize == SOCKET_ERROR )
        {
            ERR( "recvfrom() failed: %d\n", WSAGetLastError() );
        }
        else
        {
            TRACE( "Handling message from %s:%d size %d\n",
                   inet_ntoa(clientAddr.sin_addr),
                   ntohs(clientAddr.sin_port),
                   recvMsgSize );

            /* Copy client address to the header of the message, needed if
             * we need to send a reply */
            ((LPDPSP_MSG_HEADER) buff)->SockAddr.sin_addr = clientAddr.sin_addr;

            /* Send messages to the upper layer */
            IDirectPlaySP_HandleMessage( listener->lpISP,
                                         buff + sizeof(DPSP_MSG_HEADER),
                                         recvMsgSize - sizeof(DPSP_MSG_HEADER),
                                         buff );
        }
    }

    listener->is_running = FALSE;
    return 0;
}

static DWORD WINAPI tcp_listener_thread( LPVOID lpParameter )
{
    LPDPWS_THREADDATA listener = lpParameter;
    int recvMsgSize;
    char buff[4096];
    SOCKADDR_IN clientAddr;
    SOCKET clientSock;
    int clientAddrSize;
    int ret;

    listener->is_running = TRUE;

    TRACE( "listening on port %d\n", ntohs(listener->addr.sin_port) );

    for ( ;; )
    {
        clientAddrSize = sizeof(clientAddr);

        /* Waiting for clients */
        clientSock = accept( listener->sock, (LPSOCKADDR) &clientAddr,
                             &clientAddrSize );
        if ( clientSock == INVALID_SOCKET )
        {
            ERR( "accept() failed: %d\n", WSAGetLastError() );
            break;
        }

        TRACE( "Handling client %s:%d\n",
               inet_ntoa(clientAddr.sin_addr),
               ntohs(clientAddr.sin_port) );


        recvMsgSize = recv( clientSock, buff, sizeof(buff), 0 );

        if ( recvMsgSize == 0 )
        {
            TRACE( "recv(): connection closed\n" );
        }
        else if ( recvMsgSize == SOCKET_ERROR )
        {
            ERR( "recv() failed: %d\n", WSAGetLastError() );
        }
        else
        {
            TRACE( "Handling message from %s:%d size %d\n",
                   inet_ntoa(clientAddr.sin_addr),
                   ntohs(clientAddr.sin_port),
                   recvMsgSize );

            /* Copy client address to the header of the message, needed if
             * we need to send a reply */
            ((LPDPSP_MSG_HEADER) buff)->SockAddr.sin_addr = clientAddr.sin_addr;

            /* Send messages to the upper layer */
            IDirectPlaySP_HandleMessage( listener->lpISP,
                                         buff + sizeof(DPSP_MSG_HEADER),
                                         recvMsgSize - sizeof(DPSP_MSG_HEADER),
                                         buff );
        }

        ret = closesocket( clientSock );
        if ( ret == SOCKET_ERROR)
        {
            ERR( "closesocket() failed %d\n", WSAGetLastError() );
            break;
        }
    }

    listener->is_running = FALSE;
    return 0;
}

static HRESULT start_dplaysrv( LPDPWS_DATA dpwsData )
{
    LPDPWS_THREADDATA listener = &dpwsData->dplaysrv;
    int ret;

    if ( listener->is_running )
    {
        TRACE( "Thread already started\n" );
        return DP_OK;
    }

    listener->lpISP = dpwsData->lpISP;

    /* Setting up a socket listening for UDP broadcasts on port 47624 */
    /* TODO: With this implementation, if a second application tries to
     * create a session on the same machine, it will fail.
     * The native dplay approach is to run a program, dplaysvr.exe,
     * that is actually who listens on this port. On time we should move
     * to that kind of implementation. */

    listener->sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( listener->sock == INVALID_SOCKET )
    {
        ERR( "socket() failed: %d\n", WSAGetLastError() );
        return DPERR_GENERIC;
    }

    memset( &listener->addr, 0, sizeof(SOCKADDR_IN) );
    listener->addr.sin_family = AF_INET;
    listener->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listener->addr.sin_port = htons(DPWS_DPLAYSRV_PORT);

    ret = bind( listener->sock, (LPSOCKADDR) &listener->addr,
                sizeof(listener->addr) );
    if ( ret == SOCKET_ERROR )
    {
        ERR( "bind() failed: %d\n", WSAGetLastError() );
        return DPERR_GENERIC;
    }

    /* Launch thread */
    listener->handle = CreateThread( NULL, 0, udp_listener_thread,
                                     listener, 0, NULL );
    return DP_OK;
}

static HRESULT start_listener( LPDPWS_DATA dpwsData, BOOL is_tcp )
{
    LPDPWS_THREADDATA listener;
    int port, ret;

    listener = ( is_tcp
                 ? &dpwsData->tcp_listener
                 : &dpwsData->udp_listener );

    if ( listener->is_running )
    {
        TRACE( "[%s] Thread already started\n", is_tcp ? "TCP" : "UDP" );
        return DP_OK;
    }

    listener->lpISP = dpwsData->lpISP;

    /* Setting up a socket listening for TCP or UDP connections
     * on port 2300-2400 */

    listener->sock = socket( AF_INET,
                             is_tcp ? SOCK_STREAM : SOCK_DGRAM,
                             is_tcp ? IPPROTO_TCP : IPPROTO_UDP );
    if ( listener->sock == INVALID_SOCKET )
    {
        ERR( "[%s] socket() failed: %d\n",
             is_tcp ? "TCP" : "UDP", WSAGetLastError() );
        return DPERR_GENERIC;
    }

    memset( &listener->addr, 0, sizeof(SOCKADDR_IN) );
    listener->addr.sin_family = AF_INET;
    listener->addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Look for an available port */
    port = DPWS_PORT_RANGE_START;
    do
    {
        listener->addr.sin_port = htons(port);
        ret = bind( listener->sock, (LPSOCKADDR) &listener->addr,
                    sizeof(listener->addr) );
    }
    while ( ( ret == SOCKET_ERROR ) &&
            ( WSAGetLastError() == WSAEADDRINUSE ) &&
            ( ++port <= DPWS_PORT_RANGE_END ) );

    if ( ret == SOCKET_ERROR )
    {
        ERR( "[%s] bind() failed: %d\n",
             is_tcp ? "TCP" : "UDP",  WSAGetLastError() );
        return DPERR_GENERIC;
    }

    TRACE( "[%s] found available port %d\n", is_tcp ? "TCP" : "UDP", port );

    if ( is_tcp )
    {
        ret = listen( listener->sock, SOMAXCONN );
        if ( ret == SOCKET_ERROR )
        {
            ERR( "[%s] listen() failed: %d\n",
                 is_tcp ? "TCP" : "UDP", WSAGetLastError() );
            return DPERR_GENERIC;
        }
    }

    /* Launch thread */
    listener->handle = CreateThread( NULL, 0,
                                     ( is_tcp
                                       ? tcp_listener_thread
                                       : udp_listener_thread ),
                                     listener, 0, NULL );
    return DP_OK;
}


static HRESULT send_udp_message( LPVOID      message,
                                 DWORD       messageSize,
                                 LPSOCKADDR  destAddr )
{
    SOCKET sock;
    int ret;

    TRACE( "Sending message to %s:%d\n",
           inet_ntoa(((LPSOCKADDR_IN) destAddr)->sin_addr),
           ntohs(((LPSOCKADDR_IN) destAddr)->sin_port) );

    if ( messageSize > DPWS_MAXBUFFERSIZE )
    {
        return DPERR_SENDTOOBIG;
    }

    sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( sock == INVALID_SOCKET )
    {
        ERR( "socket() failed: %d\n", WSAGetLastError() );
        return DPERR_GENERIC;
    }

    if ( ((LPSOCKADDR_IN)destAddr)->sin_addr.s_addr == htonl(INADDR_BROADCAST) )
    {
        /* Allow the socket to send broadcast messages */
        BOOL allowBroadcast = TRUE;
        ret = setsockopt( sock, SOL_SOCKET, SO_BROADCAST,
                          (char*) &allowBroadcast, sizeof(BOOL) );
        if ( ret == SOCKET_ERROR )
        {
            ERR( "setsockopt() failed: %d\n", WSAGetLastError() );
            return DPERR_GENERIC;
        }
    }

    /* Send message */
    ret = sendto( sock, message, messageSize, 0, destAddr, sizeof(SOCKADDR) );
    if ( ret == SOCKET_ERROR )
    {
        ERR( "sendto() failed: %d\n", WSAGetLastError() );
        return DPERR_GENERIC;
    }
    else if ( ret != messageSize )
    {
        ERR( "%d/%d bytes sent", ret, messageSize );
        return DPERR_GENERIC;
    }

    ret = closesocket( sock );
    if ( ret == SOCKET_ERROR)
    {
        ERR( "closesocket() failed %d\n", WSAGetLastError() );
        return DPERR_GENERIC;
    }

    return DP_OK;
}

static HRESULT send_tcp_message( LPVOID      message,
                                 DWORD       messageSize,
                                 LPSOCKADDR  destAddr )
{
    SOCKET sock;
    int ret;

    /* TODO:
     *  For each message sent, we open a TCP socket, send the message and
     *  close the socket. The desired behaviour would be to open a socket
     *  only if there's no socket for the requesting address, and keep
     *  sending messages with that socket, without closing it. */

    TRACE( "Sending message to %s:%d\n",
           inet_ntoa(((LPSOCKADDR_IN) destAddr)->sin_addr),
           ntohs(((LPSOCKADDR_IN) destAddr)->sin_port) );

    if ( messageSize > DPWS_GUARANTEED_MAXBUFFERSIZE )
    {
        return DPERR_SENDTOOBIG;
    }

    sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( sock == INVALID_SOCKET )
    {
        ERR( "socket() failed: %d\n", WSAGetLastError() );
        return DPERR_GENERIC;
    }

    ret = connect( sock, destAddr, sizeof(SOCKADDR) );
    if ( ret == SOCKET_ERROR )
    {
        if ( WSAGetLastError() != WSAEISCONN )
        {
            ERR( "connect() failed: %d\n", WSAGetLastError() );
            return DPERR_GENERIC;
        }
    }

    ret = send( sock, message, messageSize, 0 );
    if ( ret == SOCKET_ERROR )
    {
        ERR( "send() failed: %d\n", WSAGetLastError() );
        return DPERR_GENERIC;
    }
    else if ( ret != messageSize )
    {
        ERR( "%d/%d bytes sent", ret, messageSize );
        return DPERR_GENERIC;
    }

    ret = closesocket( sock );
    if ( ret == SOCKET_ERROR)
    {
        ERR( "closesocket() failed %d\n", WSAGetLastError() );
        return DPERR_GENERIC;
    }

    return DP_OK;
}


static HRESULT WINAPI DPWSCB_EnumSessions( LPDPSP_ENUMSESSIONSDATA data )
{
    LPDPWS_DATA dpwsData;
    DWORD dwDataSize;
    SOCKADDR_IN destAddr;

    TRACE( "(%p,%d,%p,%u)\n",
           data->lpMessage, data->dwMessageSize,
           data->lpISP, data->bReturnStatus );

    IDirectPlaySP_GetSPData( data->lpISP, (LPVOID*) &dpwsData, &dwDataSize,
                             DPGET_LOCAL );

    /* Start listener to get replies if it's not started yet.
     * This needs to be done before we build the local address,
     * otherwise we won't know in which port we're listening. */
    if ( !dpwsData->tcp_listener.is_running )
    {
        HRESULT hr = start_listener( dpwsData, TRUE );
        if ( FAILED(hr) )
        {
            return hr;
        }
    }

    /* Destination address */
    /* TODO: Instead of getting always the broadcast address,
     * we should throw a popup message asking for an address,
     * and only broadcast if no address is provided. */
    memset( &destAddr, 0, sizeof(SOCKADDR_IN) );
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    destAddr.sin_port = htons(DPWS_DPLAYSRV_PORT);

    /* Add header to message body */
    ((LPDPSP_MSG_HEADER) data->lpMessage)->mixed
        = DPSP_MSG_MAKE_MIXED(data->dwMessageSize, DPSP_MSG_TOKEN_REMOTE);
    ((LPDPSP_MSG_HEADER) data->lpMessage)->SockAddr
        = dpwsData->tcp_listener.addr;

    return send_udp_message( data->lpMessage, data->dwMessageSize,
                             (LPSOCKADDR) &destAddr );
}

static HRESULT WINAPI DPWSCB_Reply( LPDPSP_REPLYDATA data )
{
    LPDPWS_DATA dpwsData;
    DWORD dwDataSize;
    SOCKADDR_IN destAddr;

    TRACE( "(%p,%p,%d,%d,%p)\n",
           data->lpSPMessageHeader, data->lpMessage, data->dwMessageSize,
           data->idNameServer, data->lpISP );

    IDirectPlaySP_GetSPData( data->lpISP, (LPVOID*) &dpwsData, &dwDataSize,
                             DPGET_LOCAL );

    /* Extract destination address from request header */
    destAddr = ((LPDPSP_MSG_HEADER) data->lpSPMessageHeader)->SockAddr;

    /* Add header to message body */
    ((LPDPSP_MSG_HEADER) data->lpMessage)->mixed
        = DPSP_MSG_MAKE_MIXED(data->dwMessageSize, DPSP_MSG_TOKEN_REMOTE);
    ((LPDPSP_MSG_HEADER) data->lpMessage)->SockAddr
        = dpwsData->tcp_listener.addr;

    return send_tcp_message( data->lpMessage, data->dwMessageSize,
                             (LPSOCKADDR) &destAddr );
}

static HRESULT WINAPI DPWSCB_Send( LPDPSP_SENDDATA data )
{
    LPDPWS_DATA dpwsData;
    LPDPWS_PLAYER_DATA dpwsPlayerData;
    DWORD dwDataSize, dwPlayerDataSize;
    SOCKADDR_IN destAddr;

    TRACE( "(0x%08x,0x%08x,0x%08x,%p,%d,%u,%p)\n",
           data->dwFlags, data->idPlayerTo, data->idPlayerFrom,
           data->lpMessage, data->dwMessageSize,
           data->bSystemMessage, data->lpISP );

    IDirectPlaySP_GetSPData( data->lpISP, (LPVOID*) &dpwsData, &dwDataSize,
                             DPGET_LOCAL );

    /* Get destination address from the player id */
    if ( data->idPlayerTo == 0 ) /* Message to nameserver */
    {
        destAddr = dpwsData->nameserverAddr;
    }
    else
    {
        HRESULT hr;
        hr = IDirectPlaySP_GetSPPlayerData( data->lpISP, data->idPlayerTo,
                                            (LPVOID*) &dpwsPlayerData,
                                            &dwPlayerDataSize, DPGET_REMOTE );
        if ( FAILED(hr) )
        {
            return hr;
        }

        if ( ( data->bSystemMessage ) ||
             ( data->dwFlags & DPSEND_GUARANTEED ) )
        {
            destAddr = dpwsPlayerData->tcpAddr;
        }
        else
        {
            destAddr = dpwsPlayerData->udpAddr;
        }
    }

    /* Add header to message body */
    ((LPDPSP_MSG_HEADER) data->lpMessage)->mixed
        = DPSP_MSG_MAKE_MIXED(data->dwMessageSize, DPSP_MSG_TOKEN_REMOTE);
    ((LPDPSP_MSG_HEADER) data->lpMessage)->SockAddr
        = dpwsData->tcp_listener.addr;

    /* If the flag DPSESSION_DIRECTPLAYPROTOCOL is set we are
     * supposed to send UDP messages and implement all the TCP
     * functionality ourselves in dplayx, but for now let's not
     * reinvent the wheel. */
    if ( ( data->bSystemMessage ) ||
         ( data->dwFlags & DPSEND_GUARANTEED ) )
    {
        return send_tcp_message( data->lpMessage, data->dwMessageSize,
                                 (LPSOCKADDR) &destAddr );
    }
    else
    {
        return send_udp_message( data->lpMessage, data->dwMessageSize,
                                 (LPSOCKADDR) &destAddr );
    }
}

static HRESULT WINAPI DPWSCB_CreatePlayer( LPDPSP_CREATEPLAYERDATA data )
{
    FIXME( "(%d,0x%08x,%p,%p) stub\n",
           data->idPlayer, data->dwFlags,
           data->lpSPMessageHeader, data->lpISP );
    return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI DPWSCB_DeletePlayer( LPDPSP_DELETEPLAYERDATA data )
{
    FIXME( "(%d,0x%08x,%p) stub\n",
           data->idPlayer, data->dwFlags, data->lpISP );
    return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI DPWSCB_GetAddress( LPDPSP_GETADDRESSDATA data )
{
    FIXME( "(%d,0x%08x,%p,%p,%p) stub\n",
           data->idPlayer, data->dwFlags, data->lpAddress,
           data->lpdwAddressSize, data->lpISP );
    return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI DPWSCB_GetCaps( LPDPSP_GETCAPSDATA data )
{
    TRACE( "(%d,%p,0x%08x,%p)\n",
           data->idPlayer, data->lpCaps, data->dwFlags, data->lpISP );

    data->lpCaps->dwFlags = ( DPCAPS_ASYNCSUPPORTED |
                              DPCAPS_GUARANTEEDOPTIMIZED |
                              DPCAPS_GUARANTEEDSUPPORTED );

    data->lpCaps->dwMaxQueueSize    = DPWS_MAXQUEUESIZE;
    data->lpCaps->dwHundredBaud     = DPWS_HUNDREDBAUD;
    data->lpCaps->dwLatency         = DPWS_LATENCY;
    data->lpCaps->dwMaxLocalPlayers = DPWS_MAXLOCALPLAYERS;
    data->lpCaps->dwHeaderLength    = sizeof(DPSP_MSG_HEADER);
    data->lpCaps->dwTimeout         = DPWS_TIMEOUT;

    if ( data->dwFlags & DPGETCAPS_GUARANTEED )
    {
        data->lpCaps->dwMaxBufferSize = DPWS_GUARANTEED_MAXBUFFERSIZE;
        data->lpCaps->dwMaxPlayers    = DPWS_GUARANTEED_MAXPLAYERS;
    }
    else
    {
        data->lpCaps->dwMaxBufferSize = DPWS_MAXBUFFERSIZE;
        data->lpCaps->dwMaxPlayers    = DPWS_MAXPLAYERS;
    }

    data->lpCaps->dwMaxBufferSize -= sizeof(DPSP_MSG_HEADER);

    return DP_OK;
}

static HRESULT WINAPI DPWSCB_Open( LPDPSP_OPENDATA data )
{
    LPDPWS_DATA dpwsData;
    DWORD dwDataSize;
    HRESULT hr;

    TRACE( "(%u,%p,%p,%u,0x%08x,0x%08x)\n",
           data->bCreate, data->lpSPMessageHeader, data->lpISP,
           data->bReturnStatus, data->dwOpenFlags, data->dwSessionFlags );

    IDirectPlaySP_GetSPData( data->lpISP, (LPVOID*) &dpwsData, &dwDataSize,
                             DPGET_LOCAL );

    if ( data->bCreate )
    {
        hr = start_dplaysrv( dpwsData );
        if ( FAILED(hr) )
        {
            return hr;
        }

        hr = start_listener( dpwsData, TRUE );
        if ( FAILED(hr) )
        {
            return hr;
        }
    }
    else
    {
        /* Save address of name server */
        dpwsData->nameserverAddr =
            ((LPDPSP_MSG_HEADER) data->lpSPMessageHeader)->SockAddr;
    }

    /* TCP listener was already started in EnumConnections */

    hr = start_listener( dpwsData, FALSE );
    if ( FAILED(hr) )
    {
        return hr;
    }

    return DP_OK;
}

static HRESULT stop_listener(LPDPWS_THREADDATA listener)
{
    int ret;

    if ( listener->is_running )
    {
        ret = closesocket( listener->sock );
        if ( ret == SOCKET_ERROR )
        {
            ERR( "closesocket() failed %d\n", WSAGetLastError() );
            return DPERR_GENERIC;
        }
    }

    return DP_OK;
}

static HRESULT WINAPI DPWSCB_CloseEx( LPDPSP_CLOSEDATA data )
{
    LPDPWS_DATA dpwsData;
    DWORD dwDataSize;

    TRACE( "(%p)\n", data->lpISP );

    IDirectPlaySP_GetSPData( data->lpISP, (LPVOID*) &dpwsData, &dwDataSize,
                             DPGET_LOCAL );

    stop_listener( &dpwsData->tcp_listener );
    stop_listener( &dpwsData->udp_listener );
    stop_listener( &dpwsData->dplaysrv );

    return DP_OK;
}

static HRESULT WINAPI DPWSCB_ShutdownEx( LPDPSP_SHUTDOWNDATA data )
{
    FIXME( "(%p) stub\n", data->lpISP );
    return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI DPWSCB_GetAddressChoices( LPDPSP_GETADDRESSCHOICESDATA data )
{
    FIXME( "(%p,%p,%p) stub\n",
           data->lpAddress, data->lpdwAddressSize, data->lpISP );
    return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI DPWSCB_SendEx( LPDPSP_SENDEXDATA data )
{
    FIXME( "(%p,0x%08x,%d,%d,%p,%d,%d,%d,%d,%p,%p,%u) stub\n",
           data->lpISP, data->dwFlags, data->idPlayerTo, data->idPlayerFrom,
           data->lpSendBuffers, data->cBuffers, data->dwMessageSize,
           data->dwPriority, data->dwTimeout, data->lpDPContext,
           data->lpdwSPMsgID, data->bSystemMessage );
    return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI DPWSCB_SendToGroupEx( LPDPSP_SENDTOGROUPEXDATA data )
{
    FIXME( "(%p,0x%08x,%d,%d,%p,%d,%d,%d,%d,%p,%p) stub\n",
           data->lpISP, data->dwFlags, data->idGroupTo, data->idPlayerFrom,
           data->lpSendBuffers, data->cBuffers, data->dwMessageSize,
           data->dwPriority, data->dwTimeout, data->lpDPContext,
           data->lpdwSPMsgID );
    return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI DPWSCB_Cancel( LPDPSP_CANCELDATA data )
{
    FIXME( "(%p,0x%08x,%p,%d,0x%08x,0x%08x) stub\n",
           data->lpISP, data->dwFlags, data->lprglpvSPMsgID, data->cSPMsgID,
           data->dwMinPriority, data->dwMaxPriority );
    return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI DPWSCB_GetMessageQueue( LPDPSP_GETMESSAGEQUEUEDATA data )
{
    FIXME( "(%p,0x%08x,%d,%d,%p,%p) stub\n",
           data->lpISP, data->dwFlags, data->idFrom, data->idTo,
           data->lpdwNumMsgs, data->lpdwNumBytes );
    return DPERR_UNSUPPORTED;
}

static void setup_callbacks( LPDPSP_SPCALLBACKS lpCB )
{
    lpCB->EnumSessions           = DPWSCB_EnumSessions;
    lpCB->Reply                  = DPWSCB_Reply;
    lpCB->Send                   = DPWSCB_Send;
    lpCB->CreatePlayer           = DPWSCB_CreatePlayer;
    lpCB->DeletePlayer           = DPWSCB_DeletePlayer;
    lpCB->GetAddress             = DPWSCB_GetAddress;
    lpCB->GetCaps                = DPWSCB_GetCaps;
    lpCB->Open                   = DPWSCB_Open;
    lpCB->CloseEx                = DPWSCB_CloseEx;
    lpCB->ShutdownEx             = DPWSCB_ShutdownEx;
    lpCB->GetAddressChoices      = DPWSCB_GetAddressChoices;
    lpCB->SendEx                 = DPWSCB_SendEx;
    lpCB->SendToGroupEx          = DPWSCB_SendToGroupEx;
    lpCB->Cancel                 = DPWSCB_Cancel;
    lpCB->GetMessageQueue        = DPWSCB_GetMessageQueue;

    lpCB->AddPlayerToGroup       = NULL;
    lpCB->Close                  = NULL;
    lpCB->CreateGroup            = NULL;
    lpCB->DeleteGroup            = NULL;
    lpCB->RemovePlayerFromGroup  = NULL;
    lpCB->SendToGroup            = NULL;
    lpCB->Shutdown               = NULL;
}



/******************************************************************
 *		SPInit (DPWSOCKX.1)
 */
HRESULT WINAPI SPInit( LPSPINITDATA lpspData )
{
    WSADATA wsaData;
    DPWS_DATA dpwsData;

    TRACE( "Initializing library for %s (%s)\n",
           wine_dbgstr_guid(lpspData->lpGuid), debugstr_w(lpspData->lpszName) );

    /* We only support TCP/IP service */
    if ( !IsEqualGUID(lpspData->lpGuid, &DPSPGUID_TCPIP) )
    {
        return DPERR_UNAVAILABLE;
    }

    /* Assign callback functions */
    setup_callbacks( lpspData->lpCB );

    /* Load Winsock 2.0 DLL */
    if ( WSAStartup( MAKEWORD(2, 0), &wsaData ) != 0 )
    {
        ERR( "WSAStartup() failed\n" );
        return DPERR_UNAVAILABLE;
    }

    /* Initialize internal data */
    memset( &dpwsData, 0, sizeof(DPWS_DATA) );
    dpwsData.lpISP = lpspData->lpISP;
    IDirectPlaySP_SetSPData( lpspData->lpISP, &dpwsData, sizeof(DPWS_DATA),
                             DPSET_LOCAL );

    /* dplay needs to know the size of the header */
    lpspData->dwSPHeaderSize = sizeof(DPSP_MSG_HEADER);

    return DP_OK;
}
