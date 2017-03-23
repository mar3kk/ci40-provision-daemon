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
static bool _ActiveLedOn = true;
static GArray* _ConnectedClickersId;
static int _SelectedClickerIndex = -1;
static GMutex _Mutex;

// Send ENABLE_HIGHLIGHT command to active clicker and DISABLE_HIGHLIGHT to inactive clickers
static void UpdateHighlights(void) {
    g_mutex_lock(&_Mutex);
    for (guint t = 0; t < _ConnectedClickersId->len; t++) {
        NetworkCommand cmdToSend = (t == _SelectedClickerIndex) ?
                NetworkCommand_ENABLE_HIGHLIGHT : NetworkCommand_DISABLE_HIGHLIGHT;
        int clickerId = g_array_index(_ConnectedClickersId, int, t);
        NetworkDataPack* netData = con_BuildNetworkDataPack(clickerId, cmdToSend, NULL, 0, false);
        event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);
    }
    g_mutex_unlock(&_Mutex);
}

static void SelectNextClickerCallback(void)
{
    g_mutex_lock(&_Mutex);
    _SelectedClickerIndex ++;
    if (_SelectedClickerIndex >= _ConnectedClickersId->len) {
        _SelectedClickerIndex = (_ConnectedClickersId->len > 0) ? 0 : -1;
    }
    if (_SelectedClickerIndex == -1) {
        g_message("No clicker is selected now.");
    } else {
        g_message("Selected Clicker ID : %d", g_array_index(_ConnectedClickersId, int, _SelectedClickerIndex));
    }
    g_mutex_unlock(&_Mutex);
    UpdateHighlights();
}

static void StartProvisionCallback(void)
{
    if (_SelectedClickerIndex < 0 || _ConnectedClickersId->len == 0) {
        g_critical( "Can't start provision, no clicker is selected!");
        return;
    }
    g_mutex_lock(&_Mutex);
    int clickerId = g_array_index(_ConnectedClickersId, int, _SelectedClickerIndex);
    g_mutex_unlock(&_Mutex);

    event_PushEventWithInt(EventType_CLICKER_START_PROVISION, clickerId);
}

void controls_Init(bool enableButtons) {
    g_mutex_init(&_Mutex);
    _ConnectedClickersId = g_array_new(FALSE, FALSE, sizeof(int));

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

void controls_Shutdown() {
    g_array_free(_ConnectedClickersId, TRUE);
    switch_release();
    g_mutex_clear(&_Mutex);
}

static void SetLeds(void)
{
    uint8_t mask = 0;
    int i = 0;

    g_mutex_lock(&_Mutex);
    int clickersCount = _ConnectedClickersId->len;
    int selectedIndex = _SelectedClickerIndex;
    g_mutex_unlock(&_Mutex);

    if (clickersCount == 0)
    {
        led_release();
        return;
    }

    led_init();

    for (i = 0; i < clickersCount; i++)
        mask |= 1 << i;

    if ((selectedIndex >= 0) && (selectedIndex < 8) && _ActiveLedOn) {
        mask ^= 1 << selectedIndex;
    }

    led_set(ALL_LEDS, mask);
}

void CheckForFinishedProvisionings(void) {
    g_mutex_lock(&_Mutex);
    int count = _ConnectedClickersId->len;
    g_mutex_unlock(&_Mutex);

    for (guint t = 0; t < count; t++) {
        g_mutex_lock(&_Mutex);
        int clickerId = g_array_index(_ConnectedClickersId, int, t);
        g_mutex_unlock(&_Mutex);
        Clicker* clicker = clicker_AcquireOwnership(clickerId);
        if (clicker == NULL) {
            g_critical( "No clicker with id:%d, this is internal error.", clickerId);
            continue;
        }
        if (clicker->provisionTime > 0) {
            if (g_get_monotonic_time() / 1000 - clicker->provisionTime > TIME_TO_DISCONNECT_AFTER_PROVISION) {
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

    unsigned long currentTime = g_get_monotonic_time() / 1000;
    if (currentTime - _LastBlinkTime > interval) {
        _LastBlinkTime = currentTime;
        _ActiveLedOn = !_ActiveLedOn;
    }

    SetLeds();
    CheckForFinishedProvisionings();
}

static void RemoveClickerWithID(int clickerID) {
    g_mutex_lock(&_Mutex);
    guint foundIndex = -1;
    for(guint t = 0; t < _ConnectedClickersId->len; t++) {
        if (g_array_index(_ConnectedClickersId, int, t) == clickerID) {
            foundIndex = t;
            break;
        }
    }
    if (foundIndex >= 0) {
        g_array_remove_index(_ConnectedClickersId, foundIndex);

        if (_SelectedClickerIndex >= _ConnectedClickersId->len) {
            _SelectedClickerIndex = _ConnectedClickersId->len - 1;
            if (_SelectedClickerIndex == -1) {
                g_message("No clicker is selected now.");
            } else {
                g_message("Selected Clicker ID : %d", g_array_index(_ConnectedClickersId, int, _SelectedClickerIndex));
            }
        }
    }
    g_mutex_unlock(&_Mutex);
}

static void SelectClickerWithId(int clickerId) {
    g_mutex_lock(&_Mutex);
    for (guint t = 0; t < _ConnectedClickersId->len; t++) {
        if (g_array_index(_ConnectedClickersId, int, t) == clickerId) {
            _SelectedClickerIndex = t;
            g_message( "Selected Clicker ID : %d", clickerId);
            break;
        }
    }
    g_mutex_unlock(&_Mutex);
}

int controls_GetSelectedClickerId() {
    g_mutex_lock(&_Mutex);
    int result = (_SelectedClickerIndex < 0) ? -1 : g_array_index(_ConnectedClickersId, int, _SelectedClickerIndex);
    g_mutex_unlock(&_Mutex);
    return result;
}

GArray* controls_GetAllClickersIds() {
    g_mutex_lock(&_Mutex);
    GArray* result = g_array_new(FALSE, FALSE, sizeof(int));
    int* pos = &g_array_index(_ConnectedClickersId, int, 0);
    g_array_append_vals(result, pos, _ConnectedClickersId->len);
    g_mutex_unlock(&_Mutex);
    return result;
}

bool controls_ConsumeEvent(Event* event) {
    switch(event->type) {
        case EventType_CLICKER_CREATE:
            g_mutex_lock(&_Mutex);
            g_array_append_val(_ConnectedClickersId, event->intData);
            if (_SelectedClickerIndex == -1) {
                _SelectedClickerIndex = 0;
                g_message( "Selected Clicker ID : %d", g_array_index(_ConnectedClickersId, int, _SelectedClickerIndex));
            }
            g_mutex_unlock(&_Mutex);
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
