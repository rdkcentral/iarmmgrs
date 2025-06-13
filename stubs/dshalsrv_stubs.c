#include "dsVideoPort.h"
#include "dsDisplay.h"
#include "dsInternal.h"

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <dlfcn.h>
#include "dsError.h"
#include "dsUtl.h"
#include "dsTypes.h"
#include "libIARM.h"
#include "iarmUtil.h"
#include "libIBusDaemon.h"
#include "libIBus.h"

IARM_Result_t _dsGetVideoPort(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsIsDisplayConnected(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetIgnoreEDIDStatus(void* arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsSetResolution(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetResolution(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetEDID(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetEDIDBytes(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetEDIDBytesInfo(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetForceDisable4K(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetAudioPort(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetStereoMode(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsSetStereoMode(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsSetBackgroundColor(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsSetFPState(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetEnablePersist(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetStereoAuto(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsSetStereoAuto(void *arg)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsEnableAudioPort(void *arg)
{
    return IARM_RESULT_SUCCESS;
}


extern "C" {
IARM_Result_t dsMgr_init()
{
    return IARM_RESULT_SUCCESS;
}
}

bool isComponentPortPresent()
{
    return true;
}

profile_t searchRdkProfile(void)
{
    return PROFILE_TV;
}
