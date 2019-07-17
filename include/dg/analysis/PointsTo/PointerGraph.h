#ifndef _DG_POINTER_GRAPH_H_
#define _DG_POINTER_GRAPH_H_

#include "dg/ADT/Queue.h"
#include "dg/analysis/SubgraphNode.h"
#include "dg/analysis/CallGraph.h"
#include "dg/analysis/PointsTo/PSNode.h"
#include "dg/analysis/BFS.h"

#include <cassert>
#include <cstdarg>
#include <vector>
#include <memory>

namespace dg {
namespace analysis {
namespace pta {

// special nodes and pointers to them
extern PSNode *NULLPTR;
extern PSNode *UNKNOWN_MEMORY;
extern const Pointer NullPointer;
extern const Pointer UnknownPointer;

class PointerGraph;

// A single procedure in Pointer Graph
class PointerSubgraph {
    friend class PointerGraph;

    unsigned _id{0};

    PointerSubgraph(unsigned id, PSNode *r1,
                    PSNode *r2 = nullptr, PSNode *va = nullptr)
        : _id(id), root(r1), ret(r2), vararg(va) {}

    PointerSubgraph() = default;
    PointerSubgraph(const PointerSubgraph&) = delete;

public:
    PointerSubgraph(PointerSubgraph&&) = default;

    unsigned getID() const { return _id; }

    // first and last nodes of the subgraph
    PSNode *root{nullptr};
    PSNode *ret{nullptr};

    std::set<PSNode *> returnNodes;
    // this is the node where we gather the variadic-length arguments
    PSNode *vararg{nullptr};
};


///
// Basic graph for pointer analysis - contains CFG graphs for all procedures of the program.
class PointerGraph
{
    unsigned int dfsnum;

    // root of the pointer state subgraph
    PSNode *root;

    using NodesT = std::vector<std::unique_ptr<PSNode>>;
    using SubgraphsT = std::vector<std::unique_ptr<PointerSubgraph>>;

    NodesT nodes;
    SubgraphsT _subgraphs;

    // Take care of assigning ids to new nodes
    unsigned int last_node_id = 0;
    unsigned int getNewNodeId() {
        return ++last_node_id;
    }

    GenericCallGraph<PSNode *> callGraph;

    void initStaticNodes() {
        NULLPTR->pointsTo.clear();
        UNKNOWN_MEMORY->pointsTo.clear();
        NULLPTR->pointsTo.add(Pointer(NULLPTR, 0));
        UNKNOWN_MEMORY->pointsTo.add(Pointer(UNKNOWN_MEMORY, Offset::UNKNOWN));
    }

public:
    PointerGraph() : dfsnum(0), root(nullptr) {
        // nodes[0] represents invalid node (the node with id 0)
        nodes.emplace_back(nullptr);
        initStaticNodes();
    }

    bool registerCall(PSNode *a, PSNode *b) {
        return callGraph.addCall(a, b);
    }

    const GenericCallGraph<PSNode *>& getCallGraph() const { return callGraph; }

    const NodesT& getNodes() const { return nodes; }
    size_t size() const { return nodes.size(); }

    PointerGraph(PointerGraph&&) = default;
    PointerGraph& operator=(PointerGraph&&) = default;
    PointerGraph(const PointerGraph&) = delete;
    PointerGraph operator=(const PointerGraph&) = delete;

    PSNode *getRoot() const { return root; }
    void setRoot(PSNode *r) {
#if DEBUG_ENABLED
        bool found = false;
        for (auto& n : nodes) {
            if (n.get() == r) {
                found = true;
                break;
            }
        }
        assert(found && "The root lies outside of the graph");
#endif
        root = r;
    }

    void remove(PSNode *nd) {
        assert(nd && "nullptr passed as nd");
        // the node must be isolated
        assert(nd->successors.empty() && "The node is still in graph");
        assert(nd->predecessors.empty() && "The node is still in graph");
        assert(nd->getID() < size() && "Invalid ID");
        assert(nd->getID() > 0 && "Invalid ID");
        assert(nd->users.empty() && "This node is used by other nodes");
        // if the node has operands, it means that the operands
        // have a reference (an user edge to this node).
        // We do not want to create dangling references.
        assert(nd->operands.empty() && "This node uses other nodes");
        assert(nodes[nd->getID()].get() == nd && "Inconsistency in nodes");

        // clear the nodes entry
        nodes[nd->getID()].reset();
    }

