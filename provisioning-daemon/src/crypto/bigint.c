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

#include "bigint.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static BigInt* ONE_32;
static BigInt* ONE_16;
static BigInt* TMP_16;
static BigInt* TMP_32;
static BigInt* TMP2_16;
static BigInt* TMP2_32;

void bi_GenerateConst() {
    ONE_32 = bi_CreateFromLong(1, 32);
    ONE_16 = bi_CreateFromLong(1, 16);

    TMP_16 = bi_CreateFromLong(1, 16);
    TMP_32 = bi_CreateFromLong(1, 32);

    TMP2_16 = bi_CreateFromLong(1, 16);
    TMP2_32 = bi_CreateFromLong(1, 32);
}

void bi_ReleaseConst() {
    bi_Release(&ONE_32);
    bi_Release(&ONE_16);

    bi_Release(&TMP_16);
    bi_Release(&TMP_32);

    bi_Release(&TMP2_16);
    bi_Release(&TMP2_32);
}

BigInt* bi_Create(uint8_t* buf, int length) {

    BigInt* result = malloc(sizeof(BigInt));
    result->length = length;
    result->buffer = malloc(length);
    memset(result->buffer, 0, length);

    if (buf != NULL) {
        memcpy(result->buffer, buf, length);
    }
    return result;
}

BigInt* bi_CreateFromBigInt(BigInt* bi, int length) {

    BigInt* result = malloc(sizeof(BigInt));
    result->length = length;
    result->buffer = malloc(length);

    if (bi != NULL) {
        memcpy(result->buffer, bi->buffer, bi->length);
        if (bi->length < length) {
            memset(result->buffer + bi->length, 0, length - bi->length);
        }
    } else {
        memset(result->buffer, 0, length);
    }
    return result;
}

void bi_Release(BigInt** bi) {
    if (bi) {
        free((*bi)->buffer);
        free(*bi);
        *bi = NULL;
    }
}

BigInt* bi_Clone(BigInt* bi) {
    return bi_Create(bi->buffer, bi->length);
}

BigInt* bi_CreateFromLong(long i, int length) {
    BigInt* result = malloc(sizeof(BigInt));
    result->length = length;
    result->buffer = malloc(length);
    memset(result->buffer + sizeof(i), 0, length - sizeof(i));
    memcpy(result->buffer, &i, sizeof(i));
    return result;
}

bool bi_IsNotZero(BigInt* b1) {
    int n = b1->length;
    uint8_t* s1 = b1->buffer;

    for (; n--; s1++) {
        if (*s1 != 0) {
            return true;
        }
    }
    return false;
}

bool bi_Equal(BigInt* b1, BigInt* b2) {
    return memcmp(b1->buffer, b2->buffer, b1->length) == 0;
}

bool bi_Greater(BigInt* b1, BigInt* b2) {
    int i, j;
    for (i = b1->length - 1; i >= 0; --i) {
        for (j = 1; j >= 0; --j) {
            char currentFirstChar = ((unsigned char) (b1->buffer[i] << 4 * (1 - j))) >> 4;
            char currentSecondChar = ((unsigned char) (b2->buffer[i] << 4 * (1 - j))) >> 4;

            if (currentFirstChar == currentSecondChar)
                continue;
            else
                return (currentFirstChar > currentSecondChar);
        }
    }
    return false;
}

bool bi_GreaterEq(BigInt* b1, BigInt* b2) {
    return bi_Equal(b1, b2) || bi_Greater(b1, b2);
}

size_t bi_GetDigitCapacity(BigInt* bi) {
    unsigned int dimension = bi->length;
    size_t result = dimension * 2;
    int i;
    for (i = dimension - 1; i >= 0; --i) {
        uint8_t firstPart = bi->buffer[i] >> 4;
        uint8_t secondPart = (unsigned char) (bi->buffer[i] << 4) >> 4;
        if (secondPart == 0 && firstPart == 0) {
            result -= 2;
        } else if (firstPart == 0 && secondPart != 0) {
            --result;
            return result;
        } else {
            return result;
        }
    }
    return result;
}

