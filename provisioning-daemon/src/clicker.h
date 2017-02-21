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

/**
 * @file  clicker.h
 * @brief Provides Clicker struct and set of methods to operate on the clicker list.
 */

#ifndef __CLICKER_H__
#define __CLICKER_H__

#include "crypto/diffie_hellman_keys_exchanger.h"
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Represents a single clicker
 */
typedef struct Clicker
{
    struct Clicker *next;               /**< pointer to next element of list */
    int socket;                         /**< socket descriptor on which this clicker operates */
    int clickerID;                      /**< id of clicker, must be unique. */
    unsigned long lastKeepAliveTime;    /**< unix timestamp of last KEEP_ALIVE command sent to this clicker */
    uint8_t *localKey;                  /**< Exchange key sent to remote clicker */
    uint8_t *remoteKey;                    /**< Exchange key received from remote clicker */
    uint8_t *sharedKey;                 /**< shared key used to encrypt communication with remote clicker */
    uint8_t *psk;                       /**< psk received from device server */
    uint8_t pskLen;                     /**< Length of psk key */
    uint8_t *identity;                  /**< identity received from device server */
    size_t identityLen;                /**< Length of identity field */
    bool taskInProgress;
    DiffieHellmanKeysExchanger *keysExchanger; /**< struct used to exchange crypto keys between provisioning daemon and remote clicker */
    sem_t * semaphore;                  /**< semaphore that shpuld be used to synchronize operations on this struct fields */
    uint8_t ownershipsCount;
    unsigned long provisionTime;        /**< unix timestamp telling when provisioning process of this clicker has finished. 0 of provisioning is not finished yet. */
    int error;
    char* name;
    bool provisioningInProgress;        /**< true - if provisioning is taking place on this clicker, otherwise false */
} Clicker;

/**
 * @brief Create new instance of clicker and puts it at the end of the list
 * @param[in] socket descriptor of socket on which this clicker is connected
 */
Clicker *clicker_New(int socket);

/**
 * @brief Mark the clicker as a ready to be freed.
 * @param[in] clicker to be freed
 */
void clicker_Release(Clicker *clicker);

/**
 * @brief Initialize semaphore used to synchronize operations on clickers lists.
 * Should be called before any other function is called.
 */
void clicker_InitSemaphore(void);

/**
 * @brief Get a clicker at specified index of the list.
 * @param[in] index lookup index
 * @return clicker or NULL if index is out of bounds
 */
Clicker *clicker_GetClickerAtIndex(int index);

/**
 * @brief Returns count of clickers in collections.
 * @return Count of clickers
 */
unsigned int clicker_GetClickersCount(void);

/**
 * @brief Get index of the list on which specified clicker is.
 * @param[in] clicker to look for
 * @return index represented as positive number or -1 if clicker is not in connected clickers list
 */
int clicker_GetIndexOfClicker(Clicker *clicker);

/**
 * @brief Get clicker with specified clickerID
 * @param[in] clickerID id of clicker to look for
 * @return found clicker or NULL
 */
Clicker *clicker_GetClickerByID(int clickerID);

/**
 * @brief Get head of connected clickers list
 * @return clicker or NULL if list is empty
 */
Clicker *clicker_GetClickers(void);

/**
 * @brief Mark clicker with specified ID as being used so it won't get purged until ownership is released.
 * @param[in] clickerID id of clicker
 * @return clicker or NULL if no clicker with specified ID exists in the list of connected clickers
 */
Clicker *clicker_AcquireOwnership(int clickerID);

/**
 * @brief Mark clicker with specified index as being used so it won't get purged until ownership is released.
 * @param[in] index of clicker
 * @return clicker or NULL if no clicker at specified index exists in the list of connected clickers
 */
Clicker* clicker_AcquireOwnershipAtIndex(int index);

/**
 * @brief Release ownership of clicker with specified ID so it can be purged.
 * @param[in] clicker clicker
 */
void clicker_ReleaseOwnership(Clicker *clicker);

/**
 * @brief Try to purge all clickers that are ready to be freed.
 */
void clicker_Purge(void);

void clicker_wait(void);

void clicker_post(void);

#endif
