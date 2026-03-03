#include "LuajDisassembler.h"
#include "LuajOpcodes.h"
#include <iomanip>
#include <sstream>

namespace Luaj {

    LuajDisassembler::LuajDisassembler(const LuajPrototype& mainPrototype)
        : mainPrototype_(mainPrototype) {}

    void LuajDisassembler::disassemble() {
        printFunction(mainPrototype_);
    }

    void LuajDisassembler::printFunction(const LuajPrototype& pt) {
        printHeader(pt);

        LuajAnalyzer analyzer(pt);
        analyzer.analyze();

        printCode(pt, analyzer);
        printConstants(pt, analyzer);
        printLocals(pt);
        printUpvalues(pt, analyzer);

        for (size_t i = 0; i < pt.prototypes.size(); ++i) {
            const auto& child = pt.prototypes[i];
            const auto& fun_xrefs = analyzer.getCrossReferences().functions_xrefs;

            // Just display XREFs above the function block header
            auto it = fun_xrefs.find(i);
            if (it != fun_xrefs.end() && !it->second.empty()) {
                std::cout << "\n; XREFs to function " << i << ": ";
                for (size_t j = 0; j < it->second.size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << "loc_" << it->second[j] + 1;
                }
            }
            printFunction(child);
        }
    }

    void LuajDisassembler::printHeader(const LuajPrototype& pt) {
        std::cout << "\n";
        std::cout << "main <" << (pt.source.empty() ? "?" : pt.source)
                  << ":" << pt.linedefined << "," << pt.lastlinedefined << "> "
                  << "(" << pt.code.size() << " instructions)\n";
        std::cout << (int)pt.numparams << " params, "
                  << (int)pt.maxstacksize << " slots, "
                  << pt.upvalues.size() << " upvalues, "
                  << pt.locvars.size() << " locals, "
                  << pt.constants.size() << " constants, "
                  << pt.prototypes.size() << " functions\n";
    }

    std::string LuajDisassembler::constantToString(const LuajConstant& k) {
        if (k.type == TNIL) return "nil";
        if (k.type == TBOOLEAN) return std::get<bool>(k.value) ? "true" : "false";
        if (k.type == TNUMBER) {
            if (std::holds_alternative<double>(k.value)) {
                std::stringstream ss;
                ss << std::get<double>(k.value);
                return ss.str();
            } else if (std::holds_alternative<int32_t>(k.value)) {
                return std::to_string(std::get<int32_t>(k.value));
            }
        }
        if (k.type == TSTRING) {
            return "\"" + std::get<std::string>(k.value) + "\"";
        }
        return "?";
    }

    void LuajDisassembler::printCode(const LuajPrototype& pt, const LuajAnalyzer& analyzer) {
        const auto& blocks = analyzer.getBasicBlocks();
        for (const auto& bb : blocks) {
            std::cout << "\nloc_" << bb.start_pc + 1 << ":\n";
            if (!bb.predecessors.empty()) {
                std::cout << "  ; Predecessors: ";
                for (auto it = bb.predecessors.begin(); it != bb.predecessors.end(); ++it) {
                    if (it != bb.predecessors.begin()) std::cout << ", ";
                    std::cout << "loc_" << blocks[*it].start_pc + 1;
                }
                std::cout << "\n";
            }

            for (int pc = bb.start_pc; pc <= bb.end_pc; ++pc) {
                printOpcode(pt.code[pc], pc, pt, analyzer);
            }

            if (!bb.successors.empty()) {
                std::cout << "  ; Successors: ";
                for (auto it = bb.successors.begin(); it != bb.successors.end(); ++it) {
                    if (it != bb.successors.begin()) std::cout << ", ";
                    std::cout << "loc_" << blocks[*it].start_pc + 1;
                }
                std::cout << "\n";
            }
        }
    }

