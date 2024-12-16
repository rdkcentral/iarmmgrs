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
 * utils.cpp
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
#include <utils.h>

using namespace std;

bool _fileExists(string fileName)
{
	struct stat buffer;
	return (stat(fileName.c_str(), &buffer) == 0);
}

bool _folderExists(string folderName)
{
	struct stat buffer;
	if (stat(folderName.c_str(), &buffer) == 0)
	{
		if (buffer.st_mode & __S_IFDIR != 0)
		{
			return true;
		}
	}
	return false;
}

vector<string> *getdir (string dir)
{
    DIR *dp;
    struct dirent *dirp;
    vector<string> *files=new vector<string>();
    if((dp  = opendir(dir.c_str())) == NULL) {
        return files;
    }

    while ((dirp = readdir(dp)) != NULL) {
        files->push_back(string(dirp->d_name));
    }
    closedir(dp);
    return files;
}





/** @} */
/** @} */
