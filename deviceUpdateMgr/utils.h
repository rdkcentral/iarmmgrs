/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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
/*
 * utils.h
 *
 *  Created on: Aug 25, 2014
 *      Author: tlemmons
 */



/**
* @defgroup iarmmgrs
* @{
* @defgroup deviceUpdateMgr
* @{
**/


#ifndef UTILS_H_
#define UTILS_H_
#include <assert.h>
#include <iostream>
#include <fstream>
#include <list>
#include  <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <string>
#include <iostream>
using namespace std;


bool _fileExists(string fileName);
bool _folderExists(string folderName);
vector<string> *getdir (string dir);




#endif /* UTILS_H_ */


/** @} */
/** @} */
