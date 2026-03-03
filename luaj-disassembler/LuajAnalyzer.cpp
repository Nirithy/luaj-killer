#include "LuajAnalyzer.h"
#include "LuajOpcodes.h"
#include <iostream>
#include <algorithm>

namespace Luaj {

    LuajAnalyzer::LuajAnalyzer(const LuajPrototype& pt) : pt_(pt) {}

    void LuajAnalyzer::analyze() {
        buildCFG();
        buildXREFs();
        buildDataFlow();
    }

    int LuajAnalyzer::getBlockIdByPc(int pc) const {
        for (size_t i = 0; i < basicBlocks_.size(); ++i) {
            if (pc >= basicBlocks_[i].start_pc && pc <= basicBlocks_[i].end_pc) {
                return basicBlocks_[i].id;
            }
        }
        return -1;
    }

    std::set<int> LuajAnalyzer::findLeaders() const {
        std::set<int> leaders;

        if (pt_.code.empty()) return leaders;

        // 1. First instruction is a leader
        leaders.insert(0);

        for (size_t i = 0; i < pt_.code.size(); ++i) {
            uint32_t inst = pt_.code[i];
            int o = GET_OPCODE(inst);

            // Branching instructions
            if (o == 23 || o == 32 || o == 33 || o == 35) { // OP_JMP, OP_FORLOOP, OP_FORPREP, OP_TFORLOOP
                int sbx = GETARG_sBx(inst);
                int target = i + 1 + sbx;

                // 2. Target of a jump is a leader
                if (target >= 0 && target < (int)pt_.code.size()) {
                    leaders.insert(target);
                }

                // 3. Instruction following a jump is a leader
                if (i + 1 < pt_.code.size()) {
                    leaders.insert(i + 1);
                }
            }

            // Conditional branches like EQ, LT, LE, TEST, TESTSET skip next instruction on jump
            if (o == 24 || o == 25 || o == 26 || o == 27 || o == 28) {
                // Next instruction is skipped if condition fails (usually followed by a JMP)
                // The instruction *after* the JMP (or whatever follows) is also a target technically
                if (i + 1 < pt_.code.size()) {
                    leaders.insert(i + 1);
                }
                if (i + 2 < pt_.code.size()) {
                    leaders.insert(i + 2);
                }
            }

            // Return instructions terminate block
            if (o == 31) { // OP_RETURN
                if (i + 1 < pt_.code.size()) {
                    leaders.insert(i + 1);
                }
            }
        }

        return leaders;
    }

    void LuajAnalyzer::buildCFG() {
        std::set<int> leaders = findLeaders();
        if (leaders.empty()) return;

        std::vector<int> sorted_leaders(leaders.begin(), leaders.end());

        for (size_t i = 0; i < sorted_leaders.size(); ++i) {
            BasicBlock bb;
            bb.id = i;
            bb.start_pc = sorted_leaders[i];
            bb.end_pc = (i + 1 < sorted_leaders.size()) ? sorted_leaders[i + 1] - 1 : pt_.code.size() - 1;
            basicBlocks_.push_back(bb);
        }

        for (size_t i = 0; i < basicBlocks_.size(); ++i) {
            BasicBlock& bb = basicBlocks_[i];
            int end_inst = pt_.code[bb.end_pc];
            int o = GET_OPCODE(end_inst);

            bool falls_through = true;

            if (o == 23 || o == 32 || o == 33 || o == 35) { // OP_JMP, OP_FORLOOP, OP_FORPREP, OP_TFORLOOP
                int sbx = GETARG_sBx(end_inst);
                int target = bb.end_pc + 1 + sbx;
                int target_block_id = getBlockIdByPc(target);

                if (target_block_id != -1) {
                    bb.successors.insert(target_block_id);
                    basicBlocks_[target_block_id].predecessors.insert(bb.id);
                }

                if (o == 23) { // Unconditional JMP doesn't fall through
                    falls_through = false;
                }
            } else if (o == 24 || o == 25 || o == 26 || o == 27 || o == 28) { // EQ, LT, LE, TEST, TESTSET
                // These instructions conditionally skip the next instruction.
                // The next instruction is typically a JMP.
                // Fall-through to next instruction is one path.
                // Skipping the next instruction is the other path.
                if ((size_t)bb.end_pc + 2 < pt_.code.size()) {
                    int skip_block_id = getBlockIdByPc(bb.end_pc + 2);
                    if (skip_block_id != -1) {
                        bb.successors.insert(skip_block_id);
                        basicBlocks_[skip_block_id].predecessors.insert(bb.id);
                    }
                }
            } else if (o == 31 || o == 30) { // OP_RETURN or OP_TAILCALL
                falls_through = false;
            }

            if (falls_through && (size_t)bb.end_pc + 1 < pt_.code.size()) {
                int next_block_id = getBlockIdByPc(bb.end_pc + 1);
                if (next_block_id != -1) {
                    bb.successors.insert(next_block_id);
                    basicBlocks_[next_block_id].predecessors.insert(bb.id);
                }
            }
        }
    }

