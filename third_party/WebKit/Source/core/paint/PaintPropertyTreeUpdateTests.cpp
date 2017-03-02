// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElement.h"
#include "core/paint/PaintPropertyTreeBuilderTest.h"
#include "core/paint/PaintPropertyTreePrinter.h"

namespace blink {

// Tests covering incremental updates of paint property trees.
class PaintPropertyTreeUpdateTest : public PaintPropertyTreeBuilderTest {};

INSTANTIATE_TEST_CASE_P(All, PaintPropertyTreeUpdateTest, ::testing::Bool());

TEST_P(PaintPropertyTreeUpdateTest,
       ThreadedScrollingDisabledMainThreadScrollReason) {
  setBodyInnerHTML(
      "<style>"
      "  #overflowA {"
      "    position: absolute;"
      "    overflow: scroll;"
      "    width: 20px;"
      "    height: 20px;"
      "  }"
      "  .forceScroll {"
      "    height: 4000px;"
      "  }"
      "</style>"
      "<div id='overflowA'>"
      "  <div class='forceScroll'></div>"
      "</div>"
      "<div class='forceScroll'></div>");
  Element* overflowA = document().getElementById("overflowA");
  EXPECT_FALSE(frameScroll()->threadedScrollingDisabled());
  EXPECT_FALSE(overflowA->layoutObject()
                   ->paintProperties()
                   ->scrollTranslation()
                   ->scrollNode()
                   ->threadedScrollingDisabled());

  document().settings()->setThreadedScrollingEnabled(false);
  // TODO(pdr): The main thread scrolling setting should invalidate properties.
  document().view()->setNeedsPaintPropertyUpdate();
  overflowA->layoutObject()->setNeedsPaintPropertyUpdate();
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(frameScroll()->threadedScrollingDisabled());
  EXPECT_TRUE(overflowA->layoutObject()
                  ->paintProperties()
                  ->scrollTranslation()
                  ->scrollNode()
                  ->threadedScrollingDisabled());
}

TEST_P(PaintPropertyTreeUpdateTest,
       BackgroundAttachmentFixedMainThreadScrollReasonsWithNestedScrollers) {
  setBodyInnerHTML(
      "<style>"
      "  #overflowA {"
      "    position: absolute;"
      "    overflow: scroll;"
      "    width: 20px;"
      "    height: 20px;"
      "  }"
      "  #overflowB {"
      "    position: absolute;"
      "    overflow: scroll;"
      "    width: 5px;"
      "    height: 3px;"
      "  }"
      "  .backgroundAttachmentFixed {"
      "    background-image: url('foo');"
      "    background-attachment: fixed;"
      "  }"
      "  .forceScroll {"
      "    height: 4000px;"
      "  }"
      "</style>"
      "<div id='overflowA'>"
      "  <div id='overflowB' class='backgroundAttachmentFixed'>"
      "    <div class='forceScroll'></div>"
      "  </div>"
      "  <div class='forceScroll'></div>"
      "</div>"
      "<div class='forceScroll'></div>");
  Element* overflowA = document().getElementById("overflowA");
  Element* overflowB = document().getElementById("overflowB");

  EXPECT_TRUE(frameScroll()->hasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflowA->layoutObject()
                  ->paintProperties()
                  ->scrollTranslation()
                  ->scrollNode()
                  ->hasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflowB->layoutObject()
                  ->paintProperties()
                  ->scrollTranslation()
                  ->scrollNode()
                  ->hasBackgroundAttachmentFixedDescendants());

  // Removing a main thread scrolling reason should update the entire tree.
  overflowB->removeAttribute("class");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(frameScroll()->hasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflowA->layoutObject()
                   ->paintProperties()
                   ->scrollTranslation()
                   ->scrollNode()
                   ->hasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflowB->layoutObject()
                   ->paintProperties()
                   ->scrollTranslation()
                   ->scrollNode()
                   ->hasBackgroundAttachmentFixedDescendants());

  // Adding a main thread scrolling reason should update the entire tree.
  overflowB->setAttribute(HTMLNames::classAttr, "backgroundAttachmentFixed");
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(frameScroll()->hasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflowA->layoutObject()
                  ->paintProperties()
                  ->scrollTranslation()
                  ->scrollNode()
                  ->hasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflowB->layoutObject()
                  ->paintProperties()
                  ->scrollTranslation()
                  ->scrollNode()
                  ->hasBackgroundAttachmentFixedDescendants());
}

TEST_P(PaintPropertyTreeUpdateTest, ParentFrameMainThreadScrollReasons) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0; }"
      "  .fixedBackground {"
      "    background-image: url('foo');"
      "    background-attachment: fixed;"
      "  }"
      "</style>"
      "<iframe></iframe>"
      "<div id='fixedBackground' class='fixedBackground'></div>"
      "<div id='forceScroll' style='height: 8888px;'></div>");
  setChildFrameHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='forceScroll' style='height: 8888px;'></div>");
  document().view()->updateAllLifecyclePhases();

  FrameView* parent = document().view();
  EXPECT_TRUE(frameScroll(parent)->hasBackgroundAttachmentFixedDescendants());
  FrameView* child = childDocument().view();
  EXPECT_TRUE(frameScroll(child)->hasBackgroundAttachmentFixedDescendants());

  // Removing a main thread scrolling reason should update the entire tree.
  auto* fixedBackground = document().getElementById("fixedBackground");
  fixedBackground->removeAttribute(HTMLNames::classAttr);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(frameScroll(parent)->hasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(frameScroll(child)->hasBackgroundAttachmentFixedDescendants());

  // Adding a main thread scrolling reason should update the entire tree.
  fixedBackground->setAttribute(HTMLNames::classAttr, "fixedBackground");
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(frameScroll(parent)->hasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(frameScroll(child)->hasBackgroundAttachmentFixedDescendants());
}

TEST_P(PaintPropertyTreeUpdateTest, ChildFrameMainThreadScrollReasons) {
  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<iframe></iframe>"
      "<div id='forceScroll' style='height: 8888px;'></div>");
  setChildFrameHTML(
      "<style>"
      "  body { margin: 0; }"
      "  .fixedBackground {"
      "    background-image: url('foo');"
      "    background-attachment: fixed;"
      "  }"
      "</style>"
      "<div id='fixedBackground' class='fixedBackground'></div>"
      "<div id='forceScroll' style='height: 8888px;'></div>");
  document().view()->updateAllLifecyclePhases();

  FrameView* parent = document().view();
  EXPECT_FALSE(frameScroll(parent)->hasBackgroundAttachmentFixedDescendants());
  FrameView* child = childDocument().view();
  EXPECT_TRUE(frameScroll(child)->hasBackgroundAttachmentFixedDescendants());

  // Removing a main thread scrolling reason should update the entire tree.
  auto* fixedBackground = childDocument().getElementById("fixedBackground");
  fixedBackground->removeAttribute(HTMLNames::classAttr);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(frameScroll(parent)->hasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(frameScroll(child)->hasBackgroundAttachmentFixedDescendants());

  // Adding a main thread scrolling reason should update the entire tree.
  fixedBackground->setAttribute(HTMLNames::classAttr, "fixedBackground");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(frameScroll(parent)->hasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(frameScroll(child)->hasBackgroundAttachmentFixedDescendants());
}

TEST_P(PaintPropertyTreeUpdateTest,
       BackgroundAttachmentFixedMainThreadScrollReasonsWithFixedScroller) {
  setBodyInnerHTML(
      "<style>"
      "  #overflowA {"
      "    position: absolute;"
      "    overflow: scroll;"
      "    width: 20px;"
      "    height: 20px;"
      "  }"
      "  #overflowB {"
      "    position: fixed;"
      "    overflow: scroll;"
      "    width: 5px;"
      "    height: 3px;"
      "  }"
      "  .backgroundAttachmentFixed {"
      "    background-image: url('foo');"
      "    background-attachment: fixed;"
      "  }"
      "  .forceScroll {"
      "    height: 4000px;"
      "  }"
      "</style>"
      "<div id='overflowA'>"
      "  <div id='overflowB' class='backgroundAttachmentFixed'>"
      "    <div class='forceScroll'></div>"
      "  </div>"
      "  <div class='forceScroll'></div>"
      "</div>"
      "<div class='forceScroll'></div>");
  Element* overflowA = document().getElementById("overflowA");
  Element* overflowB = document().getElementById("overflowB");

  // This should be false. We are not as strict about main thread scrolling
  // reasons as we could be.
  EXPECT_TRUE(overflowA->layoutObject()
                  ->paintProperties()
                  ->scrollTranslation()
                  ->scrollNode()
                  ->hasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflowB->layoutObject()
                   ->paintProperties()
                   ->scrollTranslation()
                   ->scrollNode()
                   ->hasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflowB->layoutObject()
                  ->paintProperties()
                  ->scrollTranslation()
                  ->scrollNode()
                  ->parent()
                  ->isRoot());

  // Removing a main thread scrolling reason should update the entire tree.
  overflowB->removeAttribute("class");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(overflowA->layoutObject()
                   ->paintProperties()
                   ->scrollTranslation()
                   ->scrollNode()
                   ->hasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflowB->layoutObject()
                   ->paintProperties()
                   ->scrollTranslation()
                   ->scrollNode()
                   ->hasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflowB->layoutObject()
                   ->paintProperties()
                   ->scrollTranslation()
                   ->scrollNode()
                   ->parent()
                   ->hasBackgroundAttachmentFixedDescendants());
}

TEST_P(PaintPropertyTreeUpdateTest, DescendantNeedsUpdateAcrossFrames) {
  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='divWithTransform' style='transform: translate3d(1px,2px,3px);'>"
      "  <iframe style='border: 7px solid black'></iframe>"
      "</div>");
  setChildFrameHTML(
      "<style>body { margin: 0; }</style><div id='transform' style='transform: "
      "translate3d(4px, 5px, 6px); width: 100px; height: 200px'></div>");

  FrameView* frameView = document().view();
  frameView->updateAllLifecyclePhases();

  LayoutObject* divWithTransform =
      document().getElementById("divWithTransform")->layoutObject();
  LayoutObject* childLayoutView = childDocument().layoutView();
  LayoutObject* innerDivWithTransform =
      childDocument().getElementById("transform")->layoutObject();

  // Initially, no objects should need a descendant update.
  EXPECT_FALSE(document().layoutView()->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(divWithTransform->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(childLayoutView->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(innerDivWithTransform->descendantNeedsPaintPropertyUpdate());

  // Marking the child div as needing a paint property update should propagate
  // up the tree and across frames.
  innerDivWithTransform->setNeedsPaintPropertyUpdate();
  EXPECT_TRUE(document().layoutView()->descendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(divWithTransform->descendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(childLayoutView->descendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(innerDivWithTransform->needsPaintPropertyUpdate());
  EXPECT_FALSE(innerDivWithTransform->descendantNeedsPaintPropertyUpdate());

  // After a lifecycle update, no nodes should need a descendant update.
  frameView->updateAllLifecyclePhases();
  EXPECT_FALSE(document().layoutView()->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(divWithTransform->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(childLayoutView->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(innerDivWithTransform->descendantNeedsPaintPropertyUpdate());

  // A child frame marked as needing a paint property update should not be
  // skipped if the owning layout tree does not need an update.
  FrameView* childFrameView = childDocument().view();
  childFrameView->setNeedsPaintPropertyUpdate();
  EXPECT_TRUE(document().layoutView()->descendantNeedsPaintPropertyUpdate());
  frameView->updateAllLifecyclePhases();
  EXPECT_FALSE(document().layoutView()->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(frameView->needsPaintPropertyUpdate());
  EXPECT_FALSE(childFrameView->needsPaintPropertyUpdate());
}

TEST_P(PaintPropertyTreeUpdateTest, UpdatingFrameViewContentClip) {
  setBodyInnerHTML("hello world.");
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600), frameContentClip()->clipRect());
  document().view()->resize(800, 599);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 599), frameContentClip()->clipRect());
  document().view()->resize(800, 600);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600), frameContentClip()->clipRect());
  document().view()->resize(5, 5);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(FloatRoundedRect(0, 0, 5, 5), frameContentClip()->clipRect());
}

// There is also FrameThrottlingTest.UpdatePaintPropertiesOnUnthrottling
// testing with real frame viewport intersection observer. This one tests
// paint property update with or without AllowThrottlingScope.
TEST_P(PaintPropertyTreeUpdateTest, BuildingStopsAtThrottledFrames) {
  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='transform' style='transform: translate3d(4px, 5px, 6px);'>"
      "</div>"
      "<iframe id='iframe' sandbox></iframe>");
  setChildFrameHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='iframeTransform'"
      "  style='transform: translate3d(4px, 5px, 6px);'/>");

  // Move the child frame offscreen so it becomes available for throttling.
  auto* iframe = toHTMLIFrameElement(document().getElementById("iframe"));
  iframe->setAttribute(HTMLNames::styleAttr, "transform: translateY(5555px)");
  document().view()->updateAllLifecyclePhases();
  // Ensure intersection observer notifications get delivered.
  testing::runPendingTasks();
  EXPECT_FALSE(document().view()->isHiddenForThrottling());
  EXPECT_TRUE(childDocument().view()->isHiddenForThrottling());

  auto* transform = document().getElementById("transform")->layoutObject();
  auto* iframeLayoutView = childDocument().layoutView();
  auto* iframeTransform =
      childDocument().getElementById("iframeTransform")->layoutObject();

  // Invalidate properties in the iframe and ensure ancestors are marked.
  iframeTransform->setNeedsPaintPropertyUpdate();
  EXPECT_FALSE(document().layoutView()->needsPaintPropertyUpdate());
  EXPECT_TRUE(document().layoutView()->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(transform->needsPaintPropertyUpdate());
  EXPECT_FALSE(transform->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframeLayoutView->needsPaintPropertyUpdate());
  EXPECT_TRUE(iframeLayoutView->descendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(iframeTransform->needsPaintPropertyUpdate());
  EXPECT_FALSE(iframeTransform->descendantNeedsPaintPropertyUpdate());

  transform->setNeedsPaintPropertyUpdate();
  EXPECT_TRUE(transform->needsPaintPropertyUpdate());
  EXPECT_FALSE(transform->descendantNeedsPaintPropertyUpdate());

  {
    DocumentLifecycle::AllowThrottlingScope throttlingScope(
        document().lifecycle());
    EXPECT_FALSE(document().view()->shouldThrottleRendering());
    EXPECT_TRUE(childDocument().view()->shouldThrottleRendering());

    // A lifecycle update should update all properties except those with
    // actively throttled descendants.
    document().view()->updateAllLifecyclePhases();
    EXPECT_FALSE(document().layoutView()->needsPaintPropertyUpdate());
    EXPECT_FALSE(document().layoutView()->descendantNeedsPaintPropertyUpdate());
    EXPECT_FALSE(transform->needsPaintPropertyUpdate());
    EXPECT_FALSE(transform->descendantNeedsPaintPropertyUpdate());
    EXPECT_FALSE(iframeLayoutView->needsPaintPropertyUpdate());
    EXPECT_TRUE(iframeLayoutView->descendantNeedsPaintPropertyUpdate());
    EXPECT_TRUE(iframeTransform->needsPaintPropertyUpdate());
    EXPECT_FALSE(iframeTransform->descendantNeedsPaintPropertyUpdate());
  }

  EXPECT_FALSE(document().view()->shouldThrottleRendering());
  EXPECT_FALSE(childDocument().view()->shouldThrottleRendering());
  // Once unthrottled, a lifecycel update should update all properties.
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(document().layoutView()->needsPaintPropertyUpdate());
  EXPECT_FALSE(document().layoutView()->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(transform->needsPaintPropertyUpdate());
  EXPECT_FALSE(transform->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframeLayoutView->needsPaintPropertyUpdate());
  EXPECT_FALSE(iframeLayoutView->descendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframeTransform->needsPaintPropertyUpdate());
  EXPECT_FALSE(iframeTransform->descendantNeedsPaintPropertyUpdate());
}

TEST_P(PaintPropertyTreeUpdateTest, ClipChangesUpdateOverflowClip) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin:0 }"
      "  #div { overflow:hidden; height:0px; }"
      "</style>"
      "<div id='div'></div>");
  auto* div = document().getElementById("div");
  div->setAttribute(HTMLNames::styleAttr, "display:inline-block; width:7px;");
  document().view()->updateAllLifecyclePhases();
  auto* clipProperties = div->layoutObject()->paintProperties()->overflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 0), clipProperties->clipRect().rect());

  // Width changes should update the overflow clip.
  div->setAttribute(HTMLNames::styleAttr, "display:inline-block; width:7px;");
  document().view()->updateAllLifecyclePhases();
  clipProperties = div->layoutObject()->paintProperties()->overflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 0), clipProperties->clipRect().rect());
  div->setAttribute(HTMLNames::styleAttr, "display:inline-block; width:9px;");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(0, 0, 9, 0), clipProperties->clipRect().rect());

  // An inline block's overflow clip should be updated when padding changes,
  // even if the border box remains unchanged.
  div->setAttribute(HTMLNames::styleAttr,
                    "display:inline-block; width:7px; padding-right:3px;");
  document().view()->updateAllLifecyclePhases();
  clipProperties = div->layoutObject()->paintProperties()->overflowClip();
  EXPECT_EQ(FloatRect(0, 0, 10, 0), clipProperties->clipRect().rect());
  div->setAttribute(HTMLNames::styleAttr,
                    "display:inline-block; width:8px; padding-right:2px;");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(0, 0, 10, 0), clipProperties->clipRect().rect());
  div->setAttribute(HTMLNames::styleAttr,
                    "display:inline-block; width:8px;"
                    "padding-right:1px; padding-left:1px;");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(0, 0, 10, 0), clipProperties->clipRect().rect());

  // An block's overflow clip should be updated when borders change.
  div->setAttribute(HTMLNames::styleAttr, "border-right:3px solid red;");
  document().view()->updateAllLifecyclePhases();
  clipProperties = div->layoutObject()->paintProperties()->overflowClip();
  EXPECT_EQ(FloatRect(0, 0, 797, 0), clipProperties->clipRect().rect());
  div->setAttribute(HTMLNames::styleAttr, "border-right:5px solid red;");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(0, 0, 795, 0), clipProperties->clipRect().rect());

  // Removing overflow clip should remove the property.
  div->setAttribute(HTMLNames::styleAttr, "overflow:hidden;");
  document().view()->updateAllLifecyclePhases();
  clipProperties = div->layoutObject()->paintProperties()->overflowClip();
  EXPECT_EQ(FloatRect(0, 0, 800, 0), clipProperties->clipRect().rect());
  div->setAttribute(HTMLNames::styleAttr, "overflow:visible;");
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(!div->layoutObject()->paintProperties() ||
              !div->layoutObject()->paintProperties()->overflowClip());
}

