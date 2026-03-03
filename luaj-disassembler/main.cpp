#include "LuajParser.h"
#include "LuajDisassembler.h"
#include "LuajAssembler.h"
#include "LuajOpcodes.h"
#include <iostream>
#include <iomanip>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [--dot <output.dot>] [--patch <output.luac>] <luac_file>\n";
        return 1;
    }

    std::string dot_filename = "";
    std::string patch_filename = "";
    std::string filename = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dot" && i + 1 < argc) {
            dot_filename = argv[++i];
        } else if (arg == "--patch" && i + 1 < argc) {
            patch_filename = argv[++i];
        } else {
            filename = arg;
        }
    }

    if (filename.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    Luaj::LuajParser parser(filename);

    if (!parser.parse()) {
        std::cerr << "Error parsing file: " << parser.getError() << "\n";
        return 1;
    }

    if (!patch_filename.empty()) {
        Luaj::LuajPrototype& pt = parser.getMainPrototype();

        // Example patch: add a new NOP-like (e.g. EXTRAARG) at the end,
        // or just re-assemble the file to test binary parity
        std::cout << "Applying patch and saving to " << patch_filename << "...\n";

        Luaj::LuajAssembler assembler(parser.getHeader(), pt);
        if (!assembler.assemble(patch_filename)) {
            std::cerr << "Error assembling file: " << assembler.getError() << "\n";
            return 1;
        }
        std::cout << "Successfully patched to " << patch_filename << "\n";
        return 0;
    }

    if (!dot_filename.empty()) {
        Luaj::LuajDisassembler disassembler(parser.getMainPrototype());
        disassembler.exportToDot(dot_filename);
        return 0;
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