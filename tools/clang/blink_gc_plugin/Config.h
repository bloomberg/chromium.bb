// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the names used by GC infrastructure.

#ifndef TOOLS_BLINK_GC_PLUGIN_CONFIG_H_
#define TOOLS_BLINK_GC_PLUGIN_CONFIG_H_

#include "clang/AST/AST.h"

const char kNewOperatorName[] = "operator new";
const char kTraceName[] = "trace";
const char kTraceAfterDispatchName[] = "traceAfterDispatch";
const char kRegisterWeakMembersName[] = "registerWeakMembers";
const char kHeapAllocatorName[] = "HeapAllocator";

// TODO: Expand template aliases instead of using StartsWith/EndsWith patterns.

class Config {
 public:

  static bool IsMember(const std::string& name) {
    return name == "Member"
        || (oilpan_enabled() && EndsWith(name, "WillBeMember"));
  }

  static bool IsWeakMember(const std::string& name) {
    return name == "WeakMember"
        || (oilpan_enabled() && EndsWith(name, "WillBeWeakMember"));
  }

  static bool IsMemberHandle(const std::string& name) {
    return IsMember(name)
        || IsWeakMember(name);
  }

  static bool IsPersistentHandle(const std::string& name) {
    return name == "Persistent"
        || (oilpan_enabled() && EndsWith(name, "WillBePersistent"));
  }

  static bool IsRefPtr(const std::string& name) {
    return name == "RefPtr"
        || (!oilpan_enabled() && StartsWith(name, "RefPtrWillBe"));
  }

  static bool IsOwnPtr(const std::string& name) {
    return name == "OwnPtr"
        || (!oilpan_enabled() && StartsWith(name, "OwnPtrWillBe"));
  }

  static bool IsWTFCollection(const std::string& name) {
    return name == "Vector"
        || name == "HashSet"
        || name == "HashMap"
        || name == "HashCountedSet"
        || name == "ListHashSet"
        || name == "Deque"
        || (!oilpan_enabled() && StartsWith(name, "WillBeHeap"));
  }

  static bool IsGCCollection(const std::string& name) {
    return name == "HeapVector"
        || name == "HeapHashMap"
        || name == "HeapHashSet"
        || (oilpan_enabled() && StartsWith(name, "WillBeHeap"));
  }

  static bool IsGCFinalizedBase(const std::string& name) {
    return name == "GarbageCollectedFinalized"
        || name == "RefCountedGarbageCollected"
        || (oilpan_enabled()
            && (EndsWith(name, "WillBeGarbageCollectedFinalized") ||
                EndsWith(name, "WillBeRefCountedGarbageCollected")));
  }

  static bool IsGCBase(const std::string& name) {
    return name == "GarbageCollected"
        || IsGCFinalizedBase(name)
        || (oilpan_enabled() && EndsWith(name, "WillBeGarbageCollected"));
  }

  static bool IsTraceMethod(clang::CXXMethodDecl* method,
                            bool* isTraceAfterDispatch = 0) {
    const std::string& name = method->getNameAsString();
    if (name == kTraceName || name == kTraceAfterDispatchName) {
      if (isTraceAfterDispatch)
        *isTraceAfterDispatch = (name == kTraceAfterDispatchName);
      return true;
    }
    return false;
  }

  static bool StartsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size())
      return false;
    return str.compare(0, prefix.size(), prefix) == 0;
  }

  static bool EndsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size())
      return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  static void set_oilpan_enabled(bool enabled) { oilpan_enabled_ = enabled; }
  static bool oilpan_enabled() { return oilpan_enabled_; }

 private:
  static bool oilpan_enabled_;
};

#endif // TOOLS_BLINK_GC_PLUGIN_CONFIG_H_
