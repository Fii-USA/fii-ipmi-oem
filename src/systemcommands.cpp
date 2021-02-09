/********************************************************************************
*                       HON HAI Precision IND.Co., LTD.                         *
*            Personal Computer & Enterprise Product Business Group              *
*                      Enterprise Product Business Gro:qup                        *
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

#include <ipmid/api.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>

#include <ipmid/api.hpp>
#include <ipmid/utils.hpp>
#include <ipmid/message.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/message/types.hpp>

#include <systemcommands.hpp>

namespace ipmi
{
static void registerSystemFunctions() __attribute__((constructor));
 
/**
* Command implementations go here.
*
void getPCIEinfo(std::vector<uint8_t> *rspVec)
{
        
    if(PCIeInfo.empty())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
		"Error geting PCIe Info came back null"
                );
        ipmi::responseUnspecifiedError();
    }
    else
    {
	//Convert PCIeInfi string to byte elements
	for (size_t i = 0; PCIeInfo.size()>i; i+=2){
		rspVec->push_back(std::stoi(PCIeInfi.substr(i,2),NULL, 16));
        }
    }
}
*/

ipmi::RspType<std::vector<uint8_t>> FiiSysPCIeInfo(boost::asio::yield_context yield)
{
    std::string PCIeInfo;

    // Read pcie bifurcation information
    // it return two bytes, 1st byte bifurcation, 2nd byte present pin
    PCIeInfo = system("i2cget -y -a -f 26 0x76 0x01 i 2");

    if (PCIeInfo.empty())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
		"Fii system cmd : Error geting PCIe Info came back null"
                );
        ipmi::responseUnspecifiedError();
    }
  
    std::vector<uint8_t> rsp;
    std::vector<uint8_t> PCIeVec;

    return ipmi::responseSuccess(rsp);   
}
    
void registerSystemFunctions()
{

    ipmi::registerHandler(ipmi::prioOemBase, ipmi::netFnOemThree, FII_CMD_SYS_PCIE_INFO, ipmi::Privilege::User,
	FiiSysPCIeInfo);
    
    return;
}
}
