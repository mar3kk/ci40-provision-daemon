/************************************************************************************************************************
 Copyright (c) 2016, Imagination Technologies Limited and/or its affiliated group companies.
 All rights reserved.
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 following conditions are met:
     1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
        following disclaimer.
     2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
        following disclaimer in the documentation and/or other materials provided with the distribution.
     3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
        products derived from this software without specific prior written permission.
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************************************************************/

#include "ubus_agent.h"

#include <stdint.h>
#include <stdbool.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <sys/prctl.h>

#include "log.h"
#include "clicker.h"
#include "provisioning_daemon.h"
#include "provision_history.h"
#include "utils.h"

//forward declarations
static int GetStateMethodHandler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
        const char *method, struct blob_attr *msg);

static int SelectMethodHandler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
        const char *method, struct blob_attr *msg);

static int StartProvisionMethodHandler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
        const char *method, struct blob_attr *msg);

//variables & structs
static struct ubus_context *_UbusCTX;
static pthread_t _UbusThread;
static char *_Path;
static struct blob_buf replyBloob;
static sem_t semaphore;
static bool uBusRunning;
static bool uBusInterruption;
static bool uBusInInterState;
struct uloop_timeout uBusHelperProcess;

static const struct blobmsg_policy _GetStatePolicy[] = {
};


enum {
    SELECT_CLICKER_ID,

    SELECT_LAST_ENUM
};

static const struct blobmsg_policy _SelectPolicy[] = {
    [SELECT_CLICKER_ID] = { .name = "clickerID", .type = BLOBMSG_TYPE_INT32 },
};

static const struct blobmsg_policy _StartProvisionPolicy[] = {
};

static const struct ubus_method _UBusAgentMethods[] = {
    UBUS_METHOD("getState", GetStateMethodHandler, _GetStatePolicy),
    UBUS_METHOD("select", SelectMethodHandler, _SelectPolicy),
    UBUS_METHOD("startProvision", StartProvisionMethodHandler, _StartProvisionPolicy)
};

static struct ubus_object_type _UBusAgentObjectType = UBUS_OBJECT_TYPE("provisioning-daemon", _UBusAgentMethods);

static struct ubus_object _UBusAgentObject = {
    .name = "provisioning-daemon",
    .type = &_UBusAgentObjectType,
    .methods = _UBusAgentMethods,
    .n_methods = ARRAY_SIZE(_UBusAgentMethods),
};

enum {
    GENERATE_PSK_RESPONSE_ID,
    GENERATE_PSK_RESPONSE_PSK_IDENTITY,
    GENERATE_PSK_RESPONSE_PSK_SECRET,
    GENERATE_PSK_RESPONSE_ERROR,
    GENERATE_PSK_RESPONSE_MAX
};

static const struct blobmsg_policy _GeneratePskResponsePolicy[GENERATE_PSK_RESPONSE_MAX] =
{
    [GENERATE_PSK_RESPONSE_ID] = {.name = "id", .type = BLOBMSG_TYPE_INT32},
    [GENERATE_PSK_RESPONSE_PSK_IDENTITY] = {.name = "pskIdentity", .type = BLOBMSG_TYPE_STRING},
    [GENERATE_PSK_RESPONSE_PSK_SECRET] = {.name = "pskSecret", .type = BLOBMSG_TYPE_STRING},
    [GENERATE_PSK_RESPONSE_ERROR] = {.name = "error", .type = BLOBMSG_TYPE_STRING},
};

static int SelectMethodHandler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
        const char *method, struct blob_attr *msg) {

    struct blob_attr* argBuffer[SELECT_LAST_ENUM];

    blobmsg_parse(_SelectPolicy, ARRAY_SIZE(_SelectPolicy), argBuffer, blob_data(msg), blob_len(msg));

    uint32_t clickerId = 0xffffffff;
    if (argBuffer[SELECT_CLICKER_ID]) {
        clickerId = blobmsg_get_u32(argBuffer[SELECT_CLICKER_ID]);
    }
    LOG(LOG_DBG, "uBusAgent: Select, move to clickerId:%d", clickerId);

    int returnedId = pd_SetSelectedClicker(clickerId);
    LOG(LOG_INFO, "uBusAgent: Tried to move to index %d, result is:%d", clickerId, returnedId);

    return returnedId == clickerId ? UBUS_STATUS_OK : UBUS_STATUS_INVALID_ARGUMENT;
}

