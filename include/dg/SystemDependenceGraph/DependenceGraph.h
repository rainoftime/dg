#ifndef _DG_DEPENDENCE_GRAPH_H_
#define _DG_DEPENDENCE_GRAPH_H_

#include "DGNode.h"

namespace dg {
namespace sdg {

class SystemDependenceGraph;

class DependenceGraph {
    friend class SystemDependenceGraph;

    unsigned _id{0};
    SystemDependenceGraph *_sdg{nullptr};

    std::vector<std::unique_ptr<DGNode>> _nodes;
    DependenceGraph(unsigned id, SystemDependenceGraph *g)
    : _id(id), _sdg(g) { assert(id > 0); }
public:

    DGNodeCall *createCall() {
        _nodes.emplace_back(new DGNodeCall(_nodes.size() + 1, this));
        return DGNodeCall::get(_nodes.back().get());
    }

    DGNodeInstruction *createInstruction() {
        _nodes.emplace_back(new DGNodeInstruction(_nodes.size() + 1, this));
        return DGNodeInstruction::get(_nodes.back().get());
    }
};

} // namespace sdg
} // namespace dg

#endif // _DG_DEPENDENCE_GRAPH_H_
