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
#include <utils.hpp>
#include <string>
#include <variant>
#include <iterator>
#include <algorithm>

namespace ipmi
{

static constexpr const char ipmiQueueService[] =
	"xyz.openbmc_project.Ipmi.Host";
static constexpr const char ipmiQueuePath[] =
	"/xyz/openbmc_project/Ipmi";
static constexpr const char ipmiQueueIntf[] =
	"xyz.openbmc_project.Ipmi.Server";
static constexpr const char ipmiQueueMethod[] = "execute";

/**
 * sendRecvIpmi - Sends an IPMI command request to ipmid and returns the response vector.
 * @arg conn - sdbus connection
 * @arg yield - yield context for yield method calls
 * @arg req - IPMI message request parameters
 *              Byte 1      (NetFn)
 *              Byte 2      (Command)
 *              Byte 3:N    (Request Data)
 * 
 * @returns IPMI message response
 *              Byte 1      (NetFn)
 *              Byte 2      (Command)
 *              Byte 3:N    (Response Data)
 */
std::vector<uint8_t> sendRecvIpmi(boost::asio::yield_context yield, std::vector<uint8_t> req)
{
	using IpmiDbusRspType = std::tuple<uint8_t, uint8_t, uint8_t, uint8_t,
										std::vector<uint8_t>>;

	std::vector<uint8_t> cmd_data;
	std::map<std::string, std::variant<int>> options; //Empty options
	boost::system::error_code ec;
	std::vector<uint8_t> response;

	if(req.size() < 2)
	{
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"sendRecvIpmi Error: Request data length invalid.");
		return response;
	}

	uint8_t netfn = req[0];
	uint8_t lun = 0x00;
	uint8_t cmd = req[1];
	if(req.size() > 2)
	{
		cmd_data.insert(cmd_data.end(), (req.begin() + 2), req.end());
	}
	else
	{
		cmd_data.clear();
	}
	
    std::shared_ptr<sdbusplus::asio::connection> dbus = getSdBus();

    auto rsp = dbus->yield_method_call<IpmiDbusRspType>(yield, ec,
	ipmiQueueService, ipmiQueuePath, ipmiQueueIntf, ipmiQueueMethod,
	netfn, lun, cmd, cmd_data, options);

	uint8_t rnetfn = std::get<0>(rsp);
	uint8_t rlun = std::get<1>(rsp);
	uint8_t rcmd = std::get<2>(rsp);
	uint8_t rcc = std::get<3>(rsp);
	response = std::get<4>(rsp);

	if (ec)
	{
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"FXN-OEM<->ipmid bus error:", phosphor::logging::entry("NETFN=0x%02x", rnetfn),
			phosphor::logging::entry("LUN=0x%02x", rlun), phosphor::logging::entry("CMD=0x%02x", rcmd),
			phosphor::logging::entry("ERROR=%s", ec.message().c_str()));
			response.clear();
	}

	if(rcc > 0)
	{
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"FXN-OEM<->ipmid bus error: Non-zero completion code.");
		response.clear();
	}

	return response;
}

/**
 * Walks the FRU device fields to obtain or set the MAC Address.
 * Temporarily the FRU Id, Field Name, and Field Index will be preset
 * values, but the intention will be to obtain these values from custom BMC 
 * Configuration settings.
 *
 * @arg conn - sdbus connection
 * @arg yield - yield context for sdbus yield method calls
 * @arg input - Byte 1      (Operation)
 *                          0 - Get
 *                          1 - Set
 *              Byte 2:N    (MAC to Set)
 * 
 * @returns function status code
 *          0 - Success
 *         -1 - Error
 */  
