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
/* * imgUpdateMgrMain.cpp
 *
 *  Created on: Jul 23, 2014
 *      Author: tlemmons
 */



/**
* @defgroup iarmmgrs
* @{
* @defgroup deviceUpdateMgr
* @{
**/


#ifdef __cplusplus
extern "C"
{
#endif
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
}
#endif

#include "libIBus.h"
#include "iarmUtil.h"
#include "deviceUpdateMgr.h"
#include "deviceUpdateMgrInternal.h"
#include "jsonParser.h"
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
#include <time.h>

#include "safec_lib.h"

extern "C"
{
#ifdef YOCTO_BUILD
#include "secure_wrapper.h"
#endif  
bool loadConfig();
pthread_mutex_t tMutexLock;
IARM_Result_t AcceptUpdate(void *arg);
void sendDownLoadInit(int id);
void sendLoadInit(int id);
bool getEventData(string filename, _IARM_Bus_DeviceUpdate_Announce_t *myData);
void processDeviceFile(string filePath, string);
void processDeviceFolder(string updatePath, string);
IARM_Result_t deviceUpdateStart(void);
void deviceUpdateRun(list<JSONParser::varVal *> *folders);
IARM_Result_t deviceUpdateStop(void);
bool initialized = false;
void _deviceUpdateEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
int nextID = 1;
map<string, JSONParser::varVal *> configuration;

string serverUpdatePath= "/srv/device_update/";
string tempFilePath="/tmp/devUpdate/";
bool interactiveDownload=false;
bool interactiveLoad=false;
int loadDelayType=0;
int loadTimeAfterInactive=0;
int timeToLoad=0;
bool backgroundDownload=true;
int requestedPercentIncrement=10;
int recheckForUpdatesMin=0;
bool loadImageImmediately=false;
bool forceUpdate=false;
int loadBeforeHour=4;
int delayTillAnnounceTimeMin=10; // default is announce after 10 min;
int announceCounter=10; // counter for wait seconds
bool oneAnnouncePerRun=false;




static pthread_mutex_t mapMutex;
}
typedef struct updateInProgress_t
{
	_IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t *acceptParams;
	int downloadPercent;
	bool loadComplete;
	int errorCode;
	string errorMsg;

} updateInProgress_t;

map<int, updateInProgress_t *> *updatesInProgress = new map<int, updateInProgress_t *>();
bool running = true;

#ifdef RDK_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

void logCallback(const char *buff)
{
	INT_LOG("%s",buff);
}
#endif
int main(int argc, char *argv[])
{
	const char* debugConfigFile = NULL;
	int itr = 0;

	while (itr < argc)
	{
		if (strcmp(argv[itr], "--debugconfig") == 0)
		{
			itr++;
			if (itr < argc)
			{
				debugConfigFile = argv[itr];
			}
			else
			{
				break;
			}
		}
		itr++;
	}

#ifdef RDK_LOGGER_ENABLED

	if(rdk_logger_init(debugConfigFile) == 0) b_rdk_logger_enabled = 1;
	IARM_Bus_RegisterForLog(logCallback);

#endif


    time_t tim=time(NULL);
     tm *now=localtime(&tim);
     INT_LOG("Date is %d/%02d/%02d\n", now->tm_year+1900, now->tm_mon+1, now->tm_mday);
     INT_LOG("Time is %02d:%02d\n", now->tm_hour, now->tm_min);


	if (deviceUpdateStart() == IARM_RESULT_SUCCESS)
	{
		loadConfig();
		JSONParser::varVal *folderRef=configuration["deviceFoldersToWatch"];
		if(folderRef!=NULL){
		list<JSONParser::varVal *> *folders = folderRef->array;
		deviceUpdateRun(folders);
		}else{
			INT_LOG("NO FOLDERS IN CONFIG FILE!!!!!!!!!!");
		}
	}
	deviceUpdateStop();
	return 0;
}

