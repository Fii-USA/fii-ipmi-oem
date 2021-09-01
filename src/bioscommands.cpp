/********************************************************************************
*                       HON HAI Precision IND.Co., LTD.                         *
*            Personal Computer & Enterprise Product Business Group              *
*                      Enterprise Product Business Gro:qup                      *
*                                                                               *
*     Copyright (c) 2010 by FOXCONN/CESBG/CABG/SRD. All rights reserved.        *
*     All data and information contained in this document is confidential       *
*     and proprietary information of FOXCONN/CESBG/CABG/SRD and all rights      *
*     are reserved. By accepting this material the recipient agrees that        *
*     the information contained therein is held in confidence and in trust      *
*     and will not be used, copied, reproduced in whole or in part, nor its     *
*     contents revealed in any manner to others without the express written     *
*     permission of FOXCONN/CESBG/CABG/SRD.                                     *
*                                                                               *
********************************************************************************/

#include <common.hpp>
#include <bioscommands.hpp>
#include "sys_file_impl.hpp"
#include <memory>
#include <unistd.h>
#include <boost/endian/arithmetic.hpp>
#include <string.h>

namespace ipmi
{
    static void registerBIOSFunctions() __attribute__((constructor));

    ipmi::RspType<std::vector<uint8_t>> FiiBIOSBootCount(boost::asio::yield_context yield, std::vector<uint8_t> reqParams)
    {
        uint8_t op;
        std::vector<uint8_t> boot_count;
        int boot_check;
        std::vector<uint8_t>clear_count;

        //std::vector<uint8_t> reset_value;
        uint32_t counter = 0, ret;

        if (reqParams.empty())
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(" Fii bios cmd : command format error.");

            return ipmi::responseReqDataLenInvalid();
        }

        op = reqParams[0] & 0b11;
        std::cout << "Araara " << "op is " << (int)op << std::endl;
        //create a object for the SysFileImpl with its path and offset
        auto file = std::make_unique<binstore::SysFileImpl>("/sys/bus/i2c/devices/4-0050/eeprom",
                                                            4096);
        boost::endian::little_uint8_t checker = 0;
        file->readToBuf(4, sizeof(checker), reinterpret_cast<char*>(&checker));
        std::cout << "Araara " << "checker " << (int)checker << std::endl;
        boot_check = (int)checker; 
        std::cout << "Araara " << "boot_check is " << boot_check << std::endl;
        std::cout << "Araara " << "boot_check is " << (int)boot_check << std::endl;

        if(boot_check == 255 && op == OP_CODE_READ)
        {
            //boot_count.clear();
            boot_count.push_back((uint8_t)0);
            boot_count.push_back((uint8_t)0);
            boot_count.push_back((uint8_t)0);
            boot_count.push_back((uint8_t)0);
            return ipmi::responseSuccess(boot_count);
        }

        /*boost::endian::little_uint32_t size = 0;
        std::string readStr=file->readAsStr(0,sizeof(size));

        char readStrCh[readStr.length()];
        for (int i=0;i<sizeof(readStrCh);i++)
        {
            readStrCh[i] = readStr[i];
            readStrCh[i] = (uint8_t)readStrCh[i];
            boot_count.push_back(readStrCh[i]);
        }*/

