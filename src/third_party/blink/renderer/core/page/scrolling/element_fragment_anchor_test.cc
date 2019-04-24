// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using test::RunPendingTasks;

class ElementFragmentAnchorTest : public SimTest {
  void SetUp() override {
    SimTest::SetUp();

    // Focus handlers aren't run unless the page is focused.
    GetDocument().GetPage()->GetFocusController().SetFocused(true);

    WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  }
};

// Ensure that the focus event handler is run before the rAF callback. We'll
// change the background color from a rAF set in the focus handler and make
// sure the computed background color of that frame was changed. See:
// https://groups.google.com/a/chromium.org/d/msg/blink-dev/5BJSTl-FMGY/JMtaKqGhBAAJ
TEST_F(ElementFragmentAnchorTest, FocusHandlerRunBeforeRaf) {
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

// This test ensures that when an iframe's document is closed, and the parent
// has dirty layout, the iframe is laid out prior to invoking its fragment
// anchor. Without performing this layout, the anchor cannot scroll to the
// correct location and it will be cleared since the document is closed.
TEST_F(ElementFragmentAnchorTest, IframeFragmentNoLayoutUntilLoad) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest child_resource("https://example.com/child.html#fragment",
                            "text/html");
  LoadURL("https://example.com/test.html");

  // Don't clcose the main document yet, since that'll cause it to layout.
  main_resource.Write(R"HTML(
      <!DOCTYPE html>
      <style>
        iframe {
          border: 0;
          width: 300px;
          height: 200px;
        }
      </style>
      <iframe id="child" src="child.html#fragment"></iframe>
    )HTML");

  // When the iframe document is loaded, it'll try to scroll the fragment into
  // view. Ensure it does so correctly by laying out first.
  child_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <div style="height:500px;">content</div>
      <div id="fragment">fragment content</div>
    )HTML");
  Compositor().BeginFrame();

  HTMLFrameOwnerElement* iframe =
      To<HTMLFrameOwnerElement>(GetDocument().getElementById("child"));
  ScrollableArea* child_viewport =
      iframe->contentDocument()->View()->LayoutViewport();
  Element* fragment = iframe->contentDocument()->getElementById("fragment");

  IntRect fragment_rect_in_frame(
      fragment->GetLayoutObject()->AbsoluteBoundingBoxRect());
  IntRect viewport_rect(IntPoint(),
                        child_viewport->VisibleContentRect().Size());

  EXPECT_TRUE(viewport_rect.Contains(fragment_rect_in_frame))
      << "Fragment element at [" << fragment_rect_in_frame.ToString()
      << "] was not scrolled into viewport rect [" << viewport_rect.ToString()
      << "]";

  main_resource.Finish();
}

// This test ensures that we correctly scroll the fragment into view in the
// case that the iframe has finished load but layout becomes dirty (in both
// parent and iframe) before we've had a chance to scroll the fragment into
// view.
TEST_F(ElementFragmentAnchorTest, IframeFragmentDirtyLayoutAfterLoad) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest child_resource("https://example.com/child.html#fragment",
                            "text/html");
  LoadURL("https://example.com/test.html");

  // Don't clcose the main document yet, since that'll cause it to layout.
  main_resource.Write(R"HTML(
      <!DOCTYPE html>
      <style>
        iframe {
          border: 0;
          width: 300px;
          height: 200px;
        }
      </style>
      <iframe id="child" src="child.html#fragment"></iframe>
    )HTML");

  // Use text so that changing the iframe width will change the y-location of
  // the fragment.
  child_resource.Complete(R"HTML(
      <!DOCTYPE html>
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      <div id="fragment">fragment content</div>
    )HTML");

  HTMLFrameOwnerElement* iframe =
      To<HTMLFrameOwnerElement>(GetDocument().getElementById("child"));
  iframe->setAttribute(html_names::kStyleAttr, "width:100px");

  Compositor().BeginFrame();

  ScrollableArea* child_viewport =
      iframe->contentDocument()->View()->LayoutViewport();
  Element* fragment = iframe->contentDocument()->getElementById("fragment");

  IntRect fragment_rect_in_frame(
      fragment->GetLayoutObject()->AbsoluteBoundingBoxRect());
  IntRect viewport_rect(IntPoint(),
                        child_viewport->VisibleContentRect().Size());

  EXPECT_TRUE(viewport_rect.Contains(fragment_rect_in_frame))
      << "Fragment element at [" << fragment_rect_in_frame.ToString()
      << "] was not scrolled into viewport rect [" << viewport_rect.ToString()
      << "]";

  main_resource.Finish();
}

}  // namespace

// Ensure that an SVG document doesn't automatically create a fragment anchor
// without the URL actually having a fragment.
TEST_F(ElementFragmentAnchorTest, SVGDocumentDoesntCreateFragment) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest svg_resource("https://example.com/file.svg", "image/svg+xml");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <img id="image" src=file.svg>
    )HTML");

  // Load an SVG that's transformed outside of the container rect. Ensure that
  // we don't scroll it into view since we didn't specify a hash fragment.
  svg_resource.Complete(R"SVG(
      <svg id="svg" width="50" height="50" xmlns="http://www.w3.org/2000/svg">
         <style>
          #svg{
            transform: translateX(200px) translateY(200px);
          }
         </style>
         <circle class="path" cx="50" cy="50" r="20" fill="red"/>
      </svg>
    )SVG");

  auto* img = ToHTMLImageElement(GetDocument().getElementById("image"));
  SVGImage* svg = ToSVGImage(img->CachedImage()->GetImage());
  auto* view =
      DynamicTo<LocalFrameView>(svg->GetPageForTesting()->MainFrame()->View());

  // Scroll should remain unchanged and no anchor should be set.
  ASSERT_EQ(ScrollOffset(), view->GetScrollableArea()->GetScrollOffset());
  ASSERT_FALSE(view->GetFragmentAnchor());

  // Check after a BeginFrame as well since SVG documents appear to process the
  // fragment at this time as well.
  Compositor().BeginFrame();
  ASSERT_EQ(ScrollOffset(), view->GetScrollableArea()->GetScrollOffset());
  ASSERT_FALSE(view->GetFragmentAnchor());
}

}  // namespace blink
