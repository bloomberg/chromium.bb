// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSGlobalRuleSet_h
#define CSSGlobalRuleSet_h

#include "core/css/RuleFeature.h"

namespace blink {

class Document;
class RuleSet;

// A per Document collection of CSS metadata used for style matching and
// invalidation. The data is aggregated from author rulesets from all TreeScopes
// in the whole Document as well as UA stylesheets and watched selectors which
// apply to elements in all TreeScopes.
//
// TODO(rune@opera.com): We would like to move as much of this data as possible
// to the ScopedStyleResolver as possible to avoid full reconstruction of these
// rulesets on shadow tree changes. See https://crbug.com/401359

class CSSGlobalRuleSet : public GarbageCollectedFinalized<CSSGlobalRuleSet> {
  WTF_MAKE_NONCOPYABLE(CSSGlobalRuleSet);

 public:
  static CSSGlobalRuleSet* Create() { return new CSSGlobalRuleSet(); }

  void Dispose();
  void InitWatchedSelectorsRuleSet(Document&);
  void MarkDirty() { is_dirty_ = true; }
  bool IsDirty() const { return is_dirty_; }
  void Update(Document&);

  const RuleFeatureSet& GetRuleFeatureSet() const {
    CHECK(features_.IsAlive());
    return features_;
  }
  RuleSet* SiblingRuleSet() const { return sibling_rule_set_; }
  RuleSet* UncommonAttributeRuleSet() const {
    return uncommon_attribute_rule_set_;
  }
  RuleSet* WatchedSelectorsRuleSet() const {
    return watched_selectors_rule_set_;
  }
  bool HasFullscreenUAStyle() const { return has_fullscreen_ua_style_; }

  DECLARE_TRACE();

 private:
  CSSGlobalRuleSet() {}
  // Constructed from rules in all TreeScopes including UA style and style
  // injected from extensions.
  RuleFeatureSet features_;
  Member<RuleSet> sibling_rule_set_;
  Member<RuleSet> uncommon_attribute_rule_set_;

  // Rules injected from extensions.
  Member<RuleSet> watched_selectors_rule_set_;

  bool has_fullscreen_ua_style_ = false;
  bool is_dirty_ = true;
};

}  // namespace blink

#endif  // CSSGlobalRuleSet_h
