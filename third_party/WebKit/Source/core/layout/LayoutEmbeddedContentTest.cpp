// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutEmbeddedContent.h"

#include "core/html/HTMLElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutEmbeddedContentTest : public RenderingTest {};

class OverriddenLayoutEmbeddedContent : public LayoutEmbeddedContent {
 public:
  explicit OverriddenLayoutEmbeddedContent(Element* element)
      : LayoutEmbeddedContent(element) {}

  const char* GetName() const override {
    return "OverriddenLayoutEmbeddedContent";
  }
};

}  // namespace blink
