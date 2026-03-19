/*
 * Stub implementation of JSONParser for the L1 test build.
 *
 * The real jsonParser.cpp uses the yajl v1 API which is not available on
 * Ubuntu 22.04 (libyajl-dev ships v2 only).  None of the deviceUpdateMgr
 * L1 tests exercise loadConfig() or any JSON parsing path, so a no-op
 * implementation is sufficient to satisfy the linker.
 */

#include "jsonParser.h"

JSONParser::JSONParser()
    : m_array(nullptr)
{
}

JSONParser::~JSONParser()
{
}

map<string, JSONParser::varVal *> JSONParser::parse(const unsigned char * /*input*/)
{
    return map<string, varVal *>();
}
