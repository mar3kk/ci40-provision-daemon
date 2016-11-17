/************************************************************************************************************************
 Copyright (c) 2016, Imagination Technologies Limited and/or its affiliated group companies.
 All rights reserved.
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 following conditions are met:
     1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
        following disclaimer.
     2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
        following disclaimer in the documentation and/or other materials provided with the distribution.
     3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
        products derived from this software without specific prior written permission.
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
 USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************************************************************/

#ifndef PROVISIONING_DAEMON_H_
#define PROVISIONING_DAEMON_H_

#include <stdbool.h>

/**
 * @brief Gets current id of selected clicker. If no clicker is selected -1 is returned.
 * @return id of selected clicker
 */
int pd_GetSelectedClickerId();

/**
 * @brief Request start of provision on currently selected clicker.
 * @return true if provision was started, otherwise false
 */
bool pd_StartProvision();

/**
 * @brief Tries to change selection of clicker.
 * @return id of newly selected clicker
 */
int pd_SetSelectedClicker(int clickerId);

#endif /* PROVISIONING_DAEMON_H_ */
