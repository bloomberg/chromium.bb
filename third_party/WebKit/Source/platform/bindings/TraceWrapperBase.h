// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceWrapperBase_h
#define TraceWrapperBase_h

#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(TraceWrapperBase);

 public:
  TraceWrapperBase() = default;
  ~TraceWrapperBase() = default;
  virtual bool IsScriptWrappable() const { return false; }

  virtual void TraceWrappers(const ScriptWrappableVisitor* visitor) const {}
};

}  // namespace blink

#endif  // TraceWrapperBase_h
