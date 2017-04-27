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
#include <string.h>
#include "clicker_sm.h"
#include "crypto/crypto_config.h"
#include "crypto/encoder.h"
#include "ubus_agent.h"
#include "connection_manager.h"
#include "utils.h"
#include "errors.h"
#include "provisioning_daemon.h"

static void HandleRemoteKeyNetworkCommand(int clickerId, uint8_t *data)
{
    Clicker *clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        g_critical( "HandleRemoteKeyNetworkCommand: Can't acquire clicker with id:%d, this is probably internal error",
                clickerId);
        return;
    }
    uint8_t dataLength = data[0];
    G_FREE_AND_NULL(clicker->remoteKey);
    clicker->remoteKey = g_malloc(dataLength);
    clicker->remoteKeyLength = dataLength;
    memcpy(clicker->remoteKey, &data[1], dataLength);
    clicker_ReleaseOwnership(clicker);

    g_message("Received exchange key from clicker : %d", clicker->clickerID);
    PRINT_BYTES(clicker->remoteKey, clicker->remoteKeyLength);
}

static void GenerateSharedClickerKey(int clickerId)
{
    Clicker *clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        g_critical( "GenerateSharedClickerKey: Can't acquire clicker with id:%d, this is probably internal error",
                clickerId);
        return;
    }

    clicker->sharedKey = dh_CompleteExchangeData(clicker->keysExchanger, clicker->remoteKey, clicker->remoteKeyLength);
    clicker->sharedKeyLength = clicker->keysExchanger->pModuleLength;

    g_message("Generated Shared Key");
    PRINT_BYTES(clicker->sharedKey, clicker->sharedKeyLength);

    clicker_ReleaseOwnership(clicker);
    event_PushEventWithInt(EventType_TRY_TO_SEND_PSK_TO_CLICKER, clickerId);
}

void TryToSendPsk(int clickerId)
{
    Clicker *clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        g_critical( "TryToSendPsk: Can't acquire clicker with id:%d, wont continue!", clickerId);
        return;
    }

    if (clicker->sharedKey != NULL && clicker->psk != NULL)
    {
        pd_DeviceServerConfig _DeviceServerConfig;
        pd_NetworkConfig _NetworkConfig;

        memset(&_DeviceServerConfig, 0, sizeof(_DeviceServerConfig));
        _DeviceServerConfig.securityMode = 0;

        memcpy(_DeviceServerConfig.psk, clicker->psk, clicker->pskLen);
        _DeviceServerConfig.pskKeySize = clicker->pskLen;

        memcpy(_DeviceServerConfig.identity, clicker->identity, clicker->identityLen);
        _DeviceServerConfig.identitySize = clicker->identityLen;

        memcpy(_DeviceServerConfig.bootstrapUri, _PDConfig.bootstrapUri, strnlen(_PDConfig.bootstrapUri, 200));

        uint8_t dataLen = 0;
        uint8_t *encodedData = softap_EncodeBytes((uint8_t *)&_DeviceServerConfig, sizeof(_DeviceServerConfig) ,
                clicker->sharedKey, &dataLen);
        NetworkDataPack* netData = con_BuildNetworkDataPack(clicker->clickerID, NetworkCommand_DEVICE_SERVER_CONFIG,
                encodedData, dataLen, true);
        event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);
        g_message("Sending Device Server Config to clicker with id : %d", clicker->clickerID);

        memset(&_NetworkConfig, 0, sizeof(_NetworkConfig));
        strlcpy((char*)&_NetworkConfig.defaultRouteUri, _PDConfig.defaultRouteUri, sizeof(_NetworkConfig.defaultRouteUri));
        strlcpy((char*)&_NetworkConfig.dnsServer, _PDConfig.dnsServer, sizeof(_NetworkConfig.dnsServer));
        strlcpy((char*)&_NetworkConfig.endpointName, clicker->name, sizeof(_NetworkConfig.endpointName));

        dataLen = 0;
        encodedData = softap_EncodeBytes((uint8_t *)&_NetworkConfig, sizeof(_NetworkConfig) , clicker->sharedKey, &dataLen);
        netData = con_BuildNetworkDataPack(clicker->clickerID, NetworkCommand_NETWORK_CONFIG, encodedData, dataLen, true);
        event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);
        G_FREE_AND_NULL(encodedData);

        g_message("Sent Network Config to clicker with id : %d", clicker->clickerID);
        g_message("Provisioning of clicker with id : %d finished, going back to LISTENING mode", clicker->clickerID);
        clicker->provisionTime = g_get_monotonic_time() / 1000;
        clicker->provisioningInProgress = false;
    } else {
        g_message("TryToSendPsk: Can't send not all data avail, this is not error.");
    }

    clicker_ReleaseOwnership(clicker);
}

