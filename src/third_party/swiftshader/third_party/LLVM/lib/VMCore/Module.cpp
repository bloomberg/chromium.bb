//===-- Module.cpp - Implement the Module class ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Module class for the VMCore library.
//
//===----------------------------------------------------------------------===//

#include "llvm/Module.h"
#include "llvm/InstrTypes.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/GVMaterializer.h"
#include "llvm/LLVMContext.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/LeakDetector.h"
#include "SymbolTableListTraitsImpl.h"
#include <algorithm>
#include <cstdarg>
#include <cstdlib>
using namespace llvm;

//===----------------------------------------------------------------------===//
// Methods to implement the globals and functions lists.
//

// Explicit instantiations of SymbolTableListTraits since some of the methods
// are not in the public header file.
template class llvm::SymbolTableListTraits<Function, Module>;
template class llvm::SymbolTableListTraits<GlobalVariable, Module>;
template class llvm::SymbolTableListTraits<GlobalAlias, Module>;

//===----------------------------------------------------------------------===//
// Primitive Module methods.
//

Module::Module(StringRef MID, LLVMContext& C)
  : Context(C), Materializer(NULL), ModuleID(MID) {
  ValSymTab = new ValueSymbolTable();
  NamedMDSymTab = new StringMap<NamedMDNode *>();
  Context.addModule(this);
}

Module::~Module() {
  Context.removeModule(this);
  dropAllReferences();
  GlobalList.clear();
  FunctionList.clear();
  AliasList.clear();
  LibraryList.clear();
  NamedMDList.clear();
  delete ValSymTab;
  delete static_cast<StringMap<NamedMDNode *> *>(NamedMDSymTab);
}

/// Target endian information.
Module::Endianness Module::getEndianness() const {
  StringRef temp = DataLayout;
  Module::Endianness ret = AnyEndianness;
  
  while (!temp.empty()) {
    std::pair<StringRef, StringRef> P = getToken(temp, "-");
    
    StringRef token = P.first;
    temp = P.second;
    
    if (token[0] == 'e') {
      ret = LittleEndian;
    } else if (token[0] == 'E') {
      ret = BigEndian;
    }
  }
  
  return ret;
}

/// Target Pointer Size information.
Module::PointerSize Module::getPointerSize() const {
  StringRef temp = DataLayout;
  Module::PointerSize ret = AnyPointerSize;
  
  while (!temp.empty()) {
    std::pair<StringRef, StringRef> TmpP = getToken(temp, "-");
    temp = TmpP.second;
    TmpP = getToken(TmpP.first, ":");
    StringRef token = TmpP.second, signalToken = TmpP.first;
    
    if (signalToken[0] == 'p') {
      int size = 0;
      getToken(token, ":").first.getAsInteger(10, size);
      if (size == 32)
        ret = Pointer32;
      else if (size == 64)
        ret = Pointer64;
    }
  }
  
  return ret;
}

/// getNamedValue - Return the first global value in the module with
/// the specified name, of arbitrary type.  This method returns null
/// if a global with the specified name is not found.
GlobalValue *Module::getNamedValue(StringRef Name) const {
  return cast_or_null<GlobalValue>(getValueSymbolTable().lookup(Name));
}

/// getMDKindID - Return a unique non-zero ID for the specified metadata kind.
/// This ID is uniqued across modules in the current LLVMContext.
unsigned Module::getMDKindID(StringRef Name) const {
  return Context.getMDKindID(Name);
}

/// getMDKindNames - Populate client supplied SmallVector with the name for
/// custom metadata IDs registered in this LLVMContext.   ID #0 is not used,
/// so it is filled in as an empty string.
void Module::getMDKindNames(SmallVectorImpl<StringRef> &Result) const {
  return Context.getMDKindNames(Result);
}


//===----------------------------------------------------------------------===//
// Methods for easy access to the functions in the module.
//

