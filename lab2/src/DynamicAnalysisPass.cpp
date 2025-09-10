#include "Instrument.h"
#include "Utils.h"

using namespace llvm;

namespace instrument {

const auto PASS_DESC = "Dynamic Analysis Pass";
const auto COVERAGE_FUNCTION_NAME = "__coverage__";
const auto BINOP_OPERANDS_FUNCTION_NAME = "__binop_op__";

void instrumentCoverage(Module *M, Instruction &I, int Line, int Col);
void instrumentBinOpOperands(Module *M, BinaryOperator *BinOp, int Line, int Col);

PreservedAnalyses DynamicAnalysisPass::run(Module &M, ModuleAnalysisManager &AM) {
  outs() << "Running " << PASS_DESC << " on module " << M.getName() << "\n";

  LLVMContext &Context = M.getContext();
  Type *VoidType = Type::getVoidTy(Context);
  Type *Int32Type = Type::getInt32Ty(Context);
  Type *Int8Type = Type::getInt8Ty(Context);

  // Declare external functions
  M.getOrInsertFunction(COVERAGE_FUNCTION_NAME, VoidType, Int32Type, Int32Type);
  M.getOrInsertFunction(BINOP_OPERANDS_FUNCTION_NAME,
      VoidType,
      Int8Type,
      Int32Type,
      Int32Type,
      Int32Type,
      Int32Type);

  for (auto &F : M) {
    if (F.isDeclaration())
      continue;

    auto FunctionName = F.getName().str();
    outs() << "Instrumenting function " << FunctionName << "\n";

    outs() << "Instrument Instructions\n";

    // Store instructions to instrument to avoid iterator invalidation
    std::vector<std::pair<Instruction *, std::pair<int, int>>> toInstrument;

    for (inst_iterator Iter = inst_begin(F), E = inst_end(F); Iter != E; ++Iter) {
      Instruction &Inst = (*Iter);
      llvm::DebugLoc DebugLoc = Inst.getDebugLoc();
      if (!DebugLoc) {
        // Skip Instruction if it doesn't have debug information.
        continue;
      }

      int Line = DebugLoc.getLine();
      int Col = DebugLoc.getCol();
      toInstrument.push_back({&Inst, {Line, Col}});
    }

    // Now instrument the collected instructions
    for (auto &instInfo : toInstrument) {
      Instruction *Inst = instInfo.first;
      int Line = instInfo.second.first;
      int Col = instInfo.second.second;

      instrumentCoverage(&M, *Inst, Line, Col);

      // Instrument binary operators
      if (auto *BinOp = dyn_cast<BinaryOperator>(Inst)) {
        instrumentBinOpOperands(&M, BinOp, Line, Col);
      }
    }
  }

  return PreservedAnalyses::none();
}

void instrumentCoverage(Module *M, Instruction &I, int Line, int Col) {
  auto &Context = M->getContext();
  auto *Int32Type = Type::getInt32Ty(Context);

  auto LineVal = ConstantInt::get(Int32Type, Line);
  auto ColVal = ConstantInt::get(Int32Type, Col);

  std::vector<Value *> Args = {LineVal, ColVal};

  auto *CoverageFunction = M->getFunction(COVERAGE_FUNCTION_NAME);
  CallInst::Create(CoverageFunction, Args, "", &I);
}

void instrumentBinOpOperands(Module *M, BinaryOperator *BinOp, int Line, int Col) {
  auto &Context = M->getContext();
  auto *Int32Type = Type::getInt32Ty(Context);
  auto *CharType = Type::getInt8Ty(Context);

  // Get operator symbol
  char symbol = getBinOpSymbol(BinOp->getOpcode());
  auto SymbolVal = ConstantInt::get(CharType, symbol);

  // Line and column constants
  auto LineVal = ConstantInt::get(Int32Type, Line);
  auto ColVal = ConstantInt::get(Int32Type, Col);

  // Operands
  Value *Op1 = BinOp->getOperand(0);
  Value *Op2 = BinOp->getOperand(1);

  std::vector<Value *> Args = {SymbolVal, LineVal, ColVal, Op1, Op2};

  auto *BinOpFunc = M->getFunction(BINOP_OPERANDS_FUNCTION_NAME);
  CallInst::Create(BinOpFunc, Args, "", BinOp);
}

// Pass registration for the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DynamicAnalysisPass", "1.0.0", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                    ModulePassManager &MPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "DynamicAnalysisPass") {
                    MPM.addPass(DynamicAnalysisPass());
                    return true;
                  }
                  return false;
                });
          }};
}

}  // namespace instrument
