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

#include <stdio.h>
#include "dsMgrInternal.h"

#ifdef FTUE_CHECK_ENABLED
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <string>
//#include <iconv.h>
#include <sqlite3.h>
#include <fstream>
#include <glib.h>

static const char *ftue_flag_file = "/opt/persistent/ftue_complete.flag";
#endif

#ifdef FTUE_CHECK_ENABLED
#if 0
static bool convert_text_encoding(char * input, std::string &output, size_t input_size)
{
    bool ret = true;
    iconv_t conv = iconv_open("UTF-8", "UTF-16LE");
    if((size_t)-1 == (size_t)conv)
    {
        INT_DEBUG("%s: there was an error.\n", __func__);
        return false;
    }

    char output_buffer[1024] = {0};
    char * output_ptr = &output_buffer[0];
    size_t out_bytes_left = sizeof(output_buffer);
    size_t result = iconv(conv, &input, &input_size, &output_ptr, &out_bytes_left);
    if((size_t)-1 == result)
    {
        INT_DEBUG("%s: conversion failed.\n", __func__);
        ret = false;
    }

    iconv_close(conv);
    output = output_buffer;
    return true;

}
#endif

static bool convert_text_encoding2(const char *input, std::string &output, size_t input_length) //Caller must free output if successful
{
    /*
        This is a limited convertor to translate UTF-16 to ASCII. All non-ASCII characters
        will be skipped in the conversion.
    */
    static const unsigned int MAX_BUFFER_SIZE = 1024;
    bool ret = false;

    if (0 != (input_length % 2))
    {
        INT_DEBUG("%s: Input buffer length is not a multiple of 2. This cannot be UTF-16.\n", __func__);
        return ret;
    }
    //Allocate enough memory to hold convered output.
    size_t output_length = input_length / 2;
    if (output_length > MAX_BUFFER_SIZE)
    {
        //Incoming buffer is too big for our limits. Truncate processing at max supported size.
        output_length = MAX_BUFFER_SIZE;
        input_length = output_length * 2;
    }

    char *output_buffer = static_cast<char *>(malloc(output_length + 1)); //extra byte for string terminator
    if (nullptr == output_buffer)
    {
        INT_DEBUG("%s: Failed to allocate memory.\n", __func__);
        return ret;
    }
    output_buffer[output_length] = 0; //String terminator.

    for (int i = 0, j = 0; i < input_length; i += 2)
    {
        char ch = input[i];
        if ((0 == input[i + 1]) && (0x20 <= ch) && (0x7e >= ch)) //Verifies that the UTF-16 character is within ASCII range.
            output_buffer[j++] = ch;                             //Assuming endianness ab 00 cd 00
    }
    output = output_buffer;
    free(output_buffer);
    return true;
}

static int get_last_station(const std::string &input)
{
    const char *search_pattern = R"(lastStation":)";
    int ret = -1;
    std::size_t start = input.find(search_pattern);
    if (std::string::npos != start)
    {
        start += strlen(search_pattern);
        std::size_t stop = input.find(",", start);
        if (std::string::npos != stop)
        {
            std::string station_number_text = input.substr(start, (stop - start));
            try
            {
                ret = std::stoi(station_number_text);
            }
            catch (...)
            {
                INT_DEBUG("%s: Exception when converting to number.\n", __func__);
                ret = -1;
            }
        }
        else
            INT_DEBUG("%s: Could not locate end of string\n", __func__);
    }
    else
        INT_DEBUG("%s: Search pattern <%s> not found\n", __func__, search_pattern);
    return ret;
}

static bool set_ftue_status_from_ra_store()
{
    const char *resident_app_local_storage = "/opt/persistent/rdkservices/ResidentApp/wpe/local-storage/http_platco.thor.local_50050.localstorage";
    sqlite3 *db;
    const char *query = R"(select * from ItemTable where key = "ftue";)";
    sqlite3_stmt *prepared_statement = nullptr;
    static const int FTUE_DONE_STATION_NUMBER = 14;
    bool retStatus = false;

    INT_DEBUG("%s: start.\n", __func__);
    int ret = sqlite3_open_v2(resident_app_local_storage, &db, SQLITE_OPEN_READWRITE, NULL);
    if (0 != ret)
    {
        INT_DEBUG("%s: Can't open database: %s\n", __func__, sqlite3_errmsg(db));
        goto clean_up_db;
    }
    ret = sqlite3_prepare_v2(db, query, -1, &prepared_statement, nullptr);
    if (SQLITE_OK != ret)
    {
        INT_DEBUG("%s: Prepare failed: %s\n", __func__, sqlite3_errmsg(db));
        goto clean_up_db;
    }

    do
    {
        ret = sqlite3_step(prepared_statement);
        switch (ret)
        {
        case SQLITE_ROW:
            {
                //The data we need is in JSON form (blob) in the second column. Read as text for convenience.
                const char *second_column = static_cast<const char *>(sqlite3_column_blob(prepared_statement, 1));
                int data_size = sqlite3_column_bytes(prepared_statement, 1);

                std::string converted_text;
                if (true == convert_text_encoding2(second_column, converted_text, data_size))
                {
                    int station = get_last_station(converted_text);
                    INT_DEBUG("%s: station number %d\n", __func__, station);
                    if (FTUE_DONE_STATION_NUMBER == station)
                    {
                        INT_DEBUG("%s: detected FTUE complete.\n", __func__);
                        std::ofstream flag(ftue_flag_file);
                        if (false == flag.is_open())
                            INT_DEBUG("%s: Error creating file %s\n", __func__, ftue_flag_file);
                        retStatus = true;
                    }
                    ret = SQLITE_DONE; //To break out of do-while loop.
                }
                break;
            }

        case SQLITE_DONE:
            break;

        default:
            INT_DEBUG("Step returned unhandled error %d\n", ret);
        }
    } while (SQLITE_ROW == ret);

    sqlite3_finalize(prepared_statement);
clean_up_db:
    sqlite3_close(db);
    return retStatus;
    INT_DEBUG("%s: exit.\n", __func__);
}
#endif // FTUE_CHECK_ENABLED

bool isTVOperatingInFactory()
{
#if (defined(FTUE_CHECK_ENABLED) && !defined(DSMGR_LOGGER_ENABLED))
    bool ret = true;
    if (0 == access(ftue_flag_file, F_OK))
        ret = false;
    else
    {
        /*FTUE was not completed according to last known data. Implications are two-fold
            1. It's safer to assume that we're in factory right now.
            2. Launch a deferred job to check FTUE status and touch the flag if complete.
        */
        static const auto CALLBACK_DELAY_SECONDS = 3;
        auto deferred_callback = [](gpointer data)
        {
            bool retFtueStatus  = set_ftue_status_from_ra_store();
            INT_DEBUG("%s: %s\n", __func__, (true == retFtueStatus ? "true" : "false"));
            return (retFtueStatus)?G_SOURCE_REMOVE:G_SOURCE_CONTINUE;
        };
        g_timeout_add_seconds(CALLBACK_DELAY_SECONDS, deferred_callback, nullptr);
    }
    INT_DEBUG("%s: %s\n", __func__, (true == ret ? "true" : "false"));
    return ret;
#else
    INT_DEBUG("%s: Warning! This is an unhandled control flow.\n", __func__);
    return false; //Dummy
#endif
}
