#ifndef _DG_DEPENDENCE_GRAPH_H_
#define _DG_DEPENDENCE_GRAPH_H_

namespace dg {
namespace sdg {

class DGNode;
class SystemDependenceGraph;

class DependenceGraph {
    friend class SystemDependenceGraph;

    unsigned _id{0};
    SystemDependenceGraph *_sdg{nullptr};

    std::set<DGNode *> _nodes;
    DependenceGraph(unsigned id, SystemDependenceGraph *g)
    : _id(id), _sdg(g) { assert(id > 0); }
};

} // namespace sdg
} // namespace dg

#endif // _DG_DEPENDENCE_GRAPH_H_
