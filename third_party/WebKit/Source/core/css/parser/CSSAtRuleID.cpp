// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSAtRuleID.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/frame/UseCounter.h"

namespace blink {

CSSAtRuleID CssAtRuleID(StringView name) {
  if (EqualIgnoringASCIICase(name, "charset"))
    return kCSSAtRuleCharset;
  if (EqualIgnoringASCIICase(name, "font-face"))
    return kCSSAtRuleFontFace;
  if (EqualIgnoringASCIICase(name, "import"))
    return kCSSAtRuleImport;
  if (EqualIgnoringASCIICase(name, "keyframes"))
    return kCSSAtRuleKeyframes;
  if (EqualIgnoringASCIICase(name, "media"))
    return kCSSAtRuleMedia;
  if (EqualIgnoringASCIICase(name, "namespace"))
    return kCSSAtRuleNamespace;
  if (EqualIgnoringASCIICase(name, "page"))
    return kCSSAtRulePage;
  if (EqualIgnoringASCIICase(name, "supports"))
    return kCSSAtRuleSupports;
  if (EqualIgnoringASCIICase(name, "viewport"))
    return kCSSAtRuleViewport;
  if (EqualIgnoringASCIICase(name, "-webkit-keyframes"))
    return kCSSAtRuleWebkitKeyframes;
  if (EqualIgnoringASCIICase(name, "apply"))
    return kCSSAtRuleApply;
  return kCSSAtRuleInvalid;
}

void CountAtRule(const CSSParserContext* context, CSSAtRuleID rule_id) {
  UseCounter::Feature feature;

  switch (rule_id) {
    case kCSSAtRuleCharset:
      feature = UseCounter::kCSSAtRuleCharset;
      break;
    case kCSSAtRuleFontFace:
      feature = UseCounter::kCSSAtRuleFontFace;
      break;
    case kCSSAtRuleImport:
      feature = UseCounter::kCSSAtRuleImport;
      break;
    case kCSSAtRuleKeyframes:
      feature = UseCounter::kCSSAtRuleKeyframes;
      break;
    case kCSSAtRuleMedia:
      feature = UseCounter::kCSSAtRuleMedia;
      break;
    case kCSSAtRuleNamespace:
      feature = UseCounter::kCSSAtRuleNamespace;
      break;
    case kCSSAtRulePage:
      feature = UseCounter::kCSSAtRulePage;
      break;
    case kCSSAtRuleSupports:
      feature = UseCounter::kCSSAtRuleSupports;
      break;
    case kCSSAtRuleViewport:
      feature = UseCounter::kCSSAtRuleViewport;
      break;

    case kCSSAtRuleWebkitKeyframes:
      feature = UseCounter::kCSSAtRuleWebkitKeyframes;
      break;

    case kCSSAtRuleApply:
      feature = UseCounter::kCSSAtRuleApply;
      break;

    case kCSSAtRuleInvalid:
    // fallthrough
    default:
      NOTREACHED();
      return;
  }
  context->Count(feature);
}

}  // namespace blink
