//===-- LLVMContextImpl.cpp - Implement LLVMContextImpl -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the opaque LLVMContextImpl.
//
//===----------------------------------------------------------------------===//

#include "LLVMContextImpl.h"
#include "llvm/Module.h"
#include "llvm/ADT/STLExtras.h"
#include <algorithm>
using namespace llvm;

LLVMContextImpl::LLVMContextImpl(LLVMContext &C)
  : TheTrueVal(0), TheFalseVal(0),
    VoidTy(C, Type::VoidTyID),
    LabelTy(C, Type::LabelTyID),
    FloatTy(C, Type::FloatTyID),
    DoubleTy(C, Type::DoubleTyID),
    MetadataTy(C, Type::MetadataTyID),
    X86_FP80Ty(C, Type::X86_FP80TyID),
    FP128Ty(C, Type::FP128TyID),
    PPC_FP128Ty(C, Type::PPC_FP128TyID),
    X86_MMXTy(C, Type::X86_MMXTyID),
    Int1Ty(C, 1),
    Int8Ty(C, 8),
    Int16Ty(C, 16),
    Int32Ty(C, 32),
    Int64Ty(C, 64) {
  InlineAsmDiagHandler = 0;
  InlineAsmDiagContext = 0;
  NamedStructTypesUniqueID = 0;
}

namespace {
struct DropReferences {
  // Takes the value_type of a ConstantUniqueMap's internal map, whose 'second'
  // is a Constant*.
  template<typename PairT>
  void operator()(const PairT &P) {
    P.second->dropAllReferences();
  }
};
}

LLVMContextImpl::~LLVMContextImpl() {
  // NOTE: We need to delete the contents of OwnedModules, but we have to
  // duplicate it into a temporary vector, because the destructor of Module
  // will try to remove itself from OwnedModules set.  This would cause
  // iterator invalidation if we iterated on the set directly.
  std::vector<Module*> Modules(OwnedModules.begin(), OwnedModules.end());
  DeleteContainerPointers(Modules);
  
  std::for_each(ExprConstants.map_begin(), ExprConstants.map_end(),
                DropReferences());
  std::for_each(ArrayConstants.map_begin(), ArrayConstants.map_end(),
                DropReferences());
  std::for_each(StructConstants.map_begin(), StructConstants.map_end(),
                DropReferences());
  std::for_each(VectorConstants.map_begin(), VectorConstants.map_end(),
                DropReferences());
  ExprConstants.freeConstants();
  ArrayConstants.freeConstants();
  StructConstants.freeConstants();
  VectorConstants.freeConstants();
  AggZeroConstants.freeConstants();
  NullPtrConstants.freeConstants();
  UndefValueConstants.freeConstants();
  InlineAsms.freeConstants();
  DeleteContainerSeconds(IntConstants);
  DeleteContainerSeconds(FPConstants);
  
  // Destroy MDNodes.  ~MDNode can move and remove nodes between the MDNodeSet
  // and the NonUniquedMDNodes sets, so copy the values out first.
  SmallVector<MDNode*, 8> MDNodes;
  MDNodes.reserve(MDNodeSet.size() + NonUniquedMDNodes.size());
  for (FoldingSetIterator<MDNode> I = MDNodeSet.begin(), E = MDNodeSet.end();
       I != E; ++I)
    MDNodes.push_back(&*I);
  MDNodes.append(NonUniquedMDNodes.begin(), NonUniquedMDNodes.end());
  for (SmallVectorImpl<MDNode *>::iterator I = MDNodes.begin(),
         E = MDNodes.end(); I != E; ++I)
    (*I)->destroy();
  assert(MDNodeSet.empty() && NonUniquedMDNodes.empty() &&
         "Destroying all MDNodes didn't empty the Context's sets.");
  // Destroy MDStrings.
  DeleteContainerSeconds(MDStringCache);
}