void bi_MultiplyBy16InPowDigits(BigInt* bi, size_t digits) {

    uint8_t tmpBuff[bi->length];
    memcpy(tmpBuff, bi->buffer, bi->length);

    memset(bi->buffer, 0, bi->length);
    const int nonParity = (int) digits % 2;
    const int digits2 = (int) digits / 2;
    int i;
    for (i = digits2; i < bi->length; ++i) {
        char current = ((unsigned char) (tmpBuff[(bi->length - 1) - i] << 4)) >> 4;
        current = current << (nonParity * 4);

        int index = i - digits2;
        if (index >= 0) {
            bi->buffer[(bi->length - 1) - index] += current;
        }

        current = ((unsigned char) (tmpBuff[(bi->length - 1) - i])) >> 4;
        current = current << ((1 - nonParity) * 4);

        index = i - digits2 - nonParity;
        if (index >= 0) {
            bi->buffer[(bi->length - 1) - index] += current;
        }
    }
}

void bi_Add(BigInt* b1, BigInt* b2) {

    uint8_t buff[b1->length];
    memset(buff, 0, b1->length);

    uint8_t* b1Buff = b1->buffer;
    uint8_t* b2Buff = b2->buffer;
    uint8_t* buffPtr = buff;

    bool moveBase = false;
    int i;
    for (i = b1->length - 1; i >= 0; --i) {
        char currentFirstChar = (*b1Buff) & 0x0F;
        char currentSecondChar = (*b2Buff) & 0x0F;

        if (moveBase) {
            currentFirstChar++;
        }

        if (currentFirstChar + currentSecondChar >= 16) {
            *buffPtr += (currentFirstChar + currentSecondChar - 16);
            moveBase = true;
        } else {
            *buffPtr += (currentFirstChar + currentSecondChar);
            moveBase = false;
        }

        currentFirstChar = (*b1Buff) >> 4;
        currentSecondChar = (*b2Buff) >> 4;

        if (moveBase) {
            currentFirstChar++;
        }

        if (currentFirstChar + currentSecondChar >= 16) {
            *buffPtr += (currentFirstChar + currentSecondChar - 16) << 4;
            moveBase = true;
        } else {
            *buffPtr += (currentFirstChar + currentSecondChar) << 4;
            moveBase = false;
        }
        buffPtr++;
        b1Buff++;
        b2Buff++;
    }
    memcpy(b1->buffer, buff, b1->length);
}

void bi_Sub(BigInt* b1, BigInt* b2) {

    uint8_t buff[b1->length];
    memset(buff, 0, b1->length);

    uint8_t* b1Buff = b1->buffer;
    uint8_t* b2Buff = b2->buffer;
    uint8_t* buffPtr = buff;

    bool moveBase = false;
    int i;
    for (i = 0; i < b1->length; ++i) {
        char currentFirstChar = (*b1Buff) & 0x0F;
        char currentSecondChar = (*b2Buff) & 0x0F;

        if (moveBase) {
            currentFirstChar -= 1;
        }
        if (currentFirstChar >= currentSecondChar) {
            *buffPtr += (currentFirstChar - currentSecondChar) << 0 * 4;
            moveBase = false;
        } else {
            *buffPtr += (currentFirstChar + 16 - currentSecondChar) << 0 * 4;
            moveBase = true;
        }

        //
        currentFirstChar = (*b1Buff) >> 4;
        currentSecondChar = (*b2Buff) >> 4;

        if (moveBase) {
            currentFirstChar -= 1;
        }
        if (currentFirstChar >= currentSecondChar) {
            *buffPtr += (currentFirstChar - currentSecondChar) << 1 * 4;
            moveBase = false;
        } else {
            *buffPtr += (currentFirstChar + 16 - currentSecondChar) << 1 * 4;
            moveBase = true;
        }

        buffPtr++;
        b1Buff++;
        b2Buff++;
    }
    memcpy(b1->buffer, buff, b1->length);
}

void bi_Assign(BigInt* b1, BigInt* b2) {
    if (b2 != NULL) {
        if (b1->length > b2->length) {
            memset(b1->buffer, 0, b1->length);
        }
        memcpy(b1->buffer, b2->buffer, b1->length);
    } else {
        memset(b1->buffer, 0, b1->length);
    }
}

