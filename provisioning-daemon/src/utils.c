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

#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


static struct timeval _Timeval;

static int
itoa(unsigned int num, char* str, int len, int base)
{
    char chars[64] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
                    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
	int sum = num;
	int i = 0;
	if (len == 0)
		return -1;
	do
	{
        printf("i : %d , index : %d \n", i, sum%base);
        str[i++] = chars[sum%base];

		sum /= base;
	} while (sum && (i < (len - 1)));
	if (i == (len - 1) && sum)
		return -1;
	str[i] = '\0';
	return 0;
}

unsigned long GetCurrentTimeMillis()
{
    gettimeofday(&_Timeval, NULL);
    return _Timeval.tv_sec * 1000 + _Timeval.tv_usec/1000;
}

void HexStringToByteArray(const char* hexstr, uint8_t * dst, size_t len)
{
    size_t final_len = strnlen(hexstr, len*2);
    for (size_t i=0, j=0; i<final_len; i+=2, j++)
        dst[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i+1] % 32 + 9) % 25;
}

bool GenerateRandomX(unsigned char* array, int length) {
  unsigned long seed = GetCurrentTimeMillis();
  srand(seed);

  for (int i = 0; i < length; ++i) {
    array[i] = (rand()%9);
  }

  return true;
}

void GenerateClickerTimeHash(char *buffer)
{
    unsigned long currentTimeSeconds = GetCurrentTimeMillis() / 1000;
    itoa(currentTimeSeconds, buffer, 10, 52);
}

void GenerateClickerName(char* outBuffer, int maxBufLen, char *pattern, char *hash, char *ip)
{
  bool inBracket = false;
  maxBufLen--;
  while( (maxBufLen > 0) && (*pattern != 0) ) {

    if (inBracket == false) {
      if (*pattern != '{') {
        *outBuffer = *pattern;
        outBuffer++;
        maxBufLen--;

      } else {
        inBracket = true;
      }
      pattern++;

    } else {
      char* token = NULL;
      char id = *pattern;
      pattern++;
      if ((id == 0) || (*pattern != '}')) {
        break;
      }

      char ids[] = {'t', 'i'};
      char* tokens[] = {hash, ip };
      for(int t = 0; t < sizeof(ids); t++) {
        if (ids[t] == id) {
          token = tokens[t];
          break;
        }
      }

      if (token != NULL) {
        pattern++;
        int len = (int)strlen(token);
        len = len > maxBufLen ? maxBufLen : len;
        strcpy(outBuffer, token);
        maxBufLen -= len;
        outBuffer += len;
        inBracket = false;
      }

    }
  }
  *outBuffer = 0;
}
