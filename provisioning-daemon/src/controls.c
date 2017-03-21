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

#include "controls.h"
#include "utils.h"
#include "connection_manager.h"
#include <letmecreate/letmecreate.h>
#include <glib.h>

#define LED_SLOW_BLINK_INTERVAL_MS              (500)
#define LED_FAST_BLINK_INTERVAL_MS              (100)
#define TIME_TO_DISCONNECT_AFTER_PROVISION      3000
/**
 * Time in millis of last led state has been changed.
 */
static unsigned long _LastBlinkTime = 0;

static bool g_activeLedOn = true;

static GArray* _connectedClickersId;
static int selectedClickerIndex = -1;
static GMutex mutex;

// Send ENABLE_HIGHLIGHT command to active clicker and DISABLE_HIGHLIGHT to inactive clickers
static void UpdateHighlights(void) {
    g_mutex_lock(&mutex);
    for (guint t = 0; t < _connectedClickersId->len; t++) {
        NetworkCommand cmdToSend = (t == selectedClickerIndex) ?
                NetworkCommand_ENABLE_HIGHLIGHT : NetworkCommand_DISABLE_HIGHLIGHT;
        int clickerId = g_array_index(_connectedClickersId, int, t);
        NetworkDataPack* netData = con_BuildNetworkDataPack(clickerId, cmdToSend, NULL, 0, false);
        event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);
    }
    g_mutex_unlock(&mutex);
}

static void SelectNextClickerCallback(void)
{
    g_mutex_lock(&mutex);
    selectedClickerIndex ++;
    if (selectedClickerIndex >= _connectedClickersId->len) {
        selectedClickerIndex = (_connectedClickersId->len > 0) ? 0 : -1;
    }
    if (selectedClickerIndex == -1) {
        g_message("No clicker is selected now.");
    } else {
        g_message("Selected Clicker ID : %d", g_array_index(_connectedClickersId, int, selectedClickerIndex));
    }
    g_mutex_unlock(&mutex);
    UpdateHighlights();
}

static void StartProvisionCallback(void)
{
    if (selectedClickerIndex < 0 || _connectedClickersId->len == 0) {
        g_critical( "Can't start provision, no clicker is selected!");
        return;
    }
    g_mutex_lock(&mutex);
    int clickerId = g_array_index(_connectedClickersId, int, selectedClickerIndex);
    g_mutex_unlock(&mutex);

    Clicker* clicker = clicker_AcquireOwnership(clickerId);
    if (clicker == NULL) {
        g_critical( "No clicker with id:%d, this is internal error.", clickerId);
        return;
    }
    clicker->provisioningInProgress = true;
    clicker_ReleaseOwnership(clicker);

    event_PushEventWithInt(EventType_CLICKER_START_PROVISION, clickerId);
    event_PushEventWithInt(EventType_HISTORY_REMOVE, clickerId);
}

void controls_init(bool enableButtons) {
    g_mutex_init(&mutex);
    _connectedClickersId = g_array_new(FALSE, FALSE, sizeof(int));

    if (enableButtons) {
        g_message( "[Setup] Enabling button controls.");
        bool result = switch_init() == 0;
        result &= switch_add_callback(SWITCH_1_PRESSED, SelectNextClickerCallback) == 0;
        result &= switch_add_callback(SWITCH_2_PRESSED, StartProvisionCallback) == 0;
        if (result == false) {
            g_critical( "[Setup] Problems while acquiring buttons, local provision control might not work.");
        }
    }
}

void controls_shutdown() {
    g_array_free(_connectedClickersId, TRUE);
    switch_release();
    g_mutex_clear(&mutex);
}

static void SetLeds(void)
{
    uint8_t mask = 0;
    int i = 0;

    g_mutex_lock(&mutex);
    int clickersCount = _connectedClickersId->len;
    int selectedIndex = selectedClickerIndex;
    g_mutex_unlock(&mutex);

    if (clickersCount == 0)
    {
        led_release();
        return;
    }

    led_init();

    for (i = 0; i < clickersCount; i++)
        mask |= 1 << i;

    if ((selectedIndex >= 0) && (selectedIndex < 8) && g_activeLedOn) {
        mask ^= 1 << selectedIndex;
    }

    led_set(ALL_LEDS, mask);
}

