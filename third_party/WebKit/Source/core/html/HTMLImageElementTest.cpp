// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLImageElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

const int kViewportWidth = 500;
const int kViewportHeight = 600;
class HTMLImageElementTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp(IntSize(kViewportWidth, kViewportHeight));
  }
};

TEST_F(HTMLImageElementTest, width) {
  HTMLImageElement* image =
      HTMLImageElement::Create(GetDocument(), /* createdByParser */ false);
  image->setAttribute(HTMLNames::widthAttr, "400");
  // TODO(yoav): `width` does not impact resourceWidth until we resolve
  // https://github.com/ResponsiveImagesCG/picture-element/issues/268
  EXPECT_EQ(500, image->GetResourceWidth().width);
  image->setAttribute(HTMLNames::sizesAttr, "100vw");
  EXPECT_EQ(500, image->GetResourceWidth().width);
}

TEST_F(HTMLImageElementTest, sourceSize) {
  HTMLImageElement* image =
      HTMLImageElement::Create(GetDocument(), /* createdByParser */ false);
  image->setAttribute(HTMLNames::widthAttr, "400");
  EXPECT_EQ(kViewportWidth, image->SourceSize(*image));
  image->setAttribute(HTMLNames::sizesAttr, "50vw");
  EXPECT_EQ(250, image->SourceSize(*image));
}

}  // namespace blink
