// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clang plugin checks various invariants of the Blink garbage
// collection infrastructure.
//
// Errors are described at:
// http://www.chromium.org/developers/blink-gc-plugin-errors

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

const char kBaseRequiresTracing[] =
    "[blink-gc] Base class %0 of derived class %1 requires tracing.";

const char kFieldsRequireTracing[] =
    "[blink-gc] Class %0 has untraced fields that require tracing.";

const char kFieldRequiresTracingNote[] =
    "[blink-gc] Untraced field %0 declared here:";

const char kClassContainsInvalidFields[] =
    "[blink-gc] Class %0 contains invalid fields.";

const char kClassContainsGCRoot[] =
    "[blink-gc] Class %0 contains GC root in field %1.";

const char kFinalizerInNonFinalizedClass[] =
    "[blink-gc] Non-finalized class %0 has a user-declared finalizer %1.";

const char kFinalizerAccessesFinalizedField[] =
    "[blink-gc] Finalizer %0 accesses potentially finalized field %1.";

const char kRawPtrToGCManagedClassNote[] =
    "[blink-gc] Raw pointer field %0 to a GC managed class declared here:";

const char kRefPtrToGCManagedClassNote[] =
    "[blink-gc] RefPtr field %0 to a GC managed class declared here:";

const char kOwnPtrToGCManagedClassNote[] =
    "[blink-gc] OwnPtr field %0 to a GC managed class declared here:";

const char kPartObjectContainsGCRoot[] =
    "[blink-gc] Field %0 with embedded GC root in %1 declared here:";

const char kFieldContainsGCRoot[] =
    "[blink-gc] Field %0 defining a GC root declared here:";

const char kFinalizedFieldNote[] =
    "[blink-gc] Potentially finalized field %0 declared here:";

struct BlinkGCPluginOptions {
  BlinkGCPluginOptions() : enable_oilpan(false) {}
  bool enable_oilpan;
  std::set<std::string> ignored_classes;
  std::set<std::string> checked_namespaces;
  std::vector<std::string> ignored_directories;
};

typedef std::vector<CXXRecordDecl*> RecordVector;
typedef std::vector<CXXMethodDecl*> MethodVector;

// Test if a template specialization is an instantiation.
static bool IsTemplateInstantiation(CXXRecordDecl* record) {
  ClassTemplateSpecializationDecl* spec =
      dyn_cast<ClassTemplateSpecializationDecl>(record);
  if (!spec)
    return false;
  switch (spec->getTemplateSpecializationKind()) {
    case TSK_ImplicitInstantiation:
    case TSK_ExplicitInstantiationDefinition:
      return true;
    case TSK_Undeclared:
    case TSK_ExplicitSpecialization:
      return false;
    // TODO: unsupported cases.
    case TSK_ExplicitInstantiationDeclaration:
      return false;
    default:
      assert(false && "Unknown template specialization kind");
  }
}

// This visitor collects the entry points for the checker.
class CollectVisitor : public RecursiveASTVisitor<CollectVisitor> {
 public:
  CollectVisitor() {}

  RecordVector& record_decls() { return record_decls_; }
  MethodVector& trace_decls() { return trace_decls_; }

  bool shouldVisitTemplateInstantiations() { return false; }

  // Collect record declarations, including nested declarations.
  bool VisitCXXRecordDecl(CXXRecordDecl* record) {
    if (record->hasDefinition() && record->isCompleteDefinition())
      record_decls_.push_back(record);
    return true;
  }

  // Collect tracing method definitions, but don't traverse method bodies.
  bool TraverseCXXMethodDecl(CXXMethodDecl* method) {
    if (method->isThisDeclarationADefinition() && Config::IsTraceMethod(method))
      trace_decls_.push_back(method);
    return true;
  }

 private:
  RecordVector record_decls_;
  MethodVector trace_decls_;
};

