// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/events/EventPath.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/PseudoElement.h"
#include "core/html_names.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class EventPathTest : public PageTestBase {};

TEST_F(EventPathTest, ShouldBeEmptyForPseudoElementWithoutParentElement) {
  Element* div =
      GetDocument().createElement(HTMLNames::divTag, kCreatedByCreateElement);
  PseudoElement* pseudo = PseudoElement::Create(div, kPseudoIdFirstLetter);
  pseudo->Dispose();
  EventPath event_path(*pseudo);
  EXPECT_TRUE(event_path.IsEmpty());
}

}  // namespace blink
