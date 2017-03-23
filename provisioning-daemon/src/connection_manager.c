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
#include <glib.h>
#include <string.h>

typedef struct {
    int clickerID; /**< id of clicker bound to this connection. */
    gint64 lastKeepAliveTime; /**< unix timestamp of last KEEP_ALIVE command sent to this clicker */
    int socket; /**< socket descriptor on which this clicker operates */
    char ip[INET6_ADDRSTRLEN]; /**< textual representation of IP, null terminated */
    uint16_t port; /** Port on which socket is bound */
} ConnectionData;

static GList* _connectionsList = NULL;

static int _MasterSocket;
static unsigned long _LastKeepAliveSendTime = 0;
static unsigned long _LastCheckConnectionsTime = 0;
static int _IDCounter = 0; /**< Used to give UIDs for newly created clickers */

void ConnectionDataToString(ConnectionData* connection, char* buf, size_t bufLen) {
    snprintf(buf, bufLen, "ClickerID:%d, ip:[%s], socket:%d, port:%d, keepAliveTime:%lu", connection->clickerID,
            connection->ip, connection->socket, connection->port, connection->lastKeepAliveTime);
}

static void HandleDisconnect(ConnectionData* connection) {
    char buf[1024];
    ConnectionDataToString(connection, buf, sizeof(buf));
    g_message("Clicker disconnected, %s\n", buf);
    close(connection->socket);

    event_PushEventWithInt(EventType_CLICKER_DESTROY, connection->clickerID);

    _connectionsList = g_list_remove(_connectionsList, connection);
    g_free(connection);
}

static void AcceptConnection() {
    int newSocket = 0;
    struct sockaddr_in6 address;
    socklen_t addrLen = sizeof(address);

    if ((newSocket = accept(_MasterSocket, (struct sockaddr *) &address, &addrLen)) < 0) {
        g_critical("Error accepting connection. Errno: %d \n", errno);
        return;
    }

    _IDCounter++;

    ConnectionData* connection = g_new0(ConnectionData, 1);
    connection->clickerID = _IDCounter;
    connection->lastKeepAliveTime = g_get_monotonic_time() / 1000;
    connection->socket = newSocket;
    connection->port = ntohs(address.sin6_port);

    memset(connection->ip, 0, sizeof(connection->ip));
    if (inet_ntop(AF_INET6, &address.sin6_addr, connection->ip, INET6_ADDRSTRLEN) < 0) {
        g_critical("Failed to convert ipv6 address to string of clicker id: %d", connection->clickerID);
        strlcpy(connection->ip, "::1", INET6_ADDRSTRLEN);
    }

    _connectionsList = g_list_prepend(_connectionsList, connection);

    event_PushEventWithInt(EventType_CLICKER_CREATE, connection->clickerID);

    char buf[1024];
    ConnectionDataToString(connection, buf, sizeof(buf));
    g_message("New clicker connected: %s\n", buf);
}

static void HandleReceivedData(ConnectionData* connection, uint8_t* buffer, size_t dataLen) {
    NetworkCommand cmd = buffer[0];
    if (cmd == NetworkCommand_KEEP_ALIVE) {
        connection->lastKeepAliveTime = g_get_monotonic_time() / 1000;
        //g_debug("Got keepalive response for clicker:%d", connection->clickerID);

    } else {
        dataLen--; //skip info about command (1 byte)
        NetworkDataPack* data = con_BuildNetworkDataPack(connection->clickerID, cmd, buffer + 1, dataLen, true);
        event_PushEventWithPtr(EventType_CONNECTION_RECEIVED_COMMAND, data, true);
    }
}

static int HandleRead(fd_set* readFS) {
    int socket = 0;
    size_t valread = 0;

    GList* tmpList = g_list_copy(_connectionsList);
    uint8_t buffer[1024];
    for (GList* iter = tmpList; iter != NULL; iter = iter->next) {
        ConnectionData* connection = (ConnectionData*) iter->data;
        socket = connection->socket;

        memset(buffer, 0, sizeof(buffer));
        if (FD_ISSET(socket, readFS)) {
            if ((valread = read(socket, buffer, 1024)) == 0) {
                g_debug("Read error. Disconnecting");
                HandleDisconnect(connection);
            } else {
                HandleReceivedData(connection, buffer, valread);
            }
        }
    }

    g_list_free(tmpList);
    return 0;
}

