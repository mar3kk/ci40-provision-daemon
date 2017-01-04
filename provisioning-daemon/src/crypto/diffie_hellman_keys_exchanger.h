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

#ifndef __DIFFIE_HELLMAN_KEYS_EXCHANGER_H__
#define __DIFFIE_HELLMAN_KEYS_EXCHANGER_H__

#include "bigint.h"

#include <stdio.h>
#include <stdbool.h>


typedef bool (*Randomizer)(unsigned char* array, int length);

typedef struct {

  unsigned char* pCryptoPModule;
  unsigned int pModuleLength;
  unsigned int pCryptoGModule;
  BigInt* x;
  Randomizer randomizer;

} DiffieHellmanKeysExchanger;

/**
 * \brief Create new exchanger.
 */
DiffieHellmanKeysExchanger* dh_newKeyExchanger(char* buffer, int PModuleLength, int pCryptoGModule, Randomizer randomizer);

/**
 * \brief Release exchanger.
 */
void dh_release(DiffieHellmanKeysExchanger**);

/**
 * \brief Generate exchange key using exchanger passed.
 * @return exchange key or NULL if operation fails
 */
unsigned char* dh_generateExchangeData(DiffieHellmanKeysExchanger* );

/**
 * \brief Generate shared key for exchanger passed using 2nd party shared key (externalData).
 * @return shared key or NULL if operation fails
 */
unsigned char* dh_completeExchangeData(DiffieHellmanKeysExchanger*, unsigned char* externalData, int dataLength);


#endif /* __DIFFIEHELLMANKEYSEXCHANGER_h__ */
