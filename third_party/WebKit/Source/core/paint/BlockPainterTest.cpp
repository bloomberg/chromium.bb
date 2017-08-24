// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockPainter.h"

#include <gtest/gtest.h>
#include "core/frame/LocalFrameView.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/ScrollHitTestDisplayItem.h"

namespace blink {

class BlockPainterTest : public ::testing::WithParamInterface<bool>,
                         private ScopedRootLayerScrollingForTest,
                         public PaintControllerPaintTestBase {
 public:
  BlockPainterTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        PaintControllerPaintTestBase(true) {}
};

INSTANTIATE_TEST_CASE_P(All, BlockPainterTest, ::testing::Bool());

TEST_P(BlockPainterTest, ScrollHitTestProperties) {
  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-scrollbar { display: none; }"
      "  body { margin: 0 }"
      "  #container { width: 200px; height: 200px;"
      "              overflow: scroll; background: blue; }"
      "  #child { width: 100px; height: 300px; background: green; }"
      "</style>"
      "<div id='container'>"
      "  <div id='child'></div>"
      "</div>");

  auto& container = *GetLayoutObjectByElementId("container");
  auto& child = *GetLayoutObjectByElementId("child");

  // The scroll hit test should be after the container background but before the
  // scrolled contents.
  EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
                      TestDisplayItem(container, kBackgroundType),
                      TestDisplayItem(container, kScrollHitTestType),
                      TestDisplayItem(child, kBackgroundType));
  auto paint_chunks = RootPaintController().GetPaintArtifact().PaintChunks();
  EXPECT_EQ(4u, paint_chunks.size());
  const auto& root_chunk = RootPaintController().PaintChunks()[0];
  EXPECT_EQ(GetLayoutView().Layer(), &root_chunk.id.client);
  const auto& container_chunk = RootPaintController().PaintChunks()[1];
  EXPECT_EQ(ToLayoutBoxModelObject(container).Layer(),
            &container_chunk.id.client);
  // The container's scroll hit test.
  const auto& scroll_hit_test_chunk = RootPaintController().PaintChunks()[2];
  EXPECT_EQ(&container, &scroll_hit_test_chunk.id.client);
  EXPECT_EQ(kScrollHitTestType, scroll_hit_test_chunk.id.type);
  // The scrolled contents.
  const auto& contents_chunk = RootPaintController().PaintChunks()[3];
  EXPECT_EQ(&container, &contents_chunk.id.client);

  // The document should not scroll so there should be no scroll offset
  // transform.
  auto* root_transform = root_chunk.properties.property_tree_state.Transform();
  EXPECT_EQ(nullptr, root_transform->ScrollNode());

  // The container's background chunk should not scroll and therefore should use
  // the root transform.
  auto* container_transform =
      container_chunk.properties.property_tree_state.Transform();
  EXPECT_EQ(root_transform, container_transform);
  EXPECT_EQ(nullptr, container_transform->ScrollNode());

  // The scroll hit test should not be scrolled and should not be clipped.
  auto* scroll_hit_test_transform =
      scroll_hit_test_chunk.properties.property_tree_state.Transform();
  EXPECT_EQ(nullptr, scroll_hit_test_transform->ScrollNode());
  EXPECT_EQ(root_transform, scroll_hit_test_transform);
  auto* scroll_hit_test_clip =
      scroll_hit_test_chunk.properties.property_tree_state.Clip();
  EXPECT_EQ(FloatRect(0, 0, 800, 600), scroll_hit_test_clip->ClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  auto* contents_transform =
      contents_chunk.properties.property_tree_state.Transform();
  auto* contents_scroll = contents_transform->ScrollNode();
  EXPECT_EQ(IntSize(200, 300), contents_scroll->Bounds());
  EXPECT_EQ(IntSize(200, 200), contents_scroll->ContainerBounds());
  auto* contents_clip = contents_chunk.properties.property_tree_state.Clip();
  EXPECT_EQ(FloatRect(0, 0, 200, 200), contents_clip->ClipRect().Rect());

  // The scroll hit test display item maintains a reference to a scroll offset
  // translation node and the contents should be scrolled by this node.
  const auto& scroll_hit_test_display_item =
      static_cast<const ScrollHitTestDisplayItem&>(
          RootPaintController()
              .GetDisplayItemList()[scroll_hit_test_chunk.begin_index]);
  EXPECT_EQ(contents_transform,
            &scroll_hit_test_display_item.scroll_offset_node());
}

TEST_P(BlockPainterTest, FrameScrollHitTestProperties) {
  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-scrollbar { display: none; }"
      "  body { margin: 0; }"
      "  #child { width: 100px; height: 2000px; background: green; }"
      "</style>"
      "<div id='child'></div>");

  auto& html = *GetDocument().documentElement()->GetLayoutObject();
  auto& child = *GetLayoutObjectByElementId("child");

  // The scroll hit test should be after the document background but before the
  // scrolled contents.
  EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
                      TestDisplayItem(GetLayoutView(), kScrollHitTestType),
                      TestDisplayItem(child, kBackgroundType));

  auto paint_chunks = RootPaintController().GetPaintArtifact().PaintChunks();
  EXPECT_EQ(3u, paint_chunks.size());
  const auto& root_chunk = RootPaintController().PaintChunks()[0];
  EXPECT_EQ(GetLayoutView().Layer(), &root_chunk.id.client);
  const auto& scroll_hit_test_chunk = RootPaintController().PaintChunks()[1];
  EXPECT_EQ(&GetLayoutView(), &scroll_hit_test_chunk.id.client);
  EXPECT_EQ(kScrollHitTestType, scroll_hit_test_chunk.id.type);
  // The scrolled contents.
  const auto& contents_chunk = RootPaintController().PaintChunks()[2];
  EXPECT_EQ(ToLayoutBoxModelObject(html).Layer(), &contents_chunk.id.client);

  // The scroll hit test should not be scrolled and should not be clipped.
  auto* scroll_hit_test_transform =
      scroll_hit_test_chunk.properties.property_tree_state.Transform();
  EXPECT_EQ(nullptr, scroll_hit_test_transform->ScrollNode());
  auto* scroll_hit_test_clip =
      scroll_hit_test_chunk.properties.property_tree_state.Clip();
  EXPECT_EQ(LayoutRect::InfiniteIntRect(),
            scroll_hit_test_clip->ClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  auto* contents_transform =
      contents_chunk.properties.property_tree_state.Transform();
  auto* contents_scroll = contents_transform->ScrollNode();
  EXPECT_EQ(IntSize(800, 2000), contents_scroll->Bounds());
  EXPECT_EQ(IntSize(800, 600), contents_scroll->ContainerBounds());
  auto* contents_clip = contents_chunk.properties.property_tree_state.Clip();
  EXPECT_EQ(FloatRect(0, 0, 800, 600), contents_clip->ClipRect().Rect());

  // The scroll hit test display item maintains a reference to a scroll offset
  // translation node and the contents should be scrolled by this node.
  const auto& scroll_hit_test_display_item =
      static_cast<const ScrollHitTestDisplayItem&>(
          RootPaintController()
              .GetDisplayItemList()[scroll_hit_test_chunk.begin_index]);
  EXPECT_EQ(contents_transform,
            &scroll_hit_test_display_item.scroll_offset_node());
}

}  // namespace blink