        else if (op == OP_CODE_READ && boot_check == 1)
        {
            boost::endian::little_uint32_t size = 0;
            std::string readStr=file->readAsStr(0,sizeof(size));

            char readStrCh[readStr.length()];
            for (int i=0;i<sizeof(readStrCh);i++)
            {
                readStrCh[i] = readStr[i];
                readStrCh[i] = (uint8_t)readStrCh[i];
                boot_count.push_back(readStrCh[i]);
            }
            return ipmi::responseSuccess(boot_count);
        }
        else if (op == OP_CODE_CLEAR)
        {
            clear_count.push_back((uint8_t)255);
            clear_count.push_back((uint8_t)255);
            clear_count.push_back((uint8_t)255);
            clear_count.push_back((uint8_t)255);
            std::vector<uint8_t>clear_check;
            clear_check.push_back(255);
            for(int i=0; i<sizeof(clear_check); i++)
            {
                std::cout << "Araara " << "clear_check " << clear_check[i] << std::endl;
            }
            std::string clear_string(clear_check.begin(),clear_check.end());
            std::cout << "Araara " << "clear_string is " << clear_string << std::endl;
            std::cout << "Araara " << "clear_string is size of " << sizeof(clear_string) << std::endl;
            std::cout << "Araara " << "clear_string is length " << clear_string.length() << std::endl;
            std::cout << "Araara " << "clear_string is size " << clear_string.size() << std::endl;
            for(int i=0; i<sizeof(clear_count); i++)
            {
                std::cout << "Araara " << "clear_count is " << i << " " << (int)clear_count[i] << std::endl;
            }
            std::string default_set(clear_count.begin(),clear_count.end());
            file->writeStr(default_set,0);
            file->writeStr(clear_string,4);
            return ipmi::responseSuccess(clear_count);

        }
        else if (op == OP_CODE_WRITE && boot_check == 255)
        {
            boost::endian::little_uint32_t sized = 0;
            std::string readString=file->readAsStr(0,sizeof(sized));

            char readStringCh[readString.length()];
            for (int i=0;i<sizeof(readStringCh);i++)
            {
                readStringCh[i] = readString[i];
                readStringCh[i] = (uint8_t)readStringCh[i];
                boot_count.push_back(readStringCh[i]);
            }
            uint32_t value = 0;
            std::vector<uint8_t>set_write;
            set_write.push_back(1);
            std::string set_check(set_write.begin(),set_write.end());
            if (reqParams.size() == 1)
            {
                boot_count.clear();
                boot_count.push_back(1);
                boot_count.push_back(0);
                boot_count.push_back(0);
                boot_count.push_back(0);
                std::string write_check(boot_count.begin(),boot_count.end());
                file->writeStr(write_check,0);
                file->writeStr(set_check,4);
            }
            else if (reqParams.size() == FII_CMD_BIOS_BOOT_COUNT_LEN)
            {
                value = reqParams[1] + + (reqParams[2] << 8) + (reqParams[3] << 16) + (reqParams[4] << 24);
                boot_count.clear();
                boot_count.insert(boot_count.begin(), reqParams.begin()+1, reqParams.end());
                std::string s(boot_count.begin(),boot_count.end());
                file->writeStr(s,0);
                file->writeStr(set_check,4);
            }
        }
        else if (op == OP_CODE_WRITE && boot_check == 1)
        {
            boost::endian::little_uint32_t sized = 0;
            std::string readString=file->readAsStr(0,sizeof(sized));

            char readStringCh[readString.length()];
            for (int i=0;i<sizeof(readStringCh);i++)
            {
                readStringCh[i] = readString[i];
                readStringCh[i] = (uint8_t)readStringCh[i];
                boot_count.push_back(readStringCh[i]);
            }
            uint32_t value = 0;
            if (reqParams.size() == 1)
            {
                value = boot_count[0] + (boot_count[1] << 8) + (boot_count[2] << 16) + (boot_count[3] << 24);
                value += 1;
                boot_count.clear();
                boot_count.push_back(static_cast<uint8_t>(value));
                boot_count.push_back(static_cast<uint8_t>(value >> 8));
                boot_count.push_back(static_cast<uint8_t>(value >> 16));
                boot_count.push_back(static_cast<uint8_t>(value >> 24));
            }
            else if (reqParams.size() == FII_CMD_BIOS_BOOT_COUNT_LEN)
            {
                value = reqParams[1] + + (reqParams[2] << 8) + (reqParams[3] << 16) + (reqParams[4] << 24);
                boot_count.clear();
                boot_count.insert(boot_count.begin(), reqParams.begin()+1, reqParams.end());
            }

            //convert the boot_count vector from uint8 to string
            std::string s(boot_count.begin(),boot_count.end());
            //write the data into EEPROM
            file->writeStr(s,0);
        }
        else
        {
            return ipmi::responseInvalidCommand();
        }
        return ipmi::responseSuccess(boot_count);
    }

    void registerBIOSFunctions()
    {
        std::fprintf(stderr, "Registering OEM:[0x34], Cmd:[%#04X] for Fii BIOS OEM Commands\n", FII_CMD_BIOS_BOOT_COUNT);
        ipmi::registerHandler(ipmi::prioOemBase, ipmi::netFnOemThree, FII_CMD_BIOS_BOOT_COUNT, ipmi::Privilege::User,
                FiiBIOSBootCount);

        return;
    }
}

