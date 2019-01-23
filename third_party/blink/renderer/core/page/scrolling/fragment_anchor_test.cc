// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using test::RunPendingTasks;

class FragmentAnchorTest : public SimTest {};

// Ensure that the focus event handler is run before the rAF callback. We'll
// change the background color from a rAF set in the focus handler and make
// sure the computed background color of that frame was changed. See:
// https://groups.google.com/a/chromium.org/d/msg/blink-dev/5BJSTl-FMGY/JMtaKqGhBAAJ
TEST_F(FragmentAnchorTest, FocusHandlerRunBeforeRaf) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/sheet.css",
                                     "text/css");
  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          background-color: red;
        }
      </style>
      <link rel="stylesheet" type="text/css" href="sheet.css">
      <a id="anchorlink" href="#bottom">Link to bottom of the page</a>
      <div style="height: 1000px;"></div>
      <input id="bottom">Bottom of the page</input>
    )HTML");

  MainFrame().ExecuteScript(WebScriptSource(R"HTML(
      document.getElementById("bottom").addEventListener('focus', () => {
        requestAnimationFrame(() => {
          document.body.style.backgroundColor = '#00FF00';
        });
      });
  )HTML"));

  // Focus handlers aren't run unless the page is focused.
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  // We're still waiting on the stylesheet to load so the load event shouldn't
  // yet dispatch and rendering is deferred.
  ASSERT_FALSE(GetDocument().IsRenderingReady());
  ASSERT_FALSE(GetDocument().IsLoadCompleted());

  // Click on the anchor element. This will cause a synchronous same-document
  // navigation.
  HTMLAnchorElement* anchor =
      ToHTMLAnchorElement(GetDocument().getElementById("anchorlink"));
  anchor->click();
  ASSERT_EQ(GetDocument().body(), GetDocument().ActiveElement())
      << "Active element changed while rendering is blocked";

  // Complete the CSS stylesheet load so the document can finish loading. The
  // fragment should be activated at that point.
  css_resource.Complete("");
  Compositor().BeginFrame();

  ASSERT_FALSE(GetDocument().IsLoadCompleted());
  ASSERT_TRUE(GetDocument().IsRenderingReady());
  ASSERT_EQ(GetDocument().getElementById("bottom"),
            GetDocument().ActiveElement())
      << "Active element wasn't changed after rendering was unblocked.";
  EXPECT_EQ(GetDocument()
                .body()
                ->GetLayoutObject()
                ->Style()
                ->VisitedDependentColor(GetCSSPropertyBackgroundColor())
                .NameForLayoutTreeAsText(),
            Color(0, 255, 0).NameForLayoutTreeAsText());
}

}  // namespace

}  // namespace blink
