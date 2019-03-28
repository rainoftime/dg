#ifndef _DG_SYSTEM_DEPENDENCE_GRAPH_H_
#define _DG_SYSTEM_DEPENDENCE_GRAPH_H_

#include <memory>

#include "DependenceGraph.h"

namespace dg {
namespace sdg {

class DGNode;

class SystemDependenceGraph {
    std::set<DGNode *> _globals;
    DependenceGraph* _entry{nullptr};
    std::vector<std::unique_ptr<DependenceGraph>> _graphs;

public:
    DependenceGraph *getEntry() { return _entry; }
    const DependenceGraph *getEntry() const { return _entry; }
    void setEntry(DependenceGraph *g) { _entry = g; }

    DependenceGraph *createGraph() {
        _graphs.emplace_back(new DependenceGraph(_graphs.size(), this));
        return _graphs.back().get();
    }
};

} // namespace sdg
} // namespace dg

#endif // _DG_SYSTEM_DEPENDENCE_GRAPH_H_
