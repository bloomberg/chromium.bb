// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ContextSnapshotExternalReferences_h
#define V8ContextSnapshotExternalReferences_h

#include <cstdint>

#include "modules/ModulesExport.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// V8ContextSnapshotExternalReferences::GetTable() provides a table of pointers
// of C++ callbacks exposed to V8. The table contains C++ callbacks for DOM
// attribute getters, setters, DOM methods, wrapper type info etc.
class MODULES_EXPORT V8ContextSnapshotExternalReferences {
  STATIC_ONLY(V8ContextSnapshotExternalReferences);

 public:
  // The definition of this method is auto-generated in
  // V8ContextSnapshotExternalReferences.cpp.
  static intptr_t* GetTable();
};

}  // namespace blink

#endif  // V8ContextSnapshotExternalReferences_h
