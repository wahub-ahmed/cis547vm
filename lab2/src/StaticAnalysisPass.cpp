#include "Instrument.h"
#include "Utils.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace instrument {

const auto PASS_DESC = "Static Analysis Pass";

PreservedAnalyses StaticAnalysisPass::run(Module &M, ModuleAnalysisManager &AM) {
  outs() << "Running " << PASS_DESC << " on module " << M.getName() << "\n";

  for (auto &F : M) {
    if (F.isDeclaration())
      continue;

    auto FunctionName = F.getName().str();
    outs() << "Running " << PASS_DESC << " on function " << FunctionName << "\n";

    outs() << "Locating Instructions\n";
    for (inst_iterator Iter = inst_begin(F), E = inst_end(F); Iter != E; ++Iter) {
      Instruction &Inst = (*Iter);
      llvm::DebugLoc DebugLoc = Inst.getDebugLoc();
      if (!DebugLoc) {
        // Skip Instruction if it doesn't have debug information.
        continue;
      }

      int Line = DebugLoc.getLine();
      int Col = DebugLoc.getCol();
      outs() << Line << ", " << Col << "\n";

      if (auto *BinOp = dyn_cast<BinaryOperator>(&Inst)) {
        char symbol = getBinOpSymbol(BinOp->getOpcode());
        std::string opName = getBinOpName(symbol);
        std::string op1 = variable(BinOp->getOperand(0));
        std::string op2 = variable(BinOp->getOperand(1));
        outs() << opName << " on Line " << Line << ", Column " << Col
               << " with first operand " << op1
               << " and second operand " << op2 << "\n";
      }
    }
  }
  return PreservedAnalyses::all();
}

// Pass registration for the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StaticAnalysisPass", "1.0.0", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                    ModulePassManager &MPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "StaticAnalysisPass") {
                    MPM.addPass(StaticAnalysisPass());
                    return true;
                  }
                  return false;
                });
          }};
}

}  // namespace instrument
