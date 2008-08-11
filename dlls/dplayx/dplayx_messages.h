/*
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

#ifndef __WINE_DPLAYX_MESSAGES__
#define __WINE_DPLAYX_MESSAGES__

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winsock2.h"
#include "dplay.h"
#include "rpc.h" /* For GUID */

#include "dplay_global.h"

DWORD CreateLobbyMessageReceptionThread( HANDLE hNotifyEvent, HANDLE hStart,
                                         HANDLE hDeath, HANDLE hConnRead );

HRESULT DP_MSG_SendRequestPlayerId( IDirectPlay2AImpl* This, DWORD dwFlags,
                                    LPDPID lpdipidAllocatedId );
HRESULT DP_MSG_ForwardPlayerCreation( IDirectPlay2AImpl* This, DPID dpidServer );
void DP_MSG_ReplyToEnumPlayersRequest( IDirectPlay2Impl* lpDP, LPVOID* lplpReply,
                                       LPDWORD lpdwMsgSize );

BOOL DP_MSG_ReplyReceived( IDirectPlay2AImpl* This, WORD wCommandId,
                           LPCVOID lpMsgHdr, LPCVOID lpMsgBody,
                           DWORD dwMsgBodySize );
void DP_MSG_ErrorReceived( IDirectPlay2AImpl* This, WORD wCommandId,
                           LPCVOID lpMsgBody, DWORD dwMsgBodySize );

/* Timings -> 1000 ticks/sec */
#define DPMSG_WAIT_5_SECS   5000
#define DPMSG_WAIT_30_SECS 30000
#define DPMSG_WAIT_60_SECS 60000
#define DPMSG_DEFAULT_WAIT_TIME DPMSG_WAIT_30_SECS
#define DPMSG_RELIABLE_API_TIMER     5000
#define DPMSG_LOGON_TIMER           25000
#define DPMSG_PACKETIZE_BASE_TIMER    900
#define DPMSG_PING_TIMER            35000
#define DPMSG_NAMETABLE_TIMER       15000

/* Message types etc. */
#include "pshpack1.h"


/* DirectX versions */
#define DX6VERSION    9
#define DX61VERSION  10
#define DX61AVERSION 11
#define DX71VERSION  12
#define DX8VERSION   13
#define DX9VERSION   14

/* MAGIC number at the start of all dplay packets ("play" in ASCII) */
#define DPMSG_SIGNATURE 0x79616c70

typedef struct tagDPLAYI_PACKEDPLAYER
{
    DWORD  Size;
    DWORD  Flags;
    DPID   PlayerID;
    DWORD  ShortNameLength;
    DWORD  LongNameLength;
    DWORD  ServiceProviderDataSize;
    DWORD  PlayerDataSize;
    DWORD  NumberOfPlayers;
    DPID   SystemPlayerID;
    DWORD  FixedSize;
    DWORD  PlayerVersion;
    DPID   ParentID;
} DPLAYI_PACKEDPLAYER, *LPDPLAYI_PACKEDPLAYER;
typedef const DPLAYI_PACKEDPLAYER* LPCDPLAYI_PACKEDPLAYER;


typedef struct tagDPLAYI_SUPERPACKEDPLAYER
{
    DWORD  Size;
    DWORD  Flags;
    DPID   ID;
    DWORD  PlayerInfoMask;
    DWORD  VersionOrSystemPlayerID;
} DPLAYI_SUPERPACKEDPLAYER, *LPDPLAYI_SUPERPACKEDPLAYER;
typedef const DPLAYI_SUPERPACKEDPLAYER* LPCDPLAYI_SUPERPACKEDPLAYER;

/* DPLAYI_SUPERPACKEDPLAYER->Flags for PlayerInfoMask */
#define SPP_SN  0x00000001
#define SPP_LN  0x00000002
#define SPP_PI  0x00000100
#define SPP_SL_OFFSET  2
#define SPP_PD_OFFSET  4
#define SPP_PC_OFFSET  6
#define SPP_SC_OFFSET  9

