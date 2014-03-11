// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Config.h"
#include "RecordInfo.h"

#include "clang/AST/Attr.h"

using namespace clang;
using std::string;

RecordInfo::RecordInfo(CXXRecordDecl* record, RecordCache* cache)
    : cache_(cache),
      record_(record),
      name_(record->getName()),
      fields_need_tracing_(TracingStatus::Unknown()),
      bases_(0),
      fields_(0),
      determined_trace_methods_(false),
      trace_method_(0),
      trace_dispatch_method_(0),
      is_gc_derived_(false),
      base_paths_(0) {}

RecordInfo::~RecordInfo() {
  delete fields_;
  delete bases_;
  delete base_paths_;
}

// Get |count| number of template arguments. Returns false if there
// are fewer than |count| arguments or any of the arguments are not
// of a valid Type structure. If |count| is non-positive, all
// arguments are collected.
bool RecordInfo::GetTemplateArgs(size_t count, TemplateArgs* output_args) {
  ClassTemplateSpecializationDecl* tmpl =
      dyn_cast<ClassTemplateSpecializationDecl>(record_);
  if (!tmpl)
    return false;
  const TemplateArgumentList& args = tmpl->getTemplateArgs();
  if (args.size() < count)
    return false;
  if (count <= 0)
    count = args.size();
  for (unsigned i = 0; i < count; ++i) {
    TemplateArgument arg = args[i];
    if (arg.getKind() == TemplateArgument::Type && !arg.getAsType().isNull()) {
      output_args->push_back(arg.getAsType().getTypePtr());
    } else {
      return false;
    }
  }
  return true;
}

// Test if a record is a HeapAllocated collection.
bool RecordInfo::IsHeapAllocatedCollection() {
  if (!Config::IsGCCollection(name_) && !Config::IsWTFCollection(name_))
    return false;

  TemplateArgs args;
  if (GetTemplateArgs(0, &args)) {
    for (TemplateArgs::iterator it = args.begin(); it != args.end(); ++it) {
      if (CXXRecordDecl* decl = (*it)->getAsCXXRecordDecl())
        if (decl->getName() == kHeapAllocatorName)
          return true;
    }
  }

  return Config::IsGCCollection(name_);
}

static bool IsGCBaseCallback(const CXXBaseSpecifier* specifier,
                             CXXBasePath& path,
                             void* data) {
  if (CXXRecordDecl* record = specifier->getType()->getAsCXXRecordDecl())
    return Config::IsGCBase(record->getName());
  return false;
}

// Test if a record is derived from a garbage collected base.
bool RecordInfo::IsGCDerived() {
  // If already computed, return the known result.
  if (base_paths_)
    return is_gc_derived_;

  base_paths_ = new CXXBasePaths(true, true, false);

  if (!record_->hasDefinition())
    return false;

  // The base classes are not themselves considered garbage collected objects.
  if (Config::IsGCBase(name_))
    return false;

  // Walk the inheritance tree to find GC base classes.
  is_gc_derived_ = record_->lookupInBases(IsGCBaseCallback, 0, *base_paths_);
  return is_gc_derived_;
}

bool RecordInfo::IsGCFinalized() {
  if (!IsGCDerived())
    return false;
  for (CXXBasePaths::paths_iterator it = base_paths_->begin();
       it != base_paths_->end();
       ++it) {
    const CXXBasePathElement& elem = (*it)[it->size() - 1];
    CXXRecordDecl* base = elem.Base->getType()->getAsCXXRecordDecl();
    if (Config::IsGCFinalizedBase(base->getName()))
      return true;
  }
  return false;
}

// Test if a record is allocated on the managed heap.
bool RecordInfo::IsGCAllocated() {
  return IsGCDerived() || IsHeapAllocatedCollection();
}

static bool IsAnnotated(Decl* decl, const string& anno) {
  AnnotateAttr* attr = decl->getAttr<AnnotateAttr>();
  return attr && (attr->getAnnotation() == anno);
}

bool RecordInfo::IsStackAllocated() {
  for (CXXRecordDecl::method_iterator it = record_->method_begin();
       it != record_->method_end();
       ++it) {
    if (it->getNameAsString() == kNewOperatorName)
      return it->isDeleted() && IsAnnotated(*it, "blink_stack_allocated");
  }
  return false;
}

// An object requires a tracing method if it has any fields that need tracing.
bool RecordInfo::RequiresTraceMethod() {
  GetFields();
  return fields_need_tracing_.IsNeeded();
}

// Get the actual tracing method (ie, can be traceAfterDispatch if there is a
// dispatch method).
CXXMethodDecl* RecordInfo::GetTraceMethod() {
  DetermineTracingMethods();
  return trace_method_;
}

// Get the static trace dispatch method.
CXXMethodDecl* RecordInfo::GetTraceDispatchMethod() {
  DetermineTracingMethods();
  return trace_dispatch_method_;
}

RecordInfo::Bases& RecordInfo::GetBases() {
  if (!bases_)
    bases_ = CollectBases();
  return *bases_;
}

bool RecordInfo::InheritsNonPureTrace() {
  if (CXXMethodDecl* trace = GetTraceMethod())
    return !trace->isPure();
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (it->second.info()->InheritsNonPureTrace())
      return true;
  }
  return false;
}