// This visitor checks that a finalizer method does not access fields that are
// potentially finalized. A potentially finalized field is either a Member, a
// heap-allocated collection or an off-heap collection that contains Members.
class CheckFinalizerVisitor
    : public RecursiveASTVisitor<CheckFinalizerVisitor> {
 private:
  // Simple visitor to determine if the content of a field might be collected
  // during finalization.
  class MightBeCollectedVisitor : public EdgeVisitor {
   public:
    MightBeCollectedVisitor() : might_be_collected_(false) {}
    bool might_be_collected() { return might_be_collected_; }
    void VisitMember(Member* edge) { might_be_collected_ = true; }
    void VisitCollection(Collection* edge) {
      if (edge->on_heap()) {
        might_be_collected_ = !edge->is_root();
      } else {
        edge->AcceptMembers(this);
      }
    }

   private:
    bool might_be_collected_;
  };

 public:
  typedef std::vector<std::pair<MemberExpr*, FieldPoint*> > Errors;

  CheckFinalizerVisitor(RecordCache* cache) : cache_(cache) {}

  Errors& finalized_fields() { return finalized_fields_; }

  bool VisitMemberExpr(MemberExpr* member) {
    FieldDecl* field = dyn_cast<FieldDecl>(member->getMemberDecl());
    if (!field)
      return true;

    RecordInfo* info = cache_->Lookup(field->getParent());
    if (!info)
      return true;

    RecordInfo::Fields::iterator it = info->GetFields().find(field);
    if (it == info->GetFields().end())
      return true;

    if (MightBeCollected(&it->second))
      finalized_fields_.push_back(std::make_pair(member, &it->second));
    return true;
  }

  bool MightBeCollected(FieldPoint* point) {
    MightBeCollectedVisitor visitor;
    point->edge()->Accept(&visitor);
    return visitor.might_be_collected();
  }

 private:
  Errors finalized_fields_;
  RecordCache* cache_;
};

// This visitor checks a tracing method by traversing its body.
// - A member field is considered traced if it is referenced in the body.
// - A base is traced if a base-qualified call to a trace method is found.
class CheckTraceVisitor : public RecursiveASTVisitor<CheckTraceVisitor> {
 public:
  CheckTraceVisitor(CXXMethodDecl* trace, RecordInfo* info)
      : trace_(trace), info_(info) {}

  // Allow recursive traversal by using VisitMemberExpr.
  bool VisitMemberExpr(MemberExpr* member) {
    // If this member expression references a field decl, mark it as traced.
    if (FieldDecl* field = dyn_cast<FieldDecl>(member->getMemberDecl())) {
      if (IsTemplateInstantiation(info_->record())) {
        // Pointer equality on fields does not work for template instantiations.
        // The trace method refers to fields of the template definition which
        // are different from the instantiated fields that need to be traced.
        const string& name = field->getNameAsString();
        for (RecordInfo::Fields::iterator it = info_->GetFields().begin();
             it != info_->GetFields().end();
             ++it) {
          if (it->first->getNameAsString() == name) {
            MarkTraced(it);
            break;
          }
        }
      } else {
        RecordInfo::Fields::iterator it = info_->GetFields().find(field);
        if (it != info_->GetFields().end())
          MarkTraced(it);
      }
      return true;
    }

    // If this is a weak callback function we only check field tracing.
    if (IsWeakCallback())
      return true;

    // For method calls, check tracing of bases and other special GC methods.
    if (CXXMethodDecl* fn = dyn_cast<CXXMethodDecl>(member->getMemberDecl())) {
      const string& name = fn->getNameAsString();
      // Check weak callbacks.
      if (name == kRegisterWeakMembersName) {
        if (fn->isTemplateInstantiation()) {
          const TemplateArgumentList& args =
              *fn->getTemplateSpecializationInfo()->TemplateArguments;
          // The second template argument is the callback method.
          if (args.size() > 1 &&
              args[1].getKind() == TemplateArgument::Declaration) {
            if (FunctionDecl* callback =
                    dyn_cast<FunctionDecl>(args[1].getAsDecl())) {
              if (callback->hasBody()) {
                CheckTraceVisitor nested_visitor(info_);
                nested_visitor.TraverseStmt(callback->getBody());
              }
            }
          }
        }
        return true;
      }

      // TODO: It is possible to have multiple bases, where one must be traced
      // using a traceAfterDispatch. In such a case we should also check that
      // the mixin does not add a vtable.
      if (Config::IsTraceMethod(fn) && member->hasQualifier()) {
        if (const Type* type = member->getQualifier()->getAsType()) {
          if (CXXRecordDecl* decl = type->getAsCXXRecordDecl()) {
            RecordInfo::Bases::iterator it = info_->GetBases().find(decl);
            if (it != info_->GetBases().end())
              it->second.MarkTraced();
          }
        }
      }
    }
    return true;
  }

 private:
  // Nested checking for weak callbacks.
  CheckTraceVisitor(RecordInfo* info) : trace_(0), info_(info) {}