static inline DWORD spp_get_optimum_size( DWORD value )
{
    /* Returns: the minimum size in bytes needed to
     * allocate the variable. Can be 1, 2 or 4. */
    if ( value > 0xFFFF )
        return 4;
    if ( value > 0xFF )
        return 2;
    return 1;
}

static inline DWORD spp_size2flags( DWORD size, DWORD offset )
{
    /* Returns: the PlayerInfoMask flags asociated to
     * the given size in bytes. */
    if ( size == 4 )
        size = 3;
    return size << offset;
}

static inline DWORD spp_flags2size( DWORD flags, DWORD offset )
{
    /* Returns: the size in bytes asociated to the
     * given PlayerInfoMask flags. */
    DWORD size = ( flags >> offset ) & 0x000000FF;
    if ( size == 3 )
        return 4;
    else
        return size;
}


/* Messages */
#define DPMSGCMD_ENUMSESSIONSREPLY       0x0001
#define DPMSGCMD_ENUMSESSIONS            0x0002
#define DPMSGCMD_ENUMPLAYERSREPLY        0x0003
#define DPMSGCMD_ENUMPLAYER              0x0004
#define DPMSGCMD_REQUESTPLAYERID         0x0005
#define DPMSGCMD_REQUESTGROUPID          0x0006
#define DPMSGCMD_REQUESTPLAYERREPLY      0x0007
#define DPMSGCMD_CREATEPLAYER            0x0008
#define DPMSGCMD_CREATEGROUP             0x0009
#define DPMSGCMD_PLAYERMESSAGE           0x000A
#define DPMSGCMD_DELETEPLAYER            0x000B
#define DPMSGCMD_DELETEGROUP             0x000C
#define DPMSGCMD_ADDPLAYERTOGROUP        0x000D
#define DPMSGCMD_DELETEPLAYERFROMGROUP   0x000E
#define DPMSGCMD_PLAYERDATACHANGED       0x000F
#define DPMSGCMD_PLAYERNAMECHANGED       0x0010
#define DPMSGCMD_GROUPDATACHANGED        0x0011
#define DPMSGCMD_GROUPNAMECHANGED        0x0012
#define DPMSGCMD_ADDFORWARDREQUEST       0x0013
#define DPMSGCMD_PACKET                  0x0015
#define DPMSGCMD_PING                    0x0016
#define DPMSGCMD_PINGREPLY               0x0017
#define DPMSGCMD_YOUAREDEAD              0x0018
#define DPMSGCMD_PLAYERWRAPPER           0x0019
#define DPMSGCMD_SESSIONDESCCHANGED      0x001A
#define DPMSGCMD_CHALLENGE               0x001C
#define DPMSGCMD_ACCESSGRANTED           0x001D
#define DPMSGCMD_LOGONDENIED             0x001E
#define DPMSGCMD_AUTHERROR               0x001F
#define DPMSGCMD_NEGOTIATE               0x0020
#define DPMSGCMD_CHALLENGERESPONSE       0x0021
#define DPMSGCMD_SIGNED                  0x0022
#define DPMSGCMD_ADDFORWARDREPLY         0x0024
#define DPMSGCMD_ASK4MULTICAST           0x0025
#define DPMSGCMD_ASK4MULTICASTGUARANTEED 0x0026
#define DPMSGCMD_ADDSHORTCUTTOGROUP      0x0027
#define DPMSGCMD_DELETEGROUPFROMGROUP    0x0028
#define DPMSGCMD_SUPERENUMPLAYERSREPLY   0x0029
#define DPMSGCMD_KEYEXCHANGE             0x002B
#define DPMSGCMD_KEYEXCHANGEREPLY        0x002C
#define DPMSGCMD_CHAT                    0x002D
#define DPMSGCMD_ADDFORWARD              0x002E
#define DPMSGCMD_ADDFORWARDACK           0x002F
#define DPMSGCMD_PACKET2_DATA            0x0030
#define DPMSGCMD_PACKET2_ACK             0x0031
#define DPMSGCMD_IAMNAMESERVER           0x0035
#define DPMSGCMD_VOICE                   0x0036
#define DPMSGCMD_MULTICASTDELIVERY       0x0037
#define DPMSGCMD_CREATEPLAYERVERIFY      0x0038



