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

#include "Edge.h"

#include "clang/AST/AST.h"
#include "clang/AST/CXXInheritance.h"

class RecordCache;

// A potentially tracable and/or lifetime affecting point in the object graph.
class GraphPoint {
 public:
  GraphPoint() : traced_(false) {}
  void MarkTraced() { traced_ = true; }
  bool IsProperlyTraced() { return traced_ || !NeedsTracing().IsNeeded(); }
  virtual const TracingStatus NeedsTracing() = 0;
 private:
  bool traced_;
};

class BasePoint : public GraphPoint {
 public:
  BasePoint(RecordInfo* info, const TracingStatus& status)
      : info_(info), status_(status) {}
  RecordInfo* info() { return info_; }
  const TracingStatus NeedsTracing() { return status_; }
  // Needed to change the status of bases with a pure-virtual trace.
  void MarkUnneeded() { status_ = TracingStatus::Unneeded(); }
 private:
  RecordInfo* info_;
  TracingStatus status_;
};

class FieldPoint : public GraphPoint {
 public:
  FieldPoint(clang::FieldDecl* field, Edge* edge) : field_(field), edge_(edge) {
    assert(edge && "FieldPoint edge must be non-null");
  }
  const TracingStatus NeedsTracing() {
    return edge_->NeedsTracing(Edge::kRecursive);
  }
  clang::FieldDecl* field() { return field_; }
  Edge* edge() { return edge_; }
 private:
  clang::FieldDecl* field_;
  Edge* edge_;
  friend class RecordCache;
  void deleteEdge() { delete edge_; }
};

// Wrapper class to lazily collect information about a C++ record.
class RecordInfo {
 public:
  typedef std::map<clang::CXXRecordDecl*, BasePoint> Bases;
  typedef std::map<clang::FieldDecl*, FieldPoint> Fields;
  typedef std::vector<const clang::Type*> TemplateArgs;

  ~RecordInfo();

  clang::CXXRecordDecl* record() const { return record_; }
  const std::string& name() const { return name_; }
  Fields& GetFields();
  Bases& GetBases();
  clang::CXXMethodDecl* GetTraceMethod();
  clang::CXXMethodDecl* GetTraceDispatchMethod();

  bool GetTemplateArgs(size_t count, TemplateArgs* output_args);

  bool IsHeapAllocatedCollection();
  bool IsGCDerived();
  bool IsGCAllocated();
  bool IsGCFinalized();

  bool IsStackAllocated();
  bool RequiresTraceMethod();
  TracingStatus NeedsTracing(Edge::NeedsTracingOption);

 private:
  RecordInfo(clang::CXXRecordDecl* record, RecordCache* cache);

  Fields* CollectFields();
  Bases* CollectBases();
  void DetermineTracingMethods();
  bool InheritsNonPureTrace();

  Edge* CreateEdge(const clang::Type* type);

  RecordCache* cache_;
  clang::CXXRecordDecl* record_;
  const std::string name_;
  TracingStatus fields_need_tracing_;
  Bases* bases_;
  Fields* fields_;

  bool determined_trace_methods_;
  clang::CXXMethodDecl* trace_method_;
  clang::CXXMethodDecl* trace_dispatch_method_;

  bool is_gc_derived_;
  clang::CXXBasePaths* base_paths_;

  friend class RecordCache;
};

class RecordCache {
 public:
  RecordInfo* Lookup(clang::CXXRecordDecl* record) {
    if (!record)
      return 0;
    Cache::iterator it = cache_.find(record);
    if (it != cache_.end())
      return &it->second;
    return &cache_.insert(std::make_pair(record, RecordInfo(record, this)))
                .first->second;
  }

  RecordInfo* Lookup(const clang::CXXRecordDecl* record) {
    return Lookup(const_cast<clang::CXXRecordDecl*>(record));
  }

  RecordInfo* Lookup(clang::Decl* decl) {
    return Lookup(clang::dyn_cast<clang::CXXRecordDecl>(decl));
  }

  ~RecordCache() {
    for (Cache::iterator it = cache_.begin(); it != cache_.end(); ++it) {
      if (!it->second.fields_)
        continue;
      for (RecordInfo::Fields::iterator fit = it->second.fields_->begin();
        fit != it->second.fields_->end();
        ++fit) {
        fit->second.deleteEdge();
      }
    }
  }

 private:
  typedef std::map<clang::CXXRecordDecl*, RecordInfo> Cache;
  Cache cache_;
};

#endif  // TOOLS_BLINK_GC_PLUGIN_RECORD_INFO_H_
