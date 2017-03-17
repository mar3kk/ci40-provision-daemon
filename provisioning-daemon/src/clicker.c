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

#include "clicker.h"
#include "utils.h"
#include "log.h"
#include "commands.h"
#include "crypto/crypto_config.h"
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <glib.h>

/**
 * Keeps a HEAD for the list of connected clickers
 */
//TODO: I'm certain that this should be hashmap
static GQueue* clickersQueue = NULL;

/** All operations are sync on this mutex */
static GMutex mutex;

static void Destroy(Clicker *clicker) {
    dh_release(&clicker->keysExchanger);
    G_FREE_AND_NULL(clicker->localKey);
    G_FREE_AND_NULL(clicker->remoteKey);
    G_FREE_AND_NULL(clicker->sharedKey);
    G_FREE_AND_NULL(clicker->psk);
    G_FREE_AND_NULL(clicker->identity);
    G_FREE_AND_NULL(clicker->name);
    g_mutex_clear(&clicker->ownershipLock);
    G_FREE_AND_NULL(clicker);
}

static void ReleaseClickerIfNotOwned(Clicker* clicker) {
    //NOTE: Should be called in critical section only!
    if (clicker->ownershipsCount > 0) {
        return;
    }
    //check for logic error, if ownershipCount == 0, this clicker can't be in clickersQueue
    if (g_queue_remove(clickersQueue, clicker) == TRUE) {
        LOG(LOG_ERR, "Internal error: Clicker with id:%d has ownershipCount = 0, but it's still in queue! Forced remove",
                clicker->clickerID);
    }
    Destroy(clicker);
}

gint compareClickerById(gpointer a, gpointer b) {
    Clicker* clicker1 = (Clicker*)a;
    Clicker* clicker2 = (Clicker*)b;
    return clicker1->clickerID - clicker2->clickerID;
}

static Clicker *InnerGetClickerByID(int id, bool doLock)
{
    if (doLock)
        g_mutex_lock(&mutex);

    Clicker tmp;
    tmp.clickerID = id;
    GList* found = g_queue_find_custom(clickersQueue, &tmp, (GCompareFunc)compareClickerById);
    Clicker* result = found != NULL ? found->data : NULL;

    if (doLock)
        g_mutex_unlock(&mutex);
    return result;
}


void CreateNewClicker(int id)
{
    Clicker *newClicker = g_new0(Clicker, 1);

    newClicker->clickerID = id;
    newClicker->taskInProgress = false;
    newClicker->keysExchanger = dh_newKeyExchanger((char*)g_KeyBuffer, P_MODULE_LENGTH, CRYPTO_G_MODULE, GenerateRandomX);
    newClicker->localKey = NULL;
    newClicker->remoteKey = NULL;
    newClicker->sharedKey = NULL;
    newClicker->psk = NULL;
    newClicker->pskLen = 0;
    newClicker->identity = NULL;
    newClicker->ownershipsCount = 0;
    newClicker->provisionTime = 0;
    newClicker->error = 0;
    newClicker->provisioningInProgress = false;
    newClicker->name = g_malloc0(COMMAND_ENDPOINT_NAME_LENGTH);
    g_mutex_init(&newClicker->ownershipLock);

    g_mutex_lock(&mutex);
    g_queue_push_tail(clickersQueue, newClicker);
    newClicker->ownershipsCount ++;
    g_mutex_unlock(&mutex);
}

void RemoveFromCollection(int clickerID)
{
    Clicker* clicker = InnerGetClickerByID(clickerID, true);
    if (clicker == NULL) {
        return;
    }
    LOG(LOG_DBG, "clicker_Release start");
    g_mutex_lock(&mutex);

    if (g_queue_remove(clickersQueue, clicker) == TRUE) {
        clicker->ownershipsCount --;
    } else {
        LOG(LOG_ERR, "Internal error: Tried to remove clicker which is not a part of collection!");
    }

    ReleaseClickerIfNotOwned(clicker);
    g_mutex_unlock(&mutex);
}

void clicker_Init(void)
{
    clickersQueue = g_queue_new();
    g_mutex_init(&mutex);
}

void clicker_Shutdown(void) {
    g_queue_free(clickersQueue);
    g_mutex_clear(&mutex);
    clickersQueue = NULL;
}

unsigned int clicker_GetClickersCount(void)
{
    g_mutex_lock(&mutex);
    int result = g_queue_get_length(clickersQueue);
    g_mutex_unlock(&mutex);
    return result;
}

Clicker *clicker_AcquireOwnership(int clickerID)
{
    g_mutex_lock(&mutex);
    Clicker *clicker = InnerGetClickerByID(clickerID, false);
    if (clicker != NULL)
        clicker->ownershipsCount++;
    g_mutex_unlock(&mutex);

    if (clicker != NULL)
        g_mutex_lock(&clicker->ownershipLock);

    return clicker;
}

void clicker_ReleaseOwnership(Clicker *clicker)
{
    g_mutex_unlock(&clicker->ownershipLock);
    g_mutex_lock(&mutex);
    clicker->ownershipsCount--;
    ReleaseClickerIfNotOwned(clicker);
    g_mutex_unlock(&mutex);
}

bool clicker_ConsumeEvent(Event* event) {
    switch(event->type) {
        case EventType_CLICKER_CREATE:
            CreateNewClicker(event->intData);
            return true;

        case EventType_CLICKER_DESTROY:
            RemoveFromCollection(event->intData);
            return true;

        default:
            break;
    }
    return false;
}
