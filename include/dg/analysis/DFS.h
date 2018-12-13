#ifndef _DG_DFS_H_
#define _DG_DFS_H_

#include "dg/analysis/NodesWalk.h"
#include "dg/ADT/Queue.h"

namespace dg {
namespace analysis {

using dg::ADT::QueueLIFO;

template <typename Node,
          typename VisitTracker = SetVisitTracker<Node>,
          typename EdgeChooser = SuccessorsEdgeChooser<Node> >
struct DFS : public NodesWalk<Node, QueueLIFO<Node *>, VisitTracker, EdgeChooser> {
    DFS() = default;
    DFS(EdgeChooser chooser) : NodesWalk<Node, QueueLIFO<Node *>, VisitTracker, EdgeChooser>(std::move(chooser)) {}
    DFS(VisitTracker tracker) : NodesWalk<Node, QueueLIFO<Node *>, VisitTracker, EdgeChooser>(std::move(tracker)) {}
    DFS(EdgeChooser chooser, VisitTracker tracker)
    : NodesWalk<Node, QueueLIFO<Node *>, VisitTracker, EdgeChooser>(std::move(chooser), std::move(tracker)) {}
};

} // namespace analysis
} // namespace dg

#endif // _DG_DFS_H_
