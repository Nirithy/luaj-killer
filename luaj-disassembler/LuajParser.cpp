#include "LuajParser.h"
#include "LuajOpcodes.h"
#include <iostream>
#include <cstring>

namespace Luaj {

    LuajParser::LuajParser(const std::string& filename) {
        file_.open(filename, std::ios::binary);
        if (!file_.is_open()) {
            error_ = "Failed to open file: " + filename;
        }
    }

    bool LuajParser::isHostLittleEndian() {
        uint16_t x = 0x0001;
        return *(uint8_t*)&x == 0x01;
    }

    bool LuajParser::swapNeeded() {
        return (header_.endianness == 1) != isHostLittleEndian();
    }

    uint32_t LuajParser::swapBytes(uint32_t val) {
        if (!swapNeeded()) return val;
        return ((val << 24) & 0xff000000) |
               ((val <<  8) & 0x00ff0000) |
               ((val >>  8) & 0x0000ff00) |
               ((val >> 24) & 0x000000ff);
    }

    uint64_t LuajParser::swapBytes(uint64_t val) {
        if (!swapNeeded()) return val;
        return ((val << 56) & 0xff00000000000000ULL) |
               ((val << 40) & 0x00ff000000000000ULL) |
               ((val << 24) & 0x0000ff0000000000ULL) |
               ((val <<  8) & 0x000000ff00000000ULL) |
               ((val >>  8) & 0x00000000ff000000ULL) |
               ((val >> 24) & 0x0000000000ff0000ULL) |
               ((val >> 40) & 0x000000000000ff00ULL) |
               ((val >> 56) & 0x00000000000000ffULL);
    }

    uint8_t LuajParser::readByte() {
        uint8_t b;
        file_.read(reinterpret_cast<char*>(&b), 1);
        return b;
    }

    uint32_t LuajParser::readInt() {
        uint32_t val = 0;
        if (header_.endianness == 1) { // Little Endian format in file
            uint8_t b[4];
            file_.read(reinterpret_cast<char*>(b), 4);
            val = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
        } else { // Big Endian format in file
            uint8_t b[4];
            file_.read(reinterpret_cast<char*>(b), 4);
            val = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
        }
        return val;
    }

    uint64_t LuajParser::readSizeT() {
        uint64_t val = 0;
        if (header_.sizeof_sizet == 4) {
            val = readInt();
        } else if (header_.sizeof_sizet == 8) {
            if (header_.endianness == 1) { // Little Endian
                uint32_t l1 = readInt();
                uint32_t l2 = readInt();
                val = ((uint64_t)l2 << 32) | l1;
            } else { // Big Endian
                file_.read(reinterpret_cast<char*>(&val), 8);
                val = swapBytes(val);
            }
        }
        return val;
    }

    double LuajParser::readDouble() {
        uint64_t val = 0;
        if (header_.endianness == 1) { // Little Endian
            uint32_t l1 = readInt();
            uint32_t l2 = readInt();
            val = ((uint64_t)l2 << 32) | l1;
        } else { // Big Endian
            file_.read(reinterpret_cast<char*>(&val), 8);
            val = swapBytes(val); // Might need a separate read method for BE 64bit if we do this manually
        }
        double d;
        std::memcpy(&d, &val, 8);
        return d;
    }

    std::string LuajParser::readString() {
        uint64_t len = readSizeT();
        if (len == 0) return "";
        std::string str(len - 1, '\0');
        file_.read(&str[0], len - 1);
        readByte(); // read trailing '\0'
        return str;
    }

    bool LuajParser::parse() {
        if (!error_.empty()) return false;

        if (!parseHeader()) return false;
        if (!parseFunction(mainPrototype_, "")) return false;

        return true;
    }

    bool LuajParser::parseHeader() {
        file_.read(reinterpret_cast<char*>(header_.signature), 4);
        if (std::memcmp(header_.signature, "\033Lua", 4) != 0) {
            error_ = "Invalid Luaj signature";
            return false;
        }
        header_.version = readByte();
        header_.format = readByte();
        header_.endianness = readByte();
        header_.sizeof_int = readByte();
        header_.sizeof_sizet = readByte();
        header_.sizeof_instruction = readByte();
        header_.sizeof_lua_number = readByte();
        header_.number_format = readByte();
        file_.read(reinterpret_cast<char*>(header_.tail), 6);
        return true;
    }

