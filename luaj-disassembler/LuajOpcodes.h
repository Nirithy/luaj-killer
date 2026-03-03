#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <variant>

// Constants for Lua limits and opcodes
namespace Luaj {

    const int TINT = -2;
    const int TNONE = -1;
    const int TNIL = 0;
    const int TBOOLEAN = 1;
    const int TLIGHTUSERDATA = 2;
    const int TNUMBER = 3;
    const int TSTRING = 4;
    const int TTABLE = 5;
    const int TFUNCTION = 6;
    const int TUSERDATA = 7;
    const int TTHREAD = 8;
    const int TVALUE = 9;

    // From Lua.java
    const int SIZE_C = 9;
    const int SIZE_B = 9;
    const int SIZE_Bx = (SIZE_C + SIZE_B);
    const int SIZE_A = 8;
    const int SIZE_Ax = (SIZE_C + SIZE_B + SIZE_A);

    const int SIZE_OP = 6;

    const int POS_OP = 0;
    const int POS_A = (POS_OP + SIZE_OP);
    const int POS_C = (POS_A + SIZE_A);
    const int POS_B = (POS_C + SIZE_C);
    const int POS_Bx = POS_C;
    const int POS_Ax = POS_A;

    const int MAXARG_Bx = ((1 << SIZE_Bx) - 1);
    const int MAXARG_sBx = (MAXARG_Bx >> 1);

    inline int GET_OPCODE(uint32_t i) { return (i >> POS_OP) & ((1 << SIZE_OP) - 1); }
    inline int GETARG_A(uint32_t i) { return (i >> POS_A) & ((1 << SIZE_A) - 1); }
    inline int GETARG_B(uint32_t i) { return (i >> POS_B) & ((1 << SIZE_B) - 1); }
    inline int GETARG_C(uint32_t i) { return (i >> POS_C) & ((1 << SIZE_C) - 1); }
    inline int GETARG_Bx(uint32_t i) { return (i >> POS_Bx) & ((1 << SIZE_Bx) - 1); }
    inline int GETARG_sBx(uint32_t i) { return GETARG_Bx(i) - MAXARG_sBx; }
    inline int GETARG_Ax(uint32_t i) { return (i >> POS_Ax) & ((1 << SIZE_Ax) - 1); }

    const char* const OPNAMES[] = {
        "MOVE",
        "LOADK",
        "LOADKX",
        "LOADBOOL",
        "LOADNIL",
        "GETUPVAL",
        "GETTABUP",
        "GETTABLE",
        "SETTABUP",
        "SETUPVAL",
        "SETTABLE",
        "NEWTABLE",
        "SELF",
        "ADD",
        "SUB",
        "MUL",
        "DIV",
        "MOD",
        "POW",
        "UNM",
        "NOT",
        "LEN",
        "CONCAT",
        "JMP",
        "EQ",
        "LT",
        "LE",
        "TEST",
        "TESTSET",
        "CALL",
        "TAILCALL",
        "RETURN",
        "FORLOOP",
        "FORPREP",
        "TFORCALL",
        "TFORLOOP",
        "SETLIST",
        "CLOSURE",
        "VARARG",
        "EXTRAARG",
        nullptr
    };

    enum OpMode { iABC = 0, iABx = 1, iAsBx = 2, iAx = 3 };

    const int OpArgN = 0;  /* argument is not used */
    const int OpArgU = 1;  /* argument is used */
    const int OpArgR = 2;  /* argument is a register or a jump offset */
    const int OpArgK = 3;  /* argument is a constant or register/constant */

    const uint32_t luaP_opmodes[] = {
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgN << 2) | (iABC),      /* OP_MOVE */
        (0 << 7) | (1 << 6) | (OpArgK << 4) | (OpArgN << 2) | (iABx),      /* OP_LOADK */
        (0 << 7) | (1 << 6) | (OpArgN << 4) | (OpArgN << 2) | (iABx),      /* OP_LOADKX */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgU << 2) | (iABC),      /* OP_LOADBOOL */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgN << 2) | (iABC),      /* OP_LOADNIL */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgN << 2) | (iABC),      /* OP_GETUPVAL */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgK << 2) | (iABC),      /* OP_GETTABUP */
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgK << 2) | (iABC),      /* OP_GETTABLE */
        (0 << 7) | (0 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_SETTABUP */
        (0 << 7) | (0 << 6) | (OpArgU << 4) | (OpArgN << 2) | (iABC),      /* OP_SETUPVAL */
        (0 << 7) | (0 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_SETTABLE */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgU << 2) | (iABC),      /* OP_NEWTABLE */
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgK << 2) | (iABC),      /* OP_SELF */
        (0 << 7) | (1 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_ADD */
        (0 << 7) | (1 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_SUB */
        (0 << 7) | (1 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_MUL */
        (0 << 7) | (1 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_DIV */
        (0 << 7) | (1 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_MOD */
        (0 << 7) | (1 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_POW */
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgN << 2) | (iABC),      /* OP_UNM */
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgN << 2) | (iABC),      /* OP_NOT */
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgN << 2) | (iABC),      /* OP_LEN */
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgR << 2) | (iABC),      /* OP_CONCAT */
        (0 << 7) | (0 << 6) | (OpArgR << 4) | (OpArgN << 2) | (iAsBx),     /* OP_JMP */
        (1 << 7) | (0 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_EQ */
        (1 << 7) | (0 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_LT */
        (1 << 7) | (0 << 6) | (OpArgK << 4) | (OpArgK << 2) | (iABC),      /* OP_LE */
        (1 << 7) | (0 << 6) | (OpArgN << 4) | (OpArgU << 2) | (iABC),      /* OP_TEST */
        (1 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgU << 2) | (iABC),      /* OP_TESTSET */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgU << 2) | (iABC),      /* OP_CALL */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgU << 2) | (iABC),      /* OP_TAILCALL */
        (0 << 7) | (0 << 6) | (OpArgU << 4) | (OpArgN << 2) | (iABC),      /* OP_RETURN */
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgN << 2) | (iAsBx),     /* OP_FORLOOP */
        (0 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgN << 2) | (iAsBx),     /* OP_FORPREP */
        (0 << 7) | (0 << 6) | (OpArgN << 4) | (OpArgU << 2) | (iABC),      /* OP_TFORCALL */
        (1 << 7) | (1 << 6) | (OpArgR << 4) | (OpArgN << 2) | (iAsBx),     /* OP_TFORLOOP */
        (0 << 7) | (0 << 6) | (OpArgU << 4) | (OpArgU << 2) | (iABC),      /* OP_SETLIST */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgN << 2) | (iABx),      /* OP_CLOSURE */
        (0 << 7) | (1 << 6) | (OpArgU << 4) | (OpArgN << 2) | (iABC),      /* OP_VARARG */
        (0 << 7) | (0 << 6) | (OpArgU << 4) | (OpArgU << 2) | (iAx),       /* OP_EXTRAARG */
    };

    inline int getOpMode(int m) { return luaP_opmodes[m] & 3; }
    inline int getBMode(int m) { return (luaP_opmodes[m] >> 4) & 3; }
    inline int getCMode(int m) { return (luaP_opmodes[m] >> 2) & 3; }
    inline bool testAMode(int m) { return (luaP_opmodes[m] & (1 << 6)) != 0; }
    inline bool testTMode(int m) { return (luaP_opmodes[m] & (1 << 7)) != 0; }

    inline bool ISK(int x) { return x >= (1 << (SIZE_B - 1)); }
    inline int INDEXK(int r) { return r & ~(1 << (SIZE_B - 1)); }

}