static int StartProvisionMethodHandler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
        const char *method, struct blob_attr *msg) {

    LOG(LOG_DBG, "uBusAgent: Requested StartProvision");

    //handle startProvision
    int ret = pd_StartProvision();
    LOG(LOG_INFO, "uBusAgent: Start provision result: %d (0 = ok)", ret);

    return ret;
}

static int GetStateMethodHandler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
        const char *method, struct blob_attr *msg)
{
    LOG(LOG_DBG, "uBusAgent: Requested GetState");

    unsigned int clickersCount = clicker_GetClickersCount();
    int selClickerId = pd_GetSelectedClickerId();
    LOG(LOG_DBG, "uBusAgent: GetState clicker count:%d, selected id:%d", clickersCount, selClickerId);

    //add history data
    int historyCount = 0;
    HistoryItem* history = NULL;
    history_GetProvisioned(&history, &historyCount);

    blob_buf_init(&replyBloob, 0);
    void* cookie_array = blobmsg_open_array(&replyBloob, "clickers");
    for(int t = 0; t < historyCount; t++) {
        void* cookie_item = blobmsg_open_table(&replyBloob, "clicker");

        blobmsg_add_u32(&replyBloob, "id", history[t].id);
        blobmsg_add_string(&replyBloob, "name", history[t].name);
        blobmsg_add_u8(&replyBloob, "selected", false);
        blobmsg_add_u8(&replyBloob, "inProvisionState", false);
        blobmsg_add_u8(&replyBloob, "isProvisioned", true);
        blobmsg_add_u8(&replyBloob, "isError", false);
        blobmsg_close_table(&replyBloob, cookie_item);
    }

    bool alreadyProvisioned = false;

    for(int t = 0; t < clickersCount; t++) {
        Clicker* clk = clicker_AcquireOwnershipAtIndex(t);
        if (clk == NULL) {
            continue;
        }
        alreadyProvisioned = false;
        for(int j = 0; j < historyCount; j++) {
            if (history[j].id == clk->clickerID) {
                alreadyProvisioned = true;
                break;
            }
        }

        if (alreadyProvisioned) {
            clicker_ReleaseOwnership(clk);
            continue;
        }
        void* cookie_item = blobmsg_open_table(&replyBloob, "clicker");

        blobmsg_add_u32(&replyBloob, "id", clk->clickerID);
        blobmsg_add_string(&replyBloob, "name", clk->name);
        blobmsg_add_u8(&replyBloob, "selected", clk->clickerID == selClickerId);
        blobmsg_add_u8(&replyBloob, "inProvisionState", clk->provisioningInProgress);
        blobmsg_add_u8(&replyBloob, "isProvisioned", false);
        blobmsg_add_u8(&replyBloob, "isError", clk->error > 0 ? true : false);

        blobmsg_close_table(&replyBloob, cookie_item);

        clicker_ReleaseOwnership(clk);
    }
    FREE_AND_NULL(history);
    blobmsg_close_array(&replyBloob, cookie_array);

    ubus_send_reply(ctx, req, replyBloob.head);
    blob_buf_free(&replyBloob);
    return UBUS_STATUS_OK;
}

static void GeneratePskResponseHandler(struct ubus_request *req, int type, struct blob_attr *msg)
{
    struct blob_attr *args[GENERATE_PSK_RESPONSE_MAX];

    blobmsg_parse(_GeneratePskResponsePolicy, GENERATE_PSK_RESPONSE_MAX, args, blob_data(msg), blob_len(msg));
    pdubus_GeneratePskCallback callback = ((pdubus_GeneratePskRequest*)req->priv)->callback;

    char *error = blobmsg_get_string(args[GENERATE_PSK_RESPONSE_ERROR]);
    if (error)
    {
        LOG(LOG_ERR, "uBusAgent: Error while generating PSK : %s", error);
        callback(NULL, 0, ((pdubus_GeneratePskRequest*)req->priv));
        return;
    }

    char *psk = blobmsg_get_string(args[GENERATE_PSK_RESPONSE_PSK_SECRET]);

    if (!psk)
    {
        LOG(LOG_ERR, "uBusAgent: UNKNOWN PSK");
        callback(NULL, 0, ((pdubus_GeneratePskRequest*)req->priv));
        return;
    }
    LOG(LOG_INFO, "uBusAgent: Obtained PSK: %s", psk);
    callback(psk, strlen(psk), ((pdubus_GeneratePskRequest*)req->priv));

    return;
}