    void LuajAnalyzer::buildXREFs() {
        for (size_t i = 0; i < pt_.code.size(); ++i) {
            uint32_t inst = pt_.code[i];
            int o = GET_OPCODE(inst);
            int b = GETARG_B(inst);
            int c = GETARG_C(inst);
            int bx = GETARG_Bx(inst);

            // Constants
            if (o == 1) { // LOADK
                xrefs_.constants_xrefs[bx].push_back(i);
            }
            if (getBMode(o) == OpArgK && ISK(b)) {
                xrefs_.constants_xrefs[INDEXK(b)].push_back(i);
            }
            if (getCMode(o) == OpArgK && ISK(c)) {
                xrefs_.constants_xrefs[INDEXK(c)].push_back(i);
            }

            // Upvalues
            if (o == 5 || o == 9) { // GETUPVAL, SETUPVAL
                xrefs_.upvalues_xrefs[b].push_back(i);
            }
            if (o == 6) { // GETTABUP
                xrefs_.upvalues_xrefs[b].push_back(i);

                // Check if it's a global access (upvalue is _ENV, usually named "_ENV" or index 0 for main chunk)
                // For a more robust check, we look if the upvalue name is "_ENV"
                if ((size_t)b < pt_.upvalues.size() && pt_.upvalues[b].name == "_ENV") {
                    if (ISK(c)) {
                        int const_idx = INDEXK(c);
                        if ((size_t)const_idx < pt_.constants.size() && pt_.constants[const_idx].type == TSTRING) {
                            std::string global_name = std::get<std::string>(pt_.constants[const_idx].value);
                            xrefs_.globals_xrefs[global_name].push_back(i);
                        }
                    }
                }
            }
            if (o == 8) { // SETTABUP (operand A is upvalue index)
                int a = GETARG_A(inst);
                xrefs_.upvalues_xrefs[a].push_back(i);

                // Check for global setting
                if ((size_t)a < pt_.upvalues.size() && pt_.upvalues[a].name == "_ENV") {
                    if (ISK(b)) {
                        int const_idx = INDEXK(b);
                        if ((size_t)const_idx < pt_.constants.size() && pt_.constants[const_idx].type == TSTRING) {
                            std::string global_name = std::get<std::string>(pt_.constants[const_idx].value);
                            xrefs_.globals_xrefs[global_name].push_back(i);
                        }
                    }
                }
            }

            // Functions
            if (o == 37) { // CLOSURE
                xrefs_.functions_xrefs[bx].push_back(i);
            }
        }
    }