void CheckForFinishedProvisionings(void) {
    g_mutex_lock(&mutex);
    int count = _connectedClickersId->len;
    g_mutex_unlock(&mutex);

    for (guint t = 0; t < count; t++) {
        g_mutex_lock(&mutex);
        int clickerId = g_array_index(_connectedClickersId, int, t);
        g_mutex_unlock(&mutex);
        Clicker* clicker = clicker_AcquireOwnership(clickerId);
        if (clicker == NULL) {
            g_critical( "No clicker with id:%d, this is internal error.", clickerId);
            continue;
        }
        if (clicker->provisionTime > 0) {
            if (g_get_monotonic_time() - clicker->provisionTime > TIME_TO_DISCONNECT_AFTER_PROVISION) {
                con_Disconnect(clicker->clickerID);
            }
        }
        clicker_ReleaseOwnership(clicker);
    }
}

/**
 * @bried Set the leds according to current app state.
 */
void controls_Update(void) {
    int clickerId = controls_GetSelectedClickerId();
    int interval = 0;
    if (clickerId >= 0) {
        Clicker* clicker = clicker_AcquireOwnership(clickerId);
        if (clicker == NULL) {
            g_critical( "No clicker with id:%d, this is internal error.", clickerId);
            return;
        }

        interval = clicker->provisioningInProgress ? LED_FAST_BLINK_INTERVAL_MS : LED_SLOW_BLINK_INTERVAL_MS;

        clicker_ReleaseOwnership(clicker);
    }

    unsigned long currentTime = g_get_monotonic_time();
    if (currentTime - _LastBlinkTime > interval) {
        _LastBlinkTime = currentTime;
        g_activeLedOn = !g_activeLedOn;
    }

    SetLeds();
    CheckForFinishedProvisionings();
}

static void RemoveClickerWithID(int clickerID) {
    g_mutex_lock(&mutex);
    guint foundIndex = -1;
    for(guint t = 0; t < _connectedClickersId->len; t++) {
        if (g_array_index(_connectedClickersId, int, t) == clickerID) {
            foundIndex = t;
            break;
        }
    }
    if (foundIndex >= 0) {
        g_array_remove_index(_connectedClickersId, foundIndex);

        if (selectedClickerIndex >= _connectedClickersId->len) {
            selectedClickerIndex = _connectedClickersId->len - 1;
            if (selectedClickerIndex == -1) {
                g_message("No clicker is selected now.");
            } else {
                g_message("Selected Clicker ID : %d", g_array_index(_connectedClickersId, int, selectedClickerIndex));
            }
        }
    }
    g_mutex_unlock(&mutex);
}

static void SelectClickerWithId(int clickerId) {
    g_mutex_lock(&mutex);
    for (guint t = 0; t < _connectedClickersId->len; t++) {
        if (g_array_index(_connectedClickersId, int, t) == clickerId) {
            selectedClickerIndex = t;
            g_message( "Selected Clicker ID : %d", clickerId);
            break;
        }
    }
    g_mutex_unlock(&mutex);
}

int controls_GetSelectedClickerId() {
    g_mutex_lock(&mutex);
    int result = (selectedClickerIndex < 0) ? -1 : g_array_index(_connectedClickersId, int, selectedClickerIndex);
    g_mutex_unlock(&mutex);
    return result;
}

GArray* controls_GetAllClickersIds() {
    g_mutex_lock(&mutex);
    GArray* result = g_array_new(FALSE, FALSE, sizeof(int));
    int* pos = &g_array_index(_connectedClickersId, int, 0);
    g_array_append_vals(result, pos, _connectedClickersId->len);
    g_mutex_unlock(&mutex);
    return result;
}

bool controls_ConsumeEvent(Event* event) {
    switch(event->type) {
        case EventType_CLICKER_CREATE:
            g_mutex_lock(&mutex);
            g_array_append_val(_connectedClickersId, event->intData);
            if (selectedClickerIndex == -1) {
                selectedClickerIndex = 0;
                g_message( "Selected Clicker ID : %d", g_array_index(_connectedClickersId, int, selectedClickerIndex));
            }
            g_mutex_unlock(&mutex);
            UpdateHighlights();
            return true;

        case EventType_CLICKER_DESTROY:
            RemoveClickerWithID(event->intData);
            UpdateHighlights();
            return true;

        case EventType_CLICKER_SELECT:
            SelectClickerWithId(event->intData);
            UpdateHighlights();
            return true;

        default:
            break;
    }

    return false;
}
