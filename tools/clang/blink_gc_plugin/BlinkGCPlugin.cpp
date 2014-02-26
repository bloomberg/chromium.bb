// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clang plugin checks various invariants of the Blink garbage
// collection infrastructure.
//
// Checks that are implemented:
// [currently none]

#include "Config.h"
#include "RecordInfo.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

using namespace clang;
using std::string;

namespace {

const char kClassRequiresTraceMethod[] =
    "[blink-gc] Class %0 requires a trace method"
    " because it contains fields that require tracing.";

const char kFieldRequiresTracingNote[] =
    "[blink-gc] Untraced field %0 declared here:";

struct BlinkGCPluginOptions {
  BlinkGCPluginOptions() {}
  std::set<std::string> ignored_classes;
  std::set<std::string> checked_namespaces;
  std::vector<std::string> ignored_directories;
};

typedef std::vector<CXXRecordDecl*> RecordVector;
typedef std::vector<CXXMethodDecl*> MethodVector;

// This visitor collects the entry points for the checker.
class CollectVisitor : public RecursiveASTVisitor<CollectVisitor> {
 public:
  CollectVisitor() {}

  RecordVector& record_decls() { return record_decls_; }

  bool shouldVisitTemplateInstantiations() { return false; }

  // Collect record declarations, including nested declarations.
  bool VisitCXXRecordDecl(CXXRecordDecl* record) {
    if (record->hasDefinition() && record->isCompleteDefinition())
      record_decls_.push_back(record);
    return true;
  }

 private:
  RecordVector record_decls_;
};

// Main class containing checks for various invariants of the Blink
// garbage collection infrastructure.
class BlinkGCPluginConsumer : public ASTConsumer {
 public:
  BlinkGCPluginConsumer(CompilerInstance& instance,
                        const BlinkGCPluginOptions& options)
      : instance_(instance),
        diagnostic_(instance.getDiagnostics()),
        options_(options) {

    // Only check structures in the blink, WebCore and WebKit namespaces.
    options_.checked_namespaces.insert("blink");
    options_.checked_namespaces.insert("WebCore");
    options_.checked_namespaces.insert("WebKit");

    // Ignore GC implementation files.
    options_.ignored_directories.push_back("/heap/");

    // Register warning/error messages.
    diag_class_requires_trace_method_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kClassRequiresTraceMethod);