static bool NetworkCommandHandler(NetworkDataPack* netData)
{
    switch (netData->command) {
        case NetworkCommand_KEY:
            g_debug("Received KEY command");
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
        g_critical( "GenerateLocalClickerKey: Can't acquire clicker with id:%d, this is probably internal error",
                clickerId);
        return;
    }
    DiffieHellmanKeysExchanger *keysExchanger = clicker->keysExchanger;
    clicker->localKey = (void*) dh_GenerateExchangeData(keysExchanger);
    clicker->localKeyLength = keysExchanger->pModuleLength;

    g_message("Generated local Key");
    PRINT_BYTES(clicker->localKey, clicker->localKeyLength);
    g_message("Sending local Key to clicker with id : %d", clickerId);

    NetworkDataPack* netData = con_BuildNetworkDataPack(clickerId, NetworkCommand_KEY, clicker->localKey,
            clicker->localKeyLength, true);
    event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);

    clicker_ReleaseOwnership(clicker);
}

static void ObtainedPSK(PreSharedKey* pskData) {

    Clicker *clicker = clicker_AcquireOwnership(pskData->clickerId);
    if (clicker == NULL) {
        g_critical("ObtainedPSK: Can't acquire clicker with id:%d, this is probably internal error",
                pskData->clickerId);
        return;
    }

    if (pskData->psk == NULL) {
        g_warning("Couldn't get PSK from Device Server");
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
    strlcpy((char*)clicker->identity, pskData->identity, pskData->identityLen + 1);

    clicker_ReleaseOwnership(clicker);
    event_PushEventWithInt(EventType_HISTORY_ADD, pskData->clickerId);
    event_PushEventWithInt(EventType_TRY_TO_SEND_PSK_TO_CLICKER, pskData->clickerId);
}

static void GenerateNameForClicker(int clickerId)
{
    char* ip = con_GetIPForClicker(clickerId);  //Note: we don't own this pointer, do not release!
    if (ip == NULL) {
        ip = "Unknown";
    }
    char hash[10];
    GenerateClickerTimeHash(hash);
    char ipFragment[5];
    memset(ipFragment, 0, 5);
    strlcpy(ipFragment, ip + strlen(ip) - 4, 4);
    Clicker* clicker = clicker_AcquireOwnership(clickerId);
    if (clicker != NULL) {
        GenerateClickerName(clicker->name, COMMAND_ENDPOINT_NAME_LENGTH, (char*)_PDConfig.endPointNamePattern, hash,
                ipFragment);
        g_message("New clicker connected, ip:[%s], id: %d, name:'%s'", ip, clicker->clickerID, clicker->name);
        clicker_ReleaseOwnership(clicker);
    }
}

static void HandleStartProvision(int clickerId) {
    Clicker* clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        g_critical("No clicker with id:%d, this is internal error.", clickerId);
        return;
    }
    clicker->provisioningInProgress = true;
    clicker_ReleaseOwnership(clicker);
    event_PushEventWithInt(EventType_HISTORY_REMOVE, clickerId);
    ubusagent_SendGeneratePskMessage(clickerId);
}

bool clicker_sm_ConsumeEvent(Event* event) {
    switch (event->type) {
        case EventType_CLICKER_DESTROY:
            return true;

        case EventType_CLICKER_CREATE:
            GenerateNameForClicker(event->intData);
            GenerateLocalClickerKey(event->intData);
            return true;

        case EventType_CONNECTION_RECEIVED_COMMAND:
            return NetworkCommandHandler((NetworkDataPack*) event->ptrData);

        case EventType_CLICKER_START_PROVISION:
            HandleStartProvision(event->intData);
            return true;

        case EventType_PSK_OBTAINED:
            ObtainedPSK((PreSharedKey*)event->ptrData);
            return true;

        case EventType_TRY_TO_SEND_PSK_TO_CLICKER:
            TryToSendPsk(event->intData);
            return true;

       default:
            break;
    }
    return false;
}
