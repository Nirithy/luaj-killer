#include "LuajAssembler.h"
#include "LuajOpcodes.h"
#include <iostream>
#include <cstring>

namespace Luaj {

    LuajAssembler::LuajAssembler(const LuajHeader& header, LuajPrototype& mainPrototype)
        : header_(header), mainPrototype_(mainPrototype) {}

    bool LuajAssembler::patchInstruction(int pc, uint32_t opcode, int a, int b, int c) {
        if (pc < 1 || pc > (int)mainPrototype_.code.size()) {
            error_ = "PC out of bounds";
            return false;
        }

        uint32_t new_inst = 0;
        int mode = getOpMode(opcode);
        switch (mode) {
            case iABC:
                new_inst = CREATE_ABC(opcode, a, b, c);
                break;
            case iABx:
                new_inst = CREATE_ABx(opcode, a, b);
                break;
            case iAsBx:
                new_inst = CREATE_AsBx(opcode, a, b);
                break;
            case iAx:
                new_inst = CREATE_Ax(opcode, a);
                break;
        }

        // 0-based index
        mainPrototype_.code[pc - 1] = new_inst;
        return true;
    }

    bool LuajAssembler::isHostLittleEndian() {
        uint16_t x = 0x0001;
        return *(uint8_t*)&x == 0x01;
    }

    bool LuajAssembler::swapNeeded() {
        return (header_.endianness == 1) != isHostLittleEndian();
    }

    uint32_t LuajAssembler::swapBytes(uint32_t val) {
        if (!swapNeeded()) return val;
        return ((val << 24) & 0xff000000) |
               ((val <<  8) & 0x00ff0000) |
               ((val >>  8) & 0x0000ff00) |
               ((val >> 24) & 0x000000ff);
    }