typedef struct tagDPSP_MSG_ENVELOPE
{
  DWORD  dwMagic;
  WORD   wCommandId;
  WORD   wVersion;
} DPSP_MSG_ENVELOPE, *LPDPSP_MSG_ENVELOPE;
typedef const DPSP_MSG_ENVELOPE* LPCDPSP_MSG_ENVELOPE;


typedef struct tagDPSP_MSG_ENUMSESSIONSREPLY
{
    DPSP_MSG_ENVELOPE  envelope;
    DPSESSIONDESC2     SessionDescription;
    DWORD              NameOffset;
} DPSP_MSG_ENUMSESSIONSREPLY, *LPDPSP_MSG_ENUMSESSIONSREPLY;
typedef const DPSP_MSG_ENUMSESSIONSREPLY* LPCDPSP_MSG_ENUMSESSIONSREPLY;


typedef struct tagDPSP_MSG_ENUMSESSIONS
{
    DPSP_MSG_ENVELOPE  envelope;
    GUID               ApplicationGuid;
    DWORD              PasswordOffset;
    DWORD              Flags;
} DPSP_MSG_ENUMSESSIONS, *LPDPSP_MSG_ENUMSESSIONS;
typedef const DPSP_MSG_ENUMSESSIONS* LPCDPSP_MSG_ENUMSESSIONS;


typedef struct tagDPSP_MSG_ENUMPLAYERSREPLY
{
    DPSP_MSG_ENVELOPE  envelope;
    DWORD              PlayerCount;
    DWORD              GroupCount;
    DWORD              PackedOffset;
    DWORD              ShortcutCount;
    DWORD              DescriptionOffset;
    DWORD              NameOffset;
    DWORD              PasswordOffset;
    DPSESSIONDESC2     DPSessionDesc;
}   DPSP_MSG_ENUMPLAYERSREPLY,      *LPDPSP_MSG_ENUMPLAYERSREPLY,
    DPSP_MSG_SUPERENUMPLAYERSREPLY, *LPDPSP_MSG_SUPERENUMPLAYERSREPLY;
typedef const DPSP_MSG_ENUMPLAYERSREPLY*      LPCDPSP_MSG_ENUMPLAYERSREPLY;
typedef const DPSP_MSG_SUPERENUMPLAYERSREPLY* LPCDPSP_MSG_SUPERENUMPLAYERSREPLY;


typedef struct tagDPSP_MSG_ENUMPLAYER
{
    DPSP_MSG_ENVELOPE  envelope;
}   DPSP_MSG_ENUMPLAYER,   *LPDPSP_MSG_ENUMPLAYER,
    DPSP_MSG_YOUAREDEAD,   *LPDPSP_MSG_YOUAREDEAD,
    DPSP_MSG_LOGONDENIED,  *LPDPSP_MSG_LOGONDENIED;
typedef const DPSP_MSG_ENUMPLAYER*   LPCDPSP_MSG_ENUMPLAYER;
typedef const DPSP_MSG_YOUAREDEAD*   LPCDPSP_MSG_YOUAREDEAD;
typedef const DPSP_MSG_LOGONDENIED*  LPCDPSP_MSG_LOGONDENIED;


typedef struct tagDPSP_MSG_REQUESTPLAYERID
{
    DPSP_MSG_ENVELOPE  envelope;
    DWORD              Flags;
}   DPSP_MSG_REQUESTPLAYERID, *LPDPSP_MSG_REQUESTPLAYERID,
    DPSP_MSG_REQUESTGROUPID,  *LPDPSP_MSG_REQUESTGROUPID;
