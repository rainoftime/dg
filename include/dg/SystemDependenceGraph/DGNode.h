#ifndef _DG_DEPENDENCE_GRAPH_NODE_H_
#define _DG_DEPENDENCE_GRAPH_NODE_H_

#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif // not NDEBUG

#include "DGParameters.h"

namespace dg {
namespace sdg {

enum class DGNodeType {
        // Invalid node
        INVALID=0,
        // Ordinary instruction
        INSTRUCTION = 1,
        ARGUMENT,
        CALL
};

inline const char *DGNodeTypeToCString(enum DGNodeType type)
{
#define ELEM(t) case t: do {return (#t); }while(0); break;
    switch(type) {
        ELEM(DGNodeType::INVALID)
        ELEM(DGNodeType::INSTRUCTION)
        ELEM(DGNodeType::ARGUMENT)
        ELEM(DGNodeType::CALL)
            return "Unknown type";
    };
#undef ELEM
}

class DGNode {
    unsigned _id{0};
    DGNodeType _type;

protected:
    DGNode(unsigned id, DGNodeType t) : _id(id), _type(t) {}

public:
    virtual ~DGNode() = default;

    DGNodeType getType() const { return _type; }
    unsigned getID() const { return _id; }

#ifndef NDEBUG
    virtual void dump() const {
        std::cout << "<"<< getID() << "> " << DGNodeTypeToCString(getType());
    }

    // verbose dump
    virtual void dumpv() const {
        dump();
        std::cout << "\n";
    }
#endif // not NDEBUG
};

// check type of node
template <DGNodeType T> bool isa(DGNode *n) {
    return n->getType() == T;
}

class DependenceGraph;
class DGNodeCall;

/// ----------------------------------------------------------------------
// Instruction
/// ----------------------------------------------------------------------
class DGNodeInstruction : public DGNode {
    friend class DGNodeCall;

    DependenceGraph *_dg;

    // for call ctor
    DGNodeInstruction(unsigned id, DGNodeType t, DependenceGraph *dg)
    : DGNode(id, DGNodeType::CALL), _dg(dg) {
        assert(DGNodeType::CALL == t);
    }

public:
    DGNodeInstruction(unsigned id, DependenceGraph *dg)
    : DGNode(id, DGNodeType::INSTRUCTION), _dg(dg) {}

    static DGNodeInstruction *get(DGNode *n) {
        return (isa<DGNodeType::INSTRUCTION>(n) ||
                isa<DGNodeType::CALL>(n)) ?
            static_cast<DGNodeInstruction *>(n) : nullptr;
    }

    DependenceGraph *getDG() { return _dg; }
    const DependenceGraph *getDG() const { return _dg; }
};

/// ----------------------------------------------------------------------
// Call
/// ----------------------------------------------------------------------
class DGNodeCall : public DGNodeInstruction {
    // called functions
    std::set<DependenceGraph *> _callees{};
    // actual parameters of the call
    std::unique_ptr<DGParameters> _parameters{};

public:
    DGNodeCall(unsigned id, DependenceGraph *dg)
    : DGNodeInstruction(id, DGNodeType::CALL, dg) {}

    static DGNodeCall *get(DGNode *n) {
        return isa<DGNodeType::CALL>(n) ?
            static_cast<DGNodeCall *>(n) : nullptr;
    }

    const std::set<DependenceGraph *>& getCallees() const { return _callees; }
    bool addCalee(DependenceGraph *g) { return _callees.insert(g).second; }

    DGParameters *getParameters() { return _parameters.get(); }
    const DGParameters *getParameters() const { return _parameters.get(); }
};

/// ----------------------------------------------------------------------
// Argument
/// ----------------------------------------------------------------------
class DGNodeArgument : public DGNode {
    DGParameters *_parent{nullptr};

public:
    DGNodeArgument(unsigned id, DGParameters *p)
    : DGNode(id, DGNodeType::ARGUMENT), _parent(p) {}

    static DGNodeArgument *get(DGNode *n) {
        return isa<DGNodeType::ARGUMENT>(n) ?
            static_cast<DGNodeArgument *>(n) : nullptr;
    }
};


} // namespace sdg
} // namespace dg

#endif