TEST_P(PaintPropertyTreeUpdateTest, ContainPaintChangesUpdateOverflowClip) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin:0 }"
      "  #div { will-change:transform; width:7px; height:6px; }"
      "</style>"
      "<div id='div' style='contain:paint;'></div>");
  document().view()->updateAllLifecyclePhases();
  auto* div = document().getElementById("div");
  auto* properties = div->layoutObject()->paintProperties()->overflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 6), properties->clipRect().rect());

  div->setAttribute(HTMLNames::styleAttr, "");
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(!div->layoutObject()->paintProperties() ||
              !div->layoutObject()->paintProperties()->overflowClip());
}

// A basic sanity check for over-invalidation of paint properties.
TEST_P(PaintPropertyTreeUpdateTest, NoPaintPropertyUpdateOnBackgroundChange) {
  setBodyInnerHTML("<div id='div' style='background-color: blue'>DIV</div>");
  auto* div = document().getElementById("div");

  document().view()->updateAllLifecyclePhases();
  div->setAttribute(HTMLNames::styleAttr, "background-color: green");
  document().view()->updateLifecycleToLayoutClean();
  EXPECT_FALSE(div->layoutObject()->needsPaintPropertyUpdate());
}

// Disabled due to stale scrollsOverflow values, see: https://crbug.com/675296.
TEST_P(PaintPropertyTreeUpdateTest,
       DISABLED_FrameVisibilityChangeUpdatesProperties) {
  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='iframeContainer'>"
      "  <iframe id='iframe' style='width: 100px; height: 100px;'></iframe>"
      "</div>");
  setChildFrameHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='forceScroll' style='height: 3000px;'></div>");

  FrameView* frameView = document().view();
  frameView->updateAllLifecyclePhases();
  EXPECT_EQ(nullptr, frameScroll(frameView));
  FrameView* childFrameView = childDocument().view();
  EXPECT_NE(nullptr, frameScroll(childFrameView));

  auto* iframeContainer = document().getElementById("iframeContainer");
  iframeContainer->setAttribute(HTMLNames::styleAttr, "visibility: hidden;");
  frameView->updateAllLifecyclePhases();

  EXPECT_EQ(nullptr, frameScroll(frameView));
  EXPECT_EQ(nullptr, frameScroll(childFrameView));
}

