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

#include <unistd.h>
#include "clicker_sm.h"
#include "log.h"
#include "crypto/crypto_config.h"
#include "ubus_agent.h"
#include "connection_manager.h"
#include "utils.h"
#include "errors.h"

static void HandleRemoteKeyNetworkCommand(int clickerId, uint8_t *data)
{
    Clicker *clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        LOG(LOG_ERR, "HandleRemoteKeyNetworkCommand: Can't acquire clicker with id:%d, this is probably internal error",
                clickerId);
        return;
    }
    uint8_t dataLength = data[0];
    G_FREE_AND_NULL(clicker->remoteKey);
    clicker->remoteKey = g_malloc(dataLength);
    clicker->remoteKeyLength = dataLength;
    memcpy(clicker->remoteKey, &data[1], dataLength);
    clicker_ReleaseOwnership(clicker);

    LOG(LOG_INFO, "Received exchange key from clicker : %d", clicker->clickerID);
    PRINT_BYTES(clicker->remoteKey, clicker->remoteKeyLength);
}


static void GenerateSharedClickerKey(int clickerId)
{
    Clicker *clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        LOG(LOG_ERR, "GenerateSharedClickerKey: Can't acquire clicker with id:%d, this is probably internal error",
                clickerId);
        return;
    }

    clicker->sharedKey = dh_completeExchangeData(clicker->keysExchanger, clicker->remoteKey, clicker->remoteKeyLength);
    clicker->sharedKeyLength = clicker->keysExchanger->pModuleLength;

    LOG(LOG_INFO, "Generated Shared Key");
    PRINT_BYTES(clicker->sharedKey, clicker->sharedKeyLength);

    clicker_ReleaseOwnership(clicker);

    event_PushEventWithInt(EventType_TRY_TO_SEND_PSK_TO_CLICKER, clickerId);
}

static bool NetworkCommandHandler(NetworkDataPack* netData)
{
    switch (netData->command) {
        case NetworkCommand_KEY:
            LOG(LOG_DBG, "Received KEY command");
            HandleRemoteKeyNetworkCommand(netData->clickerID, netData->data);
            GenerateSharedClickerKey(netData->clickerID);
            break;

        default:
            break;
    }
    return false;
}

void GenerateLocalClickerKey(int clickerId) {
    Clicker *clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        LOG(LOG_ERR, "GenerateLocalClickerKey: Can't acquire clicker with id:%d, this is probably internal error",
                clickerId);
        return;
    }
    DiffieHellmanKeysExchanger *keysExchanger = clicker->keysExchanger;
    clicker->localKey = (void*) dh_generateExchangeData(keysExchanger);
    clicker->localKeyLength = keysExchanger->pModuleLength;

    LOG(LOG_INFO, "Generated local Key");
    PRINT_BYTES(clicker->localKey, clicker->localKeyLength);
    LOG(LOG_INFO, "Sending local Key to clicker with id : %d", clickerId);

    NetworkDataPack* netData = con_BuildNetworkDataPack(clickerId, NetworkCommand_KEY, clicker->localKey,
            clicker->localKeyLength, true);
    event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);

    clicker_ReleaseOwnership(clicker);
}

static void ObtainedPSK(PreSharedKey* pskData) {

    Clicker *clicker = clicker_AcquireOwnership(pskData->clickerId);
    if (clicker == NULL) {
        LOG(LOG_ERR, "ObtainedPSK: Can't acquire clicker with id:%d, this is probably internal error",
                pskData->clickerId);
        return;
    }

    if (pskData->psk == NULL) {
        LOG(LOG_WARN, "Couldn't get PSK from Device Server");
        clicker->error = pd_Error_GENERATE_PSK;
        clicker->provisioningInProgress = false;
        clicker_ReleaseOwnership(clicker);
        return;
    }

    clicker->pskLen = pskData->pskLen / 2;
    clicker->psk = g_malloc(clicker->pskLen);
    HexStringToByteArray(pskData->psk, clicker->psk, clicker->pskLen);

    clicker->identityLen = pskData->identityLen;
    clicker->identity = g_malloc(pskData->identityLen + 1);
    strncpy((char*)clicker->identity, pskData->identity, pskData->identityLen);

    clicker_ReleaseOwnership(clicker);

    event_PushEventWithInt(EventType_HISTORY_ADD, pskData->clickerId);
    event_PushEventWithInt(EventType_TRY_TO_SEND_PSK_TO_CLICKER, pskData->clickerId);
}

bool clicker_sm_ConsumeEvent(Event* event) {
    switch (event->type) {
        case EventType_CLICKER_DESTROY:
            return true;

        case EventType_CLICKER_CREATE:
            GenerateLocalClickerKey(event->intData);
            return true;

        case EventType_CONNECTION_RECEIVED_COMMAND:
            return NetworkCommandHandler((NetworkDataPack*) event->ptrData);

        case EventType_CLICKER_START_PROVISION:
            ubusagent_SendGeneratePskMessage(event->intData);
            return true;

        case EventType_PSK_OBTAINED:
            ObtainedPSK((PreSharedKey*)event->ptrData);
            return true;

        default:
            break;
    }
    return false;
}
