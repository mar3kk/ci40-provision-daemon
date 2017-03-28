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

#include "diffie_hellman_keys_exchanger.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

DiffieHellmanKeysExchanger* dh_NewKeyExchanger(char* buffer, int PModuleLength, int pCryptoGModule, Randomizer rand) {

    DiffieHellmanKeysExchanger* result = malloc(sizeof(DiffieHellmanKeysExchanger));
    result->pModuleLength = PModuleLength;
    result->pCryptoPModule = malloc(PModuleLength);
    if (buffer) {
        memcpy(result->pCryptoPModule, buffer, PModuleLength);
    }
    result->pCryptoGModule = pCryptoGModule;
    result->x = NULL;
    result->randomizer = rand;
    return result;
}

void dh_Release(DiffieHellmanKeysExchanger** exchanger) {
    if (exchanger) {
        free((*exchanger)->pCryptoPModule);
        (*exchanger)->pCryptoPModule = NULL;
        if ((*exchanger)->x) {
            bi_Release(&(*exchanger)->x);
        }
        (*exchanger)->randomizer = NULL;
        free(*exchanger);
        *exchanger = NULL;
    }
}

void dh_InvertBinary(unsigned char* binary, int length) {
    size_t i;
    for (i = 0; i < length / 2; ++i) {
        char tmp = binary[i];
        binary[i] = binary[length - i - 1];
        binary[length - i - 1] = tmp;
    }
}

BigInt* dh_ApowBmodN(BigInt* a, BigInt* b, BigInt* n, int len) {
    BigInt* result = bi_CreateFromLong(1, len);
    BigInt* counter = bi_Clone(b);
    BigInt* base = bi_Clone(a);
    BigInt* ZERO = bi_Create(NULL, len);
    BigInt* ONE = bi_CreateFromLong(1, len);
    BigInt* TWO = bi_CreateFromLong(2, len);
    while (!bi_Equal(counter, ZERO)) {
        if (bi_IsEvenNumber(counter)) {
            bi_Divide(counter, TWO);
            bi_MultiplyAmodB(base, base, n);
        } else {
            bi_Sub(counter, ONE);
            bi_MultiplyAmodB(result, base, n);
        }
    }
    bi_Release(&counter);
    bi_Release(&base);
    bi_Release(&TWO);
    bi_Release(&ONE);
    bi_Release(&ZERO);
    return result;
}

unsigned char* dh_GenerateExchangeData(DiffieHellmanKeysExchanger* exchanger) {
    BigInt* g = bi_CreateFromLong(exchanger->pCryptoGModule, exchanger->pModuleLength);
    BigInt* p = bi_Create(exchanger->pCryptoPModule, exchanger->pModuleLength);
    int length = exchanger->pModuleLength;
    unsigned char xBuff[length];
    if (!exchanger->randomizer(xBuff, length)) {
        return NULL;
    }
    dh_InvertBinary(xBuff, length);
    exchanger->x = bi_Create(xBuff, length);
    BigInt* y = bi_CreateFromLong(0, length);
    BigInt* i69h = dh_ApowBmodN(g, exchanger->x, p, length);
    bi_Assign(y, i69h);
    bi_Release(&i69h);
    unsigned char* result = malloc(length);
    memcpy(result, y->buffer, length);
    bi_Release(&y);
    bi_Release(&p);
    bi_Release(&g);

    return result;
}

unsigned char* dh_CompleteExchangeData(DiffieHellmanKeysExchanger* exchanger, unsigned char* externalData,
        int dataLength) {
    if (exchanger->pModuleLength <= dataLength) {

        int length = exchanger->pModuleLength;
        BigInt* p = bi_Create(exchanger->pCryptoPModule, exchanger->pModuleLength);
        BigInt* extData = bi_Create(externalData, dataLength);
        BigInt* y = bi_CreateFromLong(0, length);
        BigInt* i69h = dh_ApowBmodN(extData, exchanger->x, p, length);
        bi_Assign(y, i69h);
        bi_Release(&i69h);

        unsigned char* result = malloc(length);
        memcpy(result, y->buffer, length);

        bi_Release(&y);
        bi_Release(&extData);
        bi_Release(&p);
        return result;
    }

    return NULL;
}
