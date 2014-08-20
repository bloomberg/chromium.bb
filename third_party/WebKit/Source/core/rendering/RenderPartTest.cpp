// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderPart.h"

#include "core/html/HTMLElement.h"
#include "core/rendering/ImageQualityController.h"
#include "core/rendering/RenderingTestHelper.h"
#include <gtest/gtest.h>

namespace blink {

class RenderPartTest : public RenderingTest {
};

TEST_F(RenderPartTest, DestroyUpdatesImageQualityController)
{
    RefPtrWillBeRawPtr<Element> element = HTMLElement::create(HTMLNames::divTag, document());
    RenderObject* part = new RenderPart(element.get());
    // The third and forth arguments are not important in this test.
    ImageQualityController::imageQualityController()->set(part, 0, this, LayoutSize(1, 1));
    EXPECT_TRUE(ImageQualityController::has(part));
    part->destroy();
    EXPECT_FALSE(ImageQualityController::has(part));
}

}
