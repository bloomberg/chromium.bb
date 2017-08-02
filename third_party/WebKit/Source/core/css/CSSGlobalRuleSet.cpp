// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSGlobalRuleSet.h"

#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/RuleSet.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"

namespace blink {

void CSSGlobalRuleSet::InitWatchedSelectorsRuleSet(Document& document) {
  MarkDirty();
  watched_selectors_rule_set_ = nullptr;
  CSSSelectorWatch* watch = CSSSelectorWatch::FromIfExists(document);
  if (!watch)
    return;
  const HeapVector<Member<StyleRule>>& watched_selectors =
      watch->WatchedCallbackSelectors();
  if (!watched_selectors.size())
    return;
  watched_selectors_rule_set_ = RuleSet::Create();
  for (unsigned i = 0; i < watched_selectors.size(); ++i) {
    watched_selectors_rule_set_->AddStyleRule(watched_selectors[i],
                                              kRuleHasNoSpecialState);
  }
}

void CSSGlobalRuleSet::Update(Document& document) {
  if (!is_dirty_)
    return;

  is_dirty_ = false;
  features_.Clear();
  has_fullscreen_ua_style_ = false;

  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  if (default_style_sheets.DefaultStyle()) {
    features_.Add(default_style_sheets.DefaultStyle()->Features());
    has_fullscreen_ua_style_ = default_style_sheets.FullscreenStyleSheet();
  }

  if (document.IsViewSource())
    features_.Add(default_style_sheets.DefaultViewSourceStyle()->Features());

  if (watched_selectors_rule_set_)
    features_.Add(watched_selectors_rule_set_->Features());

  document.GetStyleEngine().CollectScopedStyleFeaturesTo(features_);
}

void CSSGlobalRuleSet::Dispose() {
  features_.Clear();
  watched_selectors_rule_set_ = nullptr;
  has_fullscreen_ua_style_ = false;
  is_dirty_ = true;
}

DEFINE_TRACE(CSSGlobalRuleSet) {
  visitor->Trace(watched_selectors_rule_set_);
}

}  // namespace blink
