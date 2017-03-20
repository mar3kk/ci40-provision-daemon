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

static GQueue* eventsQueue = NULL;
static GMutex mutex;
static int _nextEventId = 0;

char* EventTypeToString(EventType type) {
    switch(type) {
        case EventType_CLICKER_CREATE:
            return "CLICKER_CREATE";

        case EventType_CLICKER_DESTROY:
            return "CLICKER_DESTROY";

        case EventType_CLICKER_SELECT:
            return "CLICKER_SELECT";

        case EventType_CLICKER_START_PROVISION:
            return "CLICKER_START_PROVISION";

        case EventType_CONNECTION_SEND_COMMAND:
            return "CONNECTION_SEND_COMMAND";

        case EventType_CONNECTION_RECEIVED_COMMAND:
            return "CONNECTION_RECEIVED_COMMAND";

        case EventType_PSK_OBTAINED:
            return "PSK_OBTAINED";

        case EventType_TRY_TO_SEND_PSK_TO_CLICKER:
            return "TRY_TO_SEND_PSK_TO_CLICKER";

        case EventType_HISTORY_REMOVE:
            return "HISTORY_REMOVE";

        case EventType_HISTORY_ADD:
            return "HISTORY_ADD";

        default:
            return "UNKNOWN";
    }
}

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
    event->id = ++_nextEventId;
    event->type = type;
    event->intData  =data;
    event->freeDataPtrOnRelease = false;
    g_queue_push_tail(eventsQueue, event);
    g_mutex_unlock(&mutex);
    g_message("[Event:%d] eventPtr:%p type:%s, int data:%d", event->id, event, EventTypeToString(type), data);
}

void event_PushEventWithPtr(EventType type, void* dataPtr, bool freeDataOnRelease) {
    g_mutex_lock(&mutex);
    Event* event = g_new(Event, 1);
    event->id = ++_nextEventId;
    event->type = type;
    event->ptrData = dataPtr;
    event->freeDataPtrOnRelease = freeDataOnRelease;

    g_queue_push_tail(eventsQueue, event);
    g_mutex_unlock(&mutex);

    g_message("[Event:%d] eventPtr:%p, type:%s, dataPtr:%p", event->id, event, EventTypeToString(type), dataPtr);
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
            g_debug("[event:%d] release event DATA ptr: %p", (*event)->id, (*event)->ptrData);
            g_free((*event)->ptrData);
        }
        g_debug("[event:%d] release event %s ptr: %p", (*event)->id, EventTypeToString((*event)->type), *event);
        g_free(*event);
        *event = NULL;
    }
}
