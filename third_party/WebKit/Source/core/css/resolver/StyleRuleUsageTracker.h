// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleRuleUsageTracker_h
#define StyleRuleUsageTracker_h

#include "core/css/CSSStyleRule.h"

namespace blink {

class StyleRule;

class StyleRuleUsageTracker : public GarbageCollected<StyleRuleUsageTracker> {
 public:
  using RuleListByStyleSheet = HeapHashMap<Member<const CSSStyleSheet>,
                                           HeapVector<Member<const StyleRule>>>;

  void Track(const CSSStyleSheet*, const StyleRule*);
  RuleListByStyleSheet TakeDelta();

  void Trace(blink::Visitor*);

 private:
  HeapHashSet<std::pair<Member<const CSSStyleSheet>, Member<const StyleRule>>>
      used_rules_;
  RuleListByStyleSheet used_rules_delta_;
};

}  // namespace blink

#endif  // StyleRuleUsageTracker_h
