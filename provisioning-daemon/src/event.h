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

#ifndef _EVENT_H_
#define _EVENT_H_

#include <glib.h>
#include <stdbool.h>

typedef enum {
    EventType_CLICKER_CREATE,   //int - id of clicker
    EventType_CLICKER_DESTROY,  //int - id of clicker
    EventType_CONNECTION_SEND_COMMAND, //ptr - points to NetworkDataPack, ownership is passed to receiver
    EventType_CONNECTION_RECEIVED_COMMAND, //ptr - points to NetworkDataPack (will be released on event destruction)
    EventType_CLICKER_START_PROVISION, //int - id of clicker to do provision
    EventType_REMOVE_FROM_HISTORY, //int - id of clicker to remove
} EventType;

typedef struct {
    EventType type;
    union {
        int     intData;
        void*   ptrData;
    };
    bool freeDataPtrOnRelease;
} Event;

void event_Init(void);
void event_Shutdown(void);

/** ----- All methods below are thread safe ---- **/
/**
 * Adds new event to queue.
 */
void event_PushEventWithInt(EventType type, int data);
void event_PushEventWithPtr(EventType type, void* dataPtr, bool freeDataOnRelease);

/**
 * Pops event from queue, if no events avail then NULL is returned. After handling returned event you should call
 * event_releaseEvent(&event).
 */
Event* event_PopEvent(void);

/**
 * Releases event struct from memory, if data associated with this event is an pointer, and event has flag
 * freeDataPtrOnRelease set to true, then this pointer is also released by call to g_free().
 */
void event_releaseEvent(Event** event);

#endif /* _EVENT_H_ */