typedef const DPSP_MSG_REQUESTPLAYERID* LPCDPSP_MSG_REQUESTPLAYERID;
typedef const DPSP_MSG_REQUESTGROUPID*  LPCDPSP_MSG_REQUESTGROUPID;

/* DPSP_MSG_REQUESTGROUPID->Flags */
#define REQUESTGROUPID_PL 0x00000004
#define REQUESTGROUPID_SS 0x00000200


typedef struct tagDPSP_MSG_REQUESTPLAYERREPLY
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               ID;
    DPSECURITYDESC     SecDesc;
    DWORD              SSPIProviderOffset;
    DWORD              CAPIProviderOffset;
    HRESULT            Result;
} DPSP_MSG_REQUESTPLAYERREPLY, *LPDPSP_MSG_REQUESTPLAYERREPLY;
typedef const DPSP_MSG_REQUESTPLAYERREPLY* LPCDPSP_MSG_REQUESTPLAYERREPLY;


typedef struct tagDPSP_MSG_ADDFORWARD
{
    DPSP_MSG_ENVELOPE    envelope;
    DPID                 IDTo;
    DPID                 PlayerID;
    DPID                 GroupID;
    DWORD                CreateOffset;
    DWORD                PasswordOffset;
}   DPSP_MSG_CREATEPLAYER,       *LPDPSP_MSG_CREATEPLAYER,
    DPSP_MSG_ADDFORWARD,         *LPDPSP_MSG_ADDFORWARD,
    DPSP_MSG_CREATEPLAYERVERIFY, *LPDPSP_MSG_CREATEPLAYERVERIFY,
    DPSP_MSG_CREATEGROUP,        *LPDPSP_MSG_CREATEGROUP;
typedef const DPSP_MSG_CREATEPLAYER*       LPCDPSP_MSG_CREATEPLAYER;
typedef const DPSP_MSG_ADDFORWARD*         LPCDPSP_MSG_ADDFORWARD;
typedef const DPSP_MSG_CREATEPLAYERVERIFY* LPCDPSP_MSG_CREATEPLAYERVERIFY;
typedef const DPSP_MSG_CREATEGROUP*        LPCDPSP_MSG_CREATEGROUP;


typedef struct tagDPSP_MSG_PLAYERMESSAGE
{
    DPSP_MSG_ENVELOPE  envelope;
    DWORD              mixed;
    SOCKADDR_IN        sockaddr;
    DPID               idFrom;
    DPID               idTo;
} DPSP_MSG_PLAYERMESSAGE, *LPDPSP_MSG_PLAYERMESSAGE;
typedef const DPSP_MSG_PLAYERMESSAGE* LPCDPSP_MSG_PLAYERMESSAGE;


typedef struct tagDPSP_MSG_DELETEPLAYER
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDTo;
    DPID               PlayerID;
    /*   "GroupID" in ADDSHORTCUTTOGROUP and DELETEGROUPFROMGROUP */
    DPID               ChildGroupID;
    /*   "ParentGroupID" in ADDSHORTCUTTOGROUP and DELETEGROUPFROMGROUP */
    DWORD              CreateOffset;
    DWORD              PasswordOffset;
}   DPSP_MSG_DELETEPLAYER,          *LPDPSP_MSG_DELETEPLAYER,
    DPSP_MSG_DELETEGROUP,           *LPDPSP_MSG_DELETEGROUP,
    DPSP_MSG_ADDPLAYERTOGROUP,      *LPDPSP_MSG_ADDPLAYERTOGROUP,
    DPSP_MSG_DELETEPLAYERFROMGROUP, *LPDPSP_MSG_DELETEPLAYERFROMGROUP,
    DPSP_MSG_ADDSHORTCUTTOGROUP,    *LPDPSP_MSG_ADDSHORTCUTTOGROUP,
    DPSP_MSG_DELETEGROUPFROMGROUP,  *LPDPSP_MSG_DELETEGROUPFROMGROUP;
