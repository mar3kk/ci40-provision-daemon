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

/***************************************************************************************************
 * Includes
 **************************************************************************************************/

#include <bits/alltypes.h>
#include <bits/signal.h>
#include <libconfig.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "provisioning_daemon.h"
#include "clicker.h"
#include "clicker_sm.h"
#include "commands.h"
#include "connection_manager.h"
#include "crypto/bigint.h"
#include "crypto/crypto_config.h"
#include "errors.h"
#include "controls.h"
#include "log.h"
#include "provision_history.h"
#include "ubus_agent.h"
#include "utils.h"
#include "event.h"

/***************************************************************************************************
 * Definitions
 **************************************************************************************************/

#define DEFAULT_PATH_CONFIG_FILE                "/etc/config/provisioning_daemon"

#define CONFIG_BOOTSTRAP_URI                    "coaps://deviceserver.creatordev.io:15684"
#define CONFIG_DEFUALT_TCP_PORT                 (49300)
#define CONFIG_DEFAULT_ENDPOINT_PATTERN         "cd_{t}_{i}"
#define CONFIG_DEFAULT_LOCAL_PROV_CTRL          (true)
#define CONFIG_DEFAULT_REMOTE_PROV_CTRL         (false)
//! @cond Doxygen_Suppress

/***************************************************************************************************
 * Globals
 **************************************************************************************************/

/**
 * Describes states provisioning daemon can be in.
 */
typedef enum {
    pd_Mode_LISTENING, // no provisioning being performed, in this state user can choose clicker to provision
    pd_Mode_PROVISIONING, // provisioning has been started
    pd_Mode_ERROR
} pd_Mode;

static bool _ModeChanged = false;

/**
 * Main loop condition.
 */
static volatile bool _KeepRunning = true;

/**
 * Current app mode telling in which step of provisioning app currently  is.
 */
static pd_Mode _Mode = pd_Mode_LISTENING;

unsigned long _ModeErrorTime = 0;

FILE * g_debugStream = NULL;
int g_debugLevel = LOG_INFO;

static config_t _Cfg;

pd_Config _PDConfig = {
    .tcpPort = 0,
    .defaultRouteUri = NULL,
    .bootstrapUri = NULL,
    .dnsServer = NULL,
    .endPointNamePattern = NULL,
    .logLevel = 0,
    .localProvisionControl = false,
    .remoteProvisionControl = false
};

sem_t debugSemapthore;
/***************************************************************************************************
 * Implementation
 **************************************************************************************************/


/**
 * @brief Handles Ctrl+C signal. Helps exit app gracefully.
 */
static void CtrlCHandler(int signal)
{
    LOG(LOG_INFO, "Exit triggered...");
    _KeepRunning = false;
}

static bool ReadConfigFile(const char *filePath)
{
    config_init(&_Cfg);
    if(! config_read_file(&_Cfg, filePath))
    {
        LOG(LOG_ERR, "Failed to open config file at path : %s", filePath);
        return false;
    }

    if (_PDConfig.bootstrapUri == NULL)
    {
        if(!config_lookup_string(&_Cfg, "BOOTSTRAP_URI", &_PDConfig.bootstrapUri))
        {
            LOG(LOG_ERR, "Config file does not contain BOOTSTRAP_URI property, using default:%s", CONFIG_BOOTSTRAP_URI);
            _PDConfig.bootstrapUri = CONFIG_BOOTSTRAP_URI;
        }
    }

    if (_PDConfig.defaultRouteUri == NULL)
    {
        if(!config_lookup_string(&_Cfg, "DEFAULT_ROUTE_URI", &_PDConfig.defaultRouteUri))
        {
            LOG(LOG_ERR, "Config file does not contain DEFAULT_ROUTE_URI property");
            return false;
        }
    }

    if (_PDConfig.dnsServer == NULL)
    {
        if(!config_lookup_string(&_Cfg, "DNS_SERVER", &_PDConfig.dnsServer))
        {
            LOG(LOG_ERR, "Config file does not contain DNS_SERVER property.");
            return false;
        }
    }

    if (_PDConfig.endPointNamePattern == NULL)
    {
        if(!config_lookup_string(&_Cfg, "ENDPOINT_NAME_PATTERN", &_PDConfig.endPointNamePattern))
        {
            LOG(LOG_ERR, "Config file does not contain ENDPOINT_NAME_PATTERN property, using default:%s.",
                    CONFIG_DEFAULT_ENDPOINT_PATTERN);
            _PDConfig.endPointNamePattern = CONFIG_DEFAULT_ENDPOINT_PATTERN;
        }
    }

    if (_PDConfig.logLevel == 0)
    {
        if(!config_lookup_int(&_Cfg, "LOG_LEVEL", &_PDConfig.logLevel))
        {
            LOG(LOG_ERR, "Config file does not contain LOG_LEVEL property. Using default value: Warning");
            _PDConfig.logLevel = LOG_WARN;
        }
    }

    if (_PDConfig.tcpPort == 0)
    {
        if(!config_lookup_int(&_Cfg, "PORT", &_PDConfig.tcpPort))
        {
            LOG(LOG_ERR, "Config file does not contain PORT property, using default: %d", CONFIG_DEFUALT_TCP_PORT);
            _PDConfig.tcpPort = CONFIG_DEFUALT_TCP_PORT;
            return false;
        }
    }

    if (_PDConfig.localProvisionControl == false)
    {
        if(!config_lookup_bool(&_Cfg, "LOCAL_PROVISION_CTRL", &_PDConfig.localProvisionControl))
        {
            LOG(LOG_ERR, "Config file does not contain LOCAL_PROVISION_CTRL property, using default: %d",
                    CONFIG_DEFAULT_LOCAL_PROV_CTRL);
            _PDConfig.localProvisionControl = CONFIG_DEFAULT_LOCAL_PROV_CTRL;
        }
    }

    if (_PDConfig.remoteProvisionControl == false)
    {
        if(!config_lookup_bool(&_Cfg, "REMOTE_PROVISION_CTRL", &_PDConfig.remoteProvisionControl))
        {
            LOG(LOG_ERR, "Config file does not contain REMOTE_PROVISION_CTRL property, using default: %d",
                    CONFIG_DEFAULT_REMOTE_PROV_CTRL);
            _PDConfig.remoteProvisionControl = CONFIG_DEFAULT_REMOTE_PROV_CTRL;
        }
    }

    return true;
}

static void Daemonise(void)
{
    pid_t pid;

    // fork off the parent process
    pid = fork();

    if (pid < 0)
    {
        LOG(LOG_ERR, "Failed to start daemon\n");
        exit(-1);
    }

    if (pid > 0)
    {
        LOG(LOG_DBG, "Daemon running as %d\n", pid);
        exit(0);
    }

    umask(0);

    // close off standard file descriptors
    close (STDIN_FILENO);
    close (STDOUT_FILENO);
    close (STDERR_FILENO);
}

static int ParseCommandArgs(int argc, char *argv[], const char **fptr)
{
    int opt, tmp;
    opterr = 0;
    char *configFilePath = NULL;

    while (1)
    {
        opt = getopt(argc, argv, "v:c:l:dr");
        if (opt == -1)
            break;

        switch (opt)
        {
            case 'v':
                tmp = (unsigned int)strtoul(optarg, NULL, 0);
                if (tmp >= LOG_FATAL && tmp <= LOG_DBG)
                {
                    g_debugLevel = tmp;
                }
                else
                {
                    LOG(LOG_ERR, "Invalid debug level");
                    return -1;
                }
                break;

            case 'c':
                configFilePath = malloc(strlen(optarg));
                sprintf(configFilePath, "%s", optarg);
                break;

            case 'l':
                *fptr = optarg;
                break;

            case 'd':
                Daemonise();
                break;

            case 'r':
                _PDConfig.remoteProvisionControl = true;
                break;

            default:
                return -1;
        }
    }

    if (configFilePath == NULL)
    {
        configFilePath = malloc(strlen(DEFAULT_PATH_CONFIG_FILE));
        sprintf(configFilePath, "%s", DEFAULT_PATH_CONFIG_FILE);
    }

    if (ReadConfigFile(configFilePath) == false)
        return -1;

    if (configFilePath != NULL)
        free(configFilePath);

    return 1;
}

void CleanupOnExit(void)
{
    ubusagent_Close();
    bi_releaseConst();
    controls_shutdown();

    config_destroy(&_Cfg);
    sem_destroy(&debugSemapthore);
    history_destroy();
}

int main(int argc, char **argv)
{
    sem_init(&debugSemapthore, 0, 1);
    int ret;
    const char *fptr = NULL;
    FILE *logFile;
    if ((ret = ParseCommandArgs(argc, argv, &fptr)) < 0)
    {
        LOG(LOG_ERR, "Invalid command args");
        return -1;
    }

    if (fptr)
    {
        logFile = fopen(fptr, "w");

        if (logFile != NULL)
            g_debugStream  = logFile;
        else
            LOG(LOG_ERR, "Failed to create or open %s file", fptr);
    }

    event_Init();

    g_debugLevel = _PDConfig.logLevel;

    struct sigaction action = { .sa_handler = CtrlCHandler, .sa_flags = 0 };
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);

    srand(time(NULL));
    bi_generateConst();
    history_init();
    controls_init(_PDConfig.localProvisionControl != 0);
    clicker_Init();

    if (ubusagent_Init() == false)
    {
        LOG(LOG_ERR, "Unable to register to uBus!");
        CleanupOnExit();
        return -1;
    }
    if (_PDConfig.remoteProvisionControl)
    {
        LOG(LOG_INFO, "Enabling provision control through uBus.");
        if (ubusagent_EnableRemoteControl() == false)
            LOG(LOG_ERR, "Problems with uBus, remote control is disabled!");
    }
    con_BindAndListen(_PDConfig.tcpPort);
    LOG(LOG_INFO, "Entering main loop");
    while(_KeepRunning)
    {
        unsigned long loopStartTime = GetCurrentTimeMillis();

        con_ProcessConnections();
        controls_Update();

        //---- EVENT LOOP ----
        while(true) {
            Event* event = event_PopEvent();

            if (event == NULL) {
                break;
            }

            //order of consumers DO MATTER !
            con_ConsumeEvent(event);
            clicker_ConsumeEvent(event);
            controls_ConsumeEvent(event);
            clicker_sm_ConsumeEvent(event);
            history_ConsumeEvent(event);

            event_releaseEvent(&event);
        }
        //-----------------

        if (_Mode == pd_Mode_ERROR && loopStartTime - _ModeErrorTime > 5000)
        {
            _Mode = pd_Mode_LISTENING;
            _ModeChanged = 1;
        }

        // disconnect already provisioned clicker after 3s timeout, timeout is important as if we disconnect clicker to early it may try to reconnect
/*TODO: Perform disconnection after timeout
        if (_SelectedClicker != NULL && _SelectedClicker->provisionTime > 0)
        {
            if (loopStartTime - _SelectedClicker->provisionTime > 3000)
                con_Disconnect(_SelectedClicker->clickerID);
        }
*/
        unsigned long loopEndTime = GetCurrentTimeMillis();
        if (loopEndTime - loopStartTime < 50)
            usleep(1000*(50-(loopEndTime-loopStartTime)));
    }

    CleanupOnExit();

    return 0;
}