void HelperTimeoutHandler(struct uloop_timeout *t) {
    sem_wait(&semaphore);
    if (uBusInterruption) {
        uloop_cancelled = true;
    }
    sem_post(&semaphore);
    uloop_timeout_set(&uBusHelperProcess, 500);
}

static void* PDUbusLoop(void *arg)
{
    LOG(LOG_INFO, "uBusAgent: uBus thread started.\n");
    while(true) {
        sem_wait(&semaphore);
        if (uBusRunning == false) {
            sem_post(&semaphore);
            break;
        }

        while(uBusInterruption) {
            uBusInInterState = true;
            LOG(LOG_INFO, "Interrupt state");
            sem_post(&semaphore);
            usleep(1000 * 1000);
            sem_wait(&semaphore);
            uBusInInterState = false;
        }
        sem_post(&semaphore);
        uloop_run();
    }
    LOG(LOG_INFO, "uBusAgent: uBus thread finish.\n");
    return NULL;
}

static void SetUBusRunning(bool state) {
    sem_wait(&semaphore);
    uBusRunning = state;
    sem_post(&semaphore);
}

static void SetUBusLoopInterruption(bool state) {
    sem_wait(&semaphore);
    uBusInterruption = state;
    uloop_cancelled = state;
    sem_post(&semaphore);
}

static void WaitForInterruptState() {
    while(true) {
        sem_wait(&semaphore);
        bool ok = uBusInInterState;
        sem_post(&semaphore);
        if (ok) {
            break;
        }
        usleep(200 * 1000);
    }
}

bool ubusagent_Init()
{
    sem_init(&semaphore, 0, 1);
    uloop_init();
    _UbusCTX = ubus_connect(_Path);
    if (!_UbusCTX)
    {
        LOG(LOG_ERR, "uBusAgent: Failed to connect to ubus");
        return false;
    }
    ubus_add_uloop(_UbusCTX);

    memset(&uBusHelperProcess, 0, sizeof(uBusHelperProcess));
    uBusHelperProcess.cb = HelperTimeoutHandler;
    uloop_timeout_set(&uBusHelperProcess, 500);

    SetUBusRunning(true);
    SetUBusLoopInterruption(false);
    uBusInInterState = false;

    if (pthread_create(&_UbusThread, NULL, PDUbusLoop, NULL) < 0)
    {
        LOG(LOG_ERR, "uBusAgent: Error creating thread.");
        return false;
    }
    return true;
}

bool ubusagent_EnableRemoteControl() {
    SetUBusLoopInterruption(true);
	WaitForInterruptState();
	LOG(LOG_INFO, "uBusAgent: Enabling provision control through uBus");
    int ret = ubus_add_object(_UbusCTX, &_UBusAgentObject);
    if (ret) {
        LOG(LOG_ERR, "uBusAgent: Failed to add object: %s\n", ubus_strerror(ret));
    }
    SetUBusLoopInterruption(false);

    return ret == 0;
}

void ubusagent_Close(void)
{
    SetUBusLoopInterruption(false);
    SetUBusRunning(false);
    if (_UbusCTX) {
        ubus_free(_UbusCTX);
    }
    uloop_done();
    sem_destroy(&semaphore);
}

//Warn: this is blocking call
bool ubusagent_SendGeneratePskMessage(pdubus_GeneratePskRequest *request)
{
    SetUBusLoopInterruption(true);
    WaitForInterruptState();
    uint32_t id, ret;
    blob_buf_init(&replyBloob, 0);
    ret = ubus_lookup_id(_UbusCTX, "creator", &id);
    if (ret)
    {
        LOG(LOG_ERR, "uBusAgent: creator ubus service not available");
        return false;
    }

    ret = ubus_invoke(_UbusCTX, id, "generatePsk", replyBloob.head, GeneratePskResponseHandler, (void*)request, 10000);
    if (ret)
    {
        LOG(LOG_ERR, "uBusAgent: Filed to invoke generatePsk");
        return false;
    }
    blob_buf_free(&replyBloob);
    SetUBusLoopInterruption(false);

    return true;
}
