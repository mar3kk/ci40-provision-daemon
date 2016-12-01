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
#include <semaphore.h>
#include "provision_history.h"
#include "log.h"

//10 minutes
#define MAX_LIVE_TIME  (10 * 60 * 1000)

typedef struct HistoryEntry_ {
    struct HistoryEntry_* next;
    char name[MAX_HISTORY_NAME];
    long timestamp;
    uint8_t remoteKey[P_MODULE_LENGTH];
    int id;
    bool isErrored;
} HistoryEntry;

static HistoryEntry* historyHead = NULL;
static sem_t semaphore;

void history_init() {
    sem_init(&semaphore, 0, 1);
}

void history_destroy() {
    sem_destroy(&semaphore);
}

int getCurrentMilis () {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    long ms = round(spec.tv_nsec / 1.0e6);
    return ms;
}

void history_AddAsProvisioned(int id, char* name, uint8_t* remoteKey, int keyLength) {
    sem_wait(&semaphore);
    HistoryEntry* entry = malloc(sizeof(HistoryEntry));
    entry->timestamp = getCurrentMilis();
    entry->id = id;
    memcpy(entry->remoteKey, remoteKey, keyLength);
    entry->isErrored = false;
    strncpy(entry->name, name, MAX_HISTORY_NAME - 1);
    entry->next = historyHead;
    historyHead = entry;
    sem_post(&semaphore);
}

void PurgeOld() {
    long current = getCurrentMilis();
    HistoryEntry* prev = NULL;
    for (HistoryEntry* tmp = historyHead; tmp != NULL; tmp = tmp->next) {
        if ( (current - tmp->timestamp) > MAX_LIVE_TIME) {

            if (prev == NULL) {
                historyHead = tmp->next;
            } else {
                prev->next = tmp->next;
            }
            free(tmp);
            break;
        }
        prev = tmp;
    }
}

void history_GetProvisioned(HistoryItem** listOfId, int* sizeOfList) {
    sem_wait(&semaphore);
    PurgeOld();
    int count = 0;
    for(HistoryEntry* tmp = historyHead; tmp != NULL; tmp = tmp->next) {
        count++;
    }
    *sizeOfList = count;
    if (count > 0) {
        HistoryItem* result = malloc(sizeof(HistoryItem) * count);
        *listOfId = result;
        for(HistoryEntry* tmp = historyHead; tmp != NULL; tmp = tmp->next) {
            result->id = tmp->id;
            strcpy( result->name, tmp->name );
            result++;
        }
    }
    sem_post(&semaphore);
}

void history_RemoveProvisioned(int id) {
    sem_wait(&semaphore);
    HistoryEntry* prev = NULL;
    for(HistoryEntry* tmp = historyHead; tmp != NULL; tmp = tmp->next) {
        if (tmp->id == id) {
            if (prev == NULL) {
                historyHead = tmp->next;
            } else {
                prev->next = tmp->next;
            }
            free(tmp);
            break;
        }
        prev = tmp;
    }
    sem_post(&semaphore);
}
