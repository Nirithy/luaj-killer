#pragma once

#include "LuajTypes.h"
#include <fstream>
#include <string>

namespace Luaj {

    class LuajAssembler {
    public:
        LuajAssembler(const LuajHeader& header, LuajPrototype& mainPrototype);
        bool assemble(const std::string& filename);
        bool patchInstruction(int pc, uint32_t opcode, int a, int b, int c);
        std::string getError() const { return error_; }

    private:
        const LuajHeader& header_;
        LuajPrototype& mainPrototype_;
        std::ofstream file_;
        std::string error_;

        // Writing helpers
        void writeByte(uint8_t b);
        void writeInt(uint32_t val);
        void writeSizeT(uint64_t val);
        void writeDouble(double val);
        void writeString(const std::string& str);

        bool writeHeader();
        bool writeFunction(const LuajPrototype& pt, const std::string& parentSource);
        bool writeCode(const LuajPrototype& pt);
        bool writeConstants(const LuajPrototype& pt, const std::string& parentSource);
        bool writeUpvalues(const LuajPrototype& pt);
        bool writeDebug(const LuajPrototype& pt);

        uint32_t swapBytes(uint32_t val);
        uint64_t swapBytes(uint64_t val);

        bool isHostLittleEndian();
        bool swapNeeded();
    };

}
