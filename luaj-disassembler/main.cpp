#include "LuajParser.h"
#include "LuajDisassembler.h"
#include <iostream>
#include <iomanip>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <luac_file>\n";
        return 1;
    }

    std::string filename = argv[1];
    Luaj::LuajParser parser(filename);

    if (!parser.parse()) {
        std::cerr << "Error parsing file: " << parser.getError() << "\n";
        return 1;
    }

    // Optional: Print header information
    const auto& header = parser.getHeader();
    std::cout << "Luaj File Header:\n";
    std::cout << "  Version: 0x" << std::hex << (int)header.version << std::dec << "\n";
    std::cout << "  Format: " << (int)header.format << "\n";
    std::cout << "  Endianness: " << (header.endianness == 0 ? "Big Endian" : "Little Endian") << "\n";
    std::cout << "  sizeof(int): " << (int)header.sizeof_int << "\n";
    std::cout << "  sizeof(size_t): " << (int)header.sizeof_sizet << "\n";
    std::cout << "  sizeof(Instruction): " << (int)header.sizeof_instruction << "\n";
    std::cout << "  sizeof(lua_Number): " << (int)header.sizeof_lua_number << "\n";
    std::cout << "  Number Format: " << (int)header.number_format << "\n";

    Luaj::LuajDisassembler disassembler(parser.getMainPrototype());
    disassembler.disassemble();

    return 0;
}