/*
// Copyright (c) 2020 Foxconn Corporation
//
//
*/

#pragma once
#include <ipmid/api.hpp>

enum fxn_net_cmds
{
    CMD_NET_MAC_ADDR = 0x10,
    CMD_NET_NETWORK_FUNCTION = 0x11,
};

enum fxn_net_functions
{
    BONDING = 0x01,
    HTTP = 0x02,
    SSH = 0x04,
    TELNET = 0x08,
    SOL = 0x10,
    REDFISH = 0x20,
    PING = 0x40,
};

constexpr const char icmpEchoIgnoreAll[] = "/proc/sys/net/ipv4/icmp_echo_ignore_all";

constexpr ipmi::Cc ccFruDataError = 0x21;

#define GET_MAC 0b00
#define SET_MAC 0b01

#define SET_NETFUN          0x00
#define ENABLE_NETFUN       0x01
#define SOL_PAYLOAD_CHANNEL 0x01

#define SIZE_MAC_ADDRESS 6