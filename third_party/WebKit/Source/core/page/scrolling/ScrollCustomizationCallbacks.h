// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollCustomizationCallbacks_h
#define ScrollCustomizationCallbacks_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class ScrollStateCallback;

class CORE_EXPORT ScrollCustomizationCallbacks
    : public GarbageCollected<ScrollCustomizationCallbacks> {
  WTF_MAKE_NONCOPYABLE(ScrollCustomizationCallbacks);

 public:
  ScrollCustomizationCallbacks() {}
  void SetDistributeScroll(Element*, ScrollStateCallback*);
  ScrollStateCallback* GetDistributeScroll(Element*);
  void SetApplyScroll(Element*, ScrollStateCallback*);
  void RemoveApplyScroll(Element*);
  ScrollStateCallback* GetApplyScroll(Element*);

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(apply_scroll_callbacks_);
    visitor->Trace(distribute_scroll_callbacks_);
  };

 private:
  using ScrollStateCallbackList =
      HeapHashMap<WeakMember<Element>, Member<ScrollStateCallback>>;
  ScrollStateCallbackList apply_scroll_callbacks_;
  ScrollStateCallbackList distribute_scroll_callbacks_;
};

}  // namespace blink

#endif  // ScrollCustomizationCallbacks_h