typedef const DPSP_MSG_DELETEPLAYER*          LPCDPSP_MSG_DELETEPLAYER;
typedef const DPSP_MSG_DELETEGROUP*           LPCDPSP_MSG_DELETEGROUP;
typedef const DPSP_MSG_ADDPLAYERTOGROUP*      LPCDPSP_MSG_ADDPLAYERTOGROUP;
typedef const DPSP_MSG_DELETEPLAYERFROMGROUP* LPCDPSP_MSG_DELETEPLAYERFROMGROUP;
typedef const DPSP_MSG_ADDSHORTCUTTOGROUP*    LPCDPSP_MSG_ADDSHORTCUTTOGROUP;
typedef const DPSP_MSG_DELETEGROUPFROMGROUP*  LPCDPSP_MSG_DELETEGROUPFROMGROUP;


typedef struct tagDPSP_MSG_PLAYERDATACHANGED
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDTo;
    DPID               PlayerID;
    DWORD              DataOffset;
} DPSP_MSG_PLAYERDATACHANGED, *LPDPSP_MSG_PLAYERDATACHANGED;
typedef const DPSP_MSG_PLAYERDATACHANGED* LPCDPSP_MSG_PLAYERDATACHANGED;


typedef struct tagDPSP_MSG_PLAYERNAMECHANGED
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDTo;
    DPID               PlayerID;
    DWORD              ShortOffset;
    DWORD              LongOffset;
} DPSP_MSG_PLAYERNAMECHANGED, *LPDPSP_MSG_PLAYERNAMECHANGED;
typedef const DPSP_MSG_PLAYERNAMECHANGED* LPCDPSP_MSG_PLAYERNAMECHANGED;


typedef struct tagDPSP_MSG_GROUPDATACHANGED
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDTo;
    DPID               GroupID;
    DWORD              dwDataSize;
    DWORD              dwDataOffset;
} DPSP_MSG_GROUPDATACHANGED, *LPDPSP_MSG_GROUPDATACHANGED;
typedef const DPSP_MSG_GROUPDATACHANGED* LPCDPSP_MSG_GROUPDATACHANGED;


typedef struct tagDPSP_MSG_GROUPNAMECHANGED
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDTo;
    DPID               GroupID;
    DWORD              ShortOffset;
    DWORD              LongOffset;
} DPSP_MSG_GROUPNAMECHANGED, *LPDPSP_MSG_GROUPNAMECHANGED;
typedef const DPSP_MSG_GROUPNAMECHANGED* LPCDPSP_MSG_GROUPNAMECHANGED;


typedef struct tagDPSP_MSG_ADDFORWARDREQUEST
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDTo;
    DPID               PlayerID;
    DPID               GroupID;
    DWORD              CreateOffset;
    DWORD              PasswordOffset;
} DPSP_MSG_ADDFORWARDREQUEST, *LPDPSP_MSG_ADDFORWARDREQUEST;
typedef const DPSP_MSG_ADDFORWARDREQUEST* LPCDPSP_MSG_ADDFORWARDREQUEST;


typedef struct tagDPSP_MSG_PACKET
{
    DPSP_MSG_ENVELOPE  envelope;
    GUID               GuidMessage;
    DWORD              PacketIndex;
    DWORD              DataSize;
    DWORD              Offset;
    DWORD              TotalPackets;
    DWORD              MessageSize;
    DWORD              PackedOffset;
}   DPSP_MSG_PACKET,       *LPDPSP_MSG_PACKET,
    DPSP_MSG_PACKET2_DATA, *LPDPSP_MSG_PACKET2_DATA;
typedef const DPSP_MSG_PACKET* LPCDPSP_MSG_PACKET;
typedef const DPSP_MSG_PACKET2_DATA* LPCDPSP_MSG_PACKET2_DATA;


typedef struct tagDPSP_MSG_PING
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDFrom;
    DWORD              TickCount;
}   DPSP_MSG_PING,      *LPDPSP_MSG_PING,
    DPSP_MSG_PINGREPLY, *LPDPSP_MSG_PINGREPLY;
