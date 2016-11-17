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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "clicker.h"
#include "commands.h"
#include <stdint.h>
#include <semaphore.h>

#define TCP_PORT                                (49300)
#define MAX_CLIENTS                             (30)
#define KEEP_ALIVE_INTERVAL_MS                  (2000)
#define KEEP_ALIVE_TIMEOUT_MS                   (30000)
#define CHECK_CONNECTIONS_INTERVAL_MS			(2000)

/**
 * @brief Keeps the number of currently connected clickers
 */
extern int g_pd_ConnectedClickers;

typedef void (*pd_CommandCallback)(Clicker *clicker, uint8_t * buffer);
typedef void (*pd_ClickerConnectedCallback)(Clicker *clicker, char *ip);
typedef void (*pd_ClickerDisconnectedCallback)(Clicker *clicker);

/**
 * @brief Initiates socket, binds to it and start listening fror incoming connections
 * @param commandCallback function called upon commands receive from clicker
 * @param clickerConnectedCallback function called upon new clicker connection
 * @param[in] clickerDisconnectedCallback function called upon clicker disconnection
 */
int con_BindAndListen(
	int tcpPort,
	pd_CommandCallback commandCallback,
	pd_ClickerConnectedCallback clickerConnectedCallback,
	pd_ClickerDisconnectedCallback clickerDisconnectedCallback);

/**
 *  @brief Accepts incoming connections and handles read from socket. Should be called periodically.
 */
void con_ProcessConnections(void);

/**
 * @brief Sends command to specified clicker.
 * @param clicker to which command will be send
 * @param command to send
 */
void con_SendCommand(Clicker* clicker, NetworkCommand command);

/**
 * @brief Send command to specified clicker.
 * @param clicker to which command will be send
 * @param command to send
 * @param data additional data that should be included in message
 * @param dataLength number of bytes in data array.
 */
void con_SendCommandWithData(Clicker* clicker, NetworkCommand command, uint8_t *data, uint8_t dataLength);

/**
 * @brief Gets position in clicker list of specified clicker
 * @return positive number being a position in list or -1 if specified clicker does not exist in list.
 */
int con_GetIndexOfClicker(Clicker* clicker);

/**
 * @brief Disconnect specified clicker
 * @param[in] clicker to disconnect
 */
void con_Disconnect(Clicker *clicker);

#endif
