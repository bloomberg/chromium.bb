// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <AVFoundation/AVFoundation.h>
#include <memory>

#include "base/mac/sdk_forward_declarations.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/mac/io_surface.h"

namespace gpu {

class CALayerTreeTest : public testing::Test {
 protected:
  void SetUp() override {
    superlayer_.reset([[CALayer alloc] init]);
    fullscreen_low_power_layer_.reset(
        [[AVSampleBufferDisplayLayer alloc] init]);
  }

  base::scoped_nsobject<CALayer> superlayer_;
  base::scoped_nsobject<AVSampleBufferDisplayLayer> fullscreen_low_power_layer_;
};

// Test updating each layer's properties.
TEST_F(CALayerTreeTest, PropertyUpdates) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888));
  bool is_clipped = true;
  gfx::Rect clip_rect(2, 4, 8, 16);
  int sorting_context_id = 0;
  gfx::Transform transform;
  transform.Translate(10, 20);
  gfx::RectF contents_rect(0.0f, 0.25f, 0.5f, 0.75f);
  gfx::Rect rect(16, 32, 64, 128);
  unsigned background_color = SkColorSetARGB(0xFF, 0xFF, 0, 0);
  unsigned edge_aa_mask = GL_CA_LAYER_EDGE_LEFT_CHROMIUM;
  float opacity = 0.5f;
  float scale_factor = 1.0f;
  bool result = false;

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;
  CALayer* root_layer = nil;
  CALayer* clip_and_sorting_layer = nil;
  CALayer* transform_layer = nil;
  CALayer* content_layer = nil;

  // Validate the initial values.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the clip and sorting context layer.
    EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
    EXPECT_EQ(gfx::Rect(clip_rect.size()),
              gfx::Rect([clip_and_sorting_layer bounds]));
    EXPECT_EQ(clip_rect.origin(),
              gfx::Point([clip_and_sorting_layer position]));
    EXPECT_EQ(-clip_rect.origin().x(),
              [clip_and_sorting_layer sublayerTransform].m41);
    EXPECT_EQ(-clip_rect.origin().y(),
              [clip_and_sorting_layer sublayerTransform].m42);

    // Validate the transform layer.
    EXPECT_EQ(transform.matrix().get(3, 0),
              [transform_layer sublayerTransform].m41);
    EXPECT_EQ(transform.matrix().get(3, 1),
              [transform_layer sublayerTransform].m42);

    // Validate the content layer.
    EXPECT_EQ(static_cast<id>(io_surface.get()), [content_layer contents]);
    EXPECT_EQ(contents_rect, gfx::RectF([content_layer contentsRect]));
    EXPECT_EQ(rect.origin(), gfx::Point([content_layer position]));
    EXPECT_EQ(gfx::Rect(rect.size()), gfx::Rect([content_layer bounds]));
    EXPECT_EQ(kCALayerLeftEdge, [content_layer edgeAntialiasingMask]);
    EXPECT_EQ(opacity, [content_layer opacity]);
    if ([content_layer respondsToSelector:(@selector(contentsScale))])
      EXPECT_EQ(scale_factor, [content_layer contentsScale]);
  }

  // Update just the clip rect and re-commit.
  {
    clip_rect = gfx::Rect(4, 8, 16, 32);
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);

    // Validate the clip and sorting context layer.
    EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
    EXPECT_EQ(gfx::Rect(clip_rect.size()),
              gfx::Rect([clip_and_sorting_layer bounds]));
    EXPECT_EQ(clip_rect.origin(),
              gfx::Point([clip_and_sorting_layer position]));
    EXPECT_EQ(-clip_rect.origin().x(),
              [clip_and_sorting_layer sublayerTransform].m41);
    EXPECT_EQ(-clip_rect.origin().y(),
              [clip_and_sorting_layer sublayerTransform].m42);
  }

  // Disable clipping and re-commit.
  {
    is_clipped = false;
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);

    // Validate the clip and sorting context layer.
    EXPECT_FALSE([clip_and_sorting_layer masksToBounds]);
    EXPECT_EQ(gfx::Rect(), gfx::Rect([clip_and_sorting_layer bounds]));
    EXPECT_EQ(gfx::Point(), gfx::Point([clip_and_sorting_layer position]));
    EXPECT_EQ(0.0, [clip_and_sorting_layer sublayerTransform].m41);
    EXPECT_EQ(0.0, [clip_and_sorting_layer sublayerTransform].m42);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
  }

  // Change the transform and re-commit.
  {
    transform.Translate(5, 5);
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);

    // Validate the transform layer.
    EXPECT_EQ(transform.matrix().get(3, 0),
              [transform_layer sublayerTransform].m41);
    EXPECT_EQ(transform.matrix().get(3, 1),
              [transform_layer sublayerTransform].m42);
  }

  // Change the edge antialiasing mask and commit.
  {
    edge_aa_mask = GL_CA_LAYER_EDGE_TOP_CHROMIUM;
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer. Note that top and bottom edges flip.
    EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
  }

  // Change the contents and commit.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        nullptr,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer. Note that edge anti-aliasing no longer flips.
    EXPECT_EQ(nil, [content_layer contents]);
    EXPECT_EQ(kCALayerTopEdge, [content_layer edgeAntialiasingMask]);
  }

  // Change the rect size.
  {
    rect = gfx::Rect(rect.origin(), gfx::Size(32, 16));
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        nullptr,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_EQ(rect.origin(), gfx::Point([content_layer position]));
    EXPECT_EQ(gfx::Rect(rect.size()), gfx::Rect([content_layer bounds]));
  }

  // Change the rect position.
  {
    rect = gfx::Rect(gfx::Point(16, 4), rect.size());
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        nullptr,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_EQ(rect.origin(), gfx::Point([content_layer position]));
    EXPECT_EQ(gfx::Rect(rect.size()), gfx::Rect([content_layer bounds]));
  }

  // Change the opacity.
  {
    opacity = 1.0f;
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        nullptr,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_EQ(opacity, [content_layer opacity]);
  }

  // Add the clipping and IOSurface contents back.
  {
    is_clipped = true;
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_EQ(static_cast<id>(io_surface.get()), [content_layer contents]);
    EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
  }

  // Change the scale factor. This should result in a new tree being created.
  {
    scale_factor = 2.0f;
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_NE(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_NE(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_NE(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_NE(content_layer, [[transform_layer sublayers] objectAtIndex:0]);
    content_layer = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the clip and sorting context layer.
    EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
    EXPECT_EQ(gfx::ConvertRectToDIP(scale_factor, gfx::Rect(clip_rect.size())),
              gfx::Rect([clip_and_sorting_layer bounds]));
    EXPECT_EQ(gfx::ConvertPointToDIP(scale_factor, clip_rect.origin()),
              gfx::Point([clip_and_sorting_layer position]));
    EXPECT_EQ(-clip_rect.origin().x() / scale_factor,
              [clip_and_sorting_layer sublayerTransform].m41);
    EXPECT_EQ(-clip_rect.origin().y() / scale_factor,
              [clip_and_sorting_layer sublayerTransform].m42);

    // Validate the transform layer.
    EXPECT_EQ(transform.matrix().get(3, 0) / scale_factor,
              [transform_layer sublayerTransform].m41);
    EXPECT_EQ(transform.matrix().get(3, 1) / scale_factor,
              [transform_layer sublayerTransform].m42);

    // Validate the content layer.
    EXPECT_EQ(static_cast<id>(io_surface.get()), [content_layer contents]);
    EXPECT_EQ(contents_rect, gfx::RectF([content_layer contentsRect]));
    EXPECT_EQ(gfx::ConvertPointToDIP(scale_factor, rect.origin()),
              gfx::Point([content_layer position]));
    EXPECT_EQ(gfx::ConvertRectToDIP(scale_factor, gfx::Rect(rect.size())),
              gfx::Rect([content_layer bounds]));
    EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
    EXPECT_EQ(opacity, [content_layer opacity]);
    if ([content_layer respondsToSelector:(@selector(contentsScale))])
      EXPECT_EQ(scale_factor, [content_layer contentsScale]);
  }
}

// Verify that sorting context zero is split at non-flat transforms.
TEST_F(CALayerTreeTest, SplitSortingContextZero) {
  bool is_clipped = false;
  gfx::Rect clip_rect;
  int sorting_context_id = 0;
  gfx::RectF contents_rect(0, 0, 1, 1);
  gfx::Rect rect(0, 0, 256, 256);
  unsigned background_color = SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF);
  unsigned edge_aa_mask = 0;
  float opacity = 1.0f;
  float scale_factor = 1.0f;

  // We'll use the IOSurface contents to identify the content layers.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surfaces[5];
  for (size_t i = 0; i < 5; ++i) {
    io_surfaces[i].reset(gfx::CreateIOSurface(
        gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888));
  }

  // Have 5 transforms:
  // * 2 flat but different (1 sorting context layer, 2 transform layers)
  // * 1 non-flat (new sorting context layer)
  // * 2 flat and the same (new sorting context layer, 1 transform layer)
  gfx::Transform transforms[5];
  transforms[0].Translate(10, 10);
  transforms[1].RotateAboutZAxis(45.0f);
  transforms[2].RotateAboutYAxis(45.0f);
  transforms[3].Translate(10, 10);
  transforms[4].Translate(10, 10);

  // Schedule and commit the layers.
  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree);
  for (size_t i = 0; i < 5; ++i) {
    bool result = ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transforms[i],
        io_surfaces[i],
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
  }
  ca_layer_tree->CommitScheduledCALayers(superlayer_, nullptr, scale_factor);

  // Validate the root layer.
  EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
  CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];

  // Validate that we have 3 sorting context layers.
  EXPECT_EQ(3u, [[root_layer sublayers] count]);
  CALayer* clip_and_sorting_layer_0 = [[root_layer sublayers] objectAtIndex:0];
  CALayer* clip_and_sorting_layer_1 = [[root_layer sublayers] objectAtIndex:1];
  CALayer* clip_and_sorting_layer_2 = [[root_layer sublayers] objectAtIndex:2];

  // Validate that the first sorting context has 2 transform layers each with
  // one content layer.
  EXPECT_EQ(2u, [[clip_and_sorting_layer_0 sublayers] count]);
  CALayer* transform_layer_0_0 =
      [[clip_and_sorting_layer_0 sublayers] objectAtIndex:0];
  CALayer* transform_layer_0_1 =
      [[clip_and_sorting_layer_0 sublayers] objectAtIndex:1];
  EXPECT_EQ(1u, [[transform_layer_0_0 sublayers] count]);
  CALayer* content_layer_0 = [[transform_layer_0_0 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[transform_layer_0_1 sublayers] count]);
  CALayer* content_layer_1 = [[transform_layer_0_1 sublayers] objectAtIndex:0];

  // Validate that the second sorting context has 1 transform layer with one
  // content layer.
  EXPECT_EQ(1u, [[clip_and_sorting_layer_1 sublayers] count]);
  CALayer* transform_layer_1_0 =
      [[clip_and_sorting_layer_1 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[transform_layer_1_0 sublayers] count]);
  CALayer* content_layer_2 = [[transform_layer_1_0 sublayers] objectAtIndex:0];

  // Validate that the third sorting context has 1 transform layer with two
  // content layers.
  EXPECT_EQ(1u, [[clip_and_sorting_layer_2 sublayers] count]);
  CALayer* transform_layer_2_0 =
      [[clip_and_sorting_layer_2 sublayers] objectAtIndex:0];
  EXPECT_EQ(2u, [[transform_layer_2_0 sublayers] count]);
  CALayer* content_layer_3 = [[transform_layer_2_0 sublayers] objectAtIndex:0];
  CALayer* content_layer_4 = [[transform_layer_2_0 sublayers] objectAtIndex:1];

  // Validate that the layers come out in order.
  EXPECT_EQ(static_cast<id>(io_surfaces[0].get()), [content_layer_0 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[1].get()), [content_layer_1 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[2].get()), [content_layer_2 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[3].get()), [content_layer_3 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[4].get()), [content_layer_4 contents]);
}

