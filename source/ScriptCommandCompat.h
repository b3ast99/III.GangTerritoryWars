#pragma once

#include "DebugLog.h"
#include "eScriptCommands.h"

#if __has_include("scripting.h")
#include "scripting.h"
#define GTW_HAS_SCRIPTING_CALLCOMMAND 1
#endif

namespace ScriptCommandCompat {
    inline bool CreateCar(int modelId, float x, float y, float z, int* outHandle)
    {
#if defined(GTW_HAS_SCRIPTING_CALLCOMMAND)
        plugin::scripting::CallCommandById(static_cast<unsigned short>(COMMAND_CREATE_CAR), modelId, x, y, z, outHandle);
        return true;
#elif defined(GTW_USE_PLUGIN_COMMAND_TEMPLATE)
        Command<COMMAND_CREATE_CAR>(modelId, x, y, z, outHandle);
        return true;
#else
        (void)modelId; (void)x; (void)y; (void)z; (void)outHandle;
        static bool s_logged = false;
        if (!s_logged) {
            s_logged = true;
            DebugLog::Write("ScriptCommandCompat: no script command backend available. Define GTW_USE_PLUGIN_COMMAND_TEMPLATE or provide scripting.h");
        }
        return false;
#endif
    }

    inline bool SetCarHeading(int vehicleHandle, float heading)
    {
#if defined(GTW_HAS_SCRIPTING_CALLCOMMAND)
        plugin::scripting::CallCommandById(static_cast<unsigned short>(COMMAND_SET_CAR_HEADING), vehicleHandle, heading);
        return true;
#elif defined(GTW_USE_PLUGIN_COMMAND_TEMPLATE)
        Command<COMMAND_SET_CAR_HEADING>(vehicleHandle, heading);
        return true;
#else
        (void)vehicleHandle; (void)heading;
        return false;
#endif
    }

    inline bool DeleteCar(int vehicleHandle)
    {
#if defined(GTW_HAS_SCRIPTING_CALLCOMMAND)
        plugin::scripting::CallCommandById(static_cast<unsigned short>(COMMAND_DELETE_CAR), vehicleHandle);
        return true;
#elif defined(GTW_USE_PLUGIN_COMMAND_TEMPLATE)
        Command<COMMAND_DELETE_CAR>(vehicleHandle);
        return true;
#else
        (void)vehicleHandle;
        return false;
#endif
    }
}