IARM_Result_t deviceUpdateStart()
{

	IARM_Result_t status = IARM_RESULT_INVALID_STATE;

	INT_LOG("Entering [%s] - [%s] - disabling io redirect buf\n", __FUNCTION__, IARM_BUS_DEVICE_UPDATE_NAME);
	setvbuf(stdout, NULL, _IOLBF, 0);
	int ret = pthread_mutex_init(&mapMutex, NULL);
        if(ret != 0) {
        INT_LOG(" pthread_mutex_init Error case: %d\n", __LINE__);
        status = IARM_RESULT_INVALID_STATE;  
        }
	if (!initialized)
	{
		IARM_Result_t rc;

		int retval = pthread_mutex_init(&tMutexLock, NULL);
                if(retval != 0) {
                INT_LOG(" pthread_mutex_init Error case: %d\n", __LINE__);
                }
		pthread_mutex_lock(&tMutexLock);
		rc = IARM_Bus_Init(IARM_BUS_DEVICE_UPDATE_NAME);
		INT_LOG("dumMgr:I-ARM IARM_Bus_Init Mgr: %d\n", rc);

		rc = IARM_Bus_Connect();
		INT_LOG("dumMgr:I-ARM IARM_Bus_Connect Mgr: %d\n", rc);

		rc = IARM_Bus_RegisterEvent(IARM_BUS_DEVICE_UPDATE_EVENT_MAX);
		INT_LOG("dumMgr:I-ARM IARM_Bus_RegisterEvent Mgr: %d\n", rc);

		rc = IARM_Bus_RegisterCall( IARM_BUS_DEVICE_UPDATE_API_AcceptUpdate, AcceptUpdate); /* RPC Method Implementation*/
		INT_LOG("dumMgr:I-ARM IARM_BUS_DEVICE_UPDATE_API_AcceptUpdate Mgr: %d\n", rc);

		IARM_Bus_RegisterEventHandler(IARM_BUS_DEVICE_UPDATE_NAME, IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD,
				_deviceUpdateEventHandler);
		IARM_Bus_RegisterEventHandler(IARM_BUS_DEVICE_UPDATE_NAME, IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS,
				_deviceUpdateEventHandler);
		IARM_Bus_RegisterEventHandler(IARM_BUS_DEVICE_UPDATE_NAME, IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_STATUS,
				_deviceUpdateEventHandler);
		IARM_Bus_RegisterEventHandler(IARM_BUS_DEVICE_UPDATE_NAME, IARM_BUS_DEVICE_UPDATE_EVENT_ERROR,
				_deviceUpdateEventHandler);

		initialized = 1;

		pthread_mutex_unlock(&tMutexLock);
		status = IARM_RESULT_SUCCESS;
	}
	else
	{
		__TIMESTAMP();
		INT_LOG("dumMgr: I-ARM Device Update Mgr Error case: %d\n", __LINE__);
		status = IARM_RESULT_INVALID_STATE;
	}
	return status;
}

bool loadConfig()
{
	string confName = "deviceUpdateConfig.json";
	FILE *fp = NULL;
	string filePath;

// Check if file exists in /opt
	filePath = "/opt/" + confName;

	if (!_fileExists(filePath))
	{
		filePath = "/mnt/nfs/env/" + confName;
		if (!_fileExists(filePath))
		{
			filePath = "/etc/" + confName;
			if (!_fileExists(filePath))
			{
				filePath = std::move(confName);
				if (!_fileExists(filePath))
				{
					__TIMESTAMP();
					INT_LOG(
						"I-ARM DevUpdate Mgr: Configuration error. Configuration file(s) missing, no folders to watch!!\n");
					return false;
				}
			}
		}
	}

	__TIMESTAMP();
	INT_LOG("loading config file %s\n", filePath.c_str());
	if ((fp = fopen(filePath.c_str(), "r")) != NULL)
	{
		string confData;
		char buf[2048] = "";  //CID:136517 - string null

		while (!feof(fp) && confData.length() < 65535)
		{
                        memset(buf,'\0', sizeof(buf));                 //CID:136517 - checked null argument
     			if (0 >= fread(buf, 1, sizeof(buf) - 1, fp))   //cID:86017 - checked return
                        {
			    INT_LOG("dumMgr fread failed  \n");
			    break; // Do not continue since READ has failed.
                        }
                        else
                        {
                            confData.append(buf);
                        }
		}

		if (fp)
			fclose(fp);

//		LOG("dumMgr: Configuration Read <%s>\n", confData.c_str());

		JSONParser parser;

		configuration = parser.parse((const unsigned char *) confData.c_str());

//		for (map<string, JSONParser::varVal *>::iterator pos = configuration.begin(); pos != configuration.end(); pos++)
//		{
//			if (pos->second->str.length() > 0)
//			{
////				LOG("config string:  %s=%s\n", pos->first.c_str(), pos->second->str.c_str());
//			}
//			else if (pos->second->array != NULL)
//			{
//				for (list<JSONParser::varVal *>::iterator arrayItr = pos->second->array->begin();
//						arrayItr != pos->second->array->end(); arrayItr++)
//				{
//					if ((*arrayItr)->str.length() > 0)
//						LOG("array:%s string:  %s\n", pos->first.c_str(), (*arrayItr)->str.c_str());
//				}
//			}
//			else
//			{
//				LOG("config string:  %s=%s\n", pos->first.c_str(), pos->second->boolean ? "true" : "false");
//			}
//		}

	}

	if(configuration["serverUpdatePath"]!=NULL)
	{
		serverUpdatePath=configuration["serverUpdatePath"]->str;
	}
	else{
		INT_LOG("error: no serverUpdatePath  in config\n");
	}
	if(configuration["tempFilePath"]!=NULL)
	{
		tempFilePath=configuration["tempFilePath"]->str;
	}
	else{
		INT_LOG("error: no  tempFilePath in config\n");
	}
	if(configuration["forceUpdate"]!=NULL)
	{
		forceUpdate=configuration["forceUpdate"]->boolean;
	}
	else{
		INT_LOG("error: no forceUpdate  in config\n");
	}
	if(configuration["interactiveDownload"]!=NULL)
	{
		interactiveDownload=configuration["interactiveDownload"]->boolean;
	}
	else{
		INT_LOG("error: no interactiveDownload  in config\n");
	}
	if(configuration["interactiveLoad"]!=NULL)
	{
		interactiveLoad=configuration["interactiveLoad"]->boolean;
	}
	else{
		INT_LOG("error: no  interactiveLoad in config\n");
	}
	if(configuration["loadDelayType"]!=NULL)
	{
		loadDelayType=atoi(configuration["loadDelayType"]->str.c_str());
	}
	else{
		INT_LOG("error: no loadDelayType  in config\n");
	}
	if(configuration["loadTimeAfterInactive"]!=NULL)
	{
		loadTimeAfterInactive=atoi(configuration["loadTimeAfterInactive"]->str.c_str());
	}
	else{
		INT_LOG("error: no  loadTimeAfterInactive in config\n");
	}
	if(configuration["timeToLoad"]!=NULL)
	{
		timeToLoad=atoi(configuration["timeToLoad"]->str.c_str());
	}
	else{
		INT_LOG("error: no  timeToLoad in config\n");
	}
	if(configuration["backgroundDownload"]!=NULL)
	{
		backgroundDownload=configuration["backgroundDownload"]->boolean;
		INT_LOG ("setting  backgroundDownload=%s\n",backgroundDownload?"true":"false");
	}
	else{
		INT_LOG("error: no backgroundDownload  in config\n");
	}

	if(configuration["loadImageImmediately"]!=NULL)
	{
		loadImageImmediately=configuration["loadImageImmediately"]->boolean;
		INT_LOG ("setting  loadImageImmediately=%s\n",loadImageImmediately?"true":"false");
	}
	else{
		INT_LOG("error: no loadImageImmediately  in config\n");
	}

	if(configuration["loadBeforeHour"]!=NULL)
	{
		loadBeforeHour=atoi(configuration["loadBeforeHour"]->str.c_str());;
	}
	else{
		INT_LOG("error: no loadBeforeHour  in config\n");
	}

	if(configuration["requestedPercentIncrement"]!=NULL)
	{
		requestedPercentIncrement=atoi(configuration["requestedPercentIncrement"]->str.c_str());
	}
	else{
		INT_LOG("error: no  requestedPercentIncrement in config\n");
	}
	if(configuration["recheckForUpdatesMin"]!=NULL)
	{
		recheckForUpdatesMin=atoi(configuration["recheckForUpdatesMin"]->str.c_str());
	}
   if(configuration["delayTillAnnounceTimeMin"]!=NULL)
    {
    	delayTillAnnounceTimeMin=atoi(configuration["delayTillAnnounceTimeMin"]->str.c_str());
        INT_LOG ("setting delayTillAnnounceTimeMin=%d\n",delayTillAnnounceTimeMin);
    }
    else{
        INT_LOG("warning: no delayTillAnnounceTimeMin in config!\n");
    }
    if(configuration["oneAnnouncePerRun"]!=NULL)
    {
    	oneAnnouncePerRun=configuration["oneAnnouncePerRun"]->boolean;
        INT_LOG ("setting oneAnnouncePerRun=%s\n",oneAnnouncePerRun?"true":"false");
    }
    else{
        INT_LOG("warning: no oneAnnouncePerRun in config!\n");
    }

	__TIMESTAMP();
	INT_LOG("running with config:\n");
	INT_LOG ("   serverUpdatePath= %s\n",serverUpdatePath.c_str());
	INT_LOG ("   tempFilePath=%s\n",tempFilePath.c_str());
	INT_LOG ("   interactiveDownload=%s\n",interactiveDownload?"true":"false");
	INT_LOG ("   interactiveLoad=%s\n",interactiveLoad?"true":"false");
	INT_LOG ("   loadDelayType=%d\n",loadDelayType);
	INT_LOG ("   loadTimeAfterInactive=%d\n",loadTimeAfterInactive);
	INT_LOG ("   timeToLoad=%d\n",timeToLoad);
	INT_LOG ("   backgroundDownload=%s\n",backgroundDownload?"true":"false");
	INT_LOG ("   loadImageImmediately=%s\n",loadImageImmediately?"true":"false");
	INT_LOG ("   loadBeforeHour=%d\n",loadBeforeHour);
	INT_LOG ("   requestedPercentIncrement=%d\n",requestedPercentIncrement);
	INT_LOG ("   recheckForUpdatesMin=%d\n",recheckForUpdatesMin);
   INT_LOG ("   delayTillAnnounceTimeMin=%d\n",delayTillAnnounceTimeMin);
	return true;
}

//bool loadTextConfig()
//{
//	string confName = "deviceUpdateConfig.json";
//	string filePath;
//
//// Check if file exists in /opt
//	filePath = "/opt/" + confName;
//
//	if (!_fileExists(filePath))
//	{
//		filePath = "/mnt/nfs/env/" + confName;
//		if (!_fileExists(filePath))
//		{
//			filePath = confName;
//			if (!_fileExists(filePath))
//			{
//				LOG(
//						"I-ARM DevUpdate Mgr: Configuration error. Configuration file(s) missing, no folders to watch!!\n");
//				return false;
//			}
//		}
//	}
//
//	LOG("loading config file %s\n", filePath.c_str());
//	string line;
//	std::ifstream myfile(filePath.c_str(), ios_base::in);
//	string updatePath;
//// TODO decide on config format.  Currently one device type to watch per line in text file
//	if (myfile.is_open())
//	{
//		while (myfile.good())
//		{
//			getline(myfile, line);
//			if (line.size() > 0)
//			{
//				updatePath = serverUpdatePath + line;
//				LOG("I-ARM DevUpdate Mgr: checking folder location <%s>\n", updatePath.c_str());
//				if (_folderExists(updatePath))
//				{
//					LOG("I-ARM DevUpdate Mgr: processing folder location <%s>\n", updatePath.c_str());
//					processDeviceFolder(updatePath, line);
//
//				}
//			}
//		}
//		myfile.close();
//	}
//	else
//	{
//		LOG("I-ARM DevUpdate Mgr: Configuration error. Unable to open file\n");
//		return false;
//	}
//	return true;
//}

void processDeviceFolder(string updatePath, string deviceName)
{
	DIR *dp;
	struct dirent *dirp;
	INT_LOG("dumMgr:processing device folder:%s\n", updatePath.c_str());
	if ((dp = opendir(updatePath.c_str())) == NULL)
	{
		INT_LOG("dumMgr:Error(%d) opening updatePath \n", errno);
		return;
	}

// loop through dir listing and get normal files
	INT_LOG("dumMgr:checking folder files\n");
	while ((dirp = readdir(dp)) != NULL)
	{
		INT_LOG("dumMgr:checking folder files:%s  - type=%d\n",dirp->d_name,dirp->d_type);
		if (dirp->d_type != DT_DIR)
		{
			// check for it being either a tgz or tar.gz file
			string filename = dirp->d_name;
			INT_LOG("dumMgr:checking file:%s\n", filename.c_str());
			unsigned int idx = filename.rfind('.');
			if (idx != string::npos)
			{
				string ext = filename.substr(idx + 1);
				if (ext == "gz")
				{
					idx = filename.rfind('.', idx);
					if (idx == string::npos)
					{
						continue;
					}
					string ext = filename.substr(idx + 1);
					if (ext == "tar.gz")
					{
						processDeviceFile(updatePath + "/" + filename, deviceName);
						continue;
					}
				}
				if (ext == "tgz")
				{
					processDeviceFile(updatePath + "/" + filename, deviceName);
					continue;
				}
			}

		}
	}
	closedir(dp);
}

