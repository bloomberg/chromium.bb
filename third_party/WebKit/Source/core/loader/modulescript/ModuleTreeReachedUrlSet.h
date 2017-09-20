// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleTreeReachedUrlSet_h
#define ModuleTreeReachedUrlSet_h

#include "core/CoreExport.h"
#include "core/dom/AncestorList.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "platform/wtf/HashSet.h"

namespace blink {

// ModuleTreeReachedUrlSet aims to reduce number of ModuleTreeLinker
// involved in loading a module graph.
//
// ModuleTreeReachedUrlSet is created per top-level ModuleTreeLinker
// invocations. The instance is shared among all descendants fetch
// ModuleTreeLinker to track a set of module sub-graph root URLs
// which we have started ModuleTreeLinker on.
//
// We consult this ModuleTreeReachedUrlSet before creating
// ModuleTreeLinker for a descendant module sub-graph.
// If the ModuleTreeReachedUrlSet indicates that the sub-graph
// fetch is already taken care of by a existing ModuleTreeLinker,
// a parent ModuleTreeLinker will defer to it and not instantiate a new
// ModuleTreeLinker.
class CORE_EXPORT ModuleTreeReachedUrlSet
    : public GarbageCollectedFinalized<ModuleTreeReachedUrlSet> {
 public:
  static ModuleTreeReachedUrlSet* CreateFromTopLevelAncestorList(
      const AncestorList& list) {
    ModuleTreeReachedUrlSet* set = new ModuleTreeReachedUrlSet;
    CHECK_LE(list.size(), 1u);
    set->set_ = list;
    return set;
  }

  void ObserveModuleTreeLink(const KURL& url) {
    auto result = set_.insert(url);
    CHECK(result.is_new_entry);
  }

  bool IsAlreadyBeingFetched(const KURL& url) const {
    return set_.Contains(url);
  }

  DEFINE_INLINE_TRACE() {}

 private:
  HashSet<KURL> set_;
};

}  // namespace blink

#endif  // ModuleTreeReachedUrlSet_h