    uint64_t LuajAssembler::swapBytes(uint64_t val) {
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

    void LuajAssembler::writeByte(uint8_t b) {
        file_.write(reinterpret_cast<const char*>(&b), 1);
    }

    void LuajAssembler::writeInt(uint32_t val) {
        if (header_.endianness == 1) { // Little Endian format in file
            uint8_t b[4];
            b[0] = val & 0xff;
            b[1] = (val >> 8) & 0xff;
            b[2] = (val >> 16) & 0xff;
            b[3] = (val >> 24) & 0xff;
            file_.write(reinterpret_cast<const char*>(b), 4);
        } else { // Big Endian format in file
            uint8_t b[4];
            b[0] = (val >> 24) & 0xff;
            b[1] = (val >> 16) & 0xff;
            b[2] = (val >> 8) & 0xff;
            b[3] = val & 0xff;
            file_.write(reinterpret_cast<const char*>(b), 4);
        }
    }

    void LuajAssembler::writeSizeT(uint64_t val) {
        if (header_.sizeof_sizet == 4) {
            writeInt(static_cast<uint32_t>(val));
        } else if (header_.sizeof_sizet == 8) {
            if (header_.endianness == 1) { // Little Endian
                writeInt(static_cast<uint32_t>(val & 0xffffffff));
                writeInt(static_cast<uint32_t>(val >> 32));
            } else { // Big Endian
                uint64_t swapped = swapBytes(val);
                file_.write(reinterpret_cast<const char*>(&swapped), 8);
            }
        }
    }

    void LuajAssembler::writeDouble(double val) {
        uint64_t d_val;
        std::memcpy(&d_val, &val, 8);
        if (header_.endianness == 1) { // Little Endian
            writeInt(static_cast<uint32_t>(d_val & 0xffffffff));
            writeInt(static_cast<uint32_t>(d_val >> 32));
        } else { // Big Endian
            uint64_t swapped = swapBytes(d_val);
            file_.write(reinterpret_cast<const char*>(&swapped), 8);
        }
    }

    void LuajAssembler::writeString(const std::string& str) {
        if (str.empty()) {
            writeSizeT(0);
        } else {
            uint64_t len = str.length() + 1; // Include '\0'
            writeSizeT(len);
            file_.write(str.c_str(), str.length());
            writeByte(0); // trailing '\0'
        }
    }

    bool LuajAssembler::assemble(const std::string& filename) {
        file_.open(filename, std::ios::binary);
        if (!file_.is_open()) {
            error_ = "Failed to open file for writing: " + filename;
            return false;
        }

        if (!writeHeader()) return false;
        if (!writeFunction(mainPrototype_, "")) return false;

        file_.close();
        return true;
    }

    bool LuajAssembler::writeHeader() {
        file_.write(reinterpret_cast<const char*>(header_.signature), 4);
        writeByte(header_.version);
        writeByte(header_.format);
        writeByte(header_.endianness);
        writeByte(header_.sizeof_int);
        writeByte(header_.sizeof_sizet);
        writeByte(header_.sizeof_instruction);
        writeByte(header_.sizeof_lua_number);
        writeByte(header_.number_format);
        file_.write(reinterpret_cast<const char*>(header_.tail), 6);
        return true;
    }

    bool LuajAssembler::writeFunction(const LuajPrototype& pt, const std::string& parentSource) {
        writeInt(pt.linedefined);
        writeInt(pt.lastlinedefined);
        writeByte(pt.numparams);
        writeByte(pt.is_vararg);
        writeByte(pt.maxstacksize);

        if (!writeCode(pt)) return false;
        if (!writeConstants(pt, parentSource)) return false;
        if (!writeUpvalues(pt)) return false;
        if (!writeDebug(pt)) return false;

        return true;
    }

    bool LuajAssembler::writeCode(const LuajPrototype& pt) {
        writeInt(pt.code.size());
        for (uint32_t inst : pt.code) {
            writeInt(inst);
        }
        return true;
    }

    bool LuajAssembler::writeConstants(const LuajPrototype& pt, const std::string& parentSource) {
        writeInt(pt.constants.size());
        for (const auto& k : pt.constants) {
            writeByte(static_cast<uint8_t>(k.type));
            switch (k.type) {
                case TNIL:
                    break;
                case TBOOLEAN:
                    writeByte(std::get<bool>(k.value) ? 1 : 0);
                    break;
                case TNUMBER: {
                    if (header_.number_format == 0 || header_.number_format == 4) { // floats/doubles
                        if (std::holds_alternative<double>(k.value)) {
                            writeDouble(std::get<double>(k.value));
                        } else if (std::holds_alternative<int32_t>(k.value)) {
                            writeDouble(static_cast<double>(std::get<int32_t>(k.value)));
                        } else {
                            writeDouble(0.0);
                        }
                    } else if (header_.number_format == 1) { // ints only
                        if (std::holds_alternative<int32_t>(k.value)) {
                            writeInt(std::get<int32_t>(k.value));
                        } else if (std::holds_alternative<double>(k.value)) {
                            writeInt(static_cast<int32_t>(std::get<double>(k.value)));
                        } else {
                            writeInt(0);
                        }
                    }
                    break;
                }
                case TSTRING:
                    if (std::holds_alternative<std::string>(k.value)) {
                        writeString(std::get<std::string>(k.value));
                    } else {
                        writeString("");
                    }
                    break;
                default:
                    error_ = "Unsupported constant type: " + std::to_string(k.type);
                    return false;
            }
        }

        writeInt(pt.prototypes.size());
        for (const auto& child : pt.prototypes) {
            if (!writeFunction(child, pt.source)) return false;
        }

        return true;
    }

    bool LuajAssembler::writeUpvalues(const LuajPrototype& pt) {
        writeInt(pt.upvalues.size());
        for (const auto& uv : pt.upvalues) {
            writeByte(uv.instack ? 1 : 0);
            writeByte(uv.idx);
        }
        return true;
    }

    bool LuajAssembler::writeDebug(const LuajPrototype& pt) {
        if (pt.source.empty()) {
            writeSizeT(0);
        } else {
            writeString(pt.source);
        }

        writeInt(pt.lineinfo.size());
        for (int line : pt.lineinfo) {
            writeInt(static_cast<uint32_t>(line));
        }

        writeInt(pt.locvars.size());
        for (const auto& locvar : pt.locvars) {
            writeString(locvar.varname);
            writeInt(locvar.startpc);
            writeInt(locvar.endpc);
        }

        writeInt(pt.upvalues.size());
        for (const auto& uv : pt.upvalues) {
            writeString(uv.name);
        }

        return true;
    }

}