// getOrInsertFunction - Look up the specified function in the module symbol
// table.  If it does not exist, add a prototype for the function and return
// it.  This is nice because it allows most passes to get away with not handling
// the symbol table directly for this common task.
//
Constant *Module::getOrInsertFunction(StringRef Name,
                                      FunctionType *Ty,
                                      AttrListPtr AttributeList) {
  // See if we have a definition for the specified function already.
  GlobalValue *F = getNamedValue(Name);
  if (F == 0) {
    // Nope, add it
    Function *New = Function::Create(Ty, GlobalVariable::ExternalLinkage, Name);
    if (!New->isIntrinsic())       // Intrinsics get attrs set on construction
      New->setAttributes(AttributeList);
    FunctionList.push_back(New);
    return New;                    // Return the new prototype.
  }

  // Okay, the function exists.  Does it have externally visible linkage?
  if (F->hasLocalLinkage()) {
    // Clear the function's name.
    F->setName("");
    // Retry, now there won't be a conflict.
    Constant *NewF = getOrInsertFunction(Name, Ty);
    F->setName(Name);
    return NewF;
  }

  // If the function exists but has the wrong type, return a bitcast to the
  // right type.
  if (F->getType() != PointerType::getUnqual(Ty))
    return ConstantExpr::getBitCast(F, PointerType::getUnqual(Ty));
  
  // Otherwise, we just found the existing function or a prototype.
  return F;  
}

Constant *Module::getOrInsertTargetIntrinsic(StringRef Name,
                                             FunctionType *Ty,
                                             AttrListPtr AttributeList) {
  // See if we have a definition for the specified function already.
  GlobalValue *F = getNamedValue(Name);
  if (F == 0) {
    // Nope, add it
    Function *New = Function::Create(Ty, GlobalVariable::ExternalLinkage, Name);
    New->setAttributes(AttributeList);
    FunctionList.push_back(New);
    return New; // Return the new prototype.
  }

  // Otherwise, we just found the existing function or a prototype.
  return F;  
}

Constant *Module::getOrInsertFunction(StringRef Name,
                                      FunctionType *Ty) {
  AttrListPtr AttributeList = AttrListPtr::get((AttributeWithIndex *)0, 0);
  return getOrInsertFunction(Name, Ty, AttributeList);
}

// getOrInsertFunction - Look up the specified function in the module symbol
// table.  If it does not exist, add a prototype for the function and return it.
// This version of the method takes a null terminated list of function
// arguments, which makes it easier for clients to use.
//
Constant *Module::getOrInsertFunction(StringRef Name,
                                      AttrListPtr AttributeList,
                                      Type *RetTy, ...) {
  va_list Args;
  va_start(Args, RetTy);

  // Build the list of argument types...
  std::vector<Type*> ArgTys;
  while (Type *ArgTy = va_arg(Args, Type*))
    ArgTys.push_back(ArgTy);

  va_end(Args);

  // Build the function type and chain to the other getOrInsertFunction...
  return getOrInsertFunction(Name,
                             FunctionType::get(RetTy, ArgTys, false),
                             AttributeList);
}

Constant *Module::getOrInsertFunction(StringRef Name,
                                      Type *RetTy, ...) {
  va_list Args;
  va_start(Args, RetTy);

  // Build the list of argument types...
  std::vector<Type*> ArgTys;
  while (Type *ArgTy = va_arg(Args, Type*))
    ArgTys.push_back(ArgTy);

  va_end(Args);

  // Build the function type and chain to the other getOrInsertFunction...
  return getOrInsertFunction(Name, 
                             FunctionType::get(RetTy, ArgTys, false),
                             AttrListPtr::get((AttributeWithIndex *)0, 0));
}

// getFunction - Look up the specified function in the module symbol table.
// If it does not exist, return null.
//
Function *Module::getFunction(StringRef Name) const {
  return dyn_cast_or_null<Function>(getNamedValue(Name));
}

//===----------------------------------------------------------------------===//
// Methods for easy access to the global variables in the module.
//

/// getGlobalVariable - Look up the specified global variable in the module
/// symbol table.  If it does not exist, return null.  The type argument
/// should be the underlying type of the global, i.e., it should not have
/// the top-level PointerType, which represents the address of the global.
/// If AllowLocal is set to true, this function will return types that
/// have an local. By default, these types are not returned.
///
GlobalVariable *Module::getGlobalVariable(StringRef Name,
                                          bool AllowLocal) const {
  if (GlobalVariable *Result = 
      dyn_cast_or_null<GlobalVariable>(getNamedValue(Name)))
    if (AllowLocal || !Result->hasLocalLinkage())
      return Result;
  return 0;
}

/// getOrInsertGlobal - Look up the specified global in the module symbol table.
///   1. If it does not exist, add a declaration of the global and return it.
///   2. Else, the global exists but has the wrong type: return the function
///      with a constantexpr cast to the right type.
///   3. Finally, if the existing global is the correct delclaration, return the
///      existing global.
Constant *Module::getOrInsertGlobal(StringRef Name, Type *Ty) {
  // See if we have a definition for the specified global already.
  GlobalVariable *GV = dyn_cast_or_null<GlobalVariable>(getNamedValue(Name));
  if (GV == 0) {
    // Nope, add it
    GlobalVariable *New =
      new GlobalVariable(*this, Ty, false, GlobalVariable::ExternalLinkage,
                         0, Name);
     return New;                    // Return the new declaration.
  }

  // If the variable exists but has the wrong type, return a bitcast to the
  // right type.
  if (GV->getType() != PointerType::getUnqual(Ty))
    return ConstantExpr::getBitCast(GV, PointerType::getUnqual(Ty));
  
  // Otherwise, we just found the existing function or a prototype.
  return GV;
}

