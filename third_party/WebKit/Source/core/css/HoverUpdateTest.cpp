// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "HTMLNames.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/page/EventHandler.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/PlatformMouseEvent.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(HoverUpdateTest, AffectedByHoverUpdate)
{
    // Check that when hovering the div in the document below, you only get a
    // single element style recalc.

    OwnPtr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    HTMLDocument* document = toHTMLDocument(&dummyPageHolder->document());
    document->documentElement()->setInnerHTML("<style>div {width:100px;height:100px} div:hover { background-color: green }</style>"
        "<div>"
        "<span></span>"
        "<span></span>"
        "<span></span>"
        "<span></span>"
        "</div>", ASSERT_NO_EXCEPTION);

    document->view()->updateLayoutAndStyleIfNeededRecursive();
    unsigned startCount = document->styleEngine()->resolverAccessCount();

    PlatformMouseEvent moveEvent(IntPoint(20, 20), IntPoint(20, 20), NoButton, PlatformEvent::MouseMoved, 0, false, false, false, false, currentTime());
    document->frame()->eventHandler().handleMouseMoveEvent(moveEvent);
    document->view()->updateLayoutAndStyleIfNeededRecursive();

    unsigned accessCount = document->styleEngine()->resolverAccessCount() - startCount;

    ASSERT_EQ(1U, accessCount);
}

} // namespace
