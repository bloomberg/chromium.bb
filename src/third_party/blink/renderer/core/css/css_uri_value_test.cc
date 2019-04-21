// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_uri_value.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace {

using namespace cssvalue;

TEST(CSSURIValueTest, ValueWithURLMadeAbsolute) {
  CSSURIValue* rel = CSSURIValue::Create("a", KURL("http://foo.com/a"));
  CSSURIValue* abs = rel->ValueWithURLMadeAbsolute(KURL("http://bar.com"),
                                                   WTF::TextEncoding());
  EXPECT_EQ("url(\"http://bar.com/a\")", abs->CssText());
}

TEST(CSSURIValueTest, AlreadyAbsoluteURLMadeAbsolute) {
  CSSURIValue* rel =
      CSSURIValue::Create("http://baz.com/a", KURL("http://baz.com/a"));
  CSSURIValue* abs = rel->ValueWithURLMadeAbsolute(KURL("http://bar.com"),
                                                   WTF::TextEncoding());
  EXPECT_EQ("url(\"http://baz.com/a\")", abs->CssText());
}

}  // namespace
}  // namespace blink
