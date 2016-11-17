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

#include "processing_queue.h"
#include "clicker.h"
#include "log.h"
#include "crypto/diffie_hellman_keys_exchanger.h"
#include "ubus_agent.h"
#include "utils.h"

#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/**
 * @brief HEAD of task queue
 */
static queue_Task *_Tasks = NULL;

/**
 * @brief last processed task
 */
static queue_Task *_Result = NULL;

static sem_t semaphore;
static pthread_t _QueueThread;
static int _KeepRunning = 1;
static char *_Psk = NULL;
static uint8_t _PskLen = 0;



static void GeneratePskCallback(char *psk, uint8_t pskLen, pdubus_GeneratePskRequest *request)
{

	queue_Task *task = request->priv;
	if (psk == NULL) {
		task->outData = NULL;
		task->outDataLength = 0;
		sem_wait(&semaphore);
		_Result = task;
		FREE_AND_NULL(request);
		sem_post(&semaphore);

		return;
	}
	sem_wait(&semaphore);
    _Psk = malloc(pskLen+1);
    strncpy(_Psk, psk, pskLen+1);
	_PskLen = pskLen;
	task->outData = _Psk;
	task->outDataLength = pskLen+1;
	_Result = task;
	FREE_AND_NULL(request);
	sem_post(&semaphore);

}

void queue_AddTask(queue_Task *task)
{
    sem_wait(&semaphore);
    if (_Tasks == NULL)
    {
		_Tasks = task;
		sem_post(&semaphore);
		return;
    }

    queue_Task *current = _Tasks;

    while (current->next != NULL)
    {
         current = current->next;
    }
    current->next = task;
	task->next = NULL;

    sem_post(&semaphore);
}

static queue_Task *queue_PopTask()
{
    sem_wait(&semaphore);
    if (_Tasks == NULL)
    {
		sem_post(&semaphore);
        return NULL;
    }
    queue_Task *task = _Tasks;
    _Tasks = _Tasks->next;
    task->next = NULL;
    sem_post(&semaphore);
    return task;
}

static void queue_HandleGeneratelocalKey(queue_Task *task)
{
	Clicker *clicker = clicker_AcquireOwnership(task->clickerID);
    sem_wait(&semaphore);
	DiffieHellmanKeysExchanger *keysExchanger = clicker->keysExchanger;
	task->outData = (void*)dh_generateExchangeData(keysExchanger);

	task->outDataLength = keysExchanger->pModuleLength;

	_Result = task;
	sem_post(&semaphore);
	clicker_ReleaseOwnership(clicker);
}

static void queue_HandleGeneratePsk(queue_Task *task)
{
	unsigned long currentTime = GetCurrentTimeMillis();
    sem_wait(&semaphore);
	_Psk = NULL;
    sem_post(&semaphore);

//toster	pdubus_Init();

	pdubus_GeneratePskRequest *request;
	request = malloc(sizeof(pdubus_GeneratePskRequest));
	request->callback = GeneratePskCallback;
	request->priv = task;
	if (ubusagent_SendGeneratePskMessage(request) == false)
	{
		sem_wait(&semaphore);
		task->outData = NULL;
		task->outDataLength = 0;
	    _Result = task;
	    sem_post(&semaphore);
		FREE_AND_NULL(request);
	}

//toster	pdubus_Close();
}

static void queue_HandleGenerateSharedKey(queue_Task *task)
{
	Clicker *clicker = clicker_AcquireOwnership(task->clickerID);
    DiffieHellmanKeysExchanger *keysExchanger = clicker->keysExchanger;
	task->outData = (void*)dh_completeExchangeData(keysExchanger, clicker->remoteKey, keysExchanger->pModuleLength);
	task->outDataLength = keysExchanger->pModuleLength;
	sem_wait(&semaphore);
	_Result = task;
	sem_post(&semaphore);
	clicker_ReleaseOwnership(clicker);

}

static void queue_HandleTask(queue_Task *task)
{
    switch (task->type)
    {
        case queue_TaskType_GENERATE_ALICE_KEY:
	    	queue_HandleGeneratelocalKey(task);
		    break;
		case queue_TaskType_GENERATE_PSK:
			queue_HandleGeneratePsk(task);
			break;
		case queue_TaskType_GENERATE_SHARED_KEY:
			queue_HandleGenerateSharedKey(task);
			break;
	default:
	    break;

    }
}

static void * queue_Loop(void *arg)
{
    int keepRunning = 1;
    sem_wait(&semaphore);
    keepRunning = _KeepRunning;
    sem_post(&semaphore);

    while (keepRunning)
    {
		sem_wait(&semaphore);
		queue_Task* t = _Tasks;
        queue_Task* r = _Result;
		sem_post(&semaphore);
        if (t != NULL && r == NULL)
        {
             queue_HandleTask(queue_PopTask());
        }
		usleep(1000 * 200);
        sem_wait(&semaphore);
        keepRunning = _KeepRunning;
        sem_post(&semaphore);
    }
//    LOG(LOG_INFO,"A");
//    sem_close(&semaphore);
//    LOG(LOG_INFO,"B");
    return NULL;
}

queue_Task *queue_NewQueueTask(queue_TaskType type, uint8_t clickerID, void * inData, uint8_t inDataLength, sem_t * sem)
{
	queue_Task * newTask = malloc(sizeof(queue_Task));
	newTask->type = type;
	newTask->clickerID = clickerID;
	newTask->inData = inData;
	newTask->inDataLength = inDataLength;
	newTask->outData = NULL;
	newTask->outDataLength = 0;
	newTask->semaphore = sem;
	newTask->next = NULL;
	return newTask;
}

void queue_Start(void)
{

    sem_init(&semaphore, 0, 1);
    if (pthread_create(&_QueueThread, NULL, queue_Loop, NULL) < 0)
    {
		LOG(LOG_DBG, "Error starting queue thread");
    }
}

void queue_Stop(void)
{
    sem_wait(&semaphore);
    _KeepRunning = 0;
    sem_post(&semaphore);
}

void queue_ReleaseTask(queue_Task * task)
{
	if (task == NULL)
		return;
	if (task->inData != NULL)
		free(task->inData);
	if (task->outData != NULL)
		free(task->outData);
	free(task);
}

queue_Task * queue_PopResult()
{
	sem_wait(&semaphore);
	queue_Task * result = _Result;
	_Result = NULL;
    sem_post(&semaphore);
	return result;
}
