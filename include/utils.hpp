/*
// Copyright (c) 2020 Foxconn Corporation
//
//
*/
#pragma once
#include <ipmid/api.hpp>
#include <ipmid/types.hpp>

struct fru_info
{
    uint16_t size;
    uint8_t access;
};

struct fru_header
{
    uint8_t version;
    uint8_t internal;
    uint8_t chassis;
    uint8_t board;
    uint8_t product;
    uint8_t multi;
    uint8_t pad;
    uint8_t checksum;
};

#define GET 0x00
#define SET 0x01

#define ACCESS_MASK 0x1
#define BOARD_AREA_START_OFFSET_INDEX 4
#define BOARD_AREA_LENGTH_INDEX 2

/* D-Bus service, object path, and interface names */
#define FREEDT_PROP_IFACE   "org.freedesktop.DBus.Properties"

#define OBJ_MAP_SERVICE     "xyz.openbmc_project.ObjectMapper"
#define OBJ_MAP_OBJECT      "/xyz/openbmc_project/object_mapper"
#define OBJ_MAP_IFACE       "xyz.openbmc_project.ObjectMapper"

#define FAN_TACH_OBJECT     "/xyz/openbmc_project/sensors/fan_tach/"
#define FAN_VALUE_IFACE     "xyz.openbmc_project.Sensor.Value"
#define FAN_CTL_IFACE       "xyz.openbmc_project.Control"

namespace ipmi
{

/**
 * @brief - A needle in the haystack to find a string in the vector.
 *
 * @arg vec - Vector of strings
 * @arg needle - String to find in the vector
 *
 * @return The string in the vector
 **/
std::string findStr(const std::vector<std::string> vec, const std::string needle);

/**
 * @brief Calls Object Mapper GetSubTree to receive the services, object paths, and interfaces
 *
 * @arg yield - Used to async block upon
 * @arg path - Relative object path
 * @arg interface - Object path's interface
 *
 * @return An ObjectTree map with data on success, else an empty map
 **/
ObjectTree GetSubTree(boost::asio::yield_context yield, const std::string path, const std::string interface);

}
