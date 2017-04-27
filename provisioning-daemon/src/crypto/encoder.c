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

#include "encoder.h"
#include "rijndael.h"
#include <string.h>
#include <stdlib.h>

uint8_t* softap_EncodeBytes(uint8_t* src, uint8_t len, uint8_t* key, uint8_t* outputSize) {
    uint8_t IV[16];
    int t;
    for (t = 0; t < 15; t++) { //spare the last byte
        IV[t] = key[15 - t];
    }

    int paddedSize = (len / 16) * 16;
    if (paddedSize < len) {
        paddedSize += 16;
    }
    *outputSize = paddedSize;

    uint8_t* result = malloc(paddedSize);

    rijndael_ctx ctx;
    rijndael_set_key(&ctx, key, 128);

    int y;
    for (t = 0; t < paddedSize; t += 16) {
        IV[15] = t / 16;
        for (y = 0; y < 16; y++) {
            if (t + y < len) {
                src[t + y] ^= IV[y];
            }
        }
        rijndael_encrypt(&ctx, src + t, result + t);
    }

    return result;
}

void softap_DecodeBytes(uint8_t* data, uint8_t len, uint8_t* key_n_iv) {
    uint8_t IV[16];
    int t;
    for (t = 0; t < 15; t++) { //spare the last byte
        IV[t] = key_n_iv[16 + t];
    }

    rijndael_ctx ctx;
    rijndael_set_key(&ctx, key_n_iv, 128);

    uint8_t tmp[16];
    int y;
    for (t = 0; t < len; t += 16) {
        rijndael_decrypt(&ctx, data + t, tmp);
        IV[15] = t / 16;
        for (y = 0; y < 16; y++) {
            tmp[y] ^= IV[y];
        }
        memcpy(data + t, tmp, 16);
    }
}
