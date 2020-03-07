#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/Instruction.h"

#include "Util.h"

#include <string>

using namespace llvm;
bool valueEscapes(Instruction *Inst) {
  BasicBlock *BB = Inst->getParent();
  for (Value::use_iterator UI = Inst->use_begin(), E = Inst->use_end(); UI != E;
       ++UI) {
    Instruction *I = cast<Instruction>(*UI);
    if (I->getParent() != BB || isa<PHINode>(I)) {
      return true;
    }
  }
  return false;
}

void fixStack(Function *f) {
  // Try to remove phi node and demote reg to stack
  std::vector<PHINode *> tmpPhi;
  std::vector<Instruction *> tmpReg;
  BasicBlock *bbEntry = &*f->begin();

  do {
    tmpPhi.clear();
    tmpReg.clear();

    for (Function::iterator i = f->begin(); i != f->end(); ++i) {

      for (BasicBlock::iterator j = i->begin(); j != i->end(); ++j) {

        if (isa<PHINode>(j)) {
          PHINode *phi = cast<PHINode>(j);
          tmpPhi.push_back(phi);
          continue;
        }
        if (!(isa<AllocaInst>(j) && j->getParent() == bbEntry) &&
            (valueEscapes(&*j) || j->isUsedOutsideOfBlock(&*i))) {
          tmpReg.push_back(&*j);
          continue;
        }
      }
    }
    for (unsigned int i = 0; i != tmpReg.size(); ++i) {
      DemoteRegToStack(*tmpReg.at(i), f->begin()->getTerminator());
    }

    for (unsigned int i = 0; i != tmpPhi.size(); ++i) {
      DemotePHIToStack(tmpPhi.at(i), f->begin()->getTerminator());
    }

  } while (tmpReg.size() != 0 || tmpPhi.size() != 0);
}

InlineAsm *generateGarbage(Function *f){
  std::string s(".byte 0xEB");
  InlineAsm *IA = InlineAsm::get(FunctionType::get(Type::getVoidTy(f->getContext()), false), s, "", true, false);
  return IA;
}

uint32_t fnvHash(const uint32_t data, uint32_t b){
  b=(b^((data>>0)&0xFF))*fnvPrime;
  b=(b^((data>>8)&0xFF))*fnvPrime;
  b=(b^((data>>16)&0xFF))*fnvPrime;
  b=(b^((data>>24)&0xFF))*fnvPrime;
  return b;
}