void bi_Multiply(BigInt* b1, BigInt* b2) {
    BigInt* counter = bi_Clone(b2);
    BigInt* result = b1->length == 16 ? TMP2_16 : TMP2_32;
    memset(result->buffer, 0, b1->length);

    BigInt* ONE = b1->length == 16 ? ONE_16 : ONE_32;
    BigInt* tmp = b1->length == 16 ? TMP_16 : TMP_32;

    size_t capacity = bi_GetDigitCapacity(counter);

    while (bi_IsNotZero(counter)) {
        bi_Assign(tmp, b1);
        bi_MultiplyBy16InPowDigits(tmp, capacity - 1);
        bi_Add(result, tmp);

        //diff
        bi_Assign(tmp, ONE);
        bi_MultiplyBy16InPowDigits(tmp, capacity - 1);
        bi_Sub(counter, tmp);
        capacity = bi_GetDigitCapacity(counter);
    }

    memcpy(b1->buffer, result->buffer, b1->length);
    bi_Release(&counter);
}

void bi_DivideInternal(BigInt* b1, BigInt* divider, BigInt* quotient, BigInt* remainder) {
    BigInt* ONE = b1->length == 16 ? ONE_16 : ONE_32;
    BigInt* dividerClone = bi_Clone(divider);

    bi_Assign(remainder, b1);
    bi_Assign(quotient, NULL);

    size_t capacityRemainder = bi_GetDigitCapacity(remainder);
    size_t capacityDivider = bi_GetDigitCapacity(divider);

    BigInt* q = bi_CreateFromLong(1, b1->length);
    BigInt* tmp = bi_Clone(dividerClone);

    while (capacityRemainder > capacityDivider) {
        bi_Assign(dividerClone, divider);
        bi_MultiplyBy16InPowDigits(dividerClone, capacityRemainder - capacityDivider - 1);

        bi_Assign(q, ONE);
        bi_MultiplyBy16InPowDigits(q, capacityRemainder - capacityDivider - 1);

        bi_Assign(tmp, dividerClone);
        bi_MultiplyBy16InPowDigits(tmp, 1);

        if (bi_GreaterEq(remainder, tmp)) {
            bi_Assign(dividerClone, tmp);
            bi_MultiplyBy16InPowDigits(q, 1);
        }
        while (bi_GreaterEq(remainder, dividerClone)) {
            bi_Sub(remainder, dividerClone);
            bi_Add(quotient, q);
        }

        capacityRemainder = bi_GetDigitCapacity(remainder);
    }
    bi_Release(&tmp);
    bi_Release(&q);

    while (bi_GreaterEq(remainder, divider)) {
        bi_Sub(remainder, divider);
        bi_Add(quotient, ONE);
    }
    bi_Release(&dividerClone);
}

void bi_Modulo(BigInt* b1, BigInt* b2) {

    BigInt* quotient = bi_Create(NULL, b1->length);
    BigInt* remainder = bi_Create(NULL, b1->length);

    bi_DivideInternal(b1, b2, quotient, remainder);
    bi_Assign(b1, remainder);

    bi_Release(&quotient);
    bi_Release(&remainder);
}

void bi_Divide(BigInt* b1, BigInt* b2) {

    BigInt* quotient = bi_Create(NULL, b1->length);
    BigInt* remainder = bi_Create(NULL, b1->length);

    bi_DivideInternal(b1, b2, quotient, remainder);
    bi_Assign(b1, quotient);

    bi_Release(&quotient);
    bi_Release(&remainder);
}

void bi_MultiplyAmodB(BigInt* bi, BigInt* a, BigInt* b) {
    if (bi_GetDigitCapacity(bi) + bi_GetDigitCapacity(a) <= bi->length) {
        bi_Multiply(bi, a);
        bi_Modulo(bi, b);
    } else {
        BigInt* extThis = bi_CreateFromBigInt(bi, bi->length * 2);
        BigInt* extA = bi_CreateFromBigInt(a, a->length * 2);
        BigInt* extB = bi_CreateFromBigInt(b, b->length * 2);
        bi_Multiply(extThis, extA);
        bi_Modulo(extThis, extB);
        bi_Assign(bi, extThis);
        bi_Release(&extB);
        bi_Release(&extA);
        bi_Release(&extThis);
    }
}

bool bi_IsEvenNumber(BigInt* bi) {
    uint8_t lastDigit = (uint8_t) (bi->buffer[0] << 4) >> 4;
    if ((lastDigit != 0) && (lastDigit % 2 != 0)) {
        return false;
    }

    return true;
}
