// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Clang plugin which prints the names and sizes of all the top-level
// structs, enums, and typedefs in the input file.

#include <cstdio>
#include <cstring>
#include <string>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/CharUnits.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

namespace {

const char* const kTypedefName = "Typedef";
const char* const kDelim = ",";
const char* const kArchDependent = "ArchDependentSize";
const char* const kNotArchDependent = "NotArchDependentSize";

// This class consumes a Clang-parsed AST and prints out information about types
// defined in the global namespace.  Specifically, for each type definition
// encountered, it prints:
// "kind,name,size,arch_dependent,source_file,first_line,last_line\n"
// Where:
// - kind:  The Clang TypeClassName (Record, Enum, Typedef, Union, etc)
// - name:  The unmangled string name of the type.
// - size:  The size in bytes of the type.
// - arch_dependent:  'ArchDependentSize' if the type has architecture-dependent
//                    size, NotArchDependentSize otherwise.
// - source_file:  The source file in which the type is defined.
// - first_line:  The first line of the definition (counting from 0).
// - last_line:  The last line of the definition (counting from 0).
class PrintNamesAndSizesConsumer : public clang::ASTConsumer {
 public:
  explicit PrintNamesAndSizesConsumer(clang::SourceManager* source_mgr)
      : source_manager_(source_mgr) {}

 private:
  // SourceManager has information about source code locations, to help us know
  // where definitions appear.  We do not own this pointer, so we must not
  // delete it.
  clang::SourceManager* source_manager_;

  // Return true if the type contains types that differ in size between 32-bit
  // and 64-bit architectures.  This is true for types that are typedefed to a
  // pointer, long, or unsigned long and also any types that contain an
  // architecture-dependent type.  Note that this is a bit overly restrictive;
  // some unions may be consistent size on 32-bit and 64-bit architectures
  // despite containing one of these types.  But it's not an important enough
  // issue to warrant coding the special case.
  // Structs, enums, and unions that do NOT contain arch-dependent types are
  // crafted to be the same size on 32-bit and 64-bit platforms by convention.
  bool HasArchDependentSize(const clang::Type& type) {
    if (type.isPointerType()) {
      return true;
    } else if (const clang::BuiltinType* builtin =
        dyn_cast<clang::BuiltinType>(&type)) {
      if ((builtin->getKind() == clang::BuiltinType::Long) ||
          (builtin->getKind() == clang::BuiltinType::ULong)) {
        return true;
      }
    } else if (const clang::ArrayType* array =
        dyn_cast<clang::ArrayType>(&type)) {
      // If it's an array, it has architecture-dependent size if its elements
      // do.
      return HasArchDependentSize(*(array->getElementType().getTypePtr()));
    } else if (const clang::TypedefType* typedef_type =
        dyn_cast<clang::TypedefType>(&type)) {
      return HasArchDependentSize(*(typedef_type->desugar().getTypePtr()));
    } else if (const clang::RecordType* record =
        dyn_cast<clang::RecordType>(&type)) {
      // If it's a struct or union, iterate through the fields.  If any of them
      // has architecture-dependent size, then we do too.
      const clang::RecordDecl* decl = record->getDecl();
      clang::RecordDecl::field_iterator iter(decl->field_begin());
      clang::RecordDecl::field_iterator end(decl->field_end());
      for (; iter != end; ++iter) {
        if (HasArchDependentSize(*(iter->getType().getTypePtr()))) {
          return true;
        }
      }
      // It's a struct or union, but contains no architecture-dependent members.
      return false;
    }
    return false;
  }

  void PrintTypeInfo(const std::string& name, const clang::Type& type,
                     const std::string& kind, const clang::CharUnits& size,
                     const clang::SourceLocation& begin_loc,
                     const clang::SourceLocation& end_loc) {
    clang::PresumedLoc presumed_begin(
        source_manager_->getPresumedLoc(begin_loc));
    clang::PresumedLoc presumed_end(source_manager_->getPresumedLoc(end_loc));
    std::printf("%s,%s,%lu,%s,%s,%u,%u\n",
                kind.c_str(),
                name.c_str(),
                size.getQuantity(),
                HasArchDependentSize(type) ? kArchDependent : kNotArchDependent,
                presumed_begin.getFilename(),
                presumed_begin.getLine(),
                presumed_end.getLine());
  }

  // Virtual function to consume top-level declarations.  For each one, we check
  // to see if it is a type definition.  If it is, we print information about
  // it.
  virtual void HandleTopLevelDecl(clang::DeclGroupRef decl_group) {
    clang::DeclGroupRef::iterator iter(decl_group.begin());
    clang::DeclGroupRef::iterator the_end(decl_group.end());
    for (; iter != the_end; ++iter) {
      const clang::Decl *decl = *iter;
      if (const clang::TypeDecl* type_decl = dyn_cast<clang::TypeDecl>(decl)) {
        std::string name(type_decl->getNameAsString());
        clang::SourceLocation start_loc = type_decl->getLocStart();
        clang::SourceLocation end_loc = type_decl->getLocEnd();
        // TagDecl covers structs, enums, unions, and classes.
        if (const clang::TagDecl* tag = dyn_cast<clang::TagDecl>(type_decl)) {
          // Only print out info when we find the definition;  ignore forward
          // references.
          if (tag->isDefinition()) {
            clang::Type* type = type_decl->getTypeForDecl();
            clang::CharUnits size =
                tag->getASTContext().getTypeSizeInChars(type);
            PrintTypeInfo(name, *type, type->getTypeClassName(), size,
                          start_loc, end_loc);
          }
        } else if (const clang::TypedefDecl* td =
                   dyn_cast<clang::TypedefDecl>(type_decl)) {
          clang::Type* type = td->getUnderlyingType().getTypePtr();
          clang::CharUnits size = td->getASTContext().getTypeSizeInChars(type);
          PrintTypeInfo(name, *type, kTypedefName, size, start_loc, end_loc);
        }
      }
    }
  }
};

class PrintNamesAndSizesAction : public clang::PluginASTAction {
 public:
  PrintNamesAndSizesAction() {}

 private:
  virtual clang::ASTConsumer *CreateASTConsumer(
      clang::CompilerInstance &instance, llvm::StringRef /*input_file*/) {
    return new PrintNamesAndSizesConsumer(
        &(instance.getDiagnostics().getSourceManager()));
  }

  // We don't accept any arguments, but ParseArgs is pure-virtual.
  virtual bool ParseArgs(const clang::CompilerInstance& /*instance*/,
                         const std::vector<std::string>& /*args*/) {
    return true;
  }
};

}  // namespace

static clang::FrontendPluginRegistry::Add<PrintNamesAndSizesAction>
X("PrintNamesAndSizes",
  "Print the names and sizes of classes, enums, and typedefs.");

