#pragma once

#include "LuajTypes.h"
#include <fstream>
#include <string>

namespace Luaj {

    class LuajParser {
    public:
        LuajParser(const std::string& filename);
        bool parse();
        const LuajHeader& getHeader() const { return header_; }
        const LuajPrototype& getMainPrototype() const { return mainPrototype_; }
        LuajPrototype& getMainPrototype() { return mainPrototype_; }
        std::string getError() const { return error_; }

    private:
        std::ifstream file_;
        std::string error_;
        LuajHeader header_;
        LuajPrototype mainPrototype_;

        // Reading helpers
        uint8_t readByte();
        uint32_t readInt();
        uint64_t readSizeT();
        double readDouble();
        std::string readString();

        bool parseHeader();
        bool parseFunction(LuajPrototype& pt, const std::string& parentSource);
        bool parseCode(LuajPrototype& pt);
        bool parseConstants(LuajPrototype& pt, const std::string& parentSource);
        bool parseUpvalues(LuajPrototype& pt);
        bool parseDebug(LuajPrototype& pt);

        uint32_t swapBytes(uint32_t val);
        uint64_t swapBytes(uint64_t val);

        bool isHostLittleEndian();
        bool swapNeeded();
    };

}