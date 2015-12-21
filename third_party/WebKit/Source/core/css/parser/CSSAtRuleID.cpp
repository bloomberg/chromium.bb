// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSAtRuleID.h"

#include "core/css/parser/CSSParserString.h"
#include "core/frame/UseCounter.h"

namespace blink {

CSSAtRuleID cssAtRuleID(const CSSParserString& name)
{
    if (name.equalIgnoringCase("charset"))
        return CSSAtRuleCharset;
    if (name.equalIgnoringCase("font-face"))
        return CSSAtRuleFontFace;
    if (name.equalIgnoringCase("import"))
        return CSSAtRuleImport;
    if (name.equalIgnoringCase("keyframes"))
        return CSSAtRuleKeyframes;
    if (name.equalIgnoringCase("media"))
        return CSSAtRuleMedia;
    if (name.equalIgnoringCase("namespace"))
        return CSSAtRuleNamespace;
    if (name.equalIgnoringCase("page"))
        return CSSAtRulePage;
    if (name.equalIgnoringCase("supports"))
        return CSSAtRuleSupports;
    if (name.equalIgnoringCase("viewport"))
        return CSSAtRuleViewport;
    if (name.equalIgnoringCase("-webkit-keyframes"))
        return CSSAtRuleWebkitKeyframes;
    return CSSAtRuleInvalid;
}

void countAtRule(UseCounter* useCounter, CSSAtRuleID ruleId)
{
    ASSERT(useCounter);
    UseCounter::Feature feature;

    switch (ruleId) {
    case CSSAtRuleCharset:
        feature = UseCounter::CSSAtRuleCharset;
        break;
    case CSSAtRuleFontFace:
        feature = UseCounter::CSSAtRuleFontFace;
        break;
    case CSSAtRuleImport:
        feature = UseCounter::CSSAtRuleImport;
        break;
    case CSSAtRuleKeyframes:
        feature = UseCounter::CSSAtRuleKeyframes;
        break;
    case CSSAtRuleMedia:
        feature = UseCounter::CSSAtRuleMedia;
        break;
    case CSSAtRuleNamespace:
        feature = UseCounter::CSSAtRuleNamespace;
        break;
    case CSSAtRulePage:
        feature = UseCounter::CSSAtRulePage;
        break;
    case CSSAtRuleSupports:
        feature = UseCounter::CSSAtRuleSupports;
        break;
    case CSSAtRuleViewport:
        feature = UseCounter::CSSAtRuleViewport;
        break;

    case CSSAtRuleWebkitKeyframes:
        feature = UseCounter::CSSAtRuleWebkitKeyframes;
        break;

    case CSSAtRuleInvalid:
        // fallthrough
    default:
        ASSERT_NOT_REACHED();
        return;
    }
    useCounter->count(feature);
}

} // namespace blink

