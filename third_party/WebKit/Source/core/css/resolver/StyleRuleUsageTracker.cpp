// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/StyleRuleUsageTracker.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleRule.h"

namespace blink {

StyleRuleUsageTracker::RuleListByStyleSheet StyleRuleUsageTracker::takeDelta() {
  RuleListByStyleSheet result;
  result.swap(m_usedRulesDelta);
  return result;
}

void StyleRuleUsageTracker::track(const CSSStyleSheet* parentSheet,
                                  const StyleRule* rule) {
  if (!parentSheet)
    return;
  if (!m_usedRules.insert(std::make_pair(parentSheet, rule)).isNewEntry)
    return;
  auto it = m_usedRulesDelta.find(parentSheet);
  if (it != m_usedRulesDelta.end()) {
    it->value.push_back(rule);
  } else {
    m_usedRulesDelta.insert(parentSheet, HeapVector<Member<const StyleRule>>())
        .storedValue->value.push_back(rule);
  }
}

DEFINE_TRACE(StyleRuleUsageTracker) {
  visitor->trace(m_usedRules);
  visitor->trace(m_usedRulesDelta);
}

}  // namespace blink
