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

#ifndef __BIGINT_H__
#define __BIGINT_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int length;
  uint8_t* buffer;
} BigInt;

//Various constructors
BigInt* bi_create(uint8_t* buf, int length);
BigInt* bi_createFromLong(long i, int length);
BigInt* bi_clone(BigInt* bi);

//destructor
void bi_release(BigInt** bi);

/**
 * @brief Returns true if b1 == b2 - false otherwise.
 */
bool bi_equal(BigInt* b1, BigInt* b2);

/**
 * @brief Returns true if parameter is even number - false otherwise.
 */
bool bi_isEvenNumber(BigInt* bi);

/**
 * @brief Addition assignment operator.
 */
void bi_add(BigInt* b1, BigInt* b2);

/**
 * @brief Subtraction assignment operator.
 */
void bi_sub(BigInt* b1, BigInt* b2);

/**
 * @brief Multiplication assignment operator.
 */
void bi_multiply(BigInt* b1, BigInt* b2);

/**
 * @brief Division assignment operator.
 */
void bi_divide(BigInt* b1, BigInt* b2);

/**
 * @brief Modulo assignment operator.
 */
void bi_modulo(BigInt* b1, BigInt* b2);

/**
 * @brief (a mod b) assignment operator.
 */
void bi_multiplyAmodB(BigInt* bi, BigInt* a, BigInt* b);

/**
 * @brief Allocates some constants which speedup calculations
 */
void bi_generateConst();

void bi_assign(BigInt* b1, BigInt* b2);

/**
 * @brief Release constants allocated by bi_generateConst
 */
void bi_releaseConst();
#endif /* __BIGINT_H__ */
