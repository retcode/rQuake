/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2024 SDL2 Port

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// net_sdl.c -- Cross-platform UDP network driver using SDL-compatible code

#include "quakedef.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#endif
#define ioctl ioctlsocket
#define close closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#ifdef __sun__
#include <sys/filio.h>
#endif
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

#include "net_udp.h"

extern cvar_t hostname;

static int net_acceptsocket = -1;
static int net_controlsocket;
static int net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;

static unsigned long myAddr;

#ifdef _WIN32
static qboolean winsock_initialized = false;
#endif

//=============================================================================

/*
============
PartialIPAddress

this lets you type only as much of the net address as required, using
the local network components to fill in the rest
============
*/
static int PartialIPAddress(char *in, struct qsockaddr *hostaddr)
{
    char buff[256];
    char *b;
    int addr;
    int num;
    int mask;
    int run;
    int port;

    buff[0] = '.';
    b = buff;
    strcpy(buff + 1, in);
    if (buff[1] == '.')
        b++;

    addr = 0;
    mask = -1;
    while (*b == '.') {
        b++;
        num = 0;
        run = 0;
        while (!(*b < '0' || *b > '9')) {
            num = num * 10 + *b++ - '0';
            if (++run > 3)
                return -1;
        }
        if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
            return -1;
        if (num < 0 || num > 255)
            return -1;
        mask <<= 8;
        addr = (addr << 8) + num;
    }

    if (*b++ == ':')
        port = Q_atoi(b);
    else
        port = net_hostport;

    hostaddr->sa_family = AF_INET;
    ((struct sockaddr_in *)hostaddr)->sin_port = htons((short)port);
    ((struct sockaddr_in *)hostaddr)->sin_addr.s_addr =
        (myAddr & htonl(mask)) | htonl(addr);

    return 0;
}

//=============================================================================

static int GetLocalAddress(void)
{
    struct hostent *local;
    char buff[MAXHOSTNAMELEN];

    if (gethostname(buff, MAXHOSTNAMELEN) == -1)
        return -1;

    local = gethostbyname(buff);
    if (local == NULL)
        return -1;

    myAddr = *(unsigned long *)local->h_addr_list[0];

    // Set hostname cvar if not already set
    if (Q_strcmp(hostname.string, "UNNAMED") == 0) {
        buff[15] = 0;
        Cvar_Set("hostname", buff);
    }

    return 0;
}

int UDP_Init(void)
{
    struct qsockaddr addr;
    char *colon;

    if (COM_CheckParm("-noudp"))
        return -1;

#ifdef _WIN32
    {
        WSADATA winsockdata;
        int r = WSAStartup(MAKEWORD(2, 2), &winsockdata);
        if (r) {
            Con_SafePrintf("Winsock initialization failed.\n");
            return -1;
        }
        winsock_initialized = true;
    }
#endif

    // Determine my name & address
    if (GetLocalAddress() == -1) {
        Con_SafePrintf("UDP_Init: Could not get local address\n");
    }

    if ((net_controlsocket = UDP_OpenSocket(0)) == -1) {
        Con_SafePrintf("UDP_Init: Unable to open control socket\n");
#ifdef _WIN32
        if (winsock_initialized) {
            WSACleanup();
            winsock_initialized = false;
        }
#endif
        return -1;
    }

    ((struct sockaddr_in *)&broadcastaddr)->sin_family = AF_INET;
    ((struct sockaddr_in *)&broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
    ((struct sockaddr_in *)&broadcastaddr)->sin_port = htons((unsigned short)net_hostport);

    UDP_GetSocketAddr(net_controlsocket, &addr);
    Q_strcpy(my_tcpip_address, UDP_AddrToString(&addr));
    colon = Q_strrchr(my_tcpip_address, ':');
    if (colon)
        *colon = 0;

    Con_Printf("UDP Initialized\n");
    tcpipAvailable = true;

    return net_controlsocket;
}

void UDP_Shutdown(void)
{
    UDP_Listen(false);
    UDP_CloseSocket(net_controlsocket);

#ifdef _WIN32
    if (winsock_initialized) {
        WSACleanup();
        winsock_initialized = false;
    }
#endif
}

void UDP_Listen(qboolean state)
{
    if (state) {
        if (net_acceptsocket != -1)
            return;
        if ((net_acceptsocket = UDP_OpenSocket(net_hostport)) == -1)
            Sys_Error("UDP_Listen: Unable to open accept socket\n");
    } else {
        if (net_acceptsocket == -1)
            return;
        UDP_CloseSocket(net_acceptsocket);
        net_acceptsocket = -1;
    }
}

int UDP_OpenSocket(int port)
{
    int newsocket;
    struct sockaddr_in address;
    unsigned long _true = 1;

    if ((newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        return -1;

    if (ioctl(newsocket, FIONBIO, &_true) == -1)
        goto ErrorReturn;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((unsigned short)port);

    if (bind(newsocket, (struct sockaddr *)&address, sizeof(address)) == -1)
        goto ErrorReturn;

    return newsocket;

ErrorReturn:
    close(newsocket);
    return -1;
}

int UDP_CloseSocket(int socket)
{
    if (socket == net_broadcastsocket)
        net_broadcastsocket = 0;
    return close(socket);
}

int UDP_Connect(int socket, struct qsockaddr *addr)
{
    return 0;
}

int UDP_CheckNewConnections(void)
{
    unsigned long available;

    if (net_acceptsocket == -1)
        return -1;

    if (ioctl(net_acceptsocket, FIONREAD, &available) == -1)
        Sys_Error("UDP: ioctlsocket (FIONREAD) failed\n");

    if (available)
        return net_acceptsocket;

    return -1;
}

int UDP_Read(int socket, byte *buf, int len, struct qsockaddr *addr)
{
    socklen_t addrlen = sizeof(struct qsockaddr);
    int ret;

    ret = recvfrom(socket, buf, len, 0, (struct sockaddr *)addr, &addrlen);

    if (ret == -1) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAECONNRESET)
            return 0;
#else
        if (errno == EWOULDBLOCK || errno == ECONNREFUSED)
            return 0;
#endif
    }

    return ret;
}

int UDP_Write(int socket, byte *buf, int len, struct qsockaddr *addr)
{
    int ret;

    ret = sendto(socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct qsockaddr));

    if (ret == -1) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
#else
        if (errno == EWOULDBLOCK)
            return 0;
#endif
    }

    return ret;
}

int UDP_Broadcast(int socket, byte *buf, int len)
{
    int ret;

    if (socket != net_broadcastsocket) {
        if (net_broadcastsocket != 0)
            Sys_Error("Attempted to use multiple broadcasts sockets\n");

        {
            int i = 1;
            ret = setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i));
            if (ret == -1) {
                Con_Printf("Unable to make socket broadcast capable\n");
                return ret;
            }
        }

        net_broadcastsocket = socket;
    }

    return UDP_Write(socket, buf, len, &broadcastaddr);
}

char *UDP_AddrToString(struct qsockaddr *addr)
{
    static char buffer[22];
    int haddr;

    haddr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
    sprintf(buffer, "%d.%d.%d.%d:%d",
            (haddr >> 24) & 0xff,
            (haddr >> 16) & 0xff,
            (haddr >> 8) & 0xff,
            haddr & 0xff,
            ntohs(((struct sockaddr_in *)addr)->sin_port));

    return buffer;
}

int UDP_StringToAddr(char *string, struct qsockaddr *addr)
{
    int ha1, ha2, ha3, ha4, hp;
    int ipaddr;

    sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
    ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;

    addr->sa_family = AF_INET;
    ((struct sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
    ((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)hp);

    return 0;
}

int UDP_GetSocketAddr(int socket, struct qsockaddr *addr)
{
    socklen_t addrlen = sizeof(struct qsockaddr);
    unsigned int a;

    Q_memset(addr, 0, sizeof(struct qsockaddr));
    getsockname(socket, (struct sockaddr *)addr, &addrlen);

    a = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
    if (a == 0 || a == htonl(INADDR_LOOPBACK))
        ((struct sockaddr_in *)addr)->sin_addr.s_addr = myAddr;

    return 0;
}

int UDP_GetNameFromAddr(struct qsockaddr *addr, char *name)
{
    struct hostent *hostentry;

    hostentry = gethostbyaddr((char *)&((struct sockaddr_in *)addr)->sin_addr,
                              sizeof(struct in_addr), AF_INET);
    if (hostentry) {
        Q_strncpy(name, (char *)hostentry->h_name, NET_NAMELEN - 1);
        return 0;
    }

    Q_strcpy(name, UDP_AddrToString(addr));
    return 0;
}

int UDP_GetAddrFromName(char *name, struct qsockaddr *addr)
{
    struct hostent *hostentry;

    if (name[0] >= '0' && name[0] <= '9')
        return PartialIPAddress(name, addr);

    hostentry = gethostbyname(name);
    if (!hostentry)
        return -1;

    addr->sa_family = AF_INET;
    ((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)net_hostport);
    ((struct sockaddr_in *)addr)->sin_addr.s_addr =
        *(unsigned long *)hostentry->h_addr_list[0];

    return 0;
}

int UDP_AddrCompare(struct qsockaddr *addr1, struct qsockaddr *addr2)
{
    if (addr1->sa_family != addr2->sa_family)
        return -1;

    if (((struct sockaddr_in *)addr1)->sin_addr.s_addr !=
        ((struct sockaddr_in *)addr2)->sin_addr.s_addr)
        return -1;

    if (((struct sockaddr_in *)addr1)->sin_port !=
        ((struct sockaddr_in *)addr2)->sin_port)
        return 1;

    return 0;
}

int UDP_GetSocketPort(struct qsockaddr *addr)
{
    return ntohs(((struct sockaddr_in *)addr)->sin_port);
}

int UDP_SetSocketPort(struct qsockaddr *addr, int port)
{
    ((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)port);
    return 0;
}

//=============================================================================
// Net drivers table (replaces net_bsd.c)
//=============================================================================

#include "net_loop.h"
#include "net_dgrm.h"

net_driver_t net_drivers[MAX_NET_DRIVERS] = {
    {
        "Loopback",
        false,
        Loop_Init,
        Loop_Listen,
        Loop_SearchForHosts,
        Loop_Connect,
        Loop_CheckNewConnections,
        Loop_GetMessage,
        Loop_SendMessage,
        Loop_SendUnreliableMessage,
        Loop_CanSendMessage,
        Loop_CanSendUnreliableMessage,
        Loop_Close,
        Loop_Shutdown
    },
    {
        "Datagram",
        false,
        Datagram_Init,
        Datagram_Listen,
        Datagram_SearchForHosts,
        Datagram_Connect,
        Datagram_CheckNewConnections,
        Datagram_GetMessage,
        Datagram_SendMessage,
        Datagram_SendUnreliableMessage,
        Datagram_CanSendMessage,
        Datagram_CanSendUnreliableMessage,
        Datagram_Close,
        Datagram_Shutdown
    }
};

int net_numdrivers = 2;

net_landriver_t net_landrivers[MAX_NET_DRIVERS] = {
    {
        "UDP",
        false,
        0,
        UDP_Init,
        UDP_Shutdown,
        UDP_Listen,
        UDP_OpenSocket,
        UDP_CloseSocket,
        UDP_Connect,
        UDP_CheckNewConnections,
        UDP_Read,
        UDP_Write,
        UDP_Broadcast,
        UDP_AddrToString,
        UDP_StringToAddr,
        UDP_GetSocketAddr,
        UDP_GetNameFromAddr,
        UDP_GetAddrFromName,
        UDP_AddrCompare,
        UDP_GetSocketPort,
        UDP_SetSocketPort
    }
};

int net_numlandrivers = 1;
