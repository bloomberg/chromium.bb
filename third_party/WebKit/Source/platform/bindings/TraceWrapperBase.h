// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceWrapperBase_h
#define TraceWrapperBase_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class ScriptWrappableVisitor;

class PLATFORM_EXPORT TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(TraceWrapperBase);

 public:
  TraceWrapperBase() = default;
  ~TraceWrapperBase() = default;
  virtual bool IsScriptWrappable() const { return false; }

  virtual void TraceWrappers(const ScriptWrappableVisitor*) const = 0;

  // Human-readable name of this object. The DevTools heap snapshot uses
  // this method to show the object.
  virtual const char* NameInHeapSnapshot() const = 0;
};

}  // namespace blink

#endif  // TraceWrapperBase_h
