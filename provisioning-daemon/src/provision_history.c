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

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "provision_history.h"
#include "clicker.h"
#include "utils.h"

//10 minutes
#define MAX_LIVE_TIME  (10 * 60 * 1000)

typedef struct {
    char name[MAX_HISTORY_NAME];
    unsigned long timestamp;
    int id;
    bool isErrored;
} HistoryEntry;

static GSList* _historyElements = NULL;
static GMutex mutex;

void history_init(void) {
    g_mutex_init(&mutex);
}

void history_destroy(void) {
    g_mutex_clear(&mutex);
}

void AddToHistory(int clickerId) {
    Clicker *clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        g_critical("AddToHistory: Can't acquire clicker with id:%d, this is probably internal error", clickerId);
        return;
    }

    HistoryEntry* entry = g_malloc(sizeof(HistoryEntry));
    entry->timestamp = GetCurrentTimeMillis();
    entry->id = clickerId;
    entry->isErrored = false;
    strlcpy(entry->name, clicker->name, MAX_HISTORY_NAME - 1);

    clicker_ReleaseOwnership(clicker);

    g_mutex_lock(&mutex);
    _historyElements = g_slist_prepend(_historyElements, entry);
    g_mutex_unlock(&mutex);

}

void PurgeOld(void) {
    //NOTE: called from inside mutex
    unsigned long current = GetCurrentTimeMillis();

    GSList* prev = NULL;
    for (GSList* iter = _historyElements; iter != NULL; iter = iter->next) {
        HistoryEntry* entry = (HistoryEntry*) iter->data;
        if ((current - entry->timestamp) > MAX_LIVE_TIME) {
            if (prev == NULL)
                _historyElements = iter->next;
            else
                prev->next = iter->next;

            g_free(entry);
            break;
        }
        prev = iter;
    }
}

GArray* history_GetProvisioned(void) {
    g_mutex_lock(&mutex);
    PurgeOld();

    GArray* result = g_array_new(FALSE, FALSE, sizeof(HistoryItem));
    for (GSList* iter = _historyElements; iter != NULL; iter = iter->next) {
        HistoryEntry* entry = (HistoryEntry*) iter->data;
        HistoryItem item;
        item.id = entry->id;
        memset(item.name, 0, MAX_HISTORY_NAME);
        strcpy(item.name, entry->name);
        item.isErrored = entry->isErrored;
        g_array_append_val(result, item);
    }
    g_mutex_unlock(&mutex);

    return result;
}

void history_RemoveProvisioned(int id) {
    g_mutex_lock(&mutex);
    GSList* prev = NULL;
    for (GSList* iter = _historyElements; iter != NULL; iter = iter->next) {
        HistoryEntry* entry = (HistoryEntry*) iter->data;
        if (entry->id == id) {
            if (prev == NULL)
                _historyElements = iter->next;
            else
                prev->next = iter->next;

            g_free(entry);
            break;
        }
        prev = iter;
    }
    g_mutex_unlock(&mutex);
}

bool history_ConsumeEvent(Event* event) {
    switch (event->type) {
        case EventType_HISTORY_REMOVE:
            history_RemoveProvisioned(event->intData);
            return true;

        case EventType_HISTORY_ADD:
            AddToHistory(event->intData);
            return true;

        default:
            break;
    }
    return false;
}
