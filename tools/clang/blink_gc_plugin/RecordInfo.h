// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a wrapper for CXXRecordDecl that accumulates GC related
// information about a class. Accumulated information is memoized and the info
// objects are stored in a RecordCache.

#ifndef TOOLS_BLINK_GC_PLUGIN_RECORD_INFO_H_
#define TOOLS_BLINK_GC_PLUGIN_RECORD_INFO_H_

#include <map>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/CXXInheritance.h"

class RecordCache;

// Value to track the tracing status of a point in the tracing graph.
// (Points that might need tracing are fields and base classes.)
class TracingStatus {
 public:
  static TracingStatus Unknown() { return kUnknown; }
  static TracingStatus Required() { return kRequired; }
  static TracingStatus Unneeded() { return kUnneeded; }
  static TracingStatus RequiredWeak() {
      return TracingStatus(kRequired, true);
  }

  bool IsTracingUnknown() const { return status_ == kUnknown; }
  bool IsTracingRequired() const { return status_ == kRequired; }
  bool IsTracingUnneeded() const { return status_ == kUnneeded; }

  // Updating functions so the status can only become more defined.
  void MarkTracingRequired() {
    if (status_ < kRequired) status_ = kRequired;
  }
  void MarkTracingUnneeded() {
    if (status_ < kUnneeded) status_ = kUnneeded;
  }

  bool is_weak() { return is_weak_; }

 private:
  enum Status {
    kUnknown,    // Point that might need to be traced.
    kRequired,   // Point that must be traced.
    kUnneeded    // Point that need not be traced.
  };

  TracingStatus(Status status)
      : status_(status),
        is_weak_(false) { }

  TracingStatus(Status status, bool is_weak)
      : status_(status),
        is_weak_(is_weak) { }

  Status status_;
  bool is_weak_;
};

// Wrapper class to lazily collect information about a C++ record.
class RecordInfo {
public:
  typedef std::map<clang::CXXRecordDecl*, TracingStatus> Bases;
  typedef std::map<clang::FieldDecl*, TracingStatus> Fields;
  typedef std::vector<RecordInfo*> TemplateArgs;

  enum NeedsTracingOption {
    kRecursive,
    kNonRecursive
  };

  ~RecordInfo();

  clang::CXXRecordDecl* record() const { return record_; }
  const std::string& name() const { return name_; }
  Fields& GetFields();
  Bases& GetBases();
  clang::CXXMethodDecl* GetTraceMethod();
  clang::CXXMethodDecl* GetTraceDispatchMethod();

  bool IsTemplate(TemplateArgs* args = 0);

  bool IsHeapAllocatedCollection(bool* is_weak = 0);
  bool IsGCDerived(clang::CXXBasePaths* paths = 0);

  bool IsNonNewable();
  bool RequiresTraceMethod();
  TracingStatus NeedsTracing(NeedsTracingOption option = kRecursive);

private:
  RecordInfo(clang::CXXRecordDecl* record, RecordCache* cache);

  Fields* CollectFields();
  Bases* CollectBases();
  void DetermineTracingMethods();

  TracingStatus NeedsTracing(clang::FieldDecl* field);

  RecordCache* cache_;
  clang::CXXRecordDecl* record_;
  const std::string name_;
  bool requires_trace_method_;
  Bases* bases_;
  Fields* fields_;

  bool determined_trace_methods_;
  clang::CXXMethodDecl* trace_method_;
  clang::CXXMethodDecl* trace_dispatch_method_;

  friend class RecordCache;
};

class RecordCache {
public:
  RecordInfo* Lookup(clang::CXXRecordDecl* record) {
    if (!record) return 0;
    Cache::iterator it = cache_.find(record);
    if (it != cache_.end())
      return &it->second;
    return &cache_.insert(std::make_pair(record, RecordInfo(record, this)))
            .first->second;
  }

  RecordInfo* Lookup(const clang::CXXRecordDecl* record) {
    return Lookup(const_cast<clang::CXXRecordDecl*>(record));
  }

private:
  typedef std::map<clang::CXXRecordDecl*, RecordInfo> Cache;
  Cache cache_;
};

#endif // TOOLS_BLINK_GC_PLUGIN_RECORD_INFO_H_
