#ifndef _DG_LLVM_SYSTEM_DEPENDENCE_GRAPH_H_
#define _DG_LLVM_SYSTEM_DEPENDENCE_GRAPH_H_

#include <unordered_map>

#include "dg/SystemDependenceGraph/SystemDependenceGraph.h"

// forward declaration of llvm classes
namespace llvm {
    class Module;
    class Value;
    class Function;
} // namespace llvm

namespace dg {

// forward declaration
class LLVMPointerAnalysis;

// FIXME: Make the namespaces consistent...
namespace analysis {
namespace rd {
    class LLVMReachingDefinitions;
}};

using analysis::rd::LLVMReachingDefinitions;

/// ------------------------------------------------------------------
//  -- LLVMDependenceGraph
/// ------------------------------------------------------------------
class LLVMSystemDependenceGraph {
public:
    using ValuesMapTy = std::unordered_map<llvm::Value *, sdg::DGNode *>;
    using FunctionsMapTy = std::unordered_map<llvm::Function *, sdg::DependenceGraph *>;
private:
    std::unique_ptr<sdg::SystemDependenceGraph> _sdg{};
    llvm::Function *_entryFunction{nullptr};
    llvm::Module *_module{nullptr};

    // points-to information (if available)
    LLVMPointerAnalysis *_PTA{nullptr};
    // reaching definitions information (if available)
    LLVMReachingDefinitions *_RDA{nullptr};

    // mapping of built functions
    FunctionsMapTy _functions;
    // mapping of built values
    ValuesMapTy _values;

public:
    LLVMSystemDependenceGraph(llvm::Module *M,
                              LLVMPointerAnalysis *PTA = nullptr,
                              LLVMReachingDefinitions *RDA = nullptr)
    : _module(M), _PTA(PTA), _RDA(RDA) {}

    bool build(const std::string& entry = "main");

    bool verify() const { assert(0 && "Not implemented"); }

    LLVMPointerAnalysis *getPTA() const { return _PTA; }
    LLVMReachingDefinitions *getRDA() const { return _RDA; }

    FunctionsMapTy& getBuiltFunctions() { return _functions; }
    const FunctionsMapTy& getBuiltFunctions() const { return _functions; }

    ValuesMapTy& getBuiltValues() { return _values; }
    const ValuesMapTy& getBuiltValues() const{ return _values; }
};

} // namespace dg

#endif // _DG_LLVM_SYSTEM_DEPENDENCE_GRAPH_H_
