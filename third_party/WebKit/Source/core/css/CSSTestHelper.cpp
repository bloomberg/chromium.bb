/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSTestHelper.h"

#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "platform/wtf/text/TextEncoding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

CSSTestHelper::~CSSTestHelper() {}

CSSTestHelper::CSSTestHelper() {
  document_ = Document::CreateForTest();
  TextPosition position;
  style_sheet_ = CSSStyleSheet::CreateInline(*document_, NullURL(), position,
                                             UTF8Encoding());
}

CSSRuleList* CSSTestHelper::CssRules() {
  return style_sheet_->cssRules();
}

RuleSet& CSSTestHelper::GetRuleSet() {
  RuleSet& rule_set = style_sheet_->Contents()->EnsureRuleSet(
      MediaQueryEvaluator(), kRuleHasNoSpecialState);
  rule_set.CompactRulesIfNeeded();
  return rule_set;
}

void CSSTestHelper::AddCSSRules(const char* css_text) {
  TextPosition position;
  unsigned sheet_length = style_sheet_->length();
  style_sheet_->Contents()->ParseStringAtPosition(css_text, position);
  ASSERT_TRUE(style_sheet_->length() > sheet_length);
}

}  // namespace blink
