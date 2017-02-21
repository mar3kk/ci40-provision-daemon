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
#include <pthread.h>

/**
 * Keeps a HEAD for the list of connected clickers
 */
static Clicker *clickers = NULL;

/**
 * Keeps a HEAD to list of clickers that are ready to be purged
 */
static Clicker *clickersToRelease = NULL;

static sem_t semaphore;
static int _IDCounter = 0;

static pthread_mutex_t mutex;
static pthread_mutexattr_t mutex_attr;




static void Destroy(Clicker *clicker)
{
    dh_release(&clicker->keysExchanger);
    FREE_AND_NULL(clicker->localKey);
    FREE_AND_NULL(clicker->remoteKey);
    FREE_AND_NULL(clicker->sharedKey);
    FREE_AND_NULL(clicker->psk);
    FREE_AND_NULL(clicker->identity);
    FREE_AND_NULL(clicker->name);
    FREE_AND_NULL(clicker);
}

static void InnerAdd(Clicker **head, Clicker *clicker)
{

    if (*head == NULL)
    {
        *head = clicker;
        (*head)->next = NULL;
        return;
    }
    Clicker * current = *head;
    while (current->next != NULL)
    {
        current = current->next;
    }
    current->next = clicker;
    clicker->next = NULL;
}

static void InnerRemove(Clicker **head, Clicker *clicker, bool doLock)
{

    Clicker* current = *head;
    Clicker* previous = NULL;

    if (*head == NULL)
    {
        return;
    }

    while (current != clicker)
    {
        if (current->next == NULL){
            return;
        }
        else {
            previous = current;
            current = current->next;
        }
    }

    if (current == *head)
        *head = current->next;
    else
        previous->next = current->next;

}

static Clicker *InnerGetClickerByID(int id, bool doLock)
{

    Clicker *ptr = clickers;
    while (ptr != NULL)
    {
        if (ptr->clickerID == id)
        {
            if (doLock)
                pthread_mutex_unlock(&mutex);
            return ptr;
        }
        ptr = ptr->next;
    }

    return NULL;
}


Clicker *clicker_New(int socket)
{
    Clicker *newClicker = malloc(sizeof(Clicker));

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
    newClicker->name = malloc(COMMAND_ENDPOINT_NAME_LENGTH);

    InnerAdd(&clickers, newClicker);
    return newClicker;
}

void clicker_InitSemaphore(void)
{
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &mutex_attr);
    //sem_init(&semaphore, 0, 1);
}

void clicker_Release(Clicker *clicker)
{
    LOG(LOG_DBG, "clicker_Release start");
    InnerRemove(&clickers, clicker, true);

    InnerAdd(&clickersToRelease, clicker);
}

//should be called in critical section
Clicker *clicker_InnerGetClickerAtIndex(int index)
{
    int i = -1;
    Clicker *ptr = clickers;
    while (ptr != NULL)
    {
        i++;
        if (i == index)
            return ptr;

        ptr = ptr->next;
    }
    return NULL;
}

Clicker *clicker_GetClickerAtIndex(int index)
{
    Clicker* result = clicker_InnerGetClickerAtIndex(index);
    return result;
}

unsigned int clicker_GetClickersCount(void)
{

    int i = 0;
    Clicker *ptr = clickers;
    while (ptr != NULL)
    {
        i++;
        ptr = ptr->next;
    }

    return i;
}

Clicker *clicker_GetClickerByID(int id)
{
    return InnerGetClickerByID(id, true);
}

int clicker_GetIndexOfClicker(Clicker* clicker)
{
    int i = -1;
    Clicker *head = clickers;
    while (head != NULL)
    {
        i++;
        if (clicker == head)
        {
            return i;
        }
        head = head->next;
    }
    return -1;
}

Clicker *clicker_GetClickers(void)
{
    return clickers;
}

Clicker *clicker_AcquireOwnership(int clickerID)
{
    Clicker *clicker = InnerGetClickerByID(clickerID, false);
    if (clicker != NULL)
        clicker->ownershipsCount++;

    return clicker;
}

Clicker* clicker_AcquireOwnershipAtIndex(int index)
{

    Clicker* clicker = clicker_InnerGetClickerAtIndex(index);
    if (clicker != NULL)
        clicker->ownershipsCount++;

    return clicker;
}

void clicker_ReleaseOwnership(Clicker *clicker)
{
    clicker->ownershipsCount--;
}

void clicker_Purge(void)
{
    Clicker *current = clickersToRelease;
    while (current != NULL)
    {
        if (current->ownershipsCount <= 0)
        {
            Clicker *tmp = current->next;
            InnerRemove(&clickersToRelease, current, false);
            Destroy(current);
            current = tmp;
        }
        else
        {
            current = current->next;
        }
    }
}

void clicker_wait(void)
{
    pthread_mutex_lock(&mutex);
}

void clicker_post(void)
{
    pthread_mutex_unlock(&mutex);
}