//===----------------------------------------------------------------------===//
// Methods for easy access to the global variables in the module.
//

// getNamedAlias - Look up the specified global in the module symbol table.
// If it does not exist, return null.
//
GlobalAlias *Module::getNamedAlias(StringRef Name) const {
  return dyn_cast_or_null<GlobalAlias>(getNamedValue(Name));
}

/// getNamedMetadata - Return the first NamedMDNode in the module with the
/// specified name. This method returns null if a NamedMDNode with the 
/// specified name is not found.
NamedMDNode *Module::getNamedMetadata(const Twine &Name) const {
  SmallString<256> NameData;
  StringRef NameRef = Name.toStringRef(NameData);
  return static_cast<StringMap<NamedMDNode*> *>(NamedMDSymTab)->lookup(NameRef);
}

/// getOrInsertNamedMetadata - Return the first named MDNode in the module 
/// with the specified name. This method returns a new NamedMDNode if a 
/// NamedMDNode with the specified name is not found.
NamedMDNode *Module::getOrInsertNamedMetadata(StringRef Name) {
  NamedMDNode *&NMD =
    (*static_cast<StringMap<NamedMDNode *> *>(NamedMDSymTab))[Name];
  if (!NMD) {
    NMD = new NamedMDNode(Name);
    NMD->setParent(this);
    NamedMDList.push_back(NMD);
  }
  return NMD;
}

void Module::eraseNamedMetadata(NamedMDNode *NMD) {
  static_cast<StringMap<NamedMDNode *> *>(NamedMDSymTab)->erase(NMD->getName());
  NamedMDList.erase(NMD);
}


//===----------------------------------------------------------------------===//
// Methods to control the materialization of GlobalValues in the Module.
//
void Module::setMaterializer(GVMaterializer *GVM) {
  assert(!Materializer &&
         "Module already has a GVMaterializer.  Call MaterializeAllPermanently"
         " to clear it out before setting another one.");
  Materializer.reset(GVM);
}

bool Module::isMaterializable(const GlobalValue *GV) const {
  if (Materializer)
    return Materializer->isMaterializable(GV);
  return false;
}

bool Module::isDematerializable(const GlobalValue *GV) const {
  if (Materializer)
    return Materializer->isDematerializable(GV);
  return false;
}

bool Module::Materialize(GlobalValue *GV, std::string *ErrInfo) {
  if (Materializer)
    return Materializer->Materialize(GV, ErrInfo);
  return false;
}

void Module::Dematerialize(GlobalValue *GV) {
  if (Materializer)
    return Materializer->Dematerialize(GV);
}

bool Module::MaterializeAll(std::string *ErrInfo) {
  if (!Materializer)
    return false;
  return Materializer->MaterializeModule(this, ErrInfo);
}

bool Module::MaterializeAllPermanently(std::string *ErrInfo) {
  if (MaterializeAll(ErrInfo))
    return true;
  Materializer.reset();
  return false;
}

//===----------------------------------------------------------------------===//
// Other module related stuff.
//


// dropAllReferences() - This function causes all the subelementss to "let go"
// of all references that they are maintaining.  This allows one to 'delete' a
// whole module at a time, even though there may be circular references... first
// all references are dropped, and all use counts go to zero.  Then everything
// is deleted for real.  Note that no operations are valid on an object that
// has "dropped all references", except operator delete.
//
void Module::dropAllReferences() {
  for(Module::iterator I = begin(), E = end(); I != E; ++I)
    I->dropAllReferences();

  for(Module::global_iterator I = global_begin(), E = global_end(); I != E; ++I)
    I->dropAllReferences();

  for(Module::alias_iterator I = alias_begin(), E = alias_end(); I != E; ++I)
    I->dropAllReferences();
}

void Module::addLibrary(StringRef Lib) {
  for (Module::lib_iterator I = lib_begin(), E = lib_end(); I != E; ++I)
    if (*I == Lib)
      return;
  LibraryList.push_back(Lib);
}

