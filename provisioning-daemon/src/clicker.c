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

#include <stdlib.h>
#include <semaphore.h>
#include <time.h>
#include <stdbool.h>
#include <glib.h>

/**
 * Keeps a HEAD for the list of connected clickers
 */
//static Clicker *clickers = NULL;
static GQueue* clickersQueue = NULL;

static GMutex mutex;
static int _IDCounter = 0;

static void Destroy(Clicker *clicker)
{
    dh_release(&clicker->keysExchanger);
    G_FREE_AND_NULL(clicker->localKey);
    G_FREE_AND_NULL(clicker->remoteKey);
    G_FREE_AND_NULL(clicker->sharedKey);
    G_FREE_AND_NULL(clicker->psk);
    G_FREE_AND_NULL(clicker->identity);
    G_FREE_AND_NULL(clicker->name);
    G_FREE_AND_NULL(clicker);
}

static void ReleaseClickerIfNotOwned(Clicker* clicker) {
    //NOTE: Should be called in critical section only!
    if (clicker->ownershipsCount > 0) {
        g_mutex_unlock(&mutex);
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


Clicker *clicker_New(int socket)
{
    Clicker *newClicker = g_new0(Clicker, 1);

    newClicker->socket = socket;
    newClicker->clickerID = ++_IDCounter;
    newClicker->lastKeepAliveTime = GetCurrentTimeMillis();
    newClicker->taskInProgress = false;
    newClicker->next = NULL;
    newClicker->keysExchanger = NULL;
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

    g_mutex_lock(&mutex);
    g_queue_push_tail(clickersQueue, newClicker);
    newClicker->ownershipsCount ++;
    g_mutex_unlock(&mutex);

    return newClicker;
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

void clicker_Release(Clicker *clicker)
{
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

Clicker *clicker_GetClickerAtIndex(int index)
{
    g_mutex_lock(&mutex);
    Clicker* result = g_queue_peek_nth(clickersQueue, index);
    g_mutex_unlock(&mutex);
    return result;
}

unsigned int clicker_GetClickersCount(void)
{
    g_mutex_lock(&mutex);
    int result = g_queue_get_length(clickersQueue);
    g_mutex_unlock(&mutex);
    return result;
}

Clicker *clicker_GetClickerByID(int id)
{
    return InnerGetClickerByID(id, true);
}

int clicker_GetIndexOfClicker(Clicker* clicker)
{
    g_mutex_lock(&mutex);
    GList* found = g_queue_find(clickersQueue, clicker);
    int result = found != NULL ? g_queue_link_index(clickersQueue, found) : -1;
    g_mutex_unlock(&mutex);
    return result;
}

Clicker *clicker_GetClickers(void)
{
    return NULL;//clickers;
}

Clicker *clicker_AcquireOwnership(int clickerID)
{
    g_mutex_lock(&mutex);
    Clicker *clicker = InnerGetClickerByID(clickerID, false);
    if (clicker != NULL)
        clicker->ownershipsCount++;
    g_mutex_unlock(&mutex);

    return clicker;
}

Clicker* clicker_AcquireOwnershipAtIndex(int index)
{
    g_mutex_lock(&mutex);

    Clicker* clicker = g_queue_peek_nth(clickersQueue, index);
    if (clicker != NULL)
        clicker->ownershipsCount++;

    g_mutex_unlock(&mutex);

    return clicker;
}

void clicker_ReleaseOwnership(Clicker *clicker)
{
    g_mutex_lock(&mutex);
    clicker->ownershipsCount--;
    ReleaseClickerIfNotOwned(clicker);
    g_mutex_unlock(&mutex);
}