int8_t accessFruMac(boost::asio::yield_context yield, std::vector<uint8_t>& input)
{

	struct fru_info fru;
	struct fru_header header;

	std::vector<uint8_t> req, req_data, resp_trim, macaddr;
	uint8_t checksum, fru_area_hdr;
	int fru_field_offset, fru_field_offset_tmp, header_offset;
	uint8_t fru_section_len, i;
    uint8_t macAccessType = input[0];
    macaddr.insert(macaddr.end(), input.begin()+1, input.end());

	uint8_t fruId = 0;
	uint8_t fieldIndex = 5;

	//1. Test if FRU device is present with Get FRU Inventory Area Info command.
	req.clear();
	req_data.clear();
	req.push_back(ipmi::netFnStorage);
	req.push_back(ipmi::storage::cmdGetFruInventoryAreaInfo);

    auto resp = sendRecvIpmi(yield,req);
	if(resp.size() == 0)
	{
		//IPMI command fails, so FRU device is not found.
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Error obtaining FRU Inventory Info: FRU Device may be missing.");
		return -1;
	}

	fru.size = (resp[1] << 8) | resp[0];
	fru.access = resp[2] & ACCESS_MASK;

	if(fru.size < 1)
	{
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Invalid FRU size from FRU Inventory Info.");
		return -1;
	}

	//2. Retrieve FRU Header
	req.clear();
	req_data.clear();
	req.push_back(ipmi::netFnStorage);
	req.push_back(ipmi::storage::cmdReadFruData);
	req_data = {fruId, 0, 0, 8}; //To read 8 bytes from offset 0 of FRU Id.
	req.insert(req.end(),req_data.begin(),req_data.end());

    resp = sendRecvIpmi(yield,req);
	if(resp.size() == 0)
	{
		//IPMI command fails, so FRU device is not found.
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Error obtaining FRU Header: FRU Device may be missing.");
		return -1;
	}
	
	resp_trim.clear();
	resp_trim.insert(resp_trim.end(), (resp.begin() + 1), resp.end()); //Skip count byte in return
	header.board = resp_trim[3];

	//2. Retrieve Board Area info using Board Offset from FRU Header
	req.clear();
	req_data.clear();
	req.push_back(ipmi::netFnStorage);
	req.push_back(ipmi::storage::cmdReadFruData);
	header_offset = (header.board * 8);
	fru_field_offset = 6; //FRU fields in Board section start at 6.
	req_data = {fruId, (uint8_t)(header_offset & 0xFF), (uint8_t)(header_offset >> 8), 3}; //Read 3 bytes of Board Area
	req.insert(req.end(),req_data.begin(),req_data.end());

    resp = sendRecvIpmi(yield,req);
	if(resp.size() == 0)
	{
		//IPMI command fails, so FRU device is not found.
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Error obtaining FRU Board Info: FRU Device may be missing.");
		return -1;
	}

	resp_trim.clear();
	resp_trim.insert(resp_trim.end(), (resp.begin() + 1), resp.end()); //Skip count byte in return
	fru_section_len = resp_trim[1];
	req_data[4] = fru_section_len; //Change length of read to full Board Area 

    resp = sendRecvIpmi(yield,req);
	if(resp.size() == 0)
	{
		//IPMI command fails, so FRU device is not found.
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Error obtaining FRU Board Info: FRU Device may be missing.");
		return -1;
	}

	resp_trim.clear();
	resp_trim.insert(resp_trim.end(), (resp.begin() + 1), resp.end()); //Skip count byte in return

	//3. Walk the dynamic fields to find the field index for the MAC Address
	fru_field_offset_tmp = fru_field_offset;
	for(i=0; i <= fieldIndex; i++)
	{
		fru_area_hdr = resp_trim[fru_field_offset_tmp + i];
		fru_field_offset_tmp += fru_area_hdr & 0x3F; //Length is 0:5
	}

	int len = (fru_area_hdr & 0x3F);
	if(len == 0)
	{
		//Field has no data
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"MAC Address field has no data. Does the field exist?");
		return -1;
	}

    if(macAccessType == 0)
    {
        //Set last 6 bytes of input to the MAC address
        std::copy((resp_trim.begin() + fru_field_offset_tmp + fieldIndex),(resp_trim.begin() + fru_field_offset_tmp + fieldIndex + len), input.begin()+1);
        return 0;
    }
    else
    {
        //4. Set new MAC into FRU field space and calculate checksum.

        if(len == macaddr.size())
        {
            //std::copy(resp_trim[fru_field_offset_tmp + fieldIndex],macaddr.begin(),macaddr.end()); //Set MAC_ADDRESS to new MAC
            std::copy(macaddr.begin(),macaddr.end(),(resp_trim.begin() + fru_field_offset_tmp + fieldIndex)); //Set MAC_ADDRESS to new MAC

            checksum = 0;
            for (i = header_offset; i < (header_offset + fru_section_len - 1); i++)
            {
                checksum += resp_trim[i];
            }
            checksum = (~checksum) + 1;
            resp_trim[header_offset + fru_section_len - 1] = checksum;

            //5. Write FRU Data using the field index and macaddr to write
            req.clear();
            req_data.clear();
            req.push_back(ipmi::netFnStorage);
            req.push_back(ipmi::storage::cmdWriteFruData);
            header_offset = (header.board * 8);
            req_data = {fruId, (uint8_t)(header_offset & 0xFF), (uint8_t)(header_offset >> 8), fru_section_len}; //Write new FRU to MAC_ADDRESS of Board Area
            req.insert(req.end(),req_data.begin(),req_data.end());

            resp = sendRecvIpmi(yield,req);

            if(resp[0] != macaddr.size())
            {
                //Error writing to FRU
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "Length written to FRU does not match size of MAC Address.");
                return -1;
            }

            return 0;
        }
        else
        {
            //New MAC has different size than old MAC
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Size of MAC Address to be written does not match current MAC size.");
            return -1;
        }
    }

}
/**
 * See utils.hpp for description.
 **/
std::string findStr(const std::vector<std::string> vec, const std::string needle) {
    auto element = std::find_if(vec.begin(), vec.end(),
                    [&needle](const std::string haystack) {
                        return haystack.find(needle) != std::string::npos;
                    });
    if (vec.end() != element) {
        return vec[distance(vec.begin(), element)];
    }

    return "";
}

/**
 * See utils.hpp for description.
 **/
ObjectTree GetSubTree(boost::asio::yield_context yield, const std::string path, const std::string interface) {
    boost::system::error_code ec;
    std::shared_ptr<sdbusplus::asio::connection> dbus = getSdBus();

    ObjectTree subtree = dbus->yield_method_call<ObjectTree>(yield, ec,
        OBJ_MAP_SERVICE,
        OBJ_MAP_OBJECT,
        OBJ_MAP_IFACE,
        "GetSubTree",
        path, 1,
        std::vector<std::string>({interface})
    );

    if (ec) {
        std::string temp = "Failed to get subtree: error(" + std::to_string(ec.value()) + ") path(" + path + ") interface(" + interface + ")";
        phosphor::logging::log<phosphor::logging::level::ERR>(temp.c_str());
        return {};
    }

    return subtree;
}

}