int con_BindAndListen(int tcpPort) {

    int reuse_addr = 1;

    _MasterSocket = socket(AF_INET6, SOCK_STREAM, 0);
    if (_MasterSocket == -1) {
        int err = errno;
        g_critical("Error opening socket. ERRNO: %d \n", err);
        return -1;
    }

    if (setsockopt(_MasterSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0) {
        g_critical("Failed to set socket option");
        return -1;
    }

    struct sockaddr_in6 address;
    address.sin6_family = AF_INET6;
    address.sin6_port = htons(tcpPort);
    address.sin6_addr = in6addr_any;

    if (bind(_MasterSocket, (struct sockaddr *) &address, sizeof(address)) == -1) {
        g_critical("Error binding socket. ERRNO: %d \n", errno);
        return -1;
    }

    listen(_MasterSocket, 5);
    return 0;
}

static void SendCommand(ConnectionData* connection, NetworkCommand command) {
    if (connection == NULL) {
        g_warning("SendCommandWithData: No connection.");
        return;
    }
    char buffer[2];
    buffer[0] = (char) command;
    send(connection->socket, buffer, 1, 0);
}

static void SendCommandWithData(ConnectionData* connection, NetworkCommand command, uint8_t *data, uint8_t dataLength) {
    if (data == NULL || dataLength == 0) {
        g_warning("SendCommandWithData: Tried to send command with no needed data.");
        return;
    }
    if (connection == NULL) {
        g_warning("SendCommandWithData: No connection.");
        return;
    }
    uint8_t buffer[dataLength + 2];

    buffer[0] = command;
    buffer[1] = dataLength;
    memcpy(&buffer[2], data, dataLength);
    send(connection->socket, buffer, dataLength + 2, 0);
}

static void CheckConnections(void) {
    gint64 currentTimeMillis = g_get_monotonic_time() / 1000;
    if (currentTimeMillis - _LastCheckConnectionsTime > CHECK_CONNECTIONS_INTERVAL_MS) {
        _LastCheckConnectionsTime = currentTimeMillis;

        for (GList* iter = _connectionsList; iter != NULL; iter = iter->next) {
            ConnectionData* connection = (ConnectionData*) iter->data;
            if (currentTimeMillis - connection->lastKeepAliveTime > KEEP_ALIVE_TIMEOUT_MS) HandleDisconnect(connection);
        }
    }
}

static void SendKeepAlive(void) {
    unsigned long currentTimeMillis = g_get_monotonic_time() / 1000;
    if (currentTimeMillis - _LastKeepAliveSendTime > KEEP_ALIVE_INTERVAL_MS) {
        _LastKeepAliveSendTime = currentTimeMillis;
        for (GList* iter = _connectionsList; iter != NULL; iter = iter->next) {
            SendCommand(iter->data, NetworkCommand_KEEP_ALIVE);
        }
    }
}

void con_ProcessConnections(void) {
    int activity;
    fd_set readFS;
    FD_ZERO(&readFS);
    FD_SET(_MasterSocket, &readFS);
    int maxSD = _MasterSocket;

    GList* iter = _connectionsList;
    while (iter != NULL) {
        ConnectionData* data = (ConnectionData*) iter->data;
        int socket = data->socket;
        if (socket > 0)
        FD_SET(socket, &readFS);

        if (socket > maxSD) maxSD = socket;

        iter = iter->next;
    }
    struct timeval selectTimeout;
    selectTimeout.tv_sec = 0;
    selectTimeout.tv_usec = 2000;

    activity = select(maxSD + 1, &readFS, NULL, NULL, &selectTimeout);

    if (activity < 0) {
        g_critical("select error. Errno: %d", errno);
        return;
    }

    if (FD_ISSET(_MasterSocket, &readFS))  //handle incoming connection
        AcceptConnection();
    else
        //handle read
        HandleRead(&readFS);

    SendKeepAlive();
    CheckConnections();
}

gint CompareConnectionByClickerId(gpointer a, gpointer b) {
    ConnectionData* conn1 = (ConnectionData*) a;
    ConnectionData* conn2 = (ConnectionData*) b;
    return conn1->clickerID - conn2->clickerID;
}

ConnectionData* ConnectionForClickerId(int clickerID) {
    ConnectionData tmp = { .clickerID = clickerID };
    GList* found = g_list_find_custom(_connectionsList, &tmp, (GCompareFunc) CompareConnectionByClickerId);
    return found != NULL ? found->data : NULL;
}

void con_Disconnect(int clickerID) {
    ConnectionData* found = ConnectionForClickerId(clickerID);
    if (found != NULL) {
        HandleDisconnect(found);
    }
}

void HandleSendCommandEvent(NetworkDataPack* data) {
    ConnectionData* connection = ConnectionForClickerId(data->clickerID);
    if (connection == NULL) {
        g_critical("Can't send data to clicker %d, connection not found! Command to send: %d", data->clickerID,
                data->command);
        return;
    }
    if (data->data != NULL && data->dataSize != 0) {
        if (data->dataSize > 255) {
            g_critical("Data size to send is to big, clickerId:%d, command:%d, size:%d", data->clickerID,
                    data->command, data->dataSize);
        }
        SendCommandWithData(connection, data->command, data->data, (uint8_t) data->dataSize);
        g_free(data->data);

    } else {
        SendCommand(connection, data->command);
    }
}

bool con_ConsumeEvent(Event* event) {
    switch (event->type) {
        case EventType_CONNECTION_SEND_COMMAND:
            HandleSendCommandEvent(event->ptrData);
            return true;

        default:
            break;
    }
    return false;
}

NetworkDataPack* con_BuildNetworkDataPack(int clickerID, NetworkCommand cmd, uint8_t* data, uint16_t dataLen,
bool copyData) {

    NetworkDataPack* pack = g_new(NetworkDataPack, 1);
    pack->clickerID = clickerID;
    pack->command = cmd;
    if (copyData && dataLen > 0) {
        pack->data = g_malloc(dataLen);
        memcpy(pack->data, data, dataLen);

    } else {
        pack->data = data;
    }
    pack->dataSize = dataLen;

    return pack;
}

char* con_GetIPForClicker(int clickerId) {
    ConnectionData* data = ConnectionForClickerId(clickerId);
    return data != NULL ? (char*) data->ip : NULL;
}
