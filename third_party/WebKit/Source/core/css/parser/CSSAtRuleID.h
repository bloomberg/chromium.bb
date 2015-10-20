// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAtRuleID_h
#define CSSAtRuleID_h

namespace blink {

struct CSSParserString;
class UseCounter;

enum CSSAtRuleID {
    CSSAtRuleInvalid = 0,

    CSSAtRuleCharset = 1,
    CSSAtRuleFontFace = 2,
    CSSAtRuleImport = 3,
    CSSAtRuleKeyframes = 4,
    CSSAtRuleMedia = 5,
    CSSAtRuleNamespace = 6,
    CSSAtRulePage = 7,
    CSSAtRuleSupports = 8,
    CSSAtRuleViewport = 9,

    CSSAtRuleWebkitKeyframes = 10,
};

CSSAtRuleID cssAtRuleID(const CSSParserString&);

void countAtRule(UseCounter*, CSSAtRuleID);

} // namespace blink

#endif // CSSAtRuleID_h