    PointerSubgraph *createSubgraph(PSNode *root, PSNode *ret = nullptr,
                                    PSNode *vararg = nullptr) {
        // NOTE: id of the subgraph is always index in _subgraphs + 1
        _subgraphs.emplace_back(new PointerSubgraph(_subgraphs.size() + 1,
                                                    root, ret, vararg));
        return _subgraphs.back().get();
    }

    PSNode *create(PSNodeType t, ...) {
        va_list args;
        PSNode *node = nullptr;

        va_start(args, t);
        switch (t) {
            case PSNodeType::ALLOC:
            case PSNodeType::DYN_ALLOC:
                node = new PSNodeAlloc(getNewNodeId(), t);
                break;
            case PSNodeType::GEP:
                node = new PSNodeGep(getNewNodeId(),
                                     va_arg(args, PSNode *),
                                     va_arg(args, Offset::type));
                break;
            case PSNodeType::MEMCPY:
                node = new PSNodeMemcpy(getNewNodeId(),
                                        va_arg(args, PSNode *),
                                        va_arg(args, PSNode *),
                                        va_arg(args, Offset::type));
                break;
            case PSNodeType::CONSTANT:
                node = new PSNode(getNewNodeId(), PSNodeType::CONSTANT,
                                  va_arg(args, PSNode *),
                                  va_arg(args, Offset::type));
                break;
            case PSNodeType::ENTRY:
                node = new PSNodeEntry(getNewNodeId());
                break;
            case PSNodeType::CALL:
                node = new PSNodeCall(getNewNodeId());
                break;
            case PSNodeType::FORK:
                node = new PSNodeFork(getNewNodeId());
                break;
            case PSNodeType::JOIN:
                node = new PSNodeJoin(getNewNodeId());
                break;
            case PSNodeType::RETURN:
                node = new PSNodeRet(getNewNodeId(), args);
                break;
            default:
                node = new PSNode(getNewNodeId(), t, args);
                break;
        }
        va_end(args);

        assert(node && "Didn't created node");
        nodes.emplace_back(node);
        return node;
    }

    // get nodes in BFS order and store them into
    // the container
    template <typename ContainerOrNode>
    std::vector<PSNode *> getNodes(const ContainerOrNode& start,
                                   unsigned expected_num = 0)
    {
        ++dfsnum;

        std::vector<PSNode *> cont;
        if (expected_num != 0)
            cont.reserve(expected_num);

        struct DfsIdTracker {
            const unsigned dfsnum;
            DfsIdTracker(unsigned dnum) : dfsnum(dnum) {}

            void visit(PSNode *n) { n->dfsid = dfsnum; }
            bool visited(PSNode *n) const { return n->dfsid == dfsnum; }
        };

        DfsIdTracker visitTracker(dfsnum);
        BFS<PSNode, DfsIdTracker> bfs(visitTracker);

        bfs.run(start, [&cont](PSNode *n) { cont.push_back(n); });

        return cont;
    }

};

///
// get nodes reachable from n (including n),
// stop at node 'exit' (excluding) if not set to null
inline std::set<PSNode *>
getReachableNodes(PSNode *n,
                  PSNode *exit = nullptr)
{
    ADT::QueueFIFO<PSNode *> fifo;
    std::set<PSNode *> cont;

    assert(n && "No starting node given.");
    fifo.push(n);

    while (!fifo.empty()) {
        PSNode *cur = fifo.pop();
        if (!cont.insert(cur).second)
            continue; // we already visited this node

        for (PSNode *succ : cur->getSuccessors()) {
            assert(succ != nullptr);

            if (succ == exit)
                continue;

            fifo.push(succ);
        }
    }

    return cont;
}

} // namespace pta
} // namespace analysis
} // namespace dg

#endif
