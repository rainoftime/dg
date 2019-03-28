#include <utility>
#include <unordered_map>
#include <set>

// ignore unused parameters in LLVM libraries
#if (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <llvm/Config/llvm-config.h>

#if ((LLVM_VERSION_MAJOR == 3) && (LLVM_VERSION_MINOR < 5))
 #include <llvm/Support/CFG.h>
#else
 #include <llvm/IR/CFG.h>
#endif

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/raw_ostream.h>

#if (__clang__)
#pragma clang diagnostic pop // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

#include "dg/llvm/SystemDependenceGraph/SystemDependenceGraph.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"

#include "dg/ADT/Queue.h"

#include "llvm/llvm-utils.h"

using llvm::errs;
using std::make_pair;

namespace dg {

using namespace sdg;

/// ------------------------------------------------------------------
//  -- LLVMSystemDependenceGraph builder
/// ------------------------------------------------------------------

class SDGBuilder {
    using ValuesMapTy = LLVMSystemDependenceGraph::ValuesMapTy;
    using FunctionsMapTy = LLVMSystemDependenceGraph::FunctionsMapTy;

    std::unique_ptr<SystemDependenceGraph> _sdg{};
    llvm::Function *_entryFunction{nullptr};
    llvm::Module *_module{nullptr};

    // mapping of built functions and values
    // these are references to the LLVMSystemDependenceGraph
    FunctionsMapTy& _functions;
    ValuesMapTy& _values;

    // points-to information (if available)
    LLVMPointerAnalysis *_PTA{nullptr};
    // reaching definitions information (if available)
    LLVMReachingDefinitions *_RDA{nullptr};

    DependenceGraph *buildRec(llvm::Function *F);
    void buildBlock(DependenceGraph *dg, llvm::BasicBlock& llvmBlock);
    DGNode *buildNode(llvm::Instruction& I, DependenceGraph *dg);
    DGNode *buildCall(llvm::CallInst& CI, DependenceGraph *dg);

public:

    SDGBuilder(llvm::Module *M,
               FunctionsMapTy& fMap,
               ValuesMapTy& vMap,
               LLVMPointerAnalysis *PTA = nullptr,
               LLVMReachingDefinitions *RDA = nullptr)
    : _module(M), _functions(fMap), _values(vMap), _PTA(PTA), _RDA(RDA) {}

    std::unique_ptr<SystemDependenceGraph>&& build(llvm::Function *F);
};


// build DG for F and recurisively all functions called from F
// and so on...
std::unique_ptr<SystemDependenceGraph>&& SDGBuilder::build(llvm::Function *F) {
    assert(F && "No entry function given");

    _entryFunction = F;

    //buildGlobals();

    buildRec(_entryFunction);

    return std::move(_sdg);
}

DependenceGraph *SDGBuilder::buildRec(llvm::Function *F) {
    assert(F && "No function given");

    auto& dg = _functions[F];
    if (dg) {
        // we already built this graph
        return dg;
    }

    // we took the pointer by reference
    assert(dg && _functions[F] == dg && "Inconsistently built DG");

    for (auto& llvmBlock : *F) {
        buildBlock(dg, llvmBlock);
    }

    return dg;
}

void SDGBuilder::buildBlock(DependenceGraph *dg, llvm::BasicBlock& llvmBlock) {
    //auto block = dg->createBlock();

    for (auto& I : llvmBlock) {
        auto node = buildNode(I, dg);
        // add mapping of the llvm value to the node
        _values[&I] = node;
    }
}

DGNode *SDGBuilder::buildNode(llvm::Instruction& I, DependenceGraph *dg) {
    if (auto CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
        return buildCall(*CI, dg);
    } else {
        return dg->createInstruction();
    }

    // unreachable
    abort();
}

DGNode *SDGBuilder::buildCall(llvm::CallInst& CI, DependenceGraph *dg) {
    return dg->createCall();
}

/// ------------------------------------------------------------------
//  -- LLVMSystemDependenceGraph
/// ------------------------------------------------------------------

bool LLVMSystemDependenceGraph::build(const std::string& entry)
{
    assert(!_sdg && "SDG is already built");

    // get entry function if not given
    _entryFunction = _module->getFunction("main");
    if (!_entryFunction) {
        errs() << "No entry function found/given\n";
        return false;
    }

    SDGBuilder builder(_module,
                       getBuiltFunctions(),
                       getBuiltValues(),
                       _PTA, _RDA);

    _sdg = std::move(builder.build(_entryFunction));

    return true;
};


} // namespace dg
