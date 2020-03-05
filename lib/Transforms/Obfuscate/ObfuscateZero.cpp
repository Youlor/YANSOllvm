#include "ObfuscateZero.h"

using namespace llvm;

bool ObfuscateZero::runOnBasicBlock(BasicBlock &BB) {
  IntegerVect.clear();
  bool modified = false;

  // We do not iterate from the beginning of a basic block, to avoid
  // obfuscating the parameters of Phi-instructions.
  for (BasicBlock::iterator I = BB.getFirstInsertionPt(),
      end = BB.end();
      I != end; ++I) {
    Instruction &Inst = *I;
    if (isValidCandidateInstruction(Inst)) {
      for (size_t i = 0; i < Inst.getNumOperands(); ++i) {
        if (Constant *C = isValidCandidateOperand(Inst.getOperand(i))) {
          if (Value *New_val = replaceZero(Inst, C)) {
            Inst.setOperand(i, New_val);
            modified = true;
            errs() << "Modified instruction!\n";
            errs() << Inst << "\n";
          }
        }
      }
    }
    registerInteger(Inst);
  }
  return modified;
}

// We do not analyze pointers, switches and call instructions.
bool ObfuscateZero::isValidCandidateInstruction(Instruction &Inst) const {
  if (isa<GetElementPtrInst>(&Inst)) {
    return false;
  } else if (isa<SwitchInst>(&Inst)) {
    return false;
  } else if (isa<CallInst>(&Inst)) {
    return false;
  } else {
    return true;
  }
}

// We do not analyze operands that are pointers, floats or the NULL value.
Constant* ObfuscateZero::isValidCandidateOperand(Value *V) const {
  if (Constant *C = dyn_cast<Constant>(V)) {
    if (isa<PointerType>(C->getType())) {
      return nullptr;
    } else if (C->getType()->isFloatingPointTy()) {
      return nullptr;
    } else if (C->isNullValue()) {
      return C;
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

void ObfuscateZero::registerInteger(Value &V) {
  if (V.getType()->isIntegerTy())
    IntegerVect.push_back(&V);
}

Value *ObfuscateZero::createExpression
(Type* IntermediaryType, const uint32_t p, IRBuilder<>& Builder) {
  // BEGIN HELP: You can use these declarations, or you can remove them.
  std::uniform_int_distribution<size_t> Rand(0, IntegerVect.size() - 1);
  std::uniform_int_distribution<size_t> RandAny(1, 10);
  size_t Index = Rand(Generator);
  Constant *any = ConstantInt::get(IntermediaryType, 1 + RandAny(Generator)),
           *prime = ConstantInt::get(IntermediaryType, p),
           *OverflowMask = ConstantInt::get(IntermediaryType, 0x00000007);
  // END HELP.

  // Tot = p*(x|any)^2
  Value *temp = Builder.CreateOr(IntegerVect[Index], any);
  temp = Builder.CreateAnd(OverflowMask, temp);
  temp = Builder.CreateMul(temp,temp);
  temp = Builder.CreateMul(prime, temp);
  Value *Tot = temp;
  registerInteger(*Tot);

  return Tot;
}

// We replace 0 with:
// prime1 * ((x | any1)**2) != prime2 * ((y | any2)**2)
// with prime1 != prime2 and any1 != 0 and any2 != 0
Value* ObfuscateZero::replaceZero(Instruction &Inst, Value *VReplace) {
  const uint32_t p1 = 431, p2 = 277;

  Type *ReplacedType = VReplace->getType(),
       *IntermediaryType = IntegerType::get(Inst.getParent()->getContext(),
           sizeof(uint32_t) * 8);

  if (IntegerVect.size() < 1) {
    return nullptr;
  }

  std::uniform_int_distribution<size_t> Rand(0, IntegerVect.size() - 1);
  std::uniform_int_distribution<size_t> RandAny(1, 10);

  IRBuilder<> Builder(&Inst);

  // lhs
  Value* LhsTot = createExpression(IntermediaryType, p1, Builder);

  // rhs
  Value* RhsTot = createExpression(IntermediaryType, p2, Builder);

  // comp
  Value *comp =
    Builder.CreateICmp(CmpInst::ICMP_EQ, LhsTot, RhsTot);
  registerInteger(*comp);
  Value *castComp = Builder.CreateZExt(comp, ReplacedType);
  registerInteger(*castComp);

  return castComp;
}

char ObfuscateZero::ID = 0;
static RegisterPass<ObfuscateZero> X("obfZero", "Obfuscates zeroes",
    false, false);

// register pass for clang use
static void registerObfuscateZeroPass(const PassManagerBuilder &,
    legacy::PassManagerBase &PM) {
  PM.add(new ObfuscateZero());
}

static RegisterStandardPasses
RegisterMBAPass(PassManagerBuilder::EP_EarlyAsPossible,
    registerObfuscateZeroPass);