TEST_P(PaintPropertyTreeUpdateTest,
       TransformNodeWithAnimationLosesNodeWhenAnimationRemoved) {
  loadTestData("transform-animation.html");
  Element* target = document().getElementById("target");
  const ObjectPaintProperties* properties =
      target->layoutObject()->paintProperties();
  EXPECT_TRUE(properties->transform()->hasDirectCompositingReasons());

  // Removing the animation should remove the transform node.
  target->removeAttribute(HTMLNames::classAttr);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(nullptr, properties->transform());
}

TEST_P(PaintPropertyTreeUpdateTest,
       EffectNodeWithAnimationLosesNodeWhenAnimationRemoved) {
  loadTestData("opacity-animation.html");
  Element* target = document().getElementById("target");
  const ObjectPaintProperties* properties =
      target->layoutObject()->paintProperties();
  EXPECT_TRUE(properties->effect()->hasDirectCompositingReasons());

  // Removing the animation should remove the effect node.
  target->removeAttribute(HTMLNames::classAttr);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(nullptr, properties->effect());
}

TEST_P(PaintPropertyTreeUpdateTest,
       TransformNodeLosesCompositorElementIdWhenAnimationRemoved) {
  loadTestData("transform-animation.html");

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "transform: translateX(2em)");
  document().view()->updateAllLifecyclePhases();

  const ObjectPaintProperties* properties =
      target->layoutObject()->paintProperties();
  EXPECT_NE(CompositorElementId(),
            properties->transform()->compositorElementId());

  // Remove the animation but keep the transform on the element.
  target->removeAttribute(HTMLNames::classAttr);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(CompositorElementId(),
            properties->transform()->compositorElementId());
}

