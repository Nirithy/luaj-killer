#include "LuajAnalyzer.h"
#include "LuajOpcodes.h"
#include <iostream>
#include <algorithm>

namespace Luaj {

    LuajAnalyzer::LuajAnalyzer(const LuajPrototype& pt) : pt_(pt) {}

    void LuajAnalyzer::analyze() {
        buildCFG();
        buildXREFs();
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
            }
            if (o == 8) { // SETTABUP (operand A is upvalue index)
                int a = GETARG_A(inst);
                xrefs_.upvalues_xrefs[a].push_back(i);
            }

            // Functions
            if (o == 37) { // CLOSURE
                xrefs_.functions_xrefs[bx].push_back(i);
            }
        }
    }

}