void Module::removeLibrary(StringRef Lib) {
  LibraryListType::iterator I = LibraryList.begin();
  LibraryListType::iterator E = LibraryList.end();
  for (;I != E; ++I)
    if (*I == Lib) {
      LibraryList.erase(I);
      return;
    }
}

//===----------------------------------------------------------------------===//
// Type finding functionality.
//===----------------------------------------------------------------------===//

namespace {
  /// TypeFinder - Walk over a module, identifying all of the types that are
  /// used by the module.
  class TypeFinder {
    // To avoid walking constant expressions multiple times and other IR
    // objects, we keep several helper maps.
    DenseSet<const Value*> VisitedConstants;
    DenseSet<Type*> VisitedTypes;
    
    std::vector<StructType*> &StructTypes;
  public:
    TypeFinder(std::vector<StructType*> &structTypes)
      : StructTypes(structTypes) {}
    
    void run(const Module &M) {
      // Get types from global variables.
      for (Module::const_global_iterator I = M.global_begin(),
           E = M.global_end(); I != E; ++I) {
        incorporateType(I->getType());
        if (I->hasInitializer())
          incorporateValue(I->getInitializer());
      }
      
      // Get types from aliases.
      for (Module::const_alias_iterator I = M.alias_begin(),
           E = M.alias_end(); I != E; ++I) {
        incorporateType(I->getType());
        if (const Value *Aliasee = I->getAliasee())
          incorporateValue(Aliasee);
      }
      
      SmallVector<std::pair<unsigned, MDNode*>, 4> MDForInst;

      // Get types from functions.
      for (Module::const_iterator FI = M.begin(), E = M.end(); FI != E; ++FI) {
        incorporateType(FI->getType());
        
        for (Function::const_iterator BB = FI->begin(), E = FI->end();
             BB != E;++BB)
          for (BasicBlock::const_iterator II = BB->begin(),
               E = BB->end(); II != E; ++II) {
            const Instruction &I = *II;
            // Incorporate the type of the instruction and all its operands.
            incorporateType(I.getType());
            for (User::const_op_iterator OI = I.op_begin(), OE = I.op_end();
                 OI != OE; ++OI)
              incorporateValue(*OI);
            
            // Incorporate types hiding in metadata.
            I.getAllMetadataOtherThanDebugLoc(MDForInst);
            for (unsigned i = 0, e = MDForInst.size(); i != e; ++i)
              incorporateMDNode(MDForInst[i].second);
            MDForInst.clear();
          }
      }
      
      for (Module::const_named_metadata_iterator I = M.named_metadata_begin(),
           E = M.named_metadata_end(); I != E; ++I) {
        const NamedMDNode *NMD = I;
        for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i)
          incorporateMDNode(NMD->getOperand(i));
      }
    }
    
  private:
    void incorporateType(Type *Ty) {
      // Check to see if we're already visited this type.
      if (!VisitedTypes.insert(Ty).second)
        return;
      
      // If this is a structure or opaque type, add a name for the type.
      if (StructType *STy = dyn_cast<StructType>(Ty))
        StructTypes.push_back(STy);
      
      // Recursively walk all contained types.
      for (Type::subtype_iterator I = Ty->subtype_begin(),
           E = Ty->subtype_end(); I != E; ++I)
        incorporateType(*I);
    }
    
    /// incorporateValue - This method is used to walk operand lists finding
    /// types hiding in constant expressions and other operands that won't be
    /// walked in other ways.  GlobalValues, basic blocks, instructions, and
    /// inst operands are all explicitly enumerated.
    void incorporateValue(const Value *V) {
      if (const MDNode *M = dyn_cast<MDNode>(V))
        return incorporateMDNode(M);
      if (!isa<Constant>(V) || isa<GlobalValue>(V)) return;
      
      // Already visited?
      if (!VisitedConstants.insert(V).second)
        return;
      
      // Check this type.
      incorporateType(V->getType());
      
      // Look in operands for types.
      const User *U = cast<User>(V);
      for (Constant::const_op_iterator I = U->op_begin(),
           E = U->op_end(); I != E;++I)
        incorporateValue(*I);
    }
    
    void incorporateMDNode(const MDNode *V) {
      
      // Already visited?
      if (!VisitedConstants.insert(V).second)
        return;
      
      // Look in operands for types.
      for (unsigned i = 0, e = V->getNumOperands(); i != e; ++i)
        if (Value *Op = V->getOperand(i))
          incorporateValue(Op);
    }
  };
} // end anonymous namespace

void Module::findUsedStructTypes(std::vector<StructType*> &StructTypes) const {
  TypeFinder(StructTypes).run(*this);
}
