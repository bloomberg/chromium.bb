// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowPerformance_h
#define DOMWindowPerformance_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalDOMWindow;
class WindowPerformance;

class CORE_EXPORT DOMWindowPerformance final
    : public GarbageCollected<DOMWindowPerformance>,
      public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(DOMWindowPerformance);

 public:
  static const char kSupplementName[];

  static DOMWindowPerformance& From(LocalDOMWindow&);
  static WindowPerformance* performance(LocalDOMWindow&);

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

 private:
  explicit DOMWindowPerformance(LocalDOMWindow&);

  WindowPerformance* performance();

  TraceWrapperMember<WindowPerformance> performance_;
  DISALLOW_COPY_AND_ASSIGN(DOMWindowPerformance);
};

}  // namespace blink

#endif  // DOMWindowPerformance_h
