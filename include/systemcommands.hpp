/*
// Copyright (c) 2020 Foxconn Corporation
//
//
*/

#pragma once
#include <ipmid/api.hpp>
#include <ipmid/utils.hpp>
#include <nlohmann/json.hpp>

enum fxn_sys_cmds
{
    CMD_SYS_HW_CONFIG = 0x00,
    CMD_SYS_GET_SYS_FW_VER = 0x01,
};

enum fxn_firm_cases
{
    BMC = 0x01,
    BIOS = 0x02,
    CPLD = 0x03,
    MOTHERBOARD = 0x04,
    PSU = 0x05,
    FRU_VPD = 0x06,
    OPENBMC = 0x07,
};

enum fxn_cpld_index
{
    MB = 0x01,
    MP = 0x02,
};

enum fxn_hardware_config
{
    NCSI = 0x01,
    VGA = 0x02,
};

enum fxn_hardware_mode
{
    CURRENT_CONFIG = 0x00,
    ONBOARD_INTER = 0x01,
    REAR_VGA = 0x01,
    OPC3_0 = 0x02,
    FRONT_VGA = 0x02,
};

const char *versionPath="/etc/foxconn/version.json";
static constexpr const char* OpenBMCVersionPurpose = "xyz.openbmc_project.Software.Version.VersionPurpose.BMC";
static constexpr const char* BiosVersionPurpose = "xyz.openbmc_project.Software.Version.VersionPurpose.HOST";