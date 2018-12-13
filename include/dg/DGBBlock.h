#ifndef _DG_BBLOCK_H_
#define _DG_BBLOCK_H_

#include <cassert>
#include <list>

#include "ADT/DGContainer.h"
#include "analysis/DependenceGraph/Analysis.h"

namespace dg {

/// ------------------------------------------------------------------
// - DGBBlock
//     Basic block structure for dependence graph
/// ------------------------------------------------------------------
template <typename NodeT>
class DGBBlock
{
public:
    using KeyT = typename NodeT::KeyType;
    using DependenceGraphT = typename NodeT::DependenceGraphType;

    struct DGBBlockEdge {
        DGBBlockEdge(DGBBlock<NodeT>* t, uint8_t label = 0)
            : target(t), label(label) {}

        DGBBlock<NodeT> *target;
        // we'll have just numbers as labels now.
        // We can change it if there's a need
        uint8_t label;

        bool operator==(const DGBBlockEdge& oth) const
        {
            return target == oth.target && label == oth.label;
        }

        bool operator!=(const DGBBlockEdge& oth) const
        {
            return operator==(oth);
        }

        bool operator<(const DGBBlockEdge& oth) const
        {
            return target == oth.target ?
                        label < oth.label : target < oth.target;
        }
    };

    DGBBlock<NodeT>(NodeT *head = nullptr, DependenceGraphT *dg = nullptr)
        : key(KeyT()), dg(dg), ipostdom(nullptr), slice_id(0)
    {
        if (head) {
            append(head);
            assert(!dg || head->getDG() == nullptr || dg == head->getDG());
        }
    }

    ~DGBBlock<NodeT>() {
        if (delete_nodes_on_destr) {
            for (NodeT *nd : nodes)
                delete nd;
        }
    }

    using DGBBlockContainerT = EdgesContainer<DGBBlock<NodeT>>;
    // we don't need labels with predecessors
    using PredContainerT = EdgesContainer<DGBBlock<NodeT>>;
    using SuccContainerT = DGContainer<DGBBlockEdge>;

    SuccContainerT& successors() { return nextBBs; }
    const SuccContainerT& successors() const { return nextBBs; }

    PredContainerT& predecessors() { return prevBBs; }
    const PredContainerT& predecessors() const { return prevBBs; }

    const DGBBlockContainerT& controlDependence() const { return controlDeps; }
    const DGBBlockContainerT& revControlDependence() const { return revControlDeps; }

    // similary to nodes, basic blocks can have keys
    // they are not stored anywhere, it is more due to debugging
    void setKey(const KeyT& k) { key = k; }
    const KeyT& getKey() const { return key; }

    // XXX we should do it a common base with node
    // to not duplicate this - something like
    // GraphElement that would contain these attributes
    void setDG(DependenceGraphT *d) { dg = d; }
    DependenceGraphT *getDG() const { return dg; }

    const std::list<NodeT *>& getNodes() const { return nodes; }
    std::list<NodeT *>& getNodes() { return nodes; }
    bool empty() const { return nodes.empty(); }
    size_t size() const { return nodes.size(); }

    void append(NodeT *n)
    {
        assert(n && "Cannot add null node to DGBBlock");

        n->setBasicBlock(this);
        nodes.push_back(n);
    }

    void prepend(NodeT *n)
    {
        assert(n && "Cannot add null node to DGBBlock");

        n->setBasicBlock(this);
        nodes.push_front(n);
    }

    bool hasControlDependence() const
    {
        return !controlDeps.empty();
    }

    // return true if all successors point
    // to the same basic block (not considering labels,
    // just the targets)
    bool successorsAreSame() const
    {
        if (nextBBs.size() < 2)
            return true;

        typename SuccContainerT::const_iterator start, iter, end;
        iter = nextBBs.begin();
        end = nextBBs.end();

        DGBBlock<NodeT> *block = iter->target;
        // iterate over all successor and
        // check if they are all the same
        for (++iter; iter != end; ++iter)
            if (iter->target != block)
                return false;

        return true;
    }

