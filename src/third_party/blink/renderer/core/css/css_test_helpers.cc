// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_test_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/property_descriptor.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace css_test_helpers {

TestStyleSheet::~TestStyleSheet() = default;

TestStyleSheet::TestStyleSheet() {
  document_ = Document::CreateForTest();
  TextPosition position;
  style_sheet_ = CSSStyleSheet::CreateInline(*document_, NullURL(), position,
                                             UTF8Encoding());
}

CSSRuleList* TestStyleSheet::CssRules() {
  DummyExceptionStateForTesting exception_state;
  CSSRuleList* result = style_sheet_->cssRules(exception_state);
  EXPECT_FALSE(exception_state.HadException());
  return result;
}

RuleSet& TestStyleSheet::GetRuleSet() {
  RuleSet& rule_set = style_sheet_->Contents()->EnsureRuleSet(
      MediaQueryEvaluator(), kRuleHasNoSpecialState);
  rule_set.CompactRulesIfNeeded();
  return rule_set;
}

void TestStyleSheet::AddCSSRules(const char* css_text, bool is_empty_sheet) {
  TextPosition position;
  unsigned sheet_length = style_sheet_->length();
  style_sheet_->Contents()->ParseStringAtPosition(css_text, position);
  if (!is_empty_sheet)
    ASSERT_GT(style_sheet_->length(), sheet_length);
  else
    ASSERT_EQ(style_sheet_->length(), sheet_length);
}

void RegisterProperty(Document& document,
                      const String& name,
                      const String& syntax,
                      const String& initial_value,
                      bool is_inherited) {
  DummyExceptionStateForTesting exception_state;
  PropertyDescriptor* property_descriptor = PropertyDescriptor::Create();
  property_descriptor->setName(name);
  property_descriptor->setSyntax(syntax);
  property_descriptor->setInitialValue(initial_value);
  property_descriptor->setInherits(is_inherited);
  PropertyRegistration::registerProperty(&document, property_descriptor,
                                         exception_state);
  ASSERT_FALSE(exception_state.HadException());
}

}  // namespace css_test_helpers
}  // namespace blink
