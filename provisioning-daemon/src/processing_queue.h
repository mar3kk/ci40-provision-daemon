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
  * Provides mechanism to put time consuming tasks of clicker provisioning onto worker thread.
  */

#ifndef __PROCESSING_QUEUE_H__
#define __PROCESSING_QUEUE_H__
#include <stdint.h>
#include <semaphore.h>

/**
 * Represents available task types that can be added to queue
 */
typedef enum
{
    queue_TaskType_GENERATE_ALICE_KEY,
    queue_TaskType_GENERATE_SHARED_KEY,
    queue_TaskType_GENERATE_PSK
} queue_TaskType;



/**
 * @brief Represents a single task than can be added to queue for processing
 */
typedef struct queue_Task
{
    queue_TaskType type;
    uint8_t clickerID;
	void * inData;
	uint8_t inDataLength;
	void *outData;
	uint8_t outDataLength;
    sem_t * semaphore;
    int statusCode;
    struct queue_Task *next;
} queue_Task;

/**
 * @brief Add new task to queue.
 * @param[in] task that will be added to queue
 */
void queue_AddTask( queue_Task *task);

/**
 * @brief Pops result of last computed task.
 * @return task with field outData filled with computation result or NULL if no result is availbale yet
 */
queue_Task * queue_PopResult();

queue_Task * queue_NewQueueTask(queue_TaskType type, uint8_t clickerID, void * inData, uint8_t inDataLength, sem_t * sem);
void queue_ReleaseTask(queue_Task * task);
void queue_Start(void);
void queue_Stop(void);
#endif