    // Register note messages.
    diag_field_requires_tracing_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kFieldRequiresTracingNote);
  }

  virtual void HandleTranslationUnit(ASTContext& context) {
    CollectVisitor visitor;
    visitor.TraverseDecl(context.getTranslationUnitDecl());

    for (RecordVector::iterator it = visitor.record_decls().begin();
         it != visitor.record_decls().end();
         ++it) {
      CheckRecord(cache_.Lookup(*it));
    }
  }

  // Main entry for checking a record declaration.
  void CheckRecord(RecordInfo* info) {
    if (IsIgnored(info))
      return;

    CXXRecordDecl* record = info->record();

    // TODO: what should we do to check unions?
    if (record->isUnion())
      return;

    // If this is the primary template declaration, check its specializations.
    if (record->isThisDeclarationADefinition() &&
        record->getDescribedClassTemplate()) {
      ClassTemplateDecl* tmpl = record->getDescribedClassTemplate();
      for (ClassTemplateDecl::spec_iterator it = tmpl->spec_begin();
           it != tmpl->spec_end();
           ++it) {
        CheckClass(cache_.Lookup(*it));
      }
      return;
    }

    CheckClass(info);
  }

  // Check a class-like object (eg, class, specialization, instantiation).
  void CheckClass(RecordInfo* info) {
    // Don't enforce tracing of stack allocated objects.
    if (info->IsStackAllocated())
      return;

    if (info->RequiresTraceMethod() && !info->GetTraceMethod())
      ReportClassRequiresTraceMethod(info);
  }

  // Adds either a warning or error, based on the current handling of -Werror.
  DiagnosticsEngine::Level getErrorLevel() {
    return diagnostic_.getWarningsAsErrors() ? DiagnosticsEngine::Error
                                             : DiagnosticsEngine::Warning;
  }

  bool IsIgnored(RecordInfo* record) {
    return !InCheckedNamespace(record) ||
           IsIgnoredClass(record) ||
           InIgnoredDirectory(record);
  }

  bool IsIgnoredClass(RecordInfo* info) {
    // Ignore any class prefixed by SameSizeAs. These are used in
    // Blink to verify class sizes and don't need checking.
    const string SameSizeAs = "SameSizeAs";
    if (info->name().compare(0, SameSizeAs.size(), SameSizeAs) == 0)
      return true;
    return options_.ignored_classes.find(info->name()) !=
           options_.ignored_classes.end();
  }

  bool InIgnoredDirectory(RecordInfo* info) {
    string filename;
    if (!GetFilename(info->record()->getLocStart(), &filename))
      return false;  // TODO: should we ignore non-existing file locations?
    std::vector<string>::iterator it = options_.ignored_directories.begin();
    for (; it != options_.ignored_directories.end(); ++it)
      if (filename.find(*it) != string::npos)
        return true;
    return false;
  }

  bool InCheckedNamespace(RecordInfo* info) {
    DeclContext* context = info->record()->getDeclContext();
    switch (context->getDeclKind()) {
      case Decl::Namespace: {
        const NamespaceDecl* decl = dyn_cast<NamespaceDecl>(context);
        if (decl->isAnonymousNamespace())
          return false;
        return options_.checked_namespaces.find(decl->getNameAsString()) !=
               options_.checked_namespaces.end();
      }
      default:
        return false;
    }
  }

  bool GetFilename(SourceLocation loc, string* filename) {
    const SourceManager& source_manager = instance_.getSourceManager();
    SourceLocation spelling_location = source_manager.getSpellingLoc(loc);
    PresumedLoc ploc = source_manager.getPresumedLoc(spelling_location);
    if (ploc.isInvalid()) {
      // If we're in an invalid location, we're looking at things that aren't
      // actually stated in the source.
      return false;
    }
    *filename = ploc.getFilename();
    return true;
  }

  void ReportClassRequiresTraceMethod(RecordInfo* info) {
    SourceLocation loc = info->record()->getInnerLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_class_requires_trace_method_)
        << info->record();
    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      if (it->second.IsTracingRequired())
        NoteFieldRequiresTracing(info, it->first);
    }
  }

  void NoteFieldRequiresTracing(RecordInfo* holder, FieldDecl* field) {
    SourceLocation loc = field->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_field_requires_tracing_note_) << field;
  }

  unsigned diag_class_requires_trace_method_;

  unsigned diag_field_requires_tracing_note_;

  CompilerInstance& instance_;
  DiagnosticsEngine& diagnostic_;
  BlinkGCPluginOptions options_;
  RecordCache cache_;
};

class BlinkGCPluginAction : public PluginASTAction {
 public:
  BlinkGCPluginAction() {}

 protected:
  // Overridden from PluginASTAction:
  virtual ASTConsumer* CreateASTConsumer(CompilerInstance& instance,
                                         llvm::StringRef ref) {
    return new BlinkGCPluginConsumer(instance, options_);
  }

  virtual bool ParseArgs(const CompilerInstance& instance,
                         const std::vector<string>& args) {
    bool parsed = true;

    for (size_t i = 0; i < args.size() && parsed; ++i) {
      if (args[i] == "enable-oilpan") {
        // TODO: Remove this once all transition types are eliminated.
        Config::set_oilpan_enabled(true);
      } else {
        parsed = false;
        llvm::errs() << "Unknown blink-gc-plugin argument: " << args[i] << "\n";
      }
    }

    return parsed;
  }

 private:
  BlinkGCPluginOptions options_;
};

}  // namespace

bool Config::oilpan_enabled_ = false;

static FrontendPluginRegistry::Add<BlinkGCPluginAction> X(
    "blink-gc-plugin",
    "Check Blink GC invariants");
