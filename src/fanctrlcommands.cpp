/*
// Copyright (c) 2020 Foxconn Corporation
//
//
 */

#include <ipmid/api.h>
#include <ipmid/types.hpp>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/message/types.hpp>
#include <fanctrlcommands.hpp>
#include <systemd/sd-bus.h>
#include <sys/types.h>
#include <variant>
#include <utils.hpp>

using fanpwm = std::variant<uint64_t>;

namespace ipmi
{
    static void registerFANFunctions() __attribute__((constructor));

    /**
     * @brief Get the fan PWM speed of all the fans (fanindex: 0) or individual fan (fanindex: 1+)
     * @arg yield - yield context for yield method calls
     * @arg reqParams - paramters arguments for GET_FAN_SPEED command request
     *                       Byte 1 - fanindex
     *                     fanindex - Operations
     *                          00  -  all fans
     *                      01 - *  -  individual corresponding fan
     * @returns the pwm value(s)
     **/
    ipmi::RspType<std::vector<uint8_t>> ipmiGetFanSpeed(boost::asio::yield_context yield, uint8_t fanindex)
    {
        uint8_t i = 0U;
        std::string msg = "";
        boost::system::error_code ec;
        std::vector<uint8_t> pwm_value;
        std::shared_ptr<sdbusplus::asio::connection> dbus = getSdBus();
        auto subtree = GetSubTree(yield, FAN_TACH_OBJECT, FAN_VALUE_IFACE);

        /* Check to see if subtree is not empty and the fanindex isn't out of range */
        if (subtree.empty()) {
            return ipmi::responseSensorInvalid();
        } else if (fanindex > subtree.size()) {
            msg = "Fan " + std::to_string(fanindex) + " is out of range - Fan size(" + std::to_string(subtree.size()) + ")";
            phosphor::logging::log<phosphor::logging::level::ERR>(msg.c_str());
            return ipmi::responseParmOutOfRange();
        }

        for (const auto& tree : subtree) {
            /**
             * Only get the fan PWM speed if fanindex is 0 (get all fan PWM speed) or
             * fanindex is the current fan index (get individual fan PWM speed).
             * Else, continue to find fan index
             **/
            if  ( (0U   != fanindex) &&
                  (i++  != (fanindex - 1U)) ) {
                continue;
            }

            /**
             * Get the fan hwmon service, object path, and control speed interface.
             * Then set the FAN PWM target value using the yield method call.
             **/
            auto objPath    = tree.first;
            auto innerTree  = tree.second;
            auto service    = innerTree.begin()->first;
            auto interface  = findStr(innerTree.begin()->second, FAN_CTL_IFACE);
            auto rsp = dbus->yield_method_call<fanpwm>(yield, ec, service,
                    objPath,
                    FREEDT_PROP_IFACE, "Get",
                    interface, "Target");

            if (ec) {
                msg = "Error in getting the fan target: error(" + std::to_string(ec.value()) + ")";
                phosphor::logging::log<phosphor::logging::level::ERR>(msg.c_str());

                if (0U != fanindex) {
                    return ipmi::responseBusy();
                }
            }
            else {
                /* Add the PWM values to return them together */
                pwm_value.push_back((uint8_t)std::get<uint64_t>(rsp));
            }
        }

        /* If the bus reponse is successful, return ipmi success with the PWM value */
        return ipmi::responseSuccess(pwm_value);
    }

    /**
     * @brief Set the fan PWM speed of all the fans (fanindex: 0) or individual fan (fanindex: 1+)
     * @arg yield - yield context for yield method calls
     * @arg reqParams - parameters arguments for SET_DUTY_CYCLE command request
     *                       Byte 1 - fanindex
     *                     fanindex - Operations
     *                          00  - all fans
     *                      01 - *  -  individual corresponding fan
     *                       Byte 2 - pwm to be set
     * @returns the updated pwm value(s)
     **/
    ipmi::RspType<std::vector<uint8_t>> ipmiSetDutyCycle(boost::asio::yield_context yield, uint8_t fanindex, uint8_t dutyCycle)
    {
        uint8_t i = 0U;
        std::string msg = "";
        boost::system::error_code ec;
        std::vector<uint8_t> dutycycle;
        std::shared_ptr<sdbusplus::asio::connection> dbus = getSdBus();
        auto subtree = GetSubTree(yield, FAN_TACH_OBJECT, FAN_VALUE_IFACE);

        /* Check to see if subtree is not empty and the fanindex isn't out of range */
        if (subtree.empty()) {
            return ipmi::responseSensorInvalid();
        } else if (fanindex > subtree.size()) {
            msg = "Fan " + std::to_string(fanindex) + " is out of range - Fan size(" + std::to_string(subtree.size()) + ")";
            phosphor::logging::log<phosphor::logging::level::ERR>(msg.c_str());
            return ipmi::responseParmOutOfRange();
        }

        for (const auto& tree : subtree) {
            /**
             * Only set the fan PWM speed if fanindex is 0 (set all fan PWM speed) or
             * fanindex is the current fan index (set individual fan PWM speed).
             * Else, continue to find fan index
             **/
            if  ( (0U   != fanindex) &&
                  (i++  != (fanindex - 1U)) ) {
                continue;
            }

            /**
             * Get the fan hwmon service, object path, and control speed interface.
             * Then set the FAN PWM target value using the yield method call.
             **/
            auto objPath    = tree.first;
            auto innerTree  = tree.second;
            auto service    = innerTree.begin()->first;
            auto interface  = findStr(innerTree.begin()->second, FAN_CTL_IFACE);
            dbus->yield_method_call<>(yield, ec, service,
                    objPath, FREEDT_PROP_IFACE,
                    "Set", interface, "Target",
                    std::variant<uint64_t>(dutyCycle));

            if (ec) {
                msg = "Error in setting the fan target: error(" + std::to_string(ec.value()) + ")";
                phosphor::logging::log<phosphor::logging::level::ERR>(msg.c_str());

                if (0U != fanindex) {
                    return ipmi::responseBusy();
                }
            } else {
                /* Run the get command D-Bus to verify the set command */
                auto rsp = dbus->yield_method_call<fanpwm>(yield, ec, service,
                        objPath,
                        FREEDT_PROP_IFACE, "Get",
                        interface, "Target");
                if (ec) {
                    msg = "Error in setting the fan target: error(" + std::to_string(ec.value()) + ")";
                    phosphor::logging::log<phosphor::logging::level::ERR>(msg.c_str());
                }
                else {
                    /* Add the PWM values to return them together */
                    dutycycle.push_back((uint8_t)std::get<uint64_t>(rsp));
                }
            }
        }

        /* If the bus reponse is successful, return ipmi success with the PWM value of the set command */
        return ipmi::responseSuccess(dutycycle);
    }

    void registerFANFunctions()
    {

        /* registerHandler({PRIORITY_VAL}, {NETFN}}, {CMD_VAL}, {PRIVILEGE_LVL},
           {HANDLER_FUNCTION});
         */
    ipmi::registerHandler(ipmi::prioOemBase, ipmi::netFnOemThree, CMD_FAN_GET_FAN_SPEED, ipmi::Privilege::User,
            ipmiGetFanSpeed);
    ipmi::registerHandler(ipmi::prioOemBase, ipmi::netFnOemThree, CMD_FAN_SET_DUTY, ipmi::Privilege::User,
            ipmiSetDutyCycle);
    return;
}

}
