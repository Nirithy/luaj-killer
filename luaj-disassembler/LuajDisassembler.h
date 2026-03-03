#pragma once

#include "LuajTypes.h"
#include <iostream>
#include <string>

namespace Luaj {

    class LuajDisassembler {
    public:
        LuajDisassembler(const LuajPrototype& mainPrototype);
        void disassemble();

    private:
        const LuajPrototype& mainPrototype_;

        void printFunction(const LuajPrototype& pt);
        void printHeader(const LuajPrototype& pt);
        void printCode(const LuajPrototype& pt);
        void printConstants(const LuajPrototype& pt);
        void printLocals(const LuajPrototype& pt);
        void printUpvalues(const LuajPrototype& pt);

        std::string constantToString(const LuajConstant& k);
        void printOpcode(uint32_t i, int pc, const LuajPrototype& pt);
    };

}