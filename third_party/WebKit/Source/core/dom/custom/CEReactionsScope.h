// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEReactionsScope_h
#define CEReactionsScope_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

class CustomElementReaction;
class Element;

// https://html.spec.whatwg.org/multipage/scripting.html#cereactions
class CORE_EXPORT CEReactionsScope final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(CEReactionsScope);

 public:
  static CEReactionsScope* Current() { return top_of_stack_; }

  CEReactionsScope() : prev_(top_of_stack_), work_to_do_(false) {
    top_of_stack_ = this;
  }

  ~CEReactionsScope() {
    if (work_to_do_)
      InvokeReactions();
    top_of_stack_ = top_of_stack_->prev_;
  }

  void EnqueueToCurrentQueue(Element*, CustomElementReaction*);

 private:
  static CEReactionsScope* top_of_stack_;

  void InvokeReactions();

  CEReactionsScope* prev_;
  bool work_to_do_;
};

}  // namespace blink

#endif  // CEReactionsScope_h
