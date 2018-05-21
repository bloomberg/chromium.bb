// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/jank_tracker.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class JankTrackerTest : public RenderingTest {
 protected:
  LocalFrameView& GetFrameView() { return *GetFrame().View(); }
  JankTracker& GetJankTracker() { return GetFrameView().GetJankTracker(); }
};

TEST_F(JankTrackerTest, SimpleBlockMovement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");

  EXPECT_EQ(0.0, GetJankTracker().Score());

  GetDocument().getElementById("j")->setAttribute(HTMLNames::styleAttr,
                                                  AtomicString("top: 60px"));
  GetFrameView().UpdateAllLifecyclePhases();
  // 300 * (100 + 60) / (default viewport size 800 * 600)
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, SVGIgnored) {
  const char data[] = "<svg></svg>";
  GetFrame().ForceSynchronousDocumentInstall(
      "image/svg+xml", SharedBuffer::Create(data, strlen(data)));
  EXPECT_FALSE(GetJankTracker().IsActive());
}

TEST_F(JankTrackerTest, Transform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #c { transform: translateX(-300px) translateY(-40px); }
      #j { position: relative; width: 600px; height: 140px; }
    </style>
    <div id='c'>
      <div id='j'></div>
    </div>
  )HTML");

  GetDocument().getElementById("j")->setAttribute(HTMLNames::styleAttr,
                                                  AtomicString("top: 60px"));
  GetFrameView().UpdateAllLifecyclePhases();
  // (600 - 300) * (140 - 40 + 60) / (default viewport size 800 * 600)
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
}

}  // namespace blink
