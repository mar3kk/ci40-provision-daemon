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
#include "event.h"

//https://www.ibm.com/developerworks/linux/tutorials/l-glib/

static GQueue* eventsQueue = NULL;
static GMutex mutex;

void event_Init(void) {
    eventsQueue = g_queue_new();
    g_mutex_init(&mutex);
}

void event_Shutdown(void) {
    if (eventsQueue != NULL) {
        g_queue_free(eventsQueue);
        g_mutex_clear(&mutex);
    }
    eventsQueue = NULL;
}

void event_PushEventWithInt(EventType type, int data) {
    g_mutex_lock(&mutex);
    Event* event = g_new(Event, 1);
    event->type = type;
    event->intData  =data;
    event->freeDataPtrOnRelease = false;
    g_queue_push_tail(eventsQueue, event);
    g_mutex_unlock(&mutex);
}

void event_PushEventWithPtr(EventType type, void* dataPtr, bool freeDataOnRelease) {
    g_mutex_lock(&mutex);
    Event* event = g_new(Event, 1);
    event->type = type;
    event->ptrData = dataPtr;
    event->freeDataPtrOnRelease = freeDataOnRelease;
    g_queue_push_tail(eventsQueue, event);
    g_mutex_unlock(&mutex);
}

Event* event_PopEvent(void) {
    g_mutex_lock(&mutex);
    Event* result = g_queue_pop_head(eventsQueue);
    g_mutex_unlock(&mutex);
    return result;
}

void event_releaseEvent(Event** event) {
    if (*event != NULL) {
        if ((*event)->freeDataPtrOnRelease) {
            g_free((*event)->ptrData);
        }
        g_free(*event);
        *event = NULL;
    }
}