TEST_P(PaintPropertyTreeUpdateTest,
       EffectNodeLosesCompositorElementIdWhenAnimationRemoved) {
  loadTestData("opacity-animation.html");

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "opacity: 0.2");
  document().view()->updateAllLifecyclePhases();

  const ObjectPaintProperties* properties =
      target->layoutObject()->paintProperties();
  EXPECT_NE(CompositorElementId(), properties->effect()->compositorElementId());

  target->removeAttribute(HTMLNames::classAttr);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(CompositorElementId(), properties->effect()->compositorElementId());
}

TEST_P(PaintPropertyTreeUpdateTest, PerspectiveOriginUpdatesOnSizeChanges) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  #perspective {"
      "    position: absolute;"
      "    perspective: 100px;"
      "    width: 100px;"
      "    perspective-origin: 50% 50% 0;"
      "  }"
      "</style>"
      "<div id='perspective'>"
      "  <div id='contents'></div>"
      "</div>");

  auto* perspective = document().getElementById("perspective")->layoutObject();
  EXPECT_EQ(TransformationMatrix().applyPerspective(100),
            perspective->paintProperties()->perspective()->matrix());
  EXPECT_EQ(FloatPoint3D(50, 0, 0),
            perspective->paintProperties()->perspective()->origin());

  auto* contents = document().getElementById("contents");
  contents->setAttribute(HTMLNames::styleAttr, "height: 200px;");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix().applyPerspective(100),
            perspective->paintProperties()->perspective()->matrix());
  EXPECT_EQ(FloatPoint3D(50, 100, 0),
            perspective->paintProperties()->perspective()->origin());
}