// Verify that sorting contexts are allocated appropriately.
TEST_F(CALayerTreeTest, SortingContexts) {
  bool is_clipped = false;
  gfx::Rect clip_rect;
  int sorting_context_ids[3] = {3, -1, 0};
  gfx::RectF contents_rect(0, 0, 1, 1);
  gfx::Rect rect(0, 0, 256, 256);
  gfx::Transform transform;
  unsigned background_color = SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF);
  unsigned edge_aa_mask = 0;
  float opacity = 1.0f;
  float scale_factor = 1.0f;

  // We'll use the IOSurface contents to identify the content layers.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surfaces[3];
  for (size_t i = 0; i < 3; ++i) {
    io_surfaces[i].reset(gfx::CreateIOSurface(
        gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888));
  }

  // Schedule and commit the layers.
  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree);
  for (size_t i = 0; i < 3; ++i) {
    bool result = ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_ids[i],
        transform,
        io_surfaces[i],
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
  }
  ca_layer_tree->CommitScheduledCALayers(superlayer_, nullptr, scale_factor);

  // Validate the root layer.
  EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
  CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];

  // Validate that we have 3 sorting context layers.
  EXPECT_EQ(3u, [[root_layer sublayers] count]);
  CALayer* clip_and_sorting_layer_0 = [[root_layer sublayers] objectAtIndex:0];
  CALayer* clip_and_sorting_layer_1 = [[root_layer sublayers] objectAtIndex:1];
  CALayer* clip_and_sorting_layer_2 = [[root_layer sublayers] objectAtIndex:2];

  // Validate that each sorting context has 1 transform layer.
  EXPECT_EQ(1u, [[clip_and_sorting_layer_0 sublayers] count]);
  CALayer* transform_layer_0 =
      [[clip_and_sorting_layer_0 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[clip_and_sorting_layer_1 sublayers] count]);
  CALayer* transform_layer_1 =
      [[clip_and_sorting_layer_1 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[clip_and_sorting_layer_2 sublayers] count]);
  CALayer* transform_layer_2 =
      [[clip_and_sorting_layer_2 sublayers] objectAtIndex:0];

  // Validate that each transform has 1 content layer.
  EXPECT_EQ(1u, [[transform_layer_0 sublayers] count]);
  CALayer* content_layer_0 = [[transform_layer_0 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[transform_layer_1 sublayers] count]);
  CALayer* content_layer_1 = [[transform_layer_1 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[transform_layer_2 sublayers] count]);
  CALayer* content_layer_2 = [[transform_layer_2 sublayers] objectAtIndex:0];

  // Validate that the layers come out in order.
  EXPECT_EQ(static_cast<id>(io_surfaces[0].get()), [content_layer_0 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[1].get()), [content_layer_1 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[2].get()), [content_layer_2 contents]);
}

