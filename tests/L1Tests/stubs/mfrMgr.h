/*
 * Stub: minimal mfrMgr.h for L1 unit tests.
 *
 * Delegates type definitions to the testframework's mfrTypes.h (which is on
 * the include path via -I$(TESTFRAMEWORK_DIR)/Tests/mocks). This avoids
 * conflicting typedef errors when Iarm.h also pulls in mfrTypes.h.
 *
 * Only the bus-level defines and the struct that sysMgr.c actually uses
 * (IARM_Bus_MFRLib_GetSerializedData_Param_t) are added here.
 */
#ifndef _MFR_MGR_STUB_H_
#define _MFR_MGR_STUB_H_

#include "mfrTypes.h"   /* testframework — provides mfrSerializedType_t etc. */
#include "libIARM.h"    /* provides IARM_Result_t                             */

#define IARM_BUS_MFRLIB_NAME                  "MFRLib"
#define IARM_BUS_MFRLIB_API_GetSerializedData "mfrGetManufacturerData"

#define MAX_SERIALIZED_BUF 2048
#define MAX_BUF            255

typedef struct _IARM_Bus_MFRLib_GetSerializedData_Param_t {
    mfrSerializedType_t type;
    char buffer[MAX_SERIALIZED_BUF];
    int  bufLen;
} IARM_Bus_MFRLib_GetSerializedData_Param_t;

#endif /* _MFR_MGR_STUB_H_ */
