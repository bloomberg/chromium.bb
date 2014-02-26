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
      requires_trace_method_(false),
      bases_(0),
      fields_(0),
      determined_trace_methods_(false),
      trace_method_(0),
      trace_dispatch_method_(0) {}

RecordInfo::~RecordInfo() {
  delete fields_;
  delete bases_;
}

bool RecordInfo::IsTemplate(TemplateArgs* output_args) {
  ClassTemplateSpecializationDecl* tmpl =
      dyn_cast<ClassTemplateSpecializationDecl>(record_);
  if (!tmpl)
    return false;
  if (!output_args)
    return true;
  const TemplateArgumentList& args = tmpl->getTemplateInstantiationArgs();
  for (unsigned i = 0; i < args.size(); ++i) {
    TemplateArgument arg = args[i];
    if (arg.getKind() == TemplateArgument::Type) {
      if (CXXRecordDecl* decl = arg.getAsType()->getAsCXXRecordDecl())
        output_args->push_back(cache_->Lookup(decl));
    }
  }
  return true;
}

// Test if a record is a HeapAllocated collection.
bool RecordInfo::IsHeapAllocatedCollection(bool* is_weak) {
  if (is_weak)
    *is_weak = false;

  if (!Config::IsGCCollection(name_) && !Config::IsWTFCollection(name_))
    return false;

  TemplateArgs args;
  if (IsTemplate(&args)) {
    for (TemplateArgs::iterator it = args.begin(); it != args.end(); ++it) {
      const string& arg = (*it)->name();

      // The allocator is always after members.
      if (arg == kHeapAllocatorName)
        return true;

      // Check for weak members.
      if (is_weak && Config::IsWeakMember(arg))
        *is_weak = true;
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
bool RecordInfo::IsGCDerived(CXXBasePaths* paths) {
  if (!record_->hasDefinition())
    return false;

  // The base classes are not themselves considered garbage collected objects.
  if (Config::IsGCBase(name_))
    return false;

  // Walk the inheritance tree to find GC base classes.
  CXXBasePaths localPaths(false, false, false);
  if (!paths)
    paths = &localPaths;
  return record_->lookupInBases(IsGCBaseCallback, 0, *paths);
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
  return requires_trace_method_;
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

RecordInfo::Bases* RecordInfo::CollectBases() {
  // Compute the collection locally to avoid inconsistent states.
  Bases* bases = new Bases;
  for (CXXRecordDecl::base_class_iterator it = record_->bases_begin();
       it != record_->bases_end();
       ++it) {
    if (CXXRecordDecl* base = it->getType()->getAsCXXRecordDecl()) {
      // Only collect bases that might need to be traced.
      TracingStatus status = cache_->Lookup(base)->NeedsTracing();
      if (!status.IsTracingUnneeded())
        bases->insert(std::make_pair(base, status));
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
  for (RecordDecl::field_iterator it = record_->field_begin();
       it != record_->field_end();
       ++it) {
    // Ignore fields annotated with the NO_TRACE_CHECKING macro.
    if (IsAnnotated(*it, "blink_no_trace_checking"))
      continue;
    // Only collect fields that might need to be traced.
    TracingStatus status = NeedsTracing(*it);
    if (!status.IsTracingUnneeded()) {
      fields->insert(std::make_pair(*it, status));
      if (status.IsTracingRequired())
        requires_trace_method_ = true;
    }
  }
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
      if (isTraceAfterDispatch)
        traceAfterDispatch = *it;
      else
        trace = *it;
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

// A type needs tracing if it is either a subclass of
// a garbage collected base or it contains fields that need tracing.
// A collection type needs tracing if it is HeapAllocated or contains elements
// that need tracing.
// As a special-case, member handles always need tracing.
TracingStatus RecordInfo::NeedsTracing(NeedsTracingOption option) {
  if (Config::IsRefPtr(name_) || Config::IsPersistentHandle(name_)) {
    return TracingStatus::Unneeded();
  }

  bool is_weak = false;
  if (Config::IsMemberHandle(name_) || IsGCDerived() ||
      IsHeapAllocatedCollection(&is_weak)) {
    return (is_weak || Config::IsWeakMember(name_))
               ? TracingStatus::RequiredWeak()
               : TracingStatus::Required();
  }

  if (option == kNonRecursive) {
    return TracingStatus::Unknown();
  }

  if (Config::IsOwnPtr(name_)) {
    TemplateArgs args;
    return (IsTemplate(&args) && args.size() > 0)
               ? args[0]->NeedsTracing(kNonRecursive)
               : TracingStatus::Unknown();
  }

  if (Config::IsWTFCollection(name_)) {
    TemplateArgs args;
    if (IsTemplate(&args)) {
      for (TemplateArgs::iterator it = args.begin(); it != args.end(); ++it) {
        TracingStatus status = (*it)->NeedsTracing(kNonRecursive);
        if (status.IsTracingRequired())
          return status;
      }
    }
    return TracingStatus::Unknown();
  }

  if (RequiresTraceMethod()) {
    return TracingStatus::Required();
  }

  return fields_->empty() ? TracingStatus::Unneeded()
                          : TracingStatus::Unknown();
}

TracingStatus RecordInfo::NeedsTracing(FieldDecl* field) {
  const QualType type = field->getType();

  if (type->isPointerType()) {
    RecordInfo* info = cache_->Lookup(type->getPointeeCXXRecordDecl());
    if (!info)
      return TracingStatus::Unneeded();

    // TODO: Once transitioning is over, reintroduce checks for pointers to
    // GC-managed objects.

    // Don't do a recursive check for pointer types.
    return TracingStatus::Unknown();
  }

  RecordInfo* info = cache_->Lookup(type->getAsCXXRecordDecl());
  return info ? info->NeedsTracing() : TracingStatus::Unneeded();
}
