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

void bi_generateConst() {
  ONE_32 = bi_createFromLong(1, 32);
  ONE_16 = bi_createFromLong(1, 16);

  TMP_16 = bi_createFromLong(1, 16);
  TMP_32 = bi_createFromLong(1, 32);

  TMP2_16 = bi_createFromLong(1, 16);
  TMP2_32 = bi_createFromLong(1, 32);
}

void bi_releaseConst() {
  bi_release(&ONE_32);
  bi_release(&ONE_16);

  bi_release(&TMP_16);
  bi_release(&TMP_32);

  bi_release(&TMP2_16);
  bi_release(&TMP2_32);
}

BigInt* bi_create(uint8_t* buf, int length) {

  BigInt* result = malloc(sizeof(BigInt));
  result->length = length;
  result->buffer = malloc(length);
  memset(result->buffer, 0, length);

  if (buf != NULL) {
    memcpy(result->buffer, buf, length);
  }
  return result;
}

BigInt* bi_createFromBigInt(BigInt* bi, int length) {

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

void bi_release(BigInt** bi) {
  if (bi) {
    free((*bi)->buffer);
    free(*bi);
    *bi = NULL;
  }
}

BigInt* bi_clone(BigInt* bi) {
  return bi_create(bi->buffer, bi->length);
}


BigInt* bi_createFromLong(long i, int length) {
  BigInt* result = malloc(sizeof(BigInt));
  result->length = length;
  result->buffer = malloc(length);
  memset(result->buffer + sizeof(i), 0, length - sizeof(i));
  memcpy(result->buffer, &i, sizeof(i));
  return result;
}

bool bi_isNotZero(BigInt* b1) {
  int n = b1->length;
  uint8_t* s1 = b1->buffer;

  for ( ; n-- ; s1++) {
    if (*s1 != 0) {
      return true;
    }
  }
  return false;
}

bool bi_equal(BigInt* b1, BigInt* b2) {
  return memcmp(b1->buffer, b2->buffer, b1->length) == 0;
}

bool bi_greater(BigInt* b1, BigInt* b2) {
  int i,j;
  for (i = b1->length - 1; i >= 0; --i)
  {
    for (j = 1; j >= 0; --j)
    {
      char currentFirstChar = ((unsigned char)(b1->buffer[i] << 4*(1 - j))) >> 4;
      char currentSecondChar = ((unsigned char)(b2->buffer[i] << 4*(1 - j))) >> 4;

      if (currentFirstChar == currentSecondChar) continue;
      else return (currentFirstChar > currentSecondChar);
    }
  }
  return false;
}

bool bi_greaterEq(BigInt* b1, BigInt* b2) {
  return bi_equal(b1, b2) || bi_greater(b1, b2);
}

size_t bi_getDigitCapacity(BigInt* bi)
{
  unsigned int dimension = bi->length;
  size_t result = dimension * 2;
  int i;
  for (i = dimension - 1; i >= 0; --i)
  {
    uint8_t firstPart = bi->buffer[i] >> 4;
    uint8_t secondPart = (unsigned char)(bi->buffer[i] << 4) >> 4;
    if (secondPart == 0 && firstPart == 0)
    {
      result -= 2;
    }
    else if (firstPart == 0 && secondPart != 0)
    {
      --result;
      return result;
    }
    else
    {
      return result;
    }
  }
  return result;
}

void bi_multiplyBy16InPowDigits(BigInt* bi, size_t digits) {

  uint8_t tmpBuff[bi->length];
  memcpy(tmpBuff, bi->buffer, bi->length);

  memset(bi->buffer, 0, bi->length);
  const int nonParity = (int)digits % 2;
  const int digits2 = (int)digits / 2;
  int i;
  for (i = digits2; i < bi->length; ++i)
  {
    char current = ((unsigned char)(tmpBuff[(bi->length - 1) - i] << 4)) >> 4;
    current = current << (nonParity * 4);

    int index = i - digits2;
    if (index >= 0)
    {
      bi->buffer[(bi->length - 1) - index] += current;
    }

    current = ((unsigned char)(tmpBuff[(bi->length - 1) - i] )) >> 4;
    current = current << ( (1 - nonParity) * 4);

    index = i - digits2 - nonParity;
    if (index >= 0)
    {
      bi->buffer[(bi->length - 1) - index] += current;
    }
  }
}

void bi_add(BigInt* b1, BigInt* b2) {

  uint8_t buff[b1->length];
  memset(buff, 0, b1->length);

  uint8_t* b1Buff = b1->buffer;
  uint8_t* b2Buff = b2->buffer;
  uint8_t* buffPtr = buff;

  bool moveBase = false;
  int i;
  for (i = b1->length-1; i >=0 ; --i)
  {
    char currentFirstChar = (*b1Buff) & 0x0F;
    char currentSecondChar = (*b2Buff) & 0x0F;

    if (moveBase) {
      currentFirstChar ++;
    }

    if (currentFirstChar + currentSecondChar >= 16) {
      *buffPtr += (currentFirstChar + currentSecondChar - 16);
      moveBase = true;
    } else {
      *buffPtr += (currentFirstChar + currentSecondChar);
      moveBase = false;
    }

    currentFirstChar = (*b1Buff) >> 4;
    currentSecondChar = (*b2Buff)  >> 4;

    if (moveBase) {
      currentFirstChar ++;
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

void bi_sub(BigInt* b1, BigInt* b2) {

  uint8_t buff[b1->length];
  memset(buff, 0, b1->length);

  uint8_t* b1Buff = b1->buffer;
  uint8_t* b2Buff = b2->buffer;
  uint8_t* buffPtr = buff;

  bool moveBase = false;
  int i;
  for (i = 0; i < b1->length; ++i)
  {
      char currentFirstChar = (*b1Buff) & 0x0F;
      char currentSecondChar = (*b2Buff) & 0x0F;

      if (moveBase)
      {
        currentFirstChar -= 1;
      }
      if (currentFirstChar >= currentSecondChar)
      {
        *buffPtr += (currentFirstChar - currentSecondChar) << 0*4;
        moveBase = false;
      }
      else
      {
        *buffPtr += (currentFirstChar + 16 - currentSecondChar) << 0*4;
        moveBase = true;
      }

      //
      currentFirstChar = (*b1Buff) >> 4;
      currentSecondChar = (*b2Buff) >> 4;

      if (moveBase)
      {
        currentFirstChar -= 1;
      }
      if (currentFirstChar >= currentSecondChar)
      {
        *buffPtr += (currentFirstChar - currentSecondChar) << 1*4;
        moveBase = false;
      }
      else
      {
        *buffPtr += (currentFirstChar + 16 - currentSecondChar) << 1*4;
        moveBase = true;
      }

    buffPtr++;
    b1Buff++;
    b2Buff++;
  }
  memcpy(b1->buffer, buff, b1->length);
}

void bi_assign(BigInt* b1, BigInt* b2) {
  if (b2 != NULL) {
    if (b1->length > b2->length) {
      memset(b1->buffer, 0, b1->length);
    }
    memcpy(b1->buffer, b2->buffer, b1->length);
  }
  else {
    memset(b1->buffer, 0, b1->length);
  }
}

void bi_multiply(BigInt* b1, BigInt* b2) {
  BigInt* counter = bi_clone(b2);
  BigInt* result = b1->length == 16 ? TMP2_16 : TMP2_32;
  memset(result->buffer, 0, b1->length);

  BigInt* ONE = b1->length == 16 ? ONE_16 : ONE_32;
  BigInt* tmp = b1->length == 16 ? TMP_16 : TMP_32;

  size_t capacity = bi_getDigitCapacity(counter);

  while( bi_isNotZero(counter) ) {
    bi_assign(tmp, b1);
    bi_multiplyBy16InPowDigits(tmp, capacity-1);
    bi_add(result, tmp);

    //diff
    bi_assign(tmp, ONE);
    bi_multiplyBy16InPowDigits(tmp, capacity-1);
    bi_sub(counter, tmp);
    capacity = bi_getDigitCapacity(counter);
  }

  memcpy(b1->buffer, result->buffer, b1->length);
  bi_release(&counter);
}

void bi_divideInternal(BigInt* b1, BigInt* divider, BigInt* quotient, BigInt* remainder)
{
  BigInt* ONE = b1->length == 16 ? ONE_16 : ONE_32;
  BigInt* dividerClone = bi_clone(divider);

  bi_assign(remainder, b1);
  bi_assign(quotient, NULL);

  size_t capacityRemainder = bi_getDigitCapacity(remainder);
  size_t capacityDivider = bi_getDigitCapacity(divider);

  BigInt* q = bi_createFromLong(1, b1->length);
  BigInt* tmp = bi_clone(dividerClone);

  while (capacityRemainder > capacityDivider)
  {
    bi_assign(dividerClone, divider);
    bi_multiplyBy16InPowDigits(dividerClone, capacityRemainder - capacityDivider - 1);

    bi_assign(q, ONE);
    bi_multiplyBy16InPowDigits(q, capacityRemainder - capacityDivider - 1);

    bi_assign(tmp, dividerClone);
    bi_multiplyBy16InPowDigits(tmp,1);

    if (bi_greaterEq(remainder,tmp))
    {
      bi_assign(dividerClone,tmp);
      bi_multiplyBy16InPowDigits(q, 1);
    }
    while (bi_greaterEq(remainder, dividerClone))
    {
      bi_sub(remainder,dividerClone);
      bi_add(quotient, q);
    }

    capacityRemainder = bi_getDigitCapacity(remainder);
  }
  bi_release(&tmp);
  bi_release(&q);

  while (bi_greaterEq(remainder, divider))
  {
    bi_sub(remainder, divider);
    bi_add(quotient, ONE);
  }
  bi_release(&dividerClone);
}


void bi_modulo(BigInt* b1, BigInt* b2) {

  BigInt* quotient = bi_create(NULL, b1->length);
  BigInt* remainder = bi_create(NULL, b1->length);

  bi_divideInternal(b1, b2, quotient, remainder);
  bi_assign(b1, remainder);

  bi_release(&quotient);
  bi_release(&remainder);
}

void bi_divide(BigInt* b1, BigInt* b2) {

  BigInt* quotient = bi_create(NULL, b1->length);
  BigInt* remainder = bi_create(NULL, b1->length);

  bi_divideInternal(b1, b2, quotient, remainder);
  bi_assign(b1, quotient);

  bi_release(&quotient);
  bi_release(&remainder);
}


void bi_multiplyAmodB(BigInt* bi, BigInt* a, BigInt* b) {
  if (bi_getDigitCapacity(bi) + bi_getDigitCapacity(a) <= bi->length) {
    bi_multiply(bi, a);
    bi_modulo(bi, b);
  }
  else {
    BigInt* extThis = bi_createFromBigInt(bi, bi->length * 2);
    BigInt* extA = bi_createFromBigInt(a, a->length * 2);
    BigInt* extB = bi_createFromBigInt(b, b->length * 2);
    bi_multiply(extThis, extA);
    bi_modulo(extThis, extB);
    bi_assign(bi, extThis);
    bi_release(&extB);
    bi_release(&extA);
    bi_release(&extThis);
  }
}

bool bi_isEvenNumber(BigInt* bi) {
  uint8_t lastDigit = (uint8_t)(bi->buffer[0] << 4) >> 4;
  if ((lastDigit != 0) && (lastDigit % 2 != 0)) {
    return false;
  }

  return true;
}
