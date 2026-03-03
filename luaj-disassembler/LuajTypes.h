#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <variant>

namespace Luaj {

    struct LuajHeader {
        uint8_t signature[4];
        uint8_t version;
        uint8_t format;
        uint8_t endianness; // 0 for big, 1 for little
        uint8_t sizeof_int;
        uint8_t sizeof_sizet;
        uint8_t sizeof_instruction;
        uint8_t sizeof_lua_number;
        uint8_t number_format;
        uint8_t tail[6];
    };

    struct LuajConstant {
        int type;
        std::variant<std::monostate, bool, double, int32_t, std::string> value;
    };

    struct LuajUpvalue {
        bool instack;
        uint8_t idx;
        std::string name; // Assigned later from debug info if available
    };

    struct LuajLocVar {
        std::string varname;
        int startpc;
        int endpc;
    };

    struct LuajPrototype {
        std::string source;
        int linedefined;
        int lastlinedefined;
        uint8_t numparams;
        uint8_t is_vararg;
        uint8_t maxstacksize;

        std::vector<uint32_t> code;
        std::vector<LuajConstant> constants;
        std::vector<LuajPrototype> prototypes;
        std::vector<LuajUpvalue> upvalues;

        std::vector<int> lineinfo;
        std::vector<LuajLocVar> locvars;
    };

}