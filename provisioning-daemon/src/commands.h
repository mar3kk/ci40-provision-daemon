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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <stdint.h>

#define COMMAND_ENDPOINT_NAME_LENGTH 24

typedef enum {
  NetworkCommand_NONE = 0,
  NetworkCommand_ENABLE_HIGHLIGHT,
  NetworkCommand_DISABLE_HIGHLIGHT,
  NetworkCommand_KEEP_ALIVE,
  NetworkCommand_KEY,
  NetworkCommand_DEVICE_SERVER_CONFIG,
  NetworkCommand_NETWORK_CONFIG
} NetworkCommand;

typedef struct __attribute__((__packed__))
{
	uint8_t securityMode;
    uint8_t pskKeySize;
    uint8_t psk[32];
    uint8_t bootstrapUri[200];
} pd_DeviceServerConfig;

typedef struct __attribute__((__packed__))
{
  char defaultRouteUri[100];  //can be IPv6 or URL
  char dnsServer[100];
  char endpointName[COMMAND_ENDPOINT_NAME_LENGTH];
} pd_NetworkConfig;




#endif
