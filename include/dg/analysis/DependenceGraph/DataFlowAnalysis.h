#ifndef _DG_DATA_FLOW_ANALYSIS_H_
#define _DG_DATA_FLOW_ANALYSIS_H_

#include <utility>
#include <set>

#include "dg/analysis/DependenceGraph/DFS.h"
#include "dg/analysis/DependenceGraph/Analysis.h"

#ifndef ENABLE_CFG
#error "Need CFG enabled for data flow analysis"
#endif

namespace dg {
namespace analysis {
namespace legacy {

struct DataFlowStatistics : public AnalysisStatistics {
    DataFlowStatistics()
        : AnalysisStatistics(), bblocksNum(0), iterationsNum(0) {}

    uint64_t bblocksNum;
    uint64_t iterationsNum;

    uint64_t getBBlocksNum() const { return bblocksNum; }
    uint64_t getIterationsNum() const { return iterationsNum; }
};

enum DataFlowAnalysisFlags {
    DATAFLOW_INTERPROCEDURAL    = 1 << 0,
    DATAFLOW_BB_NO_CALLSITES    = 1 << 1,
};

// ordering of nodes with respect to DFS order
// works for both nodes and blocks
template<typename T>
struct DFSOrderLess
{
    bool operator()(T a, T b) const
    {
        return a->getDFSOrder() < b->getDFSOrder();
    }
};

template <typename NodeT>
class DGBBlockDataFlowAnalysis : public Analysis<NodeT>
{
public:
    DGBBlockDataFlowAnalysis<NodeT>(DGBBlock<NodeT> *entryBB, uint32_t fl = 0)
        :entryBB(entryBB), flags(fl), changed(false) {}

    virtual bool runOnBlock(DGBBlock<NodeT> *BB) = 0;

    void run()
    {
        uint32_t flg = DFS_BB_CFG;

        assert(entryBB && "entry basic block is nullptr");

        /* translate flags */
        if (flags & DATAFLOW_INTERPROCEDURAL)
            flg |= DFS_INTERPROCEDURAL;
        if (flags & DATAFLOW_BB_NO_CALLSITES)
            flg |= DFS_BB_NO_CALLSITES;

        DGBBlockDFS<NodeT> DFS(flg);
        DFSDataT data(blocks, changed, this);

        // we will get all the nodes using DFS
        DFS.run(entryBB, dfs_proc_bb, data);

        // update statistics
        statistics.bblocksNum = blocks.size();
        statistics.iterationsNum = 1;
        // first run goes over each BB once
        statistics.processedBlocks = statistics.bblocksNum;

        // Iterate over the nodes in dfs reverse order, it is
        // usually good for reaching fixpoint. Later we can
        // add options what ordering to use.
        // Since we used while loop, if nothing changed after the
        // first iteration (the DFS), the loop will never run
        while (changed) {
            changed = false;
            for (auto I = blocks.rbegin(), E = blocks.rend();
                 I != E; ++I) {
                changed |= runOnBlock(*I);
                ++statistics.processedBlocks;
            }

            ++statistics.iterationsNum;
        }
    }

    uint32_t getFlags() const { return flags; }

    const DataFlowStatistics& getStatistics() const
    {
        return statistics;
    }

    bool addBB(DGBBlock<NodeT> *BB)
    {
        changed |= runOnBlock(BB);
        bool ret = blocks.insert(BB).second;
        if (ret)
            ++statistics.bblocksNum;

        changed |= ret;
        return ret;
    }

private:
    // define set of blocks to be ordered in dfs order
    // FIXME if we use dfs order, then addBB does not work,
    // because the BB's newly added does have dfsorder unset
    // and the BlocksSet thinks it already contains it, so
    // it is not added
    using BlocksSetT = std::set<DGBBlock<NodeT> * /*,
                     DFSOrderLess<DGBBlock<NodeT> *>*/>;
    struct DFSDataT
    {
        DFSDataT(BlocksSetT& b, bool& c, DGBBlockDataFlowAnalysis<NodeT> *r)
            :blocks(b), changed(c), ref(r) {}

        BlocksSetT& blocks;
        bool& changed;
        DGBBlockDataFlowAnalysis<NodeT> *ref;
    };

    static void dfs_proc_bb(DGBBlock<NodeT> *BB,
                            DFSDataT& data)
    {
        data.changed |= data.ref->runOnBlock(BB);
        data.blocks.insert(BB);
    }

    DGBBlock<NodeT> *entryBB;
    BlocksSetT blocks;
    uint32_t flags;
    bool changed;
    DataFlowStatistics statistics;
};


template <typename NodeT>
class DGDataFlowAnalysis : public DGBBlockDataFlowAnalysis<NodeT>
{
public:
    DGDataFlowAnalysis<NodeT>(DGBBlock<NodeT> *entryBB, uint32_t fl = 0)
        : DGBBlockDataFlowAnalysis<NodeT>(entryBB, fl) {};

    /* virtual */
    bool runOnBlock(DGBBlock<NodeT> *B)
    {
        bool changed = false;
        NodeT *prev = nullptr;

        for (NodeT *n : B->getNodes()) {
            changed |= runOnNode(n, prev);
            prev = n;
        }

        return changed;
    }

    virtual bool runOnNode(NodeT *n, NodeT *prev) = 0;

private:
};

} // namespace legacy
} // namespace analysis
} // namespace dg

#endif //  _DG_DATA_FLOW_ANALYSIS_H_