TEST_P(PaintPropertyTreeUpdateTest, TransformUpdatesOnRelativeLengthChanges) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  #transform {"
      "    transform: translate3d(50%, 50%, 0);"
      "    width: 100px;"
      "    height: 200px;"
      "  }"
      "</style>"
      "<div id='transform'></div>");

  auto* transform = document().getElementById("transform");
  auto* transformObject = transform->layoutObject();
  EXPECT_EQ(TransformationMatrix().translate3d(50, 100, 0),
            transformObject->paintProperties()->transform()->matrix());

  transform->setAttribute(HTMLNames::styleAttr, "width: 200px; height: 300px;");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix().translate3d(100, 150, 0),
            transformObject->paintProperties()->transform()->matrix());
}

TEST_P(PaintPropertyTreeUpdateTest, CSSClipDependingOnSize) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  #outer {"
      "    position: absolute;"
      "    width: 100px; height: 100px; top: 50px; left: 50px;"
      "  }"
      "  #clip {"
      "    position: absolute;"
      "    clip: rect(auto auto auto -5px);"
      "    top: 0; left: 0; right: 0; bottom: 0;"
      "  }"
      "</style>"
      "<div id='outer'>"
      "  <div id='clip'></div>"
      "</div>");

  auto* outer = document().getElementById("outer");
  auto* clip = getLayoutObjectByElementId("clip");
  EXPECT_EQ(FloatRect(45, 50, 105, 100),
            clip->paintProperties()->cssClip()->clipRect().rect());

  outer->setAttribute(HTMLNames::styleAttr, "height: 200px");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(45, 50, 105, 200),
            clip->paintProperties()->cssClip()->clipRect().rect());
}