    void LuajAnalyzer::buildDataFlow() {
        for (size_t i = 0; i < pt_.code.size(); ++i) {
            uint32_t inst = pt_.code[i];
            int o = GET_OPCODE(inst);
            int a = GETARG_A(inst);
            int b = GETARG_B(inst);
            int c = GETARG_C(inst);

            InstructionDataFlow df;

            // If instruction sets register A, it defines A
            if (testAMode(o)) {
                df.defs.push_back(a);
            } else {
                // Otherwise it probably uses A (unless A is not used, but safe to assume use for many)
                // Certain opcodes don't use A as a register but rather an upvalue/etc, but A is almost always a register if not defined.
                if (o != 8 && o != 9) { // SETTABUP and SETUPVAL use A as upvalue index, not register.
                    // Wait, SETTABUP: A is upvalue, B is key, C is value
                    // SETUPVAL: A is register containing value, B is upvalue index
                }
            }

            // More accurate register definitions/uses based on opcode
            switch (o) {
                case 0: // MOVE
                    df.defs.push_back(a);
                    df.uses.push_back(b);
                    break;
                case 1: // LOADK
                case 2: // LOADKX
                case 3: // LOADBOOL
                case 4: // LOADNIL
                case 5: // GETUPVAL
                case 37: // CLOSURE
                    df.defs.push_back(a);
                    break;
                case 6: // GETTABUP
                    df.defs.push_back(a);
                    if (!ISK(c)) df.uses.push_back(c);
                    break;
                case 7: // GETTABLE
                    df.defs.push_back(a);
                    df.uses.push_back(b);
                    if (!ISK(c)) df.uses.push_back(c);
                    break;
                case 8: // SETTABUP
                    if (!ISK(b)) df.uses.push_back(b);
                    if (!ISK(c)) df.uses.push_back(c);
                    break;
                case 9: // SETUPVAL
                    df.uses.push_back(a);
                    break;
                case 10: // SETTABLE
                    df.uses.push_back(a);
                    if (!ISK(b)) df.uses.push_back(b);
                    if (!ISK(c)) df.uses.push_back(c);
                    break;
                case 11: // NEWTABLE
                    df.defs.push_back(a);
                    break;
                case 12: // SELF
                    df.defs.push_back(a);
                    df.defs.push_back(a + 1);
                    df.uses.push_back(b);
                    if (!ISK(c)) df.uses.push_back(c);
                    break;
                case 13: // ADD
                case 14: // SUB
                case 15: // MUL
                case 16: // DIV
                case 17: // MOD
                case 18: // POW
                    df.defs.push_back(a);
                    if (!ISK(b)) df.uses.push_back(b);
                    if (!ISK(c)) df.uses.push_back(c);
                    break;
                case 19: // UNM
                case 20: // NOT
                case 21: // LEN
                    df.defs.push_back(a);
                    df.uses.push_back(b);
                    break;
                case 22: // CONCAT
                    df.defs.push_back(a);
                    for (int r = b; r <= c; ++r) {
                        df.uses.push_back(r);
                    }
                    break;
                case 23: // JMP
                    if (a > 0) {
                        // a-1 is closed
                    }
                    break;
                case 24: // EQ
                case 25: // LT
                case 26: // LE
                    if (!ISK(b)) df.uses.push_back(b);
                    if (!ISK(c)) df.uses.push_back(c);
                    break;
                case 27: // TEST
                    df.uses.push_back(a);
                    break;
                case 28: // TESTSET
                    df.defs.push_back(a);
                    df.uses.push_back(b);
                    break;
                case 29: // CALL
                case 30: // TAILCALL
                    df.uses.push_back(a);
                    if (b > 0) {
                        for (int r = a + 1; r < a + b; ++r) df.uses.push_back(r);
                    }
                    if (c > 0) {
                        for (int r = a; r < a + c - 1; ++r) df.defs.push_back(r);
                    }
                    break;
                case 31: // RETURN
                    if (b > 0) {
                        for (int r = a; r < a + b - 1; ++r) df.uses.push_back(r);
                    }
                    break;
                case 32: // FORLOOP
                    df.uses.push_back(a);
                    df.uses.push_back(a + 1);
                    df.uses.push_back(a + 2);
                    df.defs.push_back(a);
                    df.defs.push_back(a + 3);
                    break;
                case 33: // FORPREP
                    df.uses.push_back(a);
                    df.uses.push_back(a + 2);
                    df.defs.push_back(a);
                    break;
                case 34: // TFORCALL
                    for (int r = a; r < a + 3; ++r) df.uses.push_back(r);
                    for (int r = a + 3; r < a + 3 + c; ++r) df.defs.push_back(r);
                    break;
                case 35: // TFORLOOP
                    df.uses.push_back(a + 1);
                    df.defs.push_back(a);
                    break;
                case 36: // SETLIST
                    df.uses.push_back(a);
                    if (b > 0) {
                        for (int r = a + 1; r <= a + b; ++r) df.uses.push_back(r);
                    }
                    break;
                case 38: // VARARG
                    if (b > 0) {
                        for (int r = a; r < a + b - 1; ++r) df.defs.push_back(r);
                    }
                    break;
            }

            // Clean up: unique defs and uses, remove duplicates
            std::sort(df.defs.begin(), df.defs.end());
            df.defs.erase(std::unique(df.defs.begin(), df.defs.end()), df.defs.end());

            std::sort(df.uses.begin(), df.uses.end());
            df.uses.erase(std::unique(df.uses.begin(), df.uses.end()), df.uses.end());

            dataFlow_[i] = df;
        }
    }

}
