// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectorFilterParentScope_h
#define SelectorFilterParentScope_h

#include "core/css/SelectorFilter.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"

namespace blink {

// Maintains the parent element stack (and bloom filter) inside recalcStyle.
class SelectorFilterParentScope final {
  STACK_ALLOCATED();

 public:
  explicit SelectorFilterParentScope(Element& parent);
  ~SelectorFilterParentScope();

  static void EnsureParentStackIsPushed();

 private:
  void PushParentIfNeeded();

  Member<Element> parent_;
  bool pushed_;
  SelectorFilterParentScope* previous_;
  Member<StyleResolver> resolver_;

  static SelectorFilterParentScope* current_scope_;
};

inline SelectorFilterParentScope::SelectorFilterParentScope(Element& parent)
    : parent_(parent),
      pushed_(false),
      previous_(current_scope_),
      resolver_(parent.GetDocument().GetStyleResolver()) {
  DCHECK(parent.GetDocument().InStyleRecalc());
  current_scope_ = this;
}

inline SelectorFilterParentScope::~SelectorFilterParentScope() {
  current_scope_ = previous_;
  if (!pushed_)
    return;
  resolver_->GetSelectorFilter().PopParent(*parent_);
}

inline void SelectorFilterParentScope::EnsureParentStackIsPushed() {
  if (current_scope_)
    current_scope_->PushParentIfNeeded();
}

inline void SelectorFilterParentScope::PushParentIfNeeded() {
  if (pushed_)
    return;
  if (previous_)
    previous_->PushParentIfNeeded();
  resolver_->GetSelectorFilter().PushParent(*parent_);
  pushed_ = true;
}

}  // namespace blink

#endif  // SelectorFilterParentScope_h
