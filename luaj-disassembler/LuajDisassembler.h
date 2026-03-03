#pragma once

#include "LuajTypes.h"
#include "LuajAnalyzer.h"
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
        void printCode(const LuajPrototype& pt, const LuajAnalyzer& analyzer);
        void printConstants(const LuajPrototype& pt, const LuajAnalyzer& analyzer);
        void printLocals(const LuajPrototype& pt);
        void printUpvalues(const LuajPrototype& pt, const LuajAnalyzer& analyzer);

        std::string constantToString(const LuajConstant& k);
        void printOpcode(uint32_t i, int pc, const LuajPrototype& pt, const LuajAnalyzer& analyzer);
    };

}