    bool LuajParser::parseFunction(LuajPrototype& pt, const std::string& parentSource) {
        pt.linedefined = readInt();
        pt.lastlinedefined = readInt();
        pt.numparams = readByte();
        pt.is_vararg = readByte();
        pt.maxstacksize = readByte();

        if (!parseCode(pt)) return false;
        if (!parseConstants(pt, parentSource)) return false;
        if (!parseUpvalues(pt)) return false;
        if (!parseDebug(pt)) return false;

        return true;
    }

    bool LuajParser::parseCode(LuajPrototype& pt) {
        uint32_t n = readInt();
        pt.code.resize(n);
        for (uint32_t i = 0; i < n; i++) {
            pt.code[i] = readInt();
        }
        return true;
    }

    bool LuajParser::parseConstants(LuajPrototype& pt, const std::string& parentSource) {
        uint32_t n = readInt();
        pt.constants.resize(n);
        for (uint32_t i = 0; i < n; i++) {
            uint8_t type = readByte();
            int ctype = (int)(int8_t)type; // Cast to signed to match TINT
            pt.constants[i].type = ctype;

            switch (ctype) {
                case TNIL:
                    pt.constants[i].value = std::monostate{};
                    break;
                case TBOOLEAN:
                    pt.constants[i].value = (readByte() != 0);
                    break;
                case TNUMBER: {
                    if (header_.number_format == 0) { // floats/doubles
                        pt.constants[i].value = readDouble();
                    } else if (header_.number_format == 1) { // ints only
                        pt.constants[i].value = (int32_t)readInt();
                    } else if (header_.number_format == 4) { // num patch int32 fallback? (Double read)
                        pt.constants[i].value = readDouble();
                    } else {
                        error_ = "Unsupported number format: " + std::to_string(header_.number_format);
                        return false;
                    }
                    break;
                }
                case TINT: { // From num patch int32
                    pt.constants[i].value = (int32_t)readInt();
                    pt.constants[i].type = TNUMBER; // Treat as number
                    break;
                }
                case TSTRING:
                    pt.constants[i].value = readString();
                    break;
                default:
                    error_ = "Unsupported constant type: " + std::to_string(ctype);
                    return false;
            }
        }

        uint32_t np = readInt();
        pt.prototypes.resize(np);
        for (uint32_t i = 0; i < np; i++) {
            if (!parseFunction(pt.prototypes[i], pt.source)) return false;
        }

        return true;
    }

    bool LuajParser::parseUpvalues(LuajPrototype& pt) {
        uint32_t n = readInt();
        pt.upvalues.resize(n);
        for (uint32_t i = 0; i < n; i++) {
            pt.upvalues[i].instack = (readByte() != 0);
            pt.upvalues[i].idx = readByte();
        }
        return true;
    }

    bool LuajParser::parseDebug(LuajPrototype& pt) {
        // Look at Luaj DumpState to see how debug info is dumped
        // It uses dumpString for source (if not stripped) which uses readSizeT
        uint64_t sourceLenOrZero = readSizeT();
        if (sourceLenOrZero == 0) {
            pt.source = "";
        } else {
            std::string sourceStr(sourceLenOrZero - 1, '\0');
            file_.read(&sourceStr[0], sourceLenOrZero - 1);
            readByte(); // tail \0
            pt.source = sourceStr;
        }

        uint32_t numLineInfo = readInt();
        pt.lineinfo.resize(numLineInfo);
        for (uint32_t i = 0; i < numLineInfo; i++) {
            pt.lineinfo[i] = readInt();
        }

        uint32_t numLocVars = readInt();
        pt.locvars.resize(numLocVars);
        for (uint32_t i = 0; i < numLocVars; i++) {
            pt.locvars[i].varname = readString();
            pt.locvars[i].startpc = readInt();
            pt.locvars[i].endpc = readInt();
        }

        uint32_t numUpvalueNames = readInt();
        for (uint32_t i = 0; i < numUpvalueNames; i++) {
            std::string name = readString();
            if (i < pt.upvalues.size()) {
                pt.upvalues[i].name = name;
            }
        }

        return true;
    }
}