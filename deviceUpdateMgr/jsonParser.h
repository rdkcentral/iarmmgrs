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


/**
* @defgroup iarmmgrs
* @{
* @defgroup deviceUpdateMgr
* @{
**/


#if !defined(VREX_JSON_PARSER_H)
#define VREX_JSON_PARSER_H

#include <map>
#include <string>
#include <utility>
#include <stack>
#include <list>

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

using namespace std;

class JSONParser
{



public:
	struct varVal{
		string str;
		list<varVal *> *array;
		bool boolean;
	};

	JSONParser();
    virtual ~JSONParser();
    map<string, varVal *> parse(const unsigned char *);

    void newKey(string keyName) { m_curKey = keyName; }
    void newString(string value) {
    	varVal *vv=new varVal();
    	vv->str=value;
    	if(m_array!=NULL){
    		m_array->push_back(vv);
    	}else{
    		m_dict[m_curKey] = vv;
    	}

    }
    void newNull() { m_dict[m_curKey] = new varVal(); }
    void newBool(bool value) {
    	varVal *vv=new varVal();
    	vv->boolean=value;
    	if(m_array!=NULL){
    		m_array->push_back(vv);
    	}else{
    		m_dict[m_curKey] = vv;
    	}
    }

    void newArray(){
       		varVal *vv=new varVal();
        	vv->array=new list<varVal *>;
        	if(m_array!=NULL){
        		m_array->push_back(vv);
        		m_arrays.push(m_array);
        		m_array=vv->array;
        	}else{
        		m_dict[m_curKey] = vv;
        		m_array=vv->array;
        	}
    }

    void endArray(){
    	if(m_arrays.empty()==false){
    		m_array=m_arrays.top();
    		m_arrays.pop();
    	}
    	else{
    		m_array=NULL;
    	}
    }

private:
    string m_curKey;
    map<string, varVal *> m_dict;
    stack< list<varVal *> *> m_arrays;
    list<varVal *> *m_array;

};

#endif


/** @} */
/** @} */
