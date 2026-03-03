#pragma once

#include "LuajTypes.h"
#include "LuajAnalyzer.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace Luaj {

    class AstNode {
    public:
        virtual ~AstNode() = default;
        virtual std::string toString() const = 0;
    };

    class AstNumber : public AstNode {
    public:
        double value;
        AstNumber(double v) : value(v) {}
        std::string toString() const override;
    };

    class AstString : public AstNode {
    public:
        std::string value;
        AstString(const std::string& v) : value(v) {}
        std::string toString() const override;
    };

    class AstBoolean : public AstNode {
    public:
        bool value;
        AstBoolean(bool v) : value(v) {}
        std::string toString() const override;
    };

    class AstNil : public AstNode {
    public:
        std::string toString() const override { return "nil"; }
    };

    class AstIdentifier : public AstNode {
    public:
        std::string name;
        AstIdentifier(const std::string& n) : name(n) {}
        std::string toString() const override { return name; }
    };

    class AstAssignment : public AstNode {
    public:
        std::shared_ptr<AstNode> left;
        std::shared_ptr<AstNode> right;
        AstAssignment(std::shared_ptr<AstNode> l, std::shared_ptr<AstNode> r) : left(l), right(r) {}
        std::string toString() const override {
            return left->toString() + " = " + right->toString();
        }
    };

    class AstBinaryOp : public AstNode {
    public:
        std::string op;
        std::shared_ptr<AstNode> left;
        std::shared_ptr<AstNode> right;
        AstBinaryOp(const std::string& o, std::shared_ptr<AstNode> l, std::shared_ptr<AstNode> r)
            : op(o), left(l), right(r) {}
        std::string toString() const override {
            return "(" + left->toString() + " " + op + " " + right->toString() + ")";
        }
    };

    class AstUnaryOp : public AstNode {
    public:
        std::string op;
        std::shared_ptr<AstNode> expr;
        AstUnaryOp(const std::string& o, std::shared_ptr<AstNode> e) : op(o), expr(e) {}
        std::string toString() const override {
            return op + expr->toString();
        }
    };

    class AstFunctionCall : public AstNode {
    public:
        std::shared_ptr<AstNode> func;
        std::vector<std::shared_ptr<AstNode>> args;
        AstFunctionCall(std::shared_ptr<AstNode> f, const std::vector<std::shared_ptr<AstNode>>& a)
            : func(f), args(a) {}
        std::string toString() const override {
            std::string res = func->toString() + "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) res += ", ";
                res += args[i]->toString();
            }
            res += ")";
            return res;
        }
    };

    class AstTableAccess : public AstNode {
    public:
        std::shared_ptr<AstNode> table;
        std::shared_ptr<AstNode> key;
        AstTableAccess(std::shared_ptr<AstNode> t, std::shared_ptr<AstNode> k) : table(t), key(k) {}
        std::string toString() const override {
            // Simplified: could check if key is string to use dot notation
            return table->toString() + "[" + key->toString() + "]";
        }
    };

    class LuajDecompiler {
    public:
        LuajDecompiler(const LuajPrototype& pt, const LuajAnalyzer& analyzer);
        void decompile();

    private:
        const LuajPrototype& pt_;
        const LuajAnalyzer& analyzer_;
        std::map<int, std::shared_ptr<AstNode>> registers_; // pc to abstract value (very basic SSA)

        std::shared_ptr<AstNode> getConstantAst(int idx);
        std::shared_ptr<AstNode> getRK(int rk);
        std::string getUpvalueName(int idx);
    };

}