    // remove all edges from/to this BB and reconnect them to
    // other nodes
    void isolate()
    {
        // take every predecessor and reconnect edges from it
        // to successors
        for (DGBBlock<NodeT> *pred : prevBBs) {
            // find the edge that is going to this node
            // and create new edges to all successors. The new edges
            // will have the same label as the found one
            DGContainer<DGBBlockEdge> new_edges;
            for (auto I = pred->nextBBs.begin(),E = pred->nextBBs.end(); I != E;) {
                auto cur = I++;
                if (cur->target == this) {
                    // create edges that will go from the predecessor
                    // to every successor of this node
                    for (const DGBBlockEdge& succ : nextBBs) {
                        // we cannot create an edge to this bblock (we're isolating _this_ bblock),
                        // that would be incorrect. It can occur when we're isolatin a bblock
                        // with self-loop
                        if (succ.target != this)
                            new_edges.insert(DGBBlockEdge(succ.target, cur->label));
                    }

                    // remove the edge from predecessor
                    pred->nextBBs.erase(*cur);
                }
            }

            // add newly created edges to predecessor
            for (const DGBBlockEdge& edge : new_edges) {
                assert(edge.target != this
                       && "Adding an edge to a block that is being isolated");
                pred->addSuccessor(edge);
            }
        }

        removeSuccessors();

        // NOTE: nextBBs were cleared in removeSuccessors()
        prevBBs.clear();

        // remove reverse edges to this BB
        for (DGBBlock<NodeT> *B : controlDeps) {
            // we don't want to corrupt the iterator
            // if this block is control dependent on itself.
            // We're gonna clear it anyway
            if (B == this)
                continue;

            B->revControlDeps.erase(this);
        }

        // clear also cd edges that blocks have
        // to this block
        for (DGBBlock<NodeT> *B : revControlDeps) {
            if (B == this)
                continue;

            B->controlDeps.erase(this);
        }

        revControlDeps.clear();
        controlDeps.clear();
    }

    void remove(bool with_nodes = true)
    {
        // do not leave any dangling reference
        isolate();

        if (dg) {
#ifndef NDEBUG
            bool ret =
#endif
            dg->removeBlock(key);
            assert(ret && "BUG: block was not in DG");
            if (dg->getEntryBB() == this)
                dg->setEntryBB(nullptr);
        }

        if (with_nodes) {
            for (NodeT *n : nodes) {
                // we must set basic block to nullptr
                // otherwise the node will try to remove the
                // basic block again if it is of size 1
                n->setBasicBlock(nullptr);

                // remove dependency edges, let be CFG edges
                // as we'll destroy all the nodes
                n->removeCDs();
                n->removeDDs();
                // remove the node from dg
                assert(n->getDG());
                n->getDG()->removeNode(n);

                delete n;
            }
        }

        delete this;
    }

    void removeNode(NodeT *n) { nodes.remove(n); }

    size_t successorsNum() const { return nextBBs.size(); }
    size_t predecessorsNum() const { return prevBBs.size(); }

    bool addSuccessor(const DGBBlockEdge& edge)
    {
        bool ret = nextBBs.insert(edge);
        edge.target->prevBBs.insert(this);

        return ret;
    }

    bool addSuccessor(DGBBlock<NodeT> *b, uint8_t label = 0)
    {
        return addSuccessor(DGBBlockEdge(b, label));
    }

    void removeSuccessors()
    {
        // remove references to this node from successors
        for (const DGBBlockEdge& succ : nextBBs) {
            // This assertion does not hold anymore, since if we have
            // two edges with different labels to the same successor,
            // and we remove the successor, then we remove 'this'
            // from prevBBs twice. If we'll add labels even to predecessors,
            // this assertion must hold again
            // bool ret = succ.target->prevBBs.erase(this);
            // assert(ret && "Did not have this BB in successor's pred");
            succ.target->prevBBs.erase(this);
        }

        nextBBs.clear();
    }

    bool hasSelfLoop()
    {
        return nextBBs.contains(this);
    }

    void removeSuccessor(const DGBBlockEdge& succ)
    {
        succ.target->prevBBs.erase(this);
        nextBBs.erase(succ);
    }

    unsigned removeSuccessorsTarget(DGBBlock<NodeT> *target)
    {
        unsigned removed = 0;
        SuccContainerT tmp;
        // approx
        for (auto& edge : nextBBs) {
            if (edge.target != target)
                tmp.insert(edge);
            else
                ++removed;
        }

        nextBBs.swap(tmp);
        return removed;
    }

    void removePredecessors()
    {
        for (DGBBlock<NodeT> *BB : prevBBs)
            BB->nextBBs.erase(this);

        prevBBs.clear();
    }

    bool addControlDependence(DGBBlock<NodeT> *b)
    {
        bool ret;
#ifndef NDEBUG
        bool ret2;
#endif

        ret = controlDeps.insert(b);
#ifndef NDEBUG
        ret2 =
#endif
        b->revControlDeps.insert(this);

        // we either have both edges or none
        assert(ret == ret2);

        return ret;
    }