typedef const DPSP_MSG_PING*      LPCDPSP_MSG_PING;
typedef const DPSP_MSG_PINGREPLY* LPCDPSP_MSG_PINGREPLY;


typedef struct tagDPSP_MSG_PLAYERWRAPPER
{
    DPSP_MSG_ENVELOPE       envelope;
    DPSP_MSG_PLAYERMESSAGE  PlayerMessage;
} DPSP_MSG_PLAYERWRAPPER, *LPDPSP_MSG_PLAYERWRAPPER;
typedef const DPSP_MSG_PLAYERWRAPPER* LPCDPSP_MSG_PLAYERWRAPPER;


typedef struct tagDPSP_MSG_SESSIONDESCCHANGED
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDTo;
    DWORD              SessionNameOffset;
    DWORD              PasswordOffset;
    DPSESSIONDESC2     SessionDesc;
} DPSP_MSG_SESSIONDESCCHANGED, *LPDPSP_MSG_SESSIONDESCCHANGED;
typedef const DPSP_MSG_SESSIONDESCCHANGED* LPCDPSP_MSG_SESSIONDESCCHANGED;


typedef struct tagDPSP_MSG_CHALLENGE
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDFrom;
    DWORD              DataSize;
    DWORD              DataOffset;
} DPSP_MSG_CHALLENGE, *LPDPSP_MSG_CHALLENGE;
typedef const DPSP_MSG_CHALLENGE* LPCDPSP_MSG_CHALLENGE;


typedef struct tagDPSP_MSG_ACCESSGRANTED
{
    DPSP_MSG_ENVELOPE  envelope;
    DWORD              PublicKeySize;
    DWORD              PublicKeyOffset;
} DPSP_MSG_ACCESSGRANTED, *LPDPSP_MSG_ACCESSGRANTED;
typedef const DPSP_MSG_ACCESSGRANTED* LPCDPSP_MSG_ACCESSGRANTED;


typedef struct tagDPSP_MSG_NEGOTIATE
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDFrom;
    DWORD              DataSize;
    DWORD              DataOffset;
}   DPSP_MSG_NEGOTIATE,         *LPDPSP_MSG_NEGOTIATE,
    DPSP_MSG_CHALLENGERESPONSE, *LPDPSP_MSG_CHALLENGERESPONSE;
typedef const DPSP_MSG_NEGOTIATE*         LPCDPSP_MSG_NEGOTIATE;
typedef const DPSP_MSG_CHALLENGERESPONSE* LPCDPSP_MSG_CHALLENGERESPONSE;


typedef struct tagDPSP_MSG_SIGNED
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDFrom;
    DWORD              DataOffset;
    DWORD              DataSize;
    DWORD              SignatureSize;
    DWORD              Flags;
} DPSP_MSG_SIGNED, *LPDPSP_MSG_SIGNED;
typedef const DPSP_MSG_SIGNED* LPCDPSP_MSG_SIGNED;

/* DPSP_MSG_SIGNED->Flags */
#define SIGNED_SS 0x00000001
#define SIGNED_SC 0x00000002
#define SIGNED_EC 0x00000004


typedef struct tagDPSP_MSG_ADDFORWARDREPLY
{
    DPSP_MSG_ENVELOPE  envelope;
    HRESULT            Error;
}   DPSP_MSG_ADDFORWARDREPLY, *LPDPSP_MSG_ADDFORWARDREPLY,
    DPSP_MSG_AUTHERROR,       *LPDPSP_MSG_AUTHERROR;
typedef const DPSP_MSG_ADDFORWARDREPLY* LPCDPSP_MSG_ADDFORWARDREPLY;
typedef const DPSP_MSG_AUTHERROR*       LPCDPSP_MSG_AUTHERROR;