    void LuajDisassembler::printOpcode(uint32_t i, int pc, const LuajPrototype& pt, const LuajAnalyzer& analyzer) {
        int o = GET_OPCODE(i);
        int a = GETARG_A(i);
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        int bx = GETARG_Bx(i);
        int sbx = GETARG_sBx(i);
        int ax = GETARG_Ax(i);

        int line = (size_t)pc < pt.lineinfo.size() ? pt.lineinfo[pc] : 0;

        std::cout << "  " << pc + 1 << "  ";
        if (line > 0) std::cout << "[" << line << "]  ";
        else std::cout << "[-]  ";

        const char* name = (o < 40) ? OPNAMES[o] : "UNKNOWN";
        std::cout << std::left << std::setw(10) << name << " ";

        int mode = getOpMode(o);
        switch (mode) {
            case iABC:
                std::cout << a;
                if (getBMode(o) != OpArgN) std::cout << " " << (ISK(b) ? (-1 - INDEXK(b)) : b);
                if (getCMode(o) != OpArgN) std::cout << " " << (ISK(c) ? (-1 - INDEXK(c)) : c);
                break;
            case iABx:
                if (getBMode(o) == OpArgK) std::cout << a << " " << -1 - bx;
                else std::cout << a << " " << bx;
                break;
            case iAsBx:
                if (o == 23) // OP_JMP
                    std::cout << sbx;
                else
                    std::cout << a << " " << sbx;
                break;
            case iAx:
                std::cout << -1 - ax;
                break;
        }

        // If it's a jump, print the target loc
        if (o == 23 || o == 32 || o == 33 || o == 35) { // OP_JMP, OP_FORLOOP, OP_FORPREP, OP_TFORLOOP
            int target = pc + 1 + sbx;
            std::cout << "  ; -> loc_" << target + 1;
        }

        // Output some helpful comments based on instruction type
        switch (o) {
            case 1: // LOADK
                std::cout << "  ; " << constantToString(pt.constants[bx]);
                break;
            case 5: // GETUPVAL
            case 9: // SETUPVAL
                if ((size_t)b < pt.upvalues.size()) {
                    std::cout << "  ; " << pt.upvalues[b].name;
                }
                break;
            case 37: // CLOSURE
                if ((size_t)bx < pt.prototypes.size()) {
                    std::cout << "  ; " << std::hex << &pt.prototypes[bx] << std::dec;
                }
                break;
        }

        std::cout << "\n";
    }

    void LuajDisassembler::printConstants(const LuajPrototype& pt, const LuajAnalyzer& analyzer) {
        if (pt.constants.empty()) return;
        std::cout << "\nconstants (" << pt.constants.size() << ") for " << std::hex << &pt << std::dec << ":\n";
        const auto& const_xrefs = analyzer.getCrossReferences().constants_xrefs;

        for (size_t i = 0; i < pt.constants.size(); ++i) {
            std::cout << "\t" << i + 1 << "\t" << std::left << std::setw(20) << constantToString(pt.constants[i]);

            auto it = const_xrefs.find(i);
            if (it != const_xrefs.end() && !it->second.empty()) {
                std::cout << " ; XREFs: ";
                for (size_t j = 0; j < it->second.size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << "loc_" << it->second[j] + 1;
                }
            }
            std::cout << "\n";
        }
    }

    void LuajDisassembler::printLocals(const LuajPrototype& pt) {
        if (pt.locvars.empty()) return;
        std::cout << "\nlocals (" << pt.locvars.size() << ") for " << std::hex << &pt << std::dec << ":\n";
        for (size_t i = 0; i < pt.locvars.size(); ++i) {
            std::cout << "\t" << i << "\t" << pt.locvars[i].varname << "\t"
                      << "loc_" << pt.locvars[i].startpc + 1 << "\t" << "loc_" << pt.locvars[i].endpc + 1 << "\n";
        }
    }

    void LuajDisassembler::printUpvalues(const LuajPrototype& pt, const LuajAnalyzer& analyzer) {
        if (pt.upvalues.empty()) return;
        std::cout << "\nupvalues (" << pt.upvalues.size() << ") for " << std::hex << &pt << std::dec << ":\n";
        const auto& upval_xrefs = analyzer.getCrossReferences().upvalues_xrefs;

        for (size_t i = 0; i < pt.upvalues.size(); ++i) {
            std::string name = pt.upvalues[i].name.empty() ? "-" : pt.upvalues[i].name;
            std::cout << "\t" << i << "\t" << std::left << std::setw(10) << name
                      << "\t" << pt.upvalues[i].instack << "\t" << (int)pt.upvalues[i].idx;

            auto it = upval_xrefs.find(i);
            if (it != upval_xrefs.end() && !it->second.empty()) {
                std::cout << "\t; XREFs: ";
                for (size_t j = 0; j < it->second.size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << "loc_" << it->second[j] + 1;
                }
            }
            std::cout << "\n";
        }
    }

}