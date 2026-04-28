/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file iarm_sysmgr_stubs.cpp
 * @brief No-op stubs for the small set of IARM symbols not covered by the
 *        testframework's Iarm.cpp.
 *
 * Iarm.cpp (testframework) provides function-pointer definitions for:
 *   Init, Connect, Disconnect, Term, IsConnected, RegisterEventHandler,
 *   UnRegisterEventHandler, RemoveEventHandler, Call, BroadcastEvent,
 *   RegisterCall, Call_with_IPCTimeout.
 *
 * Only these four are missing and may be referenced by sysMgr.c at link time:
 *   RegisterEvent, WritePIDFile, Malloc, Free.
 */

#include "libIBus.h"

extern "C" {

IARM_Result_t IARM_Bus_RegisterEvent(IARM_EventId_t)          { return IARM_RESULT_SUCCESS; }
IARM_Result_t IARM_Bus_WritePIDFile(const char *)              { return IARM_RESULT_SUCCESS; }
IARM_Result_t IARM_Malloc(IARM_MemType_t, size_t, void **)    { return IARM_RESULT_SUCCESS; }
IARM_Result_t IARM_Free(IARM_MemType_t, void *)                { return IARM_RESULT_SUCCESS; }

} /* extern "C" */