typedef struct tagDPSP_MSG_ASK4MULTICAST
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               GroupTo;
    DPID               PlayerFrom;
    DWORD              MessageOffset;
}   DPSP_MSG_ASK4MULTICAST,           *LPDPSP_MSG_ASK4MULTICAST,
    DPSP_MSG_ASK4MULTICASTGUARANTEED, *LPDPSP_MSG_ASK4MULTICASTGUARANTEED;
typedef const DPSP_MSG_ASK4MULTICAST*           LPCDPSP_MSG_ASK4MULTICAST;
typedef const DPSP_MSG_ASK4MULTICASTGUARANTEED* LPCDPSP_MSG_ASK4MULTICASTGUARANTEED;


typedef struct tagDPSP_MSG_KEYEXCHANGE
{
    DPSP_MSG_ENVELOPE  envelope;
    DWORD              SessionKeyOffset;
    DWORD              PublicKeySize;
    DWORD              PublicKeyOffset;
} DPSP_MSG_KEYEXCHANGE, *LPDPSP_MSG_KEYEXCHANGE;
typedef const DPSP_MSG_KEYEXCHANGE* LPCDPSP_MSG_KEYEXCHANGE;


typedef struct tagDPSP_MSG_KEYEXCHANGEREPLY
{
    DPSP_MSG_ENVELOPE  envelope;
    DWORD              SessionKeySize;
    DWORD              SessionKeyOffset;
    DWORD              PublicKeyOffset;
} DPSP_MSG_KEYEXCHANGEREPLY, *LPDPSP_MSG_KEYEXCHANGEREPLY;
typedef const DPSP_MSG_KEYEXCHANGEREPLY* LPCDPSP_MSG_KEYEXCHANGEREPLY;


typedef struct tagDPSP_MSG_CHAT
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDFrom;
    DPID               IDTo;
    DWORD              Flags;
    DWORD              MessageOffset;
} DPSP_MSG_CHAT, *LPDPSP_MSG_CHAT;
typedef const DPSP_MSG_CHAT* LPCDPSP_MSG_CHAT;

/* DPSP_MSG_CHAT->Flags */
#define CHAT_GS 0x00000001


typedef struct tagDPSP_MSG_ADDFORWARDACK
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               ID;
} DPSP_MSG_ADDFORWARDACK, *LPDPSP_MSG_ADDFORWARDACK;
typedef const DPSP_MSG_ADDFORWARDACK* LPCDPSP_MSG_ADDFORWARDACK;


typedef struct tagDPSP_MSG_PACKET2_ACK
{
    DPSP_MSG_ENVELOPE  envelope;
    GUID               GuidMessage;
    DPID               PacketID;
} DPSP_MSG_PACKET2_ACK, *LPDPSP_MSG_PACKET2_ACK;
typedef const DPSP_MSG_PACKET2_ACK* LPCDPSP_MSG_PACKET2_ACK;


typedef struct tagDPSP_MSG_IAMNAMESERVER
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               IDTo;
    DPID               IDHost;
    DWORD              Flags;
    DWORD              SPDataSize;
} DPSP_MSG_IAMNAMESERVER, *LPDPSP_MSG_IAMNAMESERVER;
typedef const DPSP_MSG_IAMNAMESERVER* LPCDPSP_MSG_IAMNAMESERVER;


typedef struct tagDPSP_MSG_VOICE
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               dwIDFrom;
    DPID               dwIDTo;
} DPSP_MSG_VOICE, *LPDPSP_MSG_VOICE;
typedef const DPSP_MSG_VOICE* LPCDPSP_MSG_VOICE;


typedef struct tagDPSP_MSG_MULTICASTDELIVERY
{
    DPSP_MSG_ENVELOPE  envelope;
    DPID               GroupIDTo;
    DPID               PlayerIDFrom;
    DWORD              MessageOffset;
} DPSP_MSG_MULTICASTDELIVERY, *LPDPSP_MSG_MULTICASTDELIVERY;
typedef const DPSP_MSG_MULTICASTDELIVERY* LPCDPSP_MSG_MULTICASTDELIVERY;


#include "poppack.h"

#endif