  bool IsWeakCallback() { return !trace_; }

  void MarkTraced(RecordInfo::Fields::iterator it) {
    // In a weak callback we can't mark strong fields as traced.
    if (IsWeakCallback() && !it->second.edge()->IsWeakMember())
      return;
    it->second.MarkTraced();
  }

  CXXMethodDecl* trace_;
  RecordInfo* info_;
};

// This visitor checks that the fields of a class and the fields of
// its part objects don't define GC roots.
class CheckGCRootsVisitor : public RecursiveEdgeVisitor {
 public:
  typedef std::vector<FieldPoint*> RootPath;
  typedef std::vector<RootPath> Errors;

  CheckGCRootsVisitor() {}

  Errors& gc_roots() { return gc_roots_; }

  bool ContainsGCRoots(RecordInfo* info) {
    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      current_.push_back(&it->second);
      it->second.edge()->Accept(this);
      current_.pop_back();
    }
    return !gc_roots_.empty();
  }

  void VisitValue(Value* edge) {
    // TODO: what should we do to check unions?
    if (edge->value()->record()->isUnion())
      return;

    // If the value is a part object, then continue checking for roots.
    for (Context::iterator it = context().begin();
         it != context().end();
         ++it) {
      if (!(*it)->IsCollection())
        return;
    }
    ContainsGCRoots(edge->value());
  }

  void VisitPersistent(Persistent* edge) {
    gc_roots_.push_back(current_);
  }

  void AtCollection(Collection* edge) {
    if (edge->is_root())
      gc_roots_.push_back(current_);
  }

 protected:
  RootPath current_;
  Errors gc_roots_;
};

// This visitor checks that the fields of a class are "well formed".
// - OwnPtr, RefPtr and RawPtr must not point to a GC derived types.
// - An on-heap class must never contain GC roots.
class CheckFieldsVisitor : public RecursiveEdgeVisitor {
 public:
  typedef std::vector<std::pair<FieldPoint*, Edge*> > Errors;

  CheckFieldsVisitor(const BlinkGCPluginOptions& options)
      : options_(options), current_(0) {}

  Errors& invalid_fields() { return invalid_fields_; }

