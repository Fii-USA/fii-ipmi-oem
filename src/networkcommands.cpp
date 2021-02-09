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
#include <boost/thread/thread.hpp>
//#include <phosphor-net-ipmid/sol/sol_manager.hpp>
#include <networkcommands.hpp>
#include <utils.hpp>
#include <string>
#include <variant>
#include <iterator>
#include <cstdlib>

namespace ipmi
{

static void registerNETFunctions() __attribute__((constructor));

int8_t accessFruMac(boost::asio::yield_context yield, std::vector<uint8_t>& input);
std::vector<uint8_t> sendRecvIpmi(boost::asio::yield_context yield, std::vector<uint8_t> req);

/**
 * Foxconn OEM IPMI Command to get/set the MAC Address in FRU.
 * @arg yield - yield context for yield method calls
 * @arg reqParams - parameter arguments for MAC Address request
 * 			Byte 1		(Operation)
 * 						0 - Get MAC Address
 * 						1 - Set MAC Address
 * 			Byte 2:7	(MAC Address)
 * @returns Completion code and MAC Address
 * 			Byte 1		(Completion Code)
 * 			Byte 2:7	(MAC Address)
 */  
ipmi::RspType<std::vector<uint8_t>> ipmiMacAddress(boost::asio::yield_context yield, std::vector<uint8_t> reqParams)
{

	bool op = reqParams[0] & 0b11;
	std::vector<uint8_t> mac;

	if(op == SET_MAC)
	{
		mac.insert(mac.end(), reqParams.begin()+1, reqParams.end());

		if(mac.size() == SIZE_MAC_ADDRESS)
		{
			phosphor::logging::log<phosphor::logging::level::INFO>(
				"Setting MAC Address in FRU device.");

			if(accessFruMac(yield, reqParams) == 0)
			{
				return ipmi::responseSuccess(mac);
			}
			phosphor::logging::log<phosphor::logging::level::ERR>(
				"Error setting MAC Address in FRU device.");

			return ipmi::response(ccFruDataError, mac);
		}

		phosphor::logging::log<phosphor::logging::level::ERR>(
			"MAC Address size is incorrect.");

		return ipmi::responseReqDataLenInvalid();

	}
	else if (op == GET_MAC)
	{

		phosphor::logging::log<phosphor::logging::level::INFO>(
			"Getting MAC Address from FRU device.");

		if(accessFruMac(yield, reqParams) == 0)
		{
			mac.insert(mac.end(), reqParams.begin()+1, reqParams.end());
			return ipmi::responseSuccess(mac);
		}

		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Error obtaining MAC Address from FRU device.");

		return ipmi::response(ccFruDataError, mac);

	}
	else 
	{
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Mac Address command's operation value is invalid.");

		return ipmi::responseInvalidCommand();
	}

}


/**
 * Obtains the status of the SOL on SOL Payload Channel 1.
 * Parameter Selector = 1
 * Set Selector = 0
 * Block Selector = 0
 * 
 * @arg yield - The yield context for yield method calls
 */ 
uint8_t getSOLStatus(boost::asio::yield_context yield)
{
	std::vector<uint8_t> req, req_data;
	req.clear();
	req_data.clear();
	req.push_back(ipmi::netFnTransport);
	req.push_back(ipmi::transport::cmdGetSolConfigParameters);
	//req_data = {SOL_PAYLOAD_CHANNEL, 0x01, 0x00, 0x00}; //CH=8,Select=1
	req_data = {0x01, 0x01};
	req.insert(req.end(),req_data.begin(),req_data.end());

	std::shared_ptr<sdbusplus::asio::connection> dbus = getSdBus();
	//auto resp = sendRecvIpmi(*dbus,yield,req);
	auto resp = sendRecvIpmi(yield,req);
	if(resp.size() < 2)
	{
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Error obtaining status of SOL Configuration.");
		return -1;
	}

	//Skip byte 1, which is the parameter revision. Byte 2 has status.
	return (resp.back() & ENABLE_NETFUN);
}

/**
 * Obtains the status of the SOL configuration with the following assumptions:
 * SOL Payload Channel = 1.
 * Parameter Selector = 1
 * 
 * @arg yield - The yield context for yield method calls
 */ 
void setSOLStatus(boost::asio::yield_context yield, uint8_t status)
{
	std::vector<uint8_t> req, req_data;
	req.clear();
	req_data.clear();
	req.push_back(ipmi::netFnTransport);
	req.push_back(ipmi::transport::cmdSetSolConfigParameters);
	//req_data = {SOL_PAYLOAD_CHANNEL, 0x01, status};
	req_data = {0x01, 0x01, status};
	req.insert(req.end(),req_data.begin(),req_data.end());

	auto resp = sendRecvIpmi(yield,req);
}


/**
 * Foxconn OEM IPMI Command to obtain or modify the status of network functions.
 * @arg yield - yield context for yield method calls
 * @arg reqParams - The configuration and target network function for the command.
 * 					Byte 1: (Configuration)
 * 						[0:3] (Operation)
 * 							0 - Set
 * 							1 - Get
 * 						[4:7] (Modify Status)
 * 							0 - Disable
 * 							1 - Disable
 *
 * 					Byte 2: (Network Function)
 * 						0x1 - Bonding
 * 						0x2 - HTTP/HTTPS
 * 						0x4 - SSH
 * 						0x8 - Telnet
 * 						0x10 - SOL
 * 						0x20 - Redfish
 * 						0x40 - Ping
 * 
 * @return Completion code and status
 * 					Byte 1 (Completion Code)
 * 					Byte 2 (Status)
 * 						0 - Disabled
 * 						1 - Enabled
 */  
ipmi::RspType<uint8_t> ipmiNetworkFunction(boost::asio::yield_context yield, uint8_t configuration, uint8_t function)
{

	uint8_t op = (configuration & 0x0F);
	uint8_t newStatus = ((configuration & 0xF0)>>4);
	uint8_t status = 255;
	int retCode = -1;

	if((op > 1) || (op < 0))
	{
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Invalid operation for Network Function.");

		return ipmi::responseInvalidCommand();
	}

	if((newStatus > 1) || (newStatus < 0))
	{
		phosphor::logging::log<phosphor::logging::level::ERR>(
			"Invalid status setting for Network Function.");

		return ipmi::responseInvalidCommand();
	}

	std::ifstream fileIcmpEchoIgnoreAll;
	char state[10];

	switch(function)
	{
		case BONDING: //WIP, kernel warning occurs when disabling bond
			if (op == SET_NETFUN)
			{
				phosphor::logging::log<phosphor::logging::level::INFO>(
					"Setting enable/disable status of bonding interface.");

				if(newStatus == ENABLE_NETFUN)
				{
					//Check that the bond isn't enabled
					if(WEXITSTATUS(system("more /proc/net/bonding/bond0 | grep MII.*up")) != 0)
					{
						system("systemctl stop systemd-networkd");
						boost::this_thread::sleep(boost::posix_time::milliseconds(250));
						system("cp /etc/systemd/network/bmc-eth0-bond /etc/systemd/network/00-bmc-eth0.network");
						system("cp /etc/systemd/network/bmc-bond0 /etc/systemd/network/00-bmc-bond0.netdev");
						system("systemctl restart systemd-networkd");
						boost::this_thread::sleep(boost::posix_time::milliseconds(500));
					}
				}
				else
				{
					//Check that bond is already enabled
					if(WEXITSTATUS(system("more /proc/net/bonding/bond0 | grep MII.*up")) == 0)
					{
						boost::this_thread::sleep(boost::posix_time::milliseconds(250));
						system("ifenslave -d bond0 eth0");
						boost::this_thread::sleep(boost::posix_time::milliseconds(250));
						system("rm /etc/systemd/network/00-bmc-bond0.netdev");
						system("cp /etc/systemd/network/bmc-eth0-save /etc/systemd/network/00-bmc-eth0.network");
						system("systemctl restart systemd-networkd");
						boost::this_thread::sleep(boost::posix_time::milliseconds(250));
						system("ifconfig eth0 up");
						boost::this_thread::sleep(boost::posix_time::milliseconds(250));
						system("systemctl restart systemd-networkd");
						boost::this_thread::sleep(boost::posix_time::milliseconds(250));
					}
				}
			}

			//Serves as check for Set and as implementation for Get operation
			if(WEXITSTATUS(system("more /proc/net/bonding/bond0 | grep MII.*up")) == 0)
			{
				status = 1;
			}
			else
			{
				status = 0;
			}
			break;

		//Web service has both Redfish and HTTP/HTTPS, WIP
		case REDFISH:
		case HTTP: 
			if (op == SET_NETFUN)
			{
				phosphor::logging::log<phosphor::logging::level::INFO>(
					"Setting enable/disable status of web service.");
				//status = setPingFunctionStatus(newStatus);
				if(newStatus == ENABLE_NETFUN)
				{
					system("systemctl enable bmcweb.service");
					system("systemctl enable bmcweb.socket");
					system("systemctl restart bmcweb.service");
					system("systemctl restart bmcweb.socket");
				}
				else
				{
					system("systemctl disable bmcweb.service");
					system("systemctl disable bmcweb.socket");
					system("systemctl stop bmcweb.service");
					system("systemctl stop bmcweb.socket");
				}

			}

			//Serves as check for Set and as implementation for Get operation
			//Exit status is 0 for enabled, 1 for disabled, needs to be inverted
			status = (1-WEXITSTATUS(system("systemctl is-enabled bmcweb.service")));

			break;

		case SSH:
			if (op == SET_NETFUN)
			{
				phosphor::logging::log<phosphor::logging::level::INFO>(
					"Setting enable/disable status of SSH (dropbear) service.");

				if(newStatus == ENABLE_NETFUN)
				{
					system("systemctl start dropbear.socket");
					system("systemctl start obmc-console-ssh.socket");
					system("systemctl enable dropbear.socket");
					system("systemctl enable obmc-console-ssh.socket");
				}
				else
				{
					system("systemctl stop dropbear.socket");
					system("systemctl stop obmc-console-ssh.socket");
					system("systemctl disenable dropbear.socket");
					system("systemctl disenable obmc-console-ssh.socket");
				}
			}

			//Serves as check for Set and as implementation for Get operation
			//If one is active and the other isn't, then SSH still works.
			if(WEXITSTATUS(system("systemctl is-active dropbear.socket obmc-console-ssh.socket")) == 0)
			{
				status = 1;
			}
			else
			{
				status = 0;
			}

			break;

		case TELNET: //WIP
			if (op == SET_NETFUN)
			{
				phosphor::logging::log<phosphor::logging::level::INFO>(
					"Setting enable/disable status of Telnet interface.");
				//setTelnetStatus(newStatus);				
				if(newStatus == ENABLE_NETFUN)
				{
					if(WEXITSTATUS(system("iptables -L INPUT -n | grep dpt:23")) == 0)
					{
						if(WEXITSTATUS(system("iptables -L INPUT -n | grep DROP.*dpt:23")) == 0)
						{
							//Entry to block port exists, needs to be replaced
							system("iptables -D INPUT -p tcp --dport 23 -j DROP");
							system("iptables -A INPUT -p tcp --dport 23 -j ACCEPT");
						}
						//If not DROP, then it is ACCEPT, so no need to modify it
					}
					else
					{
						//Entry to allow port doesn't exist
						system("iptables -A INPUT -p tcp --dport 23 -j ACCEPT");
					}
					
					if(WEXITSTATUS(system("iptables -L INPUT -n | grep dpt:992")) == 0)
					{
						if(WEXITSTATUS(system("iptables -L INPUT -n | grep DROP.*dpt:992")) == 0)
						{
							//Entry to block port exists, needs to be replaced
							system("iptables -D INPUT -p tcp --dport 992 -j DROP");
							system("iptables -A INPUT -p tcp --dport 992 -j ACCEPT");
						}
						//If not DROP, then it is ACCEPT, so no need to modify it
					}
					else
					{
						//Entry to allow port doesn't exist
						system("iptables -A INPUT -p tcp --dport 992 -j ACCEPT");
					}

					if(WEXITSTATUS(system("iptables -L INPUT -n | grep dpt:60177")) == 0)
					{
						if(WEXITSTATUS(system("iptables -L INPUT -n | grep DROP.*dpt:60177")) == 0)
						{
							//Entry to block port exists, needs to be replaced
							system("iptables -D INPUT -p tcp --dport 60177 -j DROP");
							system("iptables -A INPUT -p tcp --dport 60177 -j ACCEPT");
						}
						//If not DROP, then it is ACCEPT, so no need to modify it
					}
					else
					{
						//Entry to allow port doesn't exist
						system("iptables -A INPUT -p tcp --dport 60177 -j ACCEPT");
					}
				}
				else //DISABLE
				{
					if(WEXITSTATUS(system("iptables -L INPUT -n | grep dpt:23")) == 0)
					{
						if(WEXITSTATUS(system("iptables -L INPUT -n | grep ACCEPT.*dpt:23")) == 0)
						{
							//Entry to accepot port exists, needs to be replaced
							system("iptables -D INPUT -p tcp --dport 23 -j ACCEPT");
							system("iptables -A INPUT -p tcp --dport 23 -j DROP");
						}
						//If not ACCEPT, then it is DROP, so no need to modify it
					}
					else
					{
						//Entry to drop port doesn't exist
						system("iptables -A INPUT -p tcp --dport 23 -j DROP");
					}
					
					if(WEXITSTATUS(system("iptables -L INPUT -n | grep dpt:992")) == 0)
					{
						if(WEXITSTATUS(system("iptables -L INPUT -n | grep ACCEPT.*dpt:992")) == 0)
						{
							//Entry to accepot port exists, needs to be replaced
							system("iptables -D INPUT -p tcp --dport 992 -j ACCEPT");
							system("iptables -A INPUT -p tcp --dport 992 -j DROP");
						}
						//If not ACCEPT, then it is DROP, so no need to modify it
					}
					else
					{
						//Entry to drop port doesn't exist
						system("iptables -A INPUT -p tcp --dport 992 -j DROP");
					}

					if(WEXITSTATUS(system("iptables -L INPUT -n | grep dpt:60177")) == 0)
					{
						if(WEXITSTATUS(system("iptables -L INPUT -n | grep ACCEPT.*dpt:60177")) == 0)
						{
							//Entry to accepot port exists, needs to be replaced
							system("iptables -D INPUT -p tcp --dport 60177 -j ACCEPT");
							system("iptables -A INPUT -p tcp --dport 60177 -j DROP");
						}
						//If not ACCEPT, then it is DROP, so no need to modify it
					}
					else
					{
						//Entry to drop port doesn't exist
						system("iptables -A INPUT -p tcp --dport 60177 -j DROP");
					}
				}

				//Save the changes
				system("iptables-save > /etc/iptables/iptables.rules");
			}

			//Serves as check for Set and as implementation for Get operation
			if(WEXITSTATUS(system("iptables -L INPUT -n | grep ACCEPT.*dpt:23")) == 0)
			{
				status = 1;
			}
			else if(WEXITSTATUS(system("iptables -L INPUT -n | grep DROP.*dpt:23")) == 0)
			{
				status = 0;
			}
			else //No entry yet, so not blocked
			{
				status = 1;
			}
			
			break;

		/*case SOL:
			if (op == SET_NETFUN)
			{
				phosphor::logging::log<phosphor::logging::level::INFO>(
					"Setting enable/disable status of SOL.");
				//Set SOL Configuration enable/disable parameter
				//setSOLStatus(yield, newStatus);
				std::get<sol::Manager&>(singletonPool).enable = newStatus;
			}

			//Serves as check for Set and as implementation for Get operation
			//status = getSOLStatus(yield); std::get<sol::Manager&>(singletonPool).enable
			status = std::get<sol::Manager&>(singletonPool).enable;
			break;*/
			
		case PING:
			if (op == SET_NETFUN)
			{
				phosphor::logging::log<phosphor::logging::level::INFO>(
					"Setting enable/disable status of ping service.");
				
				if(newStatus == ENABLE_NETFUN)
				{
					system("sysctl -w net.ipv4.icmp_echo_ignore_all=0");
				}
				else
				{
					system("sysctl -w net.ipv4.icmp_echo_ignore_all=1");
				}

			}

			//Serves as check for Set and as implementation for Get operation
			fileIcmpEchoIgnoreAll.open(icmpEchoIgnoreAll);

			if (fileIcmpEchoIgnoreAll.is_open())
			{
				while(!fileIcmpEchoIgnoreAll.eof())
				{
					fileIcmpEchoIgnoreAll >> state;
				}
			}
			fileIcmpEchoIgnoreAll.close();

			if(state[0] == '0')
			{
				status = 1;
			}
			else
			{
				status = 0;
			}
			break;

		default:
			phosphor::logging::log<phosphor::logging::level::ERR>(
				"Invalid network function selection.");

			return ipmi::responseInvalidCommand();
			break;
	}

	if(status != 255)
	{
		return ipmi::responseSuccess(status);
	}

	phosphor::logging::log<phosphor::logging::level::ERR>(
		"Network function status could not be obtained or modified.");

	return ipmi::responseBusy();

}

void registerNETFunctions()
{

	ipmi::registerHandler(ipmi::prioOemBase, ipmi::netFnOemThree, CMD_NET_MAC_ADDR, ipmi::Privilege::User,
				ipmiMacAddress);
	
	ipmi::registerHandler(ipmi::prioOemBase, ipmi::netFnOemThree, CMD_NET_NETWORK_FUNCTION, ipmi::Privilege::User,
				ipmiNetworkFunction);

	return;
}

} //namespace ipmi