    // get first node from bblock
    // or nullptr if the block is empty
    NodeT *getFirstNode() const
    {
        if (nodes.empty())
            return nullptr;

        return nodes.front();
    }

    // get last node from block
    // or nullptr if the block is empty
    NodeT *getLastNode() const
    {
        if (nodes.empty())
            return nullptr;

        return nodes.back();
    }

    // XXX: do this optional?
    DGBBlockContainerT& getPostDomFrontiers() { return postDomFrontiers; }
    const DGBBlockContainerT& getPostDomFrontiers() const { return postDomFrontiers; }

    bool addPostDomFrontier(DGBBlock<NodeT> *BB)
    {
        return postDomFrontiers.insert(BB);
    }

    bool addDomFrontier(DGBBlock<NodeT> *DF)
    {
        return domFrontiers.insert(DF);
    }

    DGBBlockContainerT& getDomFrontiers() { return domFrontiers; }
    const DGBBlockContainerT& getDomFrontiers() const { return domFrontiers; }

    void setIPostDom(DGBBlock<NodeT> *BB)
    {
        assert(!ipostdom && "Already has the immedate post-dominator");
        ipostdom = BB;
        BB->postDominators.insert(this);
    }

    DGBBlock<NodeT> *getIPostDom() { return ipostdom; }
    const DGBBlock<NodeT> *getIPostDom() const { return ipostdom; }

    DGBBlockContainerT& getPostDominators() { return postDominators; }
    const DGBBlockContainerT& getPostDominators() const { return postDominators; }

    void setIDom(DGBBlock<NodeT>* BB)
    {
        assert(!idom && "Already has immediate dominator");
        idom = BB;
        BB->addDominator(this);
    }

    void addDominator(DGBBlock<NodeT>* BB)
    {
        assert( BB && "need dominator bblock" );
        dominators.insert(BB);
    }

    DGBBlock<NodeT> *getIDom() { return idom; }
    const DGBBlock<NodeT> *getIDom() const { return idom; }

    DGBBlockContainerT& getDominators() { return dominators; }
    const DGBBlockContainerT& getDominators() const { return dominators; }

    unsigned int getDFSOrder() const
    {
        return analysisAuxData.dfsorder;
    }

    // in order to fasten up interprocedural analyses,
    // we register all the call sites in the DGBBlock
    unsigned int getCallSitesNum() const
    {
        return callSites.size();
    }

    const std::set<NodeT *>& getCallSites()
    {
        return callSites;
    }

    bool addCallsite(NodeT *n)
    {
        assert(n->getBBlock() == this
               && "Cannot add callsite from different BB");

        return callSites.insert(n).second;
    }

    bool removeCallSite(NodeT *n)
    {
        assert(n->getBBlock() == this
               && "Removing callsite from different BB");

        return callSites.erase(n) != 0;
    }

    void setSlice(uint64_t sid)
    {
        slice_id = sid;
    }

    uint64_t getSlice() const { return slice_id; }

    void deleteNodesOnDestruction(bool v = true) {
        delete_nodes_on_destr = v;
    }

private:
    // optional key
    KeyT key;

    // reference to dg if needed
    DependenceGraphT *dg;

    // nodes contained in this bblock
    std::list<NodeT *> nodes;

    SuccContainerT nextBBs;
    PredContainerT prevBBs;

    // when we have basic blocks, we do not need
    // to keep control dependencies in nodes, because
    // all nodes in block has the same control dependence
    DGBBlockContainerT controlDeps;
    DGBBlockContainerT revControlDeps;

    // post-dominator frontiers
    DGBBlockContainerT postDomFrontiers;
    DGBBlock<NodeT> *ipostdom;
    // the post-dominator tree edges
    // (reverse to immediate post-dominator)
    DGBBlockContainerT postDominators;

    // parent of @this in dominator tree
    DGBBlock<NodeT> *idom = nullptr;
    // BB.dominators = all children in dominator tree
    DGBBlockContainerT dominators;
    // dominance frontiers
    DGBBlockContainerT domFrontiers;

    // is this block in some slice?
    uint64_t slice_id;

    // delete nodes on destruction of the block
    bool delete_nodes_on_destr = false;

    // auxiliary data for analyses
    std::set<NodeT *> callSites;

    // auxiliary data for different analyses
    analysis::AnalysesAuxiliaryData analysisAuxData;
    friend class analysis::DGBBlockAnalysis<NodeT>;
};

} // namespace dg

#endif // _DG_BBLOCK_H_