RecordInfo::Bases* RecordInfo::CollectBases() {
  // Compute the collection locally to avoid inconsistent states.
  Bases* bases = new Bases;
  for (CXXRecordDecl::base_class_iterator it = record_->bases_begin();
       it != record_->bases_end();
       ++it) {
    if (CXXRecordDecl* base = it->getType()->getAsCXXRecordDecl()) {
      RecordInfo* info = cache_->Lookup(base);
      TracingStatus status = info->InheritsNonPureTrace()
                                 ? TracingStatus::Needed()
                                 : TracingStatus::Unneeded();
      bases->insert(std::make_pair(base, BasePoint(info, status)));
    }
  }
  return bases;
}

RecordInfo::Fields& RecordInfo::GetFields() {
  if (!fields_)
    fields_ = CollectFields();
  return *fields_;
}

RecordInfo::Fields* RecordInfo::CollectFields() {
  // Compute the collection locally to avoid inconsistent states.
  Fields* fields = new Fields;
  TracingStatus fields_status = TracingStatus::Unneeded();
  for (RecordDecl::field_iterator it = record_->field_begin();
       it != record_->field_end();
       ++it) {
    FieldDecl* field = *it;
    // Ignore fields annotated with the NO_TRACE_CHECKING macro.
    if (IsAnnotated(field, "blink_no_trace_checking"))
      continue;
    if (Edge* edge = CreateEdge(field->getType().getTypePtrOrNull())) {
      fields->insert(std::make_pair(field, FieldPoint(field, edge)));
      fields_status = fields_status.LUB(edge->NeedsTracing(Edge::kRecursive));
    }
  }
  fields_need_tracing_ = fields_status;
  return fields;
}

void RecordInfo::DetermineTracingMethods() {
  if (determined_trace_methods_)
    return;
  determined_trace_methods_ = true;
  CXXMethodDecl* trace = 0;
  CXXMethodDecl* traceAfterDispatch = 0;
  bool isTraceAfterDispatch;
  for (CXXRecordDecl::method_iterator it = record_->method_begin();
       it != record_->method_end();
       ++it) {
    if (Config::IsTraceMethod(*it, &isTraceAfterDispatch)) {
      // TODO: Test that the formal parameter is of type Visitor*.
      if (isTraceAfterDispatch) {
        traceAfterDispatch = *it;
      } else {
        trace = *it;
      }
    }
  }
  if (traceAfterDispatch) {
    trace_method_ = traceAfterDispatch;
    trace_dispatch_method_ = trace;
  } else {
    // TODO: Can we never have a dispatch method called trace without the same
    // class defining a traceAfterDispatch method?
    trace_method_ = trace;
    trace_dispatch_method_ = 0;
  }
}

// A class needs tracing if:
// - it is allocated on the managed heap,
// - it defines a trace method (of the proper signature), or
// - it contains fields that need tracing.
TracingStatus RecordInfo::NeedsTracing(Edge::NeedsTracingOption option) {
  if (IsGCAllocated() || GetTraceMethod())
    return TracingStatus::Needed();

  if (option == Edge::kRecursive)
    GetFields();

  return fields_need_tracing_;
}

Edge* RecordInfo::CreateEdge(const Type* type) {
  if (!type) {
    return 0;
  }

  if (type->isPointerType()) {
    if (Edge* ptr = CreateEdge(type->getPointeeType().getTypePtrOrNull()))
      return new RawPtr(ptr);
    return 0;
  }

  CXXRecordDecl* record = type->getAsCXXRecordDecl();

  // If the type is neither a pointer or a C++ record we ignore it.
  if (!record) {
    return 0;
  }

  RecordInfo* info = cache_->Lookup(record);

  TemplateArgs args;

  if (Config::IsRawPtr(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new RawPtr(ptr);
    return 0;
  }

  if (Config::IsRefPtr(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new RefPtr(ptr);
    return 0;
  }

  if (Config::IsOwnPtr(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new OwnPtr(ptr);
    return 0;
  }

  if (Config::IsMember(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new Member(ptr);
    return 0;
  }

  if (Config::IsWeakMember(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new WeakMember(ptr);
    return 0;
  }

  if (Config::IsPersistent(info->name())) {
    // Persistent might refer to v8::Persistent, so check the name space.
    // TODO: Consider using a more canonical identification than names.
    NamespaceDecl* ns =
        dyn_cast<NamespaceDecl>(info->record()->getDeclContext());
    if (!ns || ns->getName() != "WebCore")
      return 0;
    if (!info->GetTemplateArgs(1, &args))
      return 0;
    if (Edge* ptr = CreateEdge(args[0]))
      return new Persistent(ptr);
    return 0;
  }

  if (Config::IsGCCollection(info->name()) ||
      Config::IsWTFCollection(info->name())) {
    bool is_root = Config::IsPersistentGCCollection(info->name());
    bool on_heap = is_root || info->IsHeapAllocatedCollection();
    size_t count = Config::CollectionDimension(info->name());
    if (!info->GetTemplateArgs(count, &args))
      return 0;
    Collection* edge = new Collection(on_heap, is_root);
    for (TemplateArgs::iterator it = args.begin(); it != args.end(); ++it) {
      if (Edge* member = CreateEdge(*it)) {
        edge->members().push_back(member);
      } else {
        // We failed to create an edge so abort the entire edge construction.
        delete edge;  // Will delete the already allocated members.
        return 0;
      }
    }
    return edge;
  }

  return new Value(info);
}
