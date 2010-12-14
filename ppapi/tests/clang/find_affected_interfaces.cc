// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Clang plugin which finds types that are affected if the types listed in the
// plugin parameters are changed.  This is for use in determining what PPAPI
// C-level interfaces are affected if one or more PPAPI C structs are changed.

#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/CharUnits.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

namespace {

typedef std::vector<std::string> StringVec;

class FindAffectedInterfacesConsumer : public clang::ASTConsumer {
 public:
  explicit FindAffectedInterfacesConsumer(const StringVec& changed_classes) {
    StringVec::const_iterator iter(changed_classes.begin());
    StringVec::const_iterator the_end(changed_classes.end());
    for (; iter != the_end; ++iter) {
      class_is_affected_map_[*iter] = true;
    }
  }

 private:
  typedef std::map<std::string, bool> StringBoolMap;
  StringBoolMap class_is_affected_map_;

  bool IsAffected(const clang::Type& type_to_check) {
    std::string type_string(
      type_to_check.getCanonicalTypeInternal().getAsString());
    std::pair<StringBoolMap::iterator, bool> iter_success_pair =
        class_is_affected_map_.insert(
            StringBoolMap::value_type(type_string, false));
    StringBoolMap::iterator iter(iter_success_pair.first);
    bool successfully_inserted(iter_success_pair.second);
    // If we were able to insert, then we haven't yet seen this type.  Compute
    // IsAffected and put it at the newly inserted location in the map.
    if (successfully_inserted) {
      if (type_to_check.isPointerType()) {
        const clang::PointerType* pointer_type =
            dyn_cast<clang::PointerType>(&type_to_check);
        // Recurse to the pointee type.
        iter->second = IsAffected(*pointer_type->getPointeeType().getTypePtr());
      } else if (type_to_check.isFunctionProtoType()) {
        const clang::FunctionProtoType* func_type =
            dyn_cast<clang::FunctionProtoType>(&type_to_check);
        // Recurse to the return type and parameter types.
        iter->second = IsAffected(*func_type->getResultType().getTypePtr());
        if (!iter->second) {
          clang::FunctionProtoType::arg_type_iterator arg_iter =
              func_type->arg_type_begin();
          clang::FunctionProtoType::arg_type_iterator arg_end =
              func_type->arg_type_end();
          for (; (arg_iter != arg_end) && (!iter->second); ++arg_iter) {
            iter->second = IsAffected(*(arg_iter->getTypePtr()));
          }
        }
      } else if (type_to_check.isRecordType()) {
        // For records (unions, structs), recurse to the fields.
        const clang::RecordType* record =
            dyn_cast<clang::RecordType>(&type_to_check);
        const clang::RecordDecl* decl = record->getDecl();
        clang::RecordDecl::field_iterator field_iter(decl->field_begin());
        clang::RecordDecl::field_iterator field_end(decl->field_end());
        for (; (field_iter != field_end) && (!iter->second); ++field_iter) {
          iter->second = IsAffected(*(field_iter->getType().getTypePtr()));
        }
      }
    }
    // By this point, the bool in the map at the location referenced by iter has
    // the correct value.  Either it was cached, or we computed & inserted it.
    return iter->second;
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
        // TagDecl covers structs, enums, unions, and classes.
        if (const clang::TagDecl* tag = dyn_cast<clang::TagDecl>(type_decl)) {
          // Only print out info when we find the definition;  ignore forward
          // references.
          if (tag->isDefinition()) {
            clang::Type* type = type_decl->getTypeForDecl();
            if (IsAffected(*type)) {
              std::printf("%s\n", name.c_str());
            }
          }
        } else if (const clang::TypedefDecl* td =
                   dyn_cast<clang::TypedefDecl>(type_decl)) {
          clang::Type* type = td->getUnderlyingType().getTypePtr();
          if (IsAffected(*type)) {
            std::printf("%s\n", name.c_str());
          }
        }
      }
    }
  }
};

class FindAffectedInterfacesAction : public clang::PluginASTAction {
 public:
  FindAffectedInterfacesAction() {}

 private:
  StringVec types_;

  virtual clang::ASTConsumer *CreateASTConsumer(
      clang::CompilerInstance &instance, llvm::StringRef /*input_file*/) {
    return new FindAffectedInterfacesConsumer(types_);
  }

  virtual bool ParseArgs(const clang::CompilerInstance& /*instance*/,
                         const std::vector<std::string>& args) {
    // Every argument is interpreted as a comma-delimited list of names of types
    // that have been changed.
    StringVec::const_iterator iter(args.begin()), end(args.end());
    for (; iter != end; ++iter) {
      std::stringstream stream(*iter);
      std::string type_name;
      while (std::getline(stream, type_name, ',')) {
        types_.push_back(type_name);
      }
    }
    return true;
  }
};

}  // namespace

static clang::FrontendPluginRegistry::Add<FindAffectedInterfacesAction>
X("FindAffectedInterfaces",
  "Find interfaces affected by changes to the passes classes.");