void processDeviceFile(string filePath, string deviceName)
{
	INT_LOG("dumMgr:processing Device File:%s\n", filePath.c_str());
	string cmd = "rm -rf ";
	cmd += tempFilePath;
#ifdef YOCTO_BUILD
	v_secure_system(cmd.c_str());
#else
	system(cmd.c_str());
#endif
	cmd = "mkdir ";
	cmd += tempFilePath;
#ifdef YOCTO_BUILD
        v_secure_system(cmd.c_str());
#else
        system(cmd.c_str());
#endif
	cmd = "tar -xzpf  " + filePath + " -C " + tempFilePath;
#ifdef YOCTO_BUILD
        v_secure_system(cmd.c_str());
#else
        system(cmd.c_str());
#endif
	vector<string> *myfiles = getdir(tempFilePath);
	vector<string>::iterator itr;
	if(myfiles != NULL)
	{
		INT_LOG("dumMgr: searching Device File:%s\n", filePath.c_str());
		for (itr = myfiles->begin(); itr != myfiles->end(); itr++)
		{
			string filename = *itr;
	//		LOG("dumMgr:found file:%s\n", filename.c_str());
			int idx = filename.rfind('.');
			if (idx == string::npos)
			{
				continue;
			}
			string ext = filename.substr(idx + 1);
	//		LOG("dumMgr:extension:%s\n", ext.c_str());
			if (ext == "xml")
			{
				_IARM_Bus_DeviceUpdate_Announce_t myData;
				strncpy(myData.deviceImageFilePath, filePath.c_str(), (IARM_BUS_DEVICE_UPDATE_PATH_LENGTH - 1));
				myData.deviceImageFilePath[IARM_BUS_DEVICE_UPDATE_PATH_LENGTH - 1] = 0;
				if (getEventData(tempFilePath + filename, &myData))
				{
					__TIMESTAMP();
					INT_LOG("dumMgr:sending announce event");
					IARM_Result_t retval = IARM_Bus_BroadcastEvent(
					IARM_BUS_DEVICE_UPDATE_NAME, (IARM_EventId_t) IARM_BUS_DEVICE_UPDATE_EVENT_ANNOUNCE, (void *) &myData,
							sizeof(_IARM_Bus_DeviceUpdate_Announce_t));
					if (retval == IARM_RESULT_SUCCESS)
					{
	//					__TIMESTAMP();
	//					LOG("dumMgr:Announce Event sent successfully\n");
					}
					else
					{
						__TIMESTAMP();
						INT_LOG("dumMgr:Announce Event problem, %i\n", retval);
					}
				}
			}
		}
		delete myfiles;
	}
	else
	{
		INT_LOG("dumMgr: Failed in searching Device File:[%s]. getdir returned NULL\n", filePath.c_str());;
	}

}

string getXMLTagText(string xml, string tag)
{

//TODO currently this assume no spaces or tabs in the tag brackets. and no leading trailing spaces in text content
	int idx = xml.find("<" + tag);
	if (idx == string::npos)
	{
		INT_LOG("dumMgr:tag <%s> not found in xml file: aborting\n", tag.c_str());
		return "";
	}
// skip past the tag and its two brackets:
	idx += tag.length() + 2;

// find end tag
	int idx2 = xml.find("</" + tag);

//grab all content between start and end tag
	return xml.substr(idx, idx2 - idx);

}

bool getEventData(string filename, _IARM_Bus_DeviceUpdate_Announce_t *myData)
{
	errno_t rc = -1;
	string fileContents;
	std::ifstream myfile;

	INT_LOG("dumMgr:--------------------Checking tar File %s\n", filename.c_str());
	myfile.open(filename.c_str(), std::ifstream::in);
	if (myfile.is_open())
	{
		myfile.seekg(0, myfile.end);
		int length = myfile.tellg();
		myfile.seekg(0, myfile.beg);
		char * buffer = new char[length + 1];
		if(buffer == NULL)
		{
			return false;
		}
		myfile.read(buffer, length);
		myfile.close();
		fileContents = buffer;
		delete[] buffer;
	}

	string text = getXMLTagText(fileContents, "image:softwareVersion");
	if (text.length() == 0)
	{
		INT_LOG("dumMgr:--------------------Missing Version %s\n", filename.c_str());
		return false;
	}
		rc = strcpy_s(myData->deviceImageVersion,sizeof(myData->deviceImageVersion), std::move(text).c_str());
		if(rc!=EOK)
		{
			ERR_CHK(rc);
		}

	text = getXMLTagText(fileContents, "image:type");
	myData->deviceImageType = atoi(text.c_str());

	text = getXMLTagText(fileContents, "image:productName");
		rc = strcpy_s(myData->deviceName,sizeof(myData->deviceName), std::move(text).c_str());
		if(rc!=EOK)
		{
			ERR_CHK(rc);
		}
//	LOG("dumMgr:version:%s", text.c_str());
//	LOG(" type:%s", text.c_str());
//	LOG("product name:%s\n", text.c_str());

	myData->forceUpdate = forceUpdate;
	INT_LOG("dumMgr:--------------------Done with tar File %s\n", filename.c_str());
	return true;
}

