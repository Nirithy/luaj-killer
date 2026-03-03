#include "LuajDisassembler.h"
#include "LuajOpcodes.h"
#include <iomanip>
#include <sstream>
#include <fstream>

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
        printGlobals(pt, analyzer);
        printExports(pt, analyzer);

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

        const auto& df = analyzer.getDataFlow();
        auto df_it = df.find(pc);
        if (df_it != df.end() && (!df_it->second.defs.empty() || !df_it->second.uses.empty())) {
            std::cout << "  ; [";
            if (!df_it->second.defs.empty()) {
                std::cout << "Defs: ";
                for (size_t i = 0; i < df_it->second.defs.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << "R" << df_it->second.defs[i];
                }
                if (!df_it->second.uses.empty()) std::cout << " ";
            }
            if (!df_it->second.uses.empty()) {
                std::cout << "Uses: ";
                for (size_t i = 0; i < df_it->second.uses.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << "R" << df_it->second.uses[i];
                }
            }
            std::cout << "]";
        }

        std::cout << "\n";
    }

    std::string LuajDisassembler::getOpcodeString(uint32_t i, int pc, const LuajPrototype& pt, const LuajAnalyzer& analyzer) {
        std::stringstream ss;
        int o = GET_OPCODE(i);
        int a = GETARG_A(i);
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        int bx = GETARG_Bx(i);
        int sbx = GETARG_sBx(i);
        int ax = GETARG_Ax(i);

        ss << pc + 1 << "  ";
        const char* name = (o < 40) ? OPNAMES[o] : "UNKNOWN";
        ss << std::left << std::setw(10) << name << " ";

        int mode = getOpMode(o);
        switch (mode) {
            case iABC:
                ss << a;
                if (getBMode(o) != OpArgN) ss << " " << (ISK(b) ? (-1 - INDEXK(b)) : b);
                if (getCMode(o) != OpArgN) ss << " " << (ISK(c) ? (-1 - INDEXK(c)) : c);
                break;
            case iABx:
                if (getBMode(o) == OpArgK) ss << a << " " << -1 - bx;
                else ss << a << " " << bx;
                break;
            case iAsBx:
                if (o == 23) ss << sbx;
                else ss << a << " " << sbx;
                break;
            case iAx:
                ss << -1 - ax;
                break;
        }

        if (o == 23 || o == 32 || o == 33 || o == 35) {
            int target = pc + 1 + sbx;
            ss << "  ; -> loc_" << target + 1;
        }

        switch (o) {
            case 1:
                ss << "  ; " << constantToString(pt.constants[bx]);
                break;
            case 5:
            case 9:
                if ((size_t)b < pt.upvalues.size()) {
                    ss << "  ; " << pt.upvalues[b].name;
                }
                break;
        }

        return ss.str();
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

    void LuajDisassembler::printExports(const LuajPrototype& pt, const LuajAnalyzer& analyzer) {
        const auto& exports_xrefs = analyzer.getCrossReferences().exports_xrefs;
        if (exports_xrefs.empty()) return;

        std::cout << "\nexports (" << exports_xrefs.size() << ") for " << std::hex << &pt << std::dec << ":\n";

        for (auto const& [name, pcs] : exports_xrefs) {
            std::cout << "\t" << std::left << std::setw(20) << name;
            std::cout << " ; XREFs: ";
            for (size_t j = 0; j < pcs.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << "loc_" << pcs[j] + 1;
            }
            std::cout << "\n";
        }
    }

    void LuajDisassembler::printGlobals(const LuajPrototype& pt, const LuajAnalyzer& analyzer) {
        const auto& globals_xrefs = analyzer.getCrossReferences().globals_xrefs;
        if (globals_xrefs.empty()) return;

        std::cout << "\nglobals (" << globals_xrefs.size() << ") for " << std::hex << &pt << std::dec << ":\n";

        for (auto const& [name, pcs] : globals_xrefs) {
            std::cout << "\t" << std::left << std::setw(20) << name;
            std::cout << " ; XREFs: ";
            for (size_t j = 0; j < pcs.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << "loc_" << pcs[j] + 1;
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

    void LuajDisassembler::exportToDot(const std::string& filename) {
        std::ofstream out(filename);
        if (!out) {
            std::cerr << "Error: Could not open file " << filename << " for writing DOT graph.\n";
            return;
        }

        out << "digraph LuajCFG {\n";
        out << "  node [shape=box, fontname=\"Courier New\"];\n";

        int cluster_id = 0;
        exportFunctionToDot(mainPrototype_, out, cluster_id);

        out << "}\n";
        out.close();
        std::cout << "Exported CFG to " << filename << "\n";
    }

    void LuajDisassembler::exportFunctionToDot(const LuajPrototype& pt, std::ostream& out, int& cluster_id) {
        LuajAnalyzer analyzer(pt);
        analyzer.analyze();

        int current_cluster = cluster_id++;

        out << "  subgraph cluster_" << current_cluster << " {\n";
        out << "    label=\"Function at line " << pt.linedefined << "\";\n";

        const auto& blocks = analyzer.getBasicBlocks();
        std::string prefix = "f" + std::to_string(current_cluster) + "_";

        // Define nodes
        for (const auto& bb : blocks) {
            out << "    " << prefix << "bb_" << bb.id << " [label=\"loc_" << bb.start_pc + 1 << ":\\l";
            for (int pc = bb.start_pc; pc <= bb.end_pc; ++pc) {
                // Escape quotes for dot
                std::string inst_str = getOpcodeString(pt.code[pc], pc, pt, analyzer);
                size_t pos = 0;
                while((pos = inst_str.find("\"", pos)) != std::string::npos) {
                    inst_str.replace(pos, 1, "\\\"");
                    pos += 2;
                }
                out << inst_str << "\\l";
            }
            out << "\"];\n";
        }

        // Define edges
        for (const auto& bb : blocks) {
            for (int succ : bb.successors) {
                out << "    " << prefix << "bb_" << bb.id << " -> " << prefix << "bb_" << succ << ";\n";
            }
        }

        out << "  }\n";

        for (const auto& child : pt.prototypes) {
            exportFunctionToDot(child, out, cluster_id);
        }
    }

}