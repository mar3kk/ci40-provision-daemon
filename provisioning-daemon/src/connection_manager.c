/***************************************************************************************************
 * Copyright (c) 2016, Imagination Technologies Limited and/or its affiliated group companies
 * and/or licensors
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "connection_manager.h"
#include "log.h"
#include "utils.h"
#include "clicker.h"
#include "provision_history.h"

#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <stdlib.h>



static struct timeval _SelectTimeout;
static int _MasterSocket;
static struct sockaddr_in6 _Address;
static int _MaxSD;
static char _Buffer[1024];
static pd_CommandCallback _CommandCallback;
static pd_ClickerDisconnectedCallback _ClickerDisconnectedCallback;
static pd_ClickerConnectedCallback _ClickerConnectedCallback;
static unsigned long _LastKeepAliveSendTime = 0;
static unsigned long _LastCheckConnectionsTime = 0;
static int _SelectedSocket;
static int _Addrlen;
static char _Inet6AddrBuffer[INET6_ADDRSTRLEN];
static fd_set _Readfs;


int g_pd_ConnectedClickers = 0;


static void HandleDisconnect(Clicker *clicker)
{
	int socket = clicker->socket;
    getpeername(socket , (struct sockaddr*)&_Address , (socklen_t*)&_Addrlen);
	inet_ntop(AF_INET6, &(_Address).sin6_addr, _Inet6AddrBuffer, INET6_ADDRSTRLEN);
    LOG(LOG_INFO, "Clicker disconnected, id : %d , ip %s , port %d \n" , clicker->clickerID, _Inet6AddrBuffer , ntohs(_Address.sin6_port));
    close( socket );
	_ClickerDisconnectedCallback(clicker);
	clicker_Release(clicker);
	g_pd_ConnectedClickers--;
}

static void AcceptConnection(struct sockaddr_in6 *address)
{
	int newSocket = 0;

    if ((newSocket = accept(_MasterSocket, (struct sockaddr *)address, (socklen_t*)&_Addrlen))<0)
    {
        LOG(LOG_ERR, "Error accpeting connection. Errno: %d \n", errno);
        return;
    }
    getpeername(newSocket , (struct sockaddr*)&_Address , (socklen_t*)&_Addrlen);
	inet_ntop(AF_INET6, &(*address).sin6_addr, _Inet6AddrBuffer, INET6_ADDRSTRLEN);

	Clicker *newClicker = clicker_New(newSocket);

    LOG(LOG_INFO, "New clicker connected, id : %d, socket fd : %d, ip : %s, port : %d \n",
        newClicker->clickerID, newSocket, _Inet6AddrBuffer, ntohs((*address).sin6_port));
	_ClickerConnectedCallback(newClicker, _Inet6AddrBuffer);

	g_pd_ConnectedClickers++;
}

static int HandleRead(struct sockaddr_in6 *address)
{

	int sd = 0;
	size_t valread = 0;
	Clicker *clicker = clicker_GetClickers();
	while (clicker != NULL)
    {
        sd = clicker->socket;;
        if (FD_ISSET(sd, &_Readfs))
        {
            if ((valread = read(sd, _Buffer, 1024)) == 0)
            {
				LOG(LOG_DBG, "Read error. Disconnecting");
            	HandleDisconnect(clicker);
            	return 0;
            }
            else
            {
				_CommandCallback(clicker, _Buffer);
                return 1;
            }
        }
		clicker = clicker->next;
    }
    return 0;
}



int con_BindAndListen(
	int tcpPort,
	pd_CommandCallback commandCallback,
	pd_ClickerConnectedCallback clickerConnectedCallback,
	pd_ClickerDisconnectedCallback clickerDisconnectedCallback)
{
	_CommandCallback = commandCallback;
	_ClickerConnectedCallback = clickerConnectedCallback;
	_ClickerDisconnectedCallback = clickerDisconnectedCallback;

    int reuse_addr = 1;

    _SelectTimeout.tv_sec = 0;
    _SelectTimeout.tv_usec = 2000;

    _MasterSocket = socket(AF_INET6, SOCK_STREAM, 0);
    if ( _MasterSocket == -1 )
    {
        int err = errno;
        LOG(LOG_ERR, "Error opening socket. ERRNO: %d \n", err);
        return -1;
    }

    if ( setsockopt(_MasterSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0 )
    {
        LOG(LOG_ERR, "Failed to set socket option");
        return -1;
    }

    _Address.sin6_family = AF_INET6;
    _Address.sin6_port = htons(tcpPort);
    _Address.sin6_addr = in6addr_any;


    if (bind(_MasterSocket, (struct sockaddr *)&_Address, sizeof(_Address) ) == -1)
    {
        LOG(LOG_ERR, "Error binding socket. ERRNO: %d \n", errno);
        return -1;
    }

    listen(_MasterSocket, 5);
	return 0;
}

static void CheckConnections()
{
    long currentTimeMillis = GetCurrentTimeMillis();
	Clicker *clicker = clicker_GetClickers();

    while(clicker != NULL)
    {
		if (currentTimeMillis - clicker->lastKeepAliveTime > KEEP_ALIVE_TIMEOUT_MS)
		{
			HandleDisconnect(clicker);
		}
		clicker = clicker->next;
    }
}

void con_ProcessConnections()
{
	int i = 0;
	int activity, sd;
	FD_ZERO(&_Readfs);
    FD_SET(_MasterSocket, &_Readfs);
    _MaxSD = _MasterSocket;

	Clicker *ptr = clicker_GetClickers();

   	while(ptr != NULL)
	{
		sd = ptr->socket;
		if (sd > 0)
           FD_SET(sd, &_Readfs);
        if (sd > _MaxSD)
            _MaxSD = sd;
		ptr = ptr->next;
   	}

    activity = select(_MaxSD + 1, &_Readfs, NULL, NULL, &_SelectTimeout);

    if (activity < 0)
    {
        LOG(LOG_ERR, "select error. Errno: %d", errno);
		return;
    }

    if (FD_ISSET(_MasterSocket, &_Readfs))
    {
        //handle incoming connection
        AcceptConnection(&_Address);
    }
    else
    {
        //handle read
        HandleRead(&_Address);
    }

	unsigned long currentTimeMillis = GetCurrentTimeMillis();
	if (currentTimeMillis - _LastKeepAliveSendTime > KEEP_ALIVE_INTERVAL_MS)
    {

        _LastKeepAliveSendTime = currentTimeMillis;
		Clicker *ptr = clicker_GetClickers();

    	while(ptr != NULL)
		{
			con_SendCommand(ptr, NetworkCommand_KEEP_ALIVE);
			ptr = ptr->next;
    	}
    }

	if (currentTimeMillis - _LastCheckConnectionsTime > CHECK_CONNECTIONS_INTERVAL_MS)
	{

		_LastCheckConnectionsTime = currentTimeMillis;
		CheckConnections();
	}
}


void con_SendCommand(Clicker* clicker, NetworkCommand command)
{
    _Buffer[0] = command;
    send(clicker->socket, _Buffer, 1, 0);
}

void con_SendCommandWithData(Clicker *clicker, NetworkCommand command, uint8_t *data, uint8_t dataLength)
{
	uint8_t buffer[dataLength+2];

	buffer[0] = command;
	buffer[1] = dataLength;
	memcpy(&buffer[2], data, dataLength);
	send(clicker->socket, buffer, dataLength+2, 0);
}

void con_Disconnect(Clicker *clicker)
{
    int socket = clicker->socket;
    getpeername(socket , (struct sockaddr*)&_Address , (socklen_t*)&_Addrlen);
    inet_ntop(AF_INET6, &(_Address).sin6_addr, _Inet6AddrBuffer, INET6_ADDRSTRLEN);
    LOG(LOG_INFO, "Clicker disconnected, id : %d , ip %s , port %d \n" , clicker->clickerID, _Inet6AddrBuffer , ntohs(_Address.sin6_port));
    close( socket );
    _ClickerDisconnectedCallback(clicker);
    clicker_Release(clicker);
    g_pd_ConnectedClickers--;
}