// Verify that sorting contexts must all have the same clipping properties.
TEST_F(CALayerTreeTest, SortingContextMustHaveConsistentClip) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  gfx::RectF contents_rect(0, 0, 1, 1);
  gfx::Rect rect(0, 0, 256, 256);
  gfx::Transform transform;
  unsigned background_color = SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF);
  unsigned edge_aa_mask = 0;
  float opacity = 1.0f;

  // Vary the clipping parameters within sorting contexts.
  bool is_clippeds[3] = { true, true, false};
  gfx::Rect clip_rects[3] = {
      gfx::Rect(0, 0, 16, 16),
      gfx::Rect(4, 8, 16, 32),
      gfx::Rect(0, 0, 16, 16)
  };

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree);
  // First send the various clip parameters to sorting context zero. This is
  // legitimate.
  for (size_t i = 0; i < 3; ++i) {
    int sorting_context_id = 0;
    bool result = ca_layer_tree->ScheduleCALayer(
        is_clippeds[i],
        clip_rects[i],
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
  }
  // Next send the various clip parameters to a non-zero sorting context. This
  // will fail when we try to change the clip within the sorting context.
  for (size_t i = 0; i < 3; ++i) {
    int sorting_context_id = 3;
    bool result = ca_layer_tree->ScheduleCALayer(
        is_clippeds[i],
        clip_rects[i],
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    if (i == 0)
      EXPECT_TRUE(result);
    else
      EXPECT_FALSE(result);
  }
  // Try once more with the original clip and verify it works.
  {
    int sorting_context_id = 3;
    bool result = ca_layer_tree->ScheduleCALayer(
        is_clippeds[0],
        clip_rects[0],
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
  }
}

// Test updating each layer's properties.
TEST_F(CALayerTreeTest, AVLayer) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR));

  bool is_clipped = true;
  gfx::Rect clip_rect(2, 4, 8, 16);
  int sorting_context_id = 0;
  gfx::Transform transform;
  gfx::RectF contents_rect(0.0f, 0.0f, 1.0f, 1.0f);
  gfx::Rect rect(16, 32, 64, 128);
  unsigned background_color = SkColorSetARGB(0xFF, 0xFF, 0, 0);
  unsigned edge_aa_mask = GL_CA_LAYER_EDGE_LEFT_CHROMIUM;
  float opacity = 0.5f;
  float scale_factor = 1.0f;
  bool result = false;

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;
  CALayer* root_layer = nil;
  CALayer* clip_and_sorting_layer = nil;
  CALayer* transform_layer = nil;
  CALayer* content_layer1 = nil;
  CALayer* content_layer2 = nil;
  CALayer* content_layer3 = nil;

  // Validate the initial values.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect, background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer1 = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer.
    EXPECT_FALSE([content_layer1
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
  }

  io_surface.reset(gfx::CreateIOSurface(gfx::Size(256, 256),
                                        gfx::BufferFormat::YUV_420_BIPLANAR));

  // Pass another frame.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer2 = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer.
    EXPECT_FALSE([content_layer2
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_EQ(content_layer2, content_layer1);
  }

  io_surface.reset(gfx::CreateIOSurface(gfx::Size(256, 256),
                                        gfx::BufferFormat::YUV_420_BIPLANAR));
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  CVPixelBufferCreateWithIOSurface(
      nullptr, io_surface, nullptr, cv_pixel_buffer.InitializeInto());

  // Pass a frame with a CVPixelBuffer
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        cv_pixel_buffer,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer2 = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer.
    EXPECT_TRUE([content_layer2
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer2, content_layer1);
  }

  io_surface.reset(gfx::CreateIOSurface(gfx::Size(256, 256),
                                        gfx::BufferFormat::YUV_420_BIPLANAR));
  cv_pixel_buffer.reset();

  // Pass a frame that is clipped.
  contents_rect = gfx::RectF(0, 0, 1, 0.9);
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped,
        clip_rect,
        sorting_context_id,
        transform,
        io_surface,
        nullptr,
        contents_rect,
        rect,
        background_color,
        edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer3 = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer.
    EXPECT_FALSE([content_layer3
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer3, content_layer2);
  }
}

// Test fullscreen low power detection.
TEST_F(CALayerTreeTest, FullscreenLowPower) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR));

  bool is_clipped = false;
  gfx::Rect clip_rect(0, 0, 1, 1);
  int sorting_context_id = 0;
  gfx::Transform transform;
  transform.Translate(10, 20);
  transform.Scale(0.5, 2.0);
  gfx::RectF contents_rect(0.0f, 0.0f, 1.0f, 1.0f);
  gfx::Rect rect(16, 32, 64, 128);
  unsigned background_color = SkColorSetARGB(0, 0, 0, 0);
  unsigned edge_aa_mask = 0;
  float opacity = 1.0f;
  float scale_factor = 1.0f;
  bool result = false;

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;

  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  CVPixelBufferCreateWithIOSurface(nullptr, io_surface, nullptr,
                                   cv_pixel_buffer.InitializeInto());

  // Test a configuration with no background.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped, clip_rect, sorting_context_id, transform, io_surface,
        cv_pixel_buffer, contents_rect, rect, background_color, edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    bool fullscreen_low_power_valid =
        new_ca_layer_tree->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* transform_layer =
        [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    CALayer* content_layer = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE([content_layer
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_TRUE(fullscreen_low_power_valid);
  }

  // Test a configuration with a black background.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped, clip_rect, sorting_context_id, transform, nullptr, nullptr,
        contents_rect, rect, SK_ColorBLACK, edge_aa_mask, opacity);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped, clip_rect, sorting_context_id, transform, io_surface,
        cv_pixel_buffer, contents_rect, rect, background_color, edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    bool fullscreen_low_power_valid =
        new_ca_layer_tree->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* transform_layer =
        [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);
    CALayer* content_layer = [[transform_layer sublayers] objectAtIndex:1];

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE([content_layer
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_TRUE(fullscreen_low_power_valid);
  }

  // Test a configuration with a white background. It will fail.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped, clip_rect, sorting_context_id, transform, nullptr, nullptr,
        contents_rect, rect, SK_ColorWHITE, edge_aa_mask, opacity);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped, clip_rect, sorting_context_id, transform, io_surface,
        cv_pixel_buffer, contents_rect, rect, background_color, edge_aa_mask,
        opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    bool fullscreen_low_power_valid =
        new_ca_layer_tree->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* transform_layer =
        [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);
    CALayer* content_layer = [[transform_layer sublayers] objectAtIndex:1];

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE([content_layer
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_FALSE(fullscreen_low_power_valid);
  }

  // Test a configuration with a black foreground. It too will fail.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped, clip_rect, sorting_context_id, transform, io_surface,
        cv_pixel_buffer, contents_rect, rect, background_color, edge_aa_mask,
        opacity);
    result = new_ca_layer_tree->ScheduleCALayer(
        is_clipped, clip_rect, sorting_context_id, transform, nullptr, nullptr,
        contents_rect, rect, SK_ColorBLACK, edge_aa_mask, opacity);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), scale_factor);
    bool fullscreen_low_power_valid =
        new_ca_layer_tree->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* transform_layer =
        [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);
    CALayer* content_layer = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE([content_layer
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_FALSE(fullscreen_low_power_valid);
  }
}

}  // namespace gpu
