/*
// Copyright (c) 2020 Foxconn Corporation
//
//
*/

#pragma once
#include <ipmid/api.hpp>

enum fxn_fan_cmds
{
    CMD_FAN_FAN_DBG = 0x20,
    CMD_FAN_GET_FAN_SPEED = 0x21,
    CMD_FAN_SET_DUTY = 0x22,
    CMD_FAN_FSC_CONTROL = 0x23,
};
