#pragma once

#include "LuajTypes.h"
#include <vector>
#include <map>
#include <set>

namespace Luaj {

    struct BasicBlock {
        int id;
        int start_pc;
        int end_pc;
        std::set<int> predecessors;
        std::set<int> successors;
    };

    struct CrossReferences {
        std::map<int, std::vector<int>> constants_xrefs; // Constant index -> List of PCs
        std::map<int, std::vector<int>> upvalues_xrefs;  // Upvalue index -> List of PCs
        std::map<int, std::vector<int>> functions_xrefs; // Function index -> List of PCs
    };

    class LuajAnalyzer {
    public:
        LuajAnalyzer(const LuajPrototype& pt);

        void analyze();

        const std::vector<BasicBlock>& getBasicBlocks() const { return basicBlocks_; }
        const CrossReferences& getCrossReferences() const { return xrefs_; }

        // Helper to get basic block by pc
        int getBlockIdByPc(int pc) const;

    private:
        const LuajPrototype& pt_;
        std::vector<BasicBlock> basicBlocks_;
        CrossReferences xrefs_;

        void buildCFG();
        void buildXREFs();

        std::set<int> findLeaders() const;
    };

}
