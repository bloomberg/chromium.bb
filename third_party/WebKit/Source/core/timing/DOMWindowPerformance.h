// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowPerformance_h
#define DOMWindowPerformance_h

#include "core/CoreExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class LocalDOMWindow;
class Performance;

class CORE_EXPORT DOMWindowPerformance final
    : public GarbageCollected<DOMWindowPerformance>,
      public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(DOMWindowPerformance);
  WTF_MAKE_NONCOPYABLE(DOMWindowPerformance);

 public:
  static DOMWindowPerformance& From(LocalDOMWindow&);
  static Performance* performance(LocalDOMWindow&);

  void Trace(blink::Visitor*);

 private:
  explicit DOMWindowPerformance(LocalDOMWindow&);
  static const char* SupplementName();

  Performance* performance();

  Member<Performance> performance_;
};

}  // namespace blink

#endif  // DOMWindowPerformance_h