  bool ContainsInvalidFields(RecordInfo* info) {
    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      context().clear();
      current_ = &it->second;
      current_->edge()->Accept(this);
    }
    return !invalid_fields_.empty();
  }

  void VisitValue(Value* edge) {
    // TODO: what should we do to check unions?
    if (edge->value()->record()->isUnion())
      return;

    if (!Parent() || !edge->value()->IsGCAllocated())
      return;

    if (Parent()->IsOwnPtr())
      invalid_fields_.push_back(std::make_pair(current_, Parent()));

    // Don't check raw and ref pointers in transition mode.
    if (options_.enable_oilpan)
      return;

    if (Parent()->IsRawPtr() || Parent()->IsRefPtr())
      invalid_fields_.push_back(std::make_pair(current_, Parent()));
  }

 private:
  const BlinkGCPluginOptions& options_;
  FieldPoint* current_;
  Errors invalid_fields_;
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
    diag_base_requires_tracing_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kBaseRequiresTracing);
    diag_fields_require_tracing_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kFieldsRequireTracing);
    diag_class_contains_invalid_fields_ =
        diagnostic_.getCustomDiagID(getErrorLevel(),
                                    kClassContainsInvalidFields);
    diag_class_contains_gc_root_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kClassContainsGCRoot);
    diag_finalizer_in_nonfinalized_class_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kFinalizerInNonFinalizedClass);
    diag_finalizer_accesses_finalized_field_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kFinalizerAccessesFinalizedField);

    // Register note messages.
    diag_field_requires_tracing_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kFieldRequiresTracingNote);
    diag_raw_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kRawPtrToGCManagedClassNote);
    diag_ref_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kRefPtrToGCManagedClassNote);
    diag_own_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kOwnPtrToGCManagedClassNote);
    diag_part_object_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kPartObjectContainsGCRoot);
    diag_field_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kFieldContainsGCRoot);
    diag_finalized_field_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kFinalizedFieldNote);
  }

  virtual void HandleTranslationUnit(ASTContext& context) {
    CollectVisitor visitor;
    visitor.TraverseDecl(context.getTranslationUnitDecl());

    for (RecordVector::iterator it = visitor.record_decls().begin();
         it != visitor.record_decls().end();
         ++it) {
      CheckRecord(cache_.Lookup(*it));
    }

    for (MethodVector::iterator it = visitor.trace_decls().begin();
         it != visitor.trace_decls().end();
         ++it) {
      CheckTracingMethod(*it);
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

    {
      CheckFieldsVisitor visitor(options_);
      if (visitor.ContainsInvalidFields(info))
        ReportClassContainsInvalidFields(info, &visitor.invalid_fields());
    }

    if (info->IsGCDerived()) {
      {
        CheckGCRootsVisitor visitor;
        if (visitor.ContainsGCRoots(info))
          ReportClassContainsGCRoots(info, &visitor.gc_roots());
      }

      // TODO: check for non-user defined and non-trivial destructors too.
      // TODO: support overridden finalize().
      if (CXXDestructorDecl* dtor = info->record()->getDestructor()) {
        if (dtor->isUserProvided() && !info->IsGCFinalized()) {
          // Don't report if using transition types and the body is empty.
          if (!options_.enable_oilpan) {
            ReportFinalizerInNonFinalizedClass(info, dtor);
          } else {
            if (dtor->hasBody()) {
              CompoundStmt* stmt = cast<CompoundStmt>(dtor->getBody());
              if (stmt && !stmt->body_empty())
                ReportFinalizerInNonFinalizedClass(info, dtor);
            }
          }
        }

        if (dtor->hasBody()) {
          CheckFinalizerVisitor visitor(&cache_);
          visitor.TraverseCXXMethodDecl(dtor);
          if (!visitor.finalized_fields().empty()) {
            ReportFinalizerAccessesFinalizedFields(dtor,
                                                   &visitor.finalized_fields());
          }
        }
      }
    }
  }

  // This is the main entry for tracing method definitions.
  void CheckTracingMethod(CXXMethodDecl* method) {
    RecordInfo* parent = cache_.Lookup(method->getParent());
    if (IsIgnored(parent))
      return;

    // Check templated tracing methods by checking the template instantiations.
    // Specialized templates are handled as ordinary classes.
    if (ClassTemplateDecl* tmpl =
            parent->record()->getDescribedClassTemplate()) {
      for (ClassTemplateDecl::spec_iterator it = tmpl->spec_begin();
           it != tmpl->spec_end();
           ++it) {
        // Check trace using each template instantiation as the holder.
        if (IsTemplateInstantiation(*it))
          CheckTraceOrDispatchMethod(cache_.Lookup(*it), method);
      }
      return;
    }

    CheckTraceOrDispatchMethod(parent, method);
  }

  // Determine what type of tracing method this is (dispatch or trace).
  void CheckTraceOrDispatchMethod(RecordInfo* parent, CXXMethodDecl* method) {
    bool isTraceAfterDispatch;
    if (Config::IsTraceMethod(method, &isTraceAfterDispatch)) {
      if (!isTraceAfterDispatch && parent->GetTraceDispatchMethod())
        CheckTraceDispatchMethod(parent, method);
      else
        CheckTraceMethod(parent, method);
    }
  }

  // Check a tracing dispatch (ie, it dispatches to traceAfterDispatch)
  void CheckTraceDispatchMethod(RecordInfo* parent, CXXMethodDecl* trace) {
    // TODO: check correct dispatch.
  }

  // Check an actual trace method.
  void CheckTraceMethod(RecordInfo* parent, CXXMethodDecl* trace) {
    CheckTraceVisitor visitor(trace, parent);
    visitor.TraverseCXXMethodDecl(trace);

    for (RecordInfo::Bases::iterator it = parent->GetBases().begin();
         it != parent->GetBases().end();
         ++it) {
      if (!it->second.IsProperlyTraced())
        ReportBaseRequiresTracing(parent, trace, it->first);
    }

    for (RecordInfo::Fields::iterator it = parent->GetFields().begin();
         it != parent->GetFields().end();
         ++it) {
      if (!it->second.IsProperlyTraced()) {
        // Discontinue once an untraced-field error is found.
        ReportFieldsRequireTracing(parent, trace);
        break;
      }
    }
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
      if (!it->second.IsProperlyTraced())
        NoteFieldRequiresTracing(info, it->first);
    }
  }

  void ReportBaseRequiresTracing(RecordInfo* derived,
                                 CXXMethodDecl* trace,
                                 CXXRecordDecl* base) {
    SourceLocation loc = trace->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_base_requires_tracing_)
        << base << derived->record();
  }

  void ReportFieldsRequireTracing(RecordInfo* info, CXXMethodDecl* trace) {
    SourceLocation loc = trace->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_fields_require_tracing_)
        << info->record();
    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      if (!it->second.IsProperlyTraced())
        NoteFieldRequiresTracing(info, it->first);
    }
  }

  void ReportClassContainsInvalidFields(RecordInfo* info,
                                        CheckFieldsVisitor::Errors* errors) {
    SourceLocation loc = info->record()->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_class_contains_invalid_fields_)
        << info->record();
    for (CheckFieldsVisitor::Errors::iterator it = errors->begin();
         it != errors->end();
         ++it) {
      if (it->second->IsRawPtr()) {
        NoteField(it->first, diag_raw_ptr_to_gc_managed_class_note_);
      } else if (it->second->IsRefPtr()) {
        NoteField(it->first, diag_ref_ptr_to_gc_managed_class_note_);
      } else if (it->second->IsOwnPtr()) {
        NoteField(it->first, diag_own_ptr_to_gc_managed_class_note_);
      }
    }
  }

  void ReportClassContainsGCRoots(RecordInfo* info,
                                  CheckGCRootsVisitor::Errors* errors) {
    SourceLocation loc = info->record()->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    for (CheckGCRootsVisitor::Errors::iterator it = errors->begin();
         it != errors->end();
         ++it) {
      CheckGCRootsVisitor::RootPath::iterator path = it->begin();
      FieldPoint* point = *path;
      diagnostic_.Report(full_loc, diag_class_contains_gc_root_)
          << info->record() << point->field();
      while (++path != it->end()) {
        NotePartObjectContainsGCRoot(point);
        point = *path;
      }
      NoteFieldContainsGCRoot(point);
    }
  }

  void ReportFinalizerInNonFinalizedClass(RecordInfo* info,
                                          CXXMethodDecl* dtor) {
    SourceLocation loc = dtor->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_finalizer_in_nonfinalized_class_)
        << info->record() << dtor;
  }

  void ReportFinalizerAccessesFinalizedFields(
      CXXMethodDecl* dtor,
      CheckFinalizerVisitor::Errors* fields) {
    for (CheckFinalizerVisitor::Errors::iterator it = fields->begin();
         it != fields->end();
         ++it) {
      SourceLocation loc = it->first->getLocStart();
      SourceManager& manager = instance_.getSourceManager();
      FullSourceLoc full_loc(loc, manager);
      diagnostic_.Report(full_loc, diag_finalizer_accesses_finalized_field_)
          << dtor << it->second->field();
      NoteField(it->second, diag_finalized_field_note_);
    }
  }

  void NoteFieldRequiresTracing(RecordInfo* holder, FieldDecl* field) {
    NoteField(field, diag_field_requires_tracing_note_);
  }

  void NotePartObjectContainsGCRoot(FieldPoint* point) {
    FieldDecl* field = point->field();
    SourceLocation loc = field->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_part_object_contains_gc_root_note_)
        << field << field->getParent();
  }

  void NoteFieldContainsGCRoot(FieldPoint* point) {
    NoteField(point, diag_field_contains_gc_root_note_);
  }

  void NoteField(FieldPoint* point, unsigned note) {
    NoteField(point->field(), note);
  }

  void NoteField(FieldDecl* field, unsigned note) {
    SourceLocation loc = field->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, note) << field;
  }

  unsigned diag_class_requires_trace_method_;
  unsigned diag_base_requires_tracing_;
  unsigned diag_fields_require_tracing_;
  unsigned diag_class_contains_invalid_fields_;
  unsigned diag_class_contains_gc_root_;
  unsigned diag_finalizer_in_nonfinalized_class_;
  unsigned diag_finalizer_accesses_finalized_field_;

  unsigned diag_field_requires_tracing_note_;
  unsigned diag_raw_ptr_to_gc_managed_class_note_;
  unsigned diag_ref_ptr_to_gc_managed_class_note_;
  unsigned diag_own_ptr_to_gc_managed_class_note_;
  unsigned diag_part_object_contains_gc_root_note_;
  unsigned diag_field_contains_gc_root_note_;
  unsigned diag_finalized_field_note_;

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
        options_.enable_oilpan = true;
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

static FrontendPluginRegistry::Add<BlinkGCPluginAction> X(
    "blink-gc-plugin",
    "Check Blink GC invariants");
