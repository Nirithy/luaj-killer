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
        std::map<std::string, std::vector<int>> globals_xrefs; // Global name -> List of PCs
        std::map<std::string, std::vector<int>> exports_xrefs; // Exported keys -> List of PCs
    };

    struct InstructionDataFlow {
        std::vector<int> defs; // Registers defined (written to)
        std::vector<int> uses; // Registers used (read from)
        std::vector<int> constants; // Constants used
    };

    class LuajAnalyzer {
    public:
        LuajAnalyzer(const LuajPrototype& pt);

        void analyze();

        const std::vector<BasicBlock>& getBasicBlocks() const { return basicBlocks_; }
        const CrossReferences& getCrossReferences() const { return xrefs_; }
        const std::map<int, InstructionDataFlow>& getDataFlow() const { return dataFlow_; }

        // Helper to get basic block by pc
        int getBlockIdByPc(int pc) const;

    private:
        const LuajPrototype& pt_;
        std::vector<BasicBlock> basicBlocks_;
        CrossReferences xrefs_;
        std::map<int, InstructionDataFlow> dataFlow_;

        void buildCFG();
        void buildXREFs();
        void buildDataFlow();

        std::set<int> findLeaders() const;
    };

}
