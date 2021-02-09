/*
// Copyright (c) 2020 Foxconn Corporation
//
//
*/

#include <ipmid/api.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/message/types.hpp>
#include <systemcommands.hpp>
#include <sys/types.h>
#include <variant>

using variant = std::variant<std::vector<std::string>>;
using VariantType = std::variant<bool,std::string, uint64_t, uint32_t>;
using propertyType = boost::container::flat_map<std::string, VariantType>;
using retObj = std::pair<std::string, std::vector<std::string>>;
using objType = std::vector<retObj>;
using json = nlohmann::json;

namespace ipmi
{
    static void registerSYSFunctions() __attribute__((constructor));


    /**
    * Obtains the BMC version from the version.json file found in the
    * etc/foxconn/ folder
    * 
    * @arg rspVec - vector used for the response of the ipmi command
    */ 
    void getBMCVersion(std::vector<uint8_t> *rspVec)
    {
        //Read and Transfer the version json
        std::ifstream ifs(versionPath);
        json versionJS = json::parse(ifs);

        std::string version = versionJS["firmwareVersion"]; //get BMC version from JSON

        if(version.empty())
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                        "Error firmwareVersion came back null"
                        );
            ipmi::responseUnspecifiedError();
        }
        else
        {
            //Convert version string to byte elements
            for(size_t i = 0; version.size()>i; i+=2){
                rspVec->push_back(std::stoi(version.substr(i,2),NULL, 16));
            }
        }
    }

    /**
    * Performs sdbus calls to obtain the version from Software.Versions objects
    * in the object mapper
    * 
    * @arg yield - The yield context for yield method calls
    * @arg fwVersionPurpose - Software.Version Purpose
    * @arg rspVec - vector used for the response of the ipmi command
    */ 
    void getFWVersion(boost::asio::yield_context yield, std::string fwVersionPurpose, 
        std::vector<uint8_t> *rspVec)
    {
        std::vector<uint8_t> versionvec;
        auto dbus = getSdBus();
        boost::system::error_code ec;
        std::vector<std::string> endpoints;

        //Dbus call to get endpoints
        variant endpointReply = dbus->yield_method_call<variant>(yield, ec, 
            "xyz.openbmc_project.ObjectMapper", 
            "/xyz/openbmc_project/software/functional",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Association", "endpoints");

        if (ec)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error getting endpoints"
            );
        }

        else
        {   
            endpoints = std::get<std::vector<std::string>>(endpointReply);

            for (auto &fw : endpoints)
            {
                //Get Software object from each endpoint
                auto objectReply = dbus->yield_method_call<objType>(yield, ec, 
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", 
                    "GetObject",fw, 
                    std::vector<std::string>({"xyz.openbmc_project.Software.Activation"}));

                if (ec)
                {
                    phosphor::logging::log<phosphor::logging::level::ERR>(
                        "Error getting object"
                    );
                }
                else if(objectReply.size() >= 1)
                {
                    //get the version properties for current endpoint
                    propertyType propertiesList = dbus->yield_method_call<propertyType>(yield, ec, 
                    objectReply[0].first, fw,
                    "org.freedesktop.DBus.Properties", "GetAll",
                    "xyz.openbmc_project.Software.Version");
                    if (ec)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "Error getting Version Property"
                        );
                    }
                    else
                    {
                        //seach for the purpose to see what device the version is for
                        //For isntance: BMC for openbmc version HOST for bios version
                        boost::container::flat_map<std::string, VariantType>::const_iterator it =
                            propertiesList.find("Purpose");

                        if(it == propertiesList.end())
                        {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "No Property found"
                            ); 
                        }
                        const std::string *swInvPurpose = std::get_if<std::string>(&it->second);

                        if(*swInvPurpose == fwVersionPurpose)
                        {
                            //If Purpose match get version
                            it = propertiesList.find("Version");
                            const std::string *version = std::get_if<std::string>(&it->second);
                            //Get first 3 ints from string for version num
                            char character;
                            uint8_t num;
                            std::stringstream ss(*version);
                            
                            for (size_t i = 0; i < 5; i++)
                            {
                                ss >> character;
                                if(character != '.')
                                {   
                                    num = character - '0';
                                    rspVec->push_back(num);
                                } 
                            }
                            if(rspVec->size() == 0)
                            {
                                phosphor::logging::log<phosphor::logging::level::ERR>(
                                "Error finding firmware version property"
                                );
                            }
                        }
                                        
                    }
                    
                }

            }
        }
    }

    /**
    * Foxconn OEM IPMI Command to configure system hardware.
    * @arg reqParams - parameter arguments for 
    * 			Byte 1		(Device)
    * 						1 - NCSI device
    * 						2 - VGA decice
    * 			Byte 2      (Mode)
    *                       NCSI
    *                       0 - Get current configuration
    *                       1 - Onboard interface
    *                       2 - OCP3.0
    *                       VGA
    *                       0 - Get current configuration
    *                       1 - Rear end VGA port
    *                       2 - Front end VGA port
    * @returns Response Data
    * 			Byte 1      (Completion Code)		
    * 			Byte 2      (Configuration device)
    *                       1 - NCSI
    *                       2 - VGA
    *           Byte 3      (Mode)
    *                       NCSI
    *                       1 - Onboard interface
    *                       2 - OCP3.0
    *                       VGA
    *                       1 - Rear end VGA port
    *                       2 - Front end VGA port
    */  
    ipmi::RspType<std::vector<uint8_t>> hardwareConfig(uint8_t device, uint8_t mode) //WIP
    {
        std::vector<uint8_t> rsp;
        if(device == NCSI)
        { 
            switch(mode)
            {
            case CURRENT_CONFIG:

                break;
                
            case ONBOARD_INTER:

                break;

            case OPC3_0:

                break;
                
            default:

                return ipmi::responseParmOutOfRange();
            }
        }

        else if(device == VGA)
        { 
            switch(mode)
            {
                case CURRENT_CONFIG:

                    break;
                
                case REAR_VGA:

                    break;

                case FRONT_VGA:

                    break;
                
                default:

                    return ipmi::responseParmOutOfRange();
            }
        }

        else
        {

        }
        return ipmi::responseSuccess(rsp);
    }

    /**
    * Foxconn OEM IPMI Command to get Firmware version from target system
    * @arg yield - yield context for yield method calls
    * @arg reqParams - parameter arguments for 
    * 			Byte 1		(Firmware type)
    * 						1 - BMC
    * 						2 - BIOS
    *                       3 - CPLD
    *                       4 - Motherboard
    *                       5 - PSU
    *                       6 - FRU VPD
    * 			Byte 2      (Index)
    *                       CPLD
    *                       1 - MB
    *                       2 - MP
    *                       PSU
    *                       N: index of PUS
    *                       FRU
    *                       N: index of FRU	
    * @returns Response Data
    * 			Byte 1		(Completion Code)
    * 			Byte 2		(Firmware type)
    * 						1 - BMC
    * 						2 - BIOS
    *                       3 - CPLD
    *                       4 - Motherboard
    *                       5 - PSU
    *                       6 - FRU VPD
    *           Byte 3      (Byte number of firmware version)
    *           Byte 4:N    (firmware Version)
    *           Byte N:M    (Version based on type)
    */  
    ipmi::RspType<std::vector<uint8_t>> getFirmwareVersion(boost::asio::yield_context yield, uint8_t firmwareType, uint8_t index)
    {
        
        std::vector<uint8_t> rsp;
        std::vector<uint8_t> versionVec;

        switch (firmwareType)
        {
            case BMC:
            {
                rsp.push_back(BMC);
                //Read BMC firmware version
                getBMCVersion(&versionVec);
                break;
            }
            case BIOS:
            {
                rsp.push_back(BIOS);
                //Read BMC firmware version
                //WIP
                break;
            }
            case CPLD:
            {
                rsp.push_back(CPLD);
                if(index == MB)
                {
                    //Read CPLD at MB
                    //WIP
                }
                else if(index == MP)
                {
                    //Read CPLD at MP
                    //WIP
                }
                else
                {
                    return ipmi::responseParmOutOfRange();
                    //IPMIFailure
                }
                break;
            }
            case MOTHERBOARD:
            {
                rsp.push_back(MOTHERBOARD);
                //Read Motherboard Version
                //WIP
                break;
            }
            case PSU:
            {
                rsp.push_back(PSU);
                //Read PSU Version
                //WIP
                break;
            }
            case FRU_VPD:
            {
                rsp.push_back(FRU_VPD);
                //Read FRU version
                //WIP
                break;    
            }
            case OPENBMC:
            {
                rsp.push_back(OPENBMC);
                //Read BMC firmware version
                getFWVersion(yield, OpenBMCVersionPurpose, &versionVec);
                break;
            }
            default:
            {
                return ipmi::responseParmOutOfRange();
            }  
        }
        rsp.push_back(versionVec.size());
        rsp.insert(rsp.end(), versionVec.begin(), versionVec.end());
        return ipmi::responseSuccess(rsp);  
    }

    void registerSYSFunctions()
    {
        ipmi::registerHandler(ipmi::prioOemBase, ipmi::netFnOemThree, CMD_SYS_HW_CONFIG, ipmi::Privilege::User, 
                            hardwareConfig);
        ipmi::registerHandler(ipmi::prioOemBase, ipmi::netFnOemThree, CMD_SYS_GET_SYS_FW_VER, ipmi::Privilege::User, 
                            getFirmwareVersion);
        return;
    }    

}