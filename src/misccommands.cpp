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
#include <misccommands.hpp>

namespace ipmi
{
    static void registerMISCFunctions() __attribute__((constructor));
 
    /**
     * Command implementations go here.
     *
    */


    /* ipmi::RspType<> {HANDLER_FUNCTION}
    {

        //Statements of function.
        return CC_INVALID;

    }   */

void registerMISCFunctions()
{

    /*    registerHandler({PRIORITY_VAL}, {NETFN}}, {CMD_VAL}, {PRIVILEGE_LVL},
                    {HANDLER_FUNCTION});
    */
 
    return;
}

}