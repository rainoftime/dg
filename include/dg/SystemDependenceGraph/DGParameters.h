#ifndef _DG_DEPENDENCE_GRAPH_PARAMS_H_
#define _DG_DEPENDENCE_GRAPH_PARAMS_H_

namespace dg {
namespace sdg {

class DGNode;
class DGNodeCall;
class DependenceGraph;

enum class DGParametersType {
    ACTUAL,
    FORMAL
};


///
// Base class for dg parameters
class DGParameters {
    DGParametersType _type;
protected:

    DGParameters(DGParametersType t) : _type(t) {}

public:
    DGParametersType getType() const { return _type; }
};

///
// Formal parameters assigned to dependence graphs
//
class DGFormalParameters : public DGParameters {
    DependenceGraph *_dg{nullptr};
public:
    DGFormalParameters(DependenceGraph *g)
    : DGParameters(DGParametersType::FORMAL), _dg(g) {}

    DependenceGraph *getDG() { return _dg; }
    const DependenceGraph *getDG() const { return _dg; }
};

///
// Actual parameters assigned to call nodes
//
class DGActualParameters : public DGParameters {
    DGNodeCall *_parent;
public:
    DGActualParameters(DGNodeCall *C)
    : DGParameters(DGParametersType::ACTUAL), _parent(C) {}

    DGNodeCall *getCall() { return _parent; }
    const DGNodeCall *getCall() const { return _parent; }
};


} // namespace sdg
} // namespace dg

#endif // _DG_DEPENDENCE_GRAPH_PARAMS_H_