void deviceUpdateRun(list<JSONParser::varVal *> *folders)
{
	int i=0;
	while (running)
	{
		i++;
		sleep(1);
		if(announceCounter==(delayTillAnnounceTimeMin*60)){
			if (folders != NULL)
			{
					int numFolders=folders->size();
				INT_LOG("We have %d folders to watch\n",numFolders);
				if(oneAnnouncePerRun){
					// since TI boxes can only announce one update we space updates on days of month to make
					// sure all types get updated.  This is a temporary hack until control manager takes over from
					// rfMgr
					time_t theTime = time(NULL);
					struct tm *aTime = localtime(&theTime);
					int day = aTime->tm_mday;
					int selectedUpdate=day%(numFolders);
					INT_LOG("today is :%d, so only checking folder %d (mod %d)\n",day,selectedUpdate,numFolders);

					for (list<JSONParser::varVal *>::iterator arrayItr = folders->begin(); arrayItr != folders->end();arrayItr++)
					{
						string updateFolder = std::move((*arrayItr)->str);
						if(selectedUpdate<=0){
							string updatePath = std::move(serverUpdatePath) + std::move(updateFolder);
							INT_LOG("checking folder:%s\n",updatePath.c_str());

							if (updatePath.length() > 0)
							{
								if (_folderExists(updatePath))
								{
									INT_LOG("I-ARM DevUpdate Mgr: processing folder location <%s>\n", updatePath.c_str());
									processDeviceFolder(updatePath, updateFolder);

								}
							}
							break;
						}else{
							INT_LOG("skipping folder:%s\n",updateFolder.c_str());  //CID:127614 - Print args
						}
						selectedUpdate--;
					}
				}
				else
				{
					for (list<JSONParser::varVal *>::iterator arrayItr = folders->begin(); arrayItr != folders->end();arrayItr++)
					{
						string updateFolder = std::move((*arrayItr)->str);
						string updatePath = std::move(serverUpdatePath) + std::move(updateFolder);
						INT_LOG("checking folder:%s\n",updatePath.c_str());

						if (updatePath.length() > 0)
						{
							if (_folderExists(updatePath))
							{
								INT_LOG("I-ARM DevUpdate Mgr: processing folder location <%s>\n", updatePath.c_str());
								processDeviceFolder(updatePath, updateFolder);

							}
						}
					}
				}
				__TIMESTAMP();
				INT_LOG("Done with FOLDERS TO WATCH!!!  \n");

			}
			else
			{
				INT_LOG("ERROR - NO FOLDERS TO WATCH!!!  \n");
			}
		}

	announceCounter++;


		if(i>60){
		if (updatesInProgress->size() > 0)
		{
			__TIMESTAMP();
			INT_LOG("deviceUpdateMgr - updates in progress:\n");
			map<int, updateInProgress_t *>::iterator pos = updatesInProgress->begin();
			while (pos != updatesInProgress->end())
			{
				INT_LOG("     UpdateID:%d deviceID:%d percentDownload:%d Loaded:%d file:%s\n", pos->first,
						pos->second->acceptParams->deviceID, pos->second->downloadPercent, pos->second->loadComplete,
						pos->second->acceptParams->deviceImageFilePath);
				if (pos->second->errorCode > 0)
				{
					INT_LOG("              ERROR on UpdateID:%d Type:%d Message:%s\n", pos->first, pos->second->errorCode,
							pos->second->errorMsg.c_str());
				}
				pos++;
			}

		}
i=0;
		}
			fflush(stdout);
	}
	return;
}

IARM_Result_t AcceptUpdate(void *arg)
{
	errno_t rc = -1;
	__TIMESTAMP();
	INT_LOG("dumMgr:Accept Update recieved\n");
	_IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t *updateParams = new _IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t();
	IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t *param = (IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t *) arg;
	param->updateSessionID = nextID++;
	INT_LOG("dumMgr:update Session ID=%d\n", param->updateSessionID);
	param->interactiveDownload = interactiveDownload;
	param->interactiveLoad = interactiveLoad;
		rc = memcpy_s(updateParams,sizeof(_IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t), param, sizeof(_IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t));
		if(rc!=EOK)
		{
			ERR_CHK(rc);
		}
	updateInProgress_t *uip = new updateInProgress_t();
	uip->acceptParams = updateParams;
	pthread_mutex_lock(&mapMutex);
	updatesInProgress->insert(pair<int, updateInProgress_t *>(param->updateSessionID, uip));
	pthread_mutex_unlock(&mapMutex);

	__TIMESTAMP();
	INT_LOG("dumMgr:Accept Update return success\n");
	return IARM_RESULT_SUCCESS;
}

IARM_Result_t deviceUpdateStop(void)
{
	if (initialized)
	{
		pthread_mutex_lock(&tMutexLock);
		IARM_Bus_UnRegisterEventHandler(IARM_BUS_DEVICE_UPDATE_NAME, IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD);
		IARM_Bus_Disconnect();
		IARM_Bus_Term();
		pthread_mutex_unlock(&tMutexLock);
		pthread_mutex_destroy(&tMutexLock);
		initialized = false;
		return IARM_RESULT_SUCCESS;
	}
	else
	{
		return IARM_RESULT_INVALID_STATE;
	}
}

typedef struct
{
	string mimeType;
	string subType;
	string language;
} AudioInfo;

updateInProgress_t *getUpdateInProgress(int id)
{
	pthread_mutex_lock(&mapMutex);
	updateInProgress_t *uip = updatesInProgress->find(id)->second;
	pthread_mutex_unlock(&mapMutex);
	return uip;

}

void _deviceUpdateEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{

	/*Handle only Device Update Manager Events */
	__TIMESTAMP();
	if (strcmp(owner, IARM_BUS_DEVICE_UPDATE_NAME) == 0)
	{
		pthread_mutex_lock(&tMutexLock);

		switch (eventId)
		{
		case IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD:
		{
			IARM_Bus_DeviceUpdate_ReadyToDownload_t *eventData = (IARM_Bus_DeviceUpdate_ReadyToDownload_t *) data;
			INT_LOG("I-ARM: got event IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD\n");

			updateInProgress_t *uip = getUpdateInProgress(eventData->updateSessionID);

			if (uip != NULL)
			{
				INT_LOG("I-ARM: got valid updateSessionID  Details:\n");
				INT_LOG("I-ARM: 			currentSWVersion:%s\n", eventData->deviceCurrentSWVersion);
				INT_LOG("I-ARM: 			newSWVersion:%s\n", eventData->deviceNewSoftwareVersion);
				INT_LOG("I-ARM: 			deviceHWVersion:%s\n", eventData->deviceHWVersion);
				INT_LOG("I-ARM: 			deviceBootloaderVersion:%s\n", eventData->deviceBootloaderVersion);
				INT_LOG("I-ARM: 			deviceName:%s\n", eventData->deviceName);
				INT_LOG("I-ARM: 			totalSize:%i\n", eventData->totalSize);
				INT_LOG("I-ARM: 			deviceImageType:%i\n", eventData->deviceImageType);

				if(interactiveDownload==false){
					sendDownLoadInit(eventData->updateSessionID);
				}else
				{
					INT_LOG("I-ARM: 	Interactive Download is true so not sending download Init message\n");
				}
			}
			else
			{
				INT_LOG("I-ARM: did not get valid updateSessionID  Got id:%d\n", eventData->updateSessionID);
			}

		}
			break;
		case IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS:
		{
			IARM_Bus_DeviceUpdate_DownloadStatus_t *eventData = (IARM_Bus_DeviceUpdate_DownloadStatus_t *) data;
			INT_LOG("I-ARM: got event IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS\n");
			updateInProgress_t *uip = getUpdateInProgress(eventData->updateSessionID);

			if (uip != NULL)
			{
				INT_LOG("I-ARM: 			percentComplete:%i\n", eventData->percentComplete);

				uip->downloadPercent = eventData->percentComplete;
				if(uip->downloadPercent>=100){
					if(interactiveLoad==false){
						sendLoadInit(eventData->updateSessionID);
					}
					else{
						INT_LOG("I-ARM: 	Interactive load is true so not sending load Init message\n");
					}
				}
				else{
					INT_LOG("I-ARM: 			Not ready to load yet:%i\n", eventData->percentComplete);
				}
			}
			else
			{
				INT_LOG("I-ARM: IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS invalid updateSessionID  Got id:%d\n",
						eventData->updateSessionID);
			}
		}
			break;
		case IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_STATUS:
		{
			_IARM_Bus_DeviceUpdate_LoadStatus_t *eventData = (_IARM_Bus_DeviceUpdate_LoadStatus_t *) data;
			INT_LOG("I-ARM: got event IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_STATUS\n");
			updateInProgress_t *uip = getUpdateInProgress(eventData->updateSessionID);

			if (uip != NULL)
			{
				INT_LOG("I-ARM: 			Complete:%s\n", eventData->loadStatus == 1 ? "true" : "false");
				uip->loadComplete = eventData->loadStatus == 1 ? true : false;
				if (uip->loadComplete == 1)
				{
					INT_LOG("I-ARM:load Complete!!!\n");
					// TODO should remove update from list???
				}

			}
			else
			{
				INT_LOG("I-ARM: IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS invalid updateSessionID  Got id:%d\n",
						eventData->updateSessionID);
			}
		}
			break;
		case IARM_BUS_DEVICE_UPDATE_EVENT_ERROR:
		{
			IARM_Bus_Device_Update_Error_t *eventData = (IARM_Bus_Device_Update_Error_t *) data;
			INT_LOG("I-ARM: got event IARM_BUS_DEVICE_UPDATE_EVENT_ERROR\n");

			updateInProgress_t *uip = getUpdateInProgress(eventData->updateSessionID);

			if (uip != NULL)
			{
				uip->errorCode = eventData->errorType;
				uip->errorMsg.assign((const char *) (eventData->errorMessage));
				INT_LOG("I-ARM: 			errorMessage:%s\n", eventData->errorMessage);
				INT_LOG("I-ARM: 			errorType:%i\n", eventData->errorType);
			}
			else
			{
				INT_LOG("I-ARM: IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS invalid updateSessionID  Got id:%d\n",
						eventData->updateSessionID);
			}
		}
			break;
		default:
			INT_LOG("I-ARM: unknown event type \n");
			break;
		}

		pthread_mutex_unlock(&tMutexLock);
	}
	else
	{
		__TIMESTAMP();
		INT_LOG("I-ARM:_DevUpdateEventHandler event type not meant for me <%s>...\n", owner);
	}
}

void sendDownLoadInit(int id)
{
	_IARM_Bus_DeviceUpdate_DownloadInitiate_t eventData;
	updateInProgress_t *uip = getUpdateInProgress(id);

	memset(&eventData, 0, sizeof(eventData));
	eventData.updateSessionID = id;
	eventData.backgroundDownload = backgroundDownload;
	eventData.requestedPercentIncrement = requestedPercentIncrement;
	eventData.loadImageImmediately=loadImageImmediately;
	IARM_Result_t retval = IARM_Bus_BroadcastEvent(IARM_BUS_DEVICE_UPDATE_NAME,
			(IARM_EventId_t) IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_INITIATE, (void *) &eventData, sizeof(eventData));
	if (retval == IARM_RESULT_SUCCESS)
	{
		__TIMESTAMP();
		INT_LOG("I-ARM:IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_INITIATE Event sent successfully");
	}
	else
	{
		__TIMESTAMP();
		INT_LOG("I-ARM:IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_INITIATE Event problem, %i", retval);
	}
}

void sendLoadInit(int id)
{
	_IARM_Bus_DeviceUpdate_LoadInitiate_t eventData;
	updateInProgress_t *uip = getUpdateInProgress(id);

	memset(&eventData, 0, sizeof(eventData));
	eventData.updateSessionID = id;
	eventData.loadDelayType = (IARM_Bus_Device_Update_LoadDelayType_t)loadDelayType;
	eventData.timeAfterInactive = loadTimeAfterInactive;

	// time to load is set to immediate if we are before the time of day as we normally reboot very early
	// morning and time to load should also be early morning
	// however we do a sanity check so if we reboot later in the day we hold off till load time using time to load
    time_t tim=time(NULL);
     tm *now=localtime(&tim);
     INT_LOG("Time is %02d:%02d\n", now->tm_hour, now->tm_min);



     INT_LOG("diff time is %i hours\n",loadBeforeHour-now->tm_hour);
	if(loadBeforeHour>now->tm_hour){
		// we are before the load time so load immedialy
		eventData.timeToLoad = 0;
		__TIMESTAMP();
		INT_LOG("doing immediate load\n");
	}else
	{
		// we are after the time so load at that time tomorrow
		// 24 hours worth of seconds pluss the negative number of diff time
		eventData.timeToLoad=(24+(loadBeforeHour-now->tm_hour))*60*60;
		__TIMESTAMP();
		INT_LOG("Waiting till tomorrow - delay of %i seconds\n",eventData.timeToLoad);
	}

	// force now
//	eventData.timeToLoad=60;

	IARM_Result_t retval = IARM_Bus_BroadcastEvent(IARM_BUS_DEVICE_UPDATE_NAME,
			(IARM_EventId_t) IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_INITIATE, (void *) &eventData, sizeof(eventData));
	if (retval == IARM_RESULT_SUCCESS)
	{

		__TIMESTAMP();
		INT_LOG("I-ARM:IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_INITIATE Event sent successfully");
	}
	else
	{
		__TIMESTAMP();
		INT_LOG("I-ARM:IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_INITIATE Event problem, %i", retval);
	}
}


/** @} */
/** @} */
