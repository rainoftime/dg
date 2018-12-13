#ifndef _DG_POST_DOMINANCE_FRONTIERS_H_
#define _DG_POST_DOMINANCE_FRONTIERS_H_

#include <vector>

#include "dg/DGBBlock.h"
#include "dg/analysis/BFS.h"

namespace dg {
namespace analysis {

///
// Compute post-dominance frontiers
//
// \param root   root of post-dominators tree
//
// This algorithm takes post-dominator tree
// (edges of the tree are in BBlocks) and computes
// post-dominator frontiers for every node
//
// The algorithm is due:
//
// R. Cytron, J. Ferrante, B. K. Rosen, M. N. Wegman, and F. K. Zadeck. 1989.
// An efficient method of computing static single assignment form.
// In Proceedings of the 16th ACM SIGPLAN-SIGACT symposium on Principles of programming languages (POPL '89),
// CORPORATE New York, NY Association for Computing Machinery (Ed.). ACM, New York, NY, USA, 25-35.
// DOI=http://dx.doi.org/10.1145/75277.75280
//
template <typename NodeT>
class PostDominanceFrontiers
{
    static void queuePostDomBBs(DGBBlock<NodeT> *BB,
                                std::vector<DGBBlock<NodeT> *> *blocks)
    {
        blocks->push_back(BB);
    }

    void computePDFrontiers(DGBBlock<NodeT> *BB, bool add_cd)
    {
        // compute DFlocal
        for (DGBBlock<NodeT> *pred : BB->predecessors()) {
            DGBBlock<NodeT> *ipdom = pred->getIPostDom();
            if (ipdom && ipdom != BB) {
                BB->addPostDomFrontier(pred);

                // pd-frontiers are the reverse control dependencies
                if (add_cd)
                    pred->addControlDependence(BB);
            }
        }

        for (DGBBlock<NodeT> *pdom : BB->getPostDominators()) {
            for (DGBBlock<NodeT> *df : pdom->getPostDomFrontiers()) {
                DGBBlock<NodeT> *ipdom = df->getIPostDom();
                if (ipdom && ipdom != BB && df != BB) {
                    BB->addPostDomFrontier(df);

                    if (add_cd)
                        df->addControlDependence(BB);
                }
            }
        }
    }

public:
    void compute(DGBBlock<NodeT> *root, bool add_cd = false)
    {
        std::vector<DGBBlock<NodeT> *> blocks;
        legacy::DGBBlockBFS<NodeT> bfs(legacy::BFS_BB_POSTDOM);

        // get BBs in the order of post-dom tree edges (BFS),
        // so that we process it bottom-up
        bfs.run(root, queuePostDomBBs, &blocks);

        // go bottom-up the post-dom tree and compute post-domninance frontiers
        for (int i = blocks.size() - 1; i >= 0; --i)
            computePDFrontiers(blocks[i], add_cd);
    }
};

} // namespace analysis
} // namespace dg

#endif // _DG_POST_DOMINANCE_FRONTIERS_H_