TEST_P(PaintPropertyTreeUpdateTest, ScrollBoundsChange) {
  setBodyInnerHTML(
      "<div id='container'"
      "    style='width: 100px; height: 100px; overflow: scroll'>"
      "  <div id='content' style='width: 200px; height: 200px'></div>"
      "</div>");

  auto* container = getLayoutObjectByElementId("container");
  auto* scrollNode =
      container->paintProperties()->scrollTranslation()->scrollNode();
  EXPECT_EQ(IntSize(100, 100), scrollNode->clip());
  EXPECT_EQ(IntSize(200, 200), scrollNode->bounds());

  document().getElementById("content")->setAttribute(
      HTMLNames::styleAttr, "width: 200px; height: 300px");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(scrollNode,
            container->paintProperties()->scrollTranslation()->scrollNode());
  EXPECT_EQ(IntSize(100, 100), scrollNode->clip());
  EXPECT_EQ(IntSize(200, 300), scrollNode->bounds());
}

TEST_P(PaintPropertyTreeUpdateTest, ScrollbarWidthChange) {
  setBodyInnerHTML(
      "<style>::-webkit-scrollbar {width: 20px; height: 20px}</style>"
      "<div id='container'"
      "    style='width: 100px; height: 100px; overflow: scroll'>"
      "  <div id='content' style='width: 200px; height: 200px'></div>"
      "</div>");

  auto* container = getLayoutObjectByElementId("container");
  auto* overflowClip = container->paintProperties()->overflowClip();
  EXPECT_EQ(FloatSize(80, 80), overflowClip->clipRect().rect().size());

  auto* newStyle = document().createElement("style");
  newStyle->setTextContent("::-webkit-scrollbar {width: 40px; height: 40px}");
  document().body()->appendChild(newStyle);

  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(overflowClip, container->paintProperties()->overflowClip());
  EXPECT_EQ(FloatSize(60, 60), overflowClip->clipRect().rect().size());
}

}  // namespace blink
