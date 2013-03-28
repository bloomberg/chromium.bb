// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include "cc/layers/picture_image_layer.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/layer_tree_host_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "ui/gfx/point3_f.h"
#include "webkit/compositor_bindings/web_layer_impl_fixed_bounds.h"

using WebKit::WebFloatPoint;
using WebKit::WebSize;

namespace webkit {
namespace {

TEST(WebLayerImplFixedBoundsTest, IdentityBounds) {
  scoped_ptr<WebLayerImplFixedBounds> layer(new WebLayerImplFixedBounds());
  layer->setAnchorPoint(WebFloatPoint(0, 0));
  layer->SetFixedBounds(gfx::Size(100, 100));
  layer->setBounds(WebSize(100, 100));
  EXPECT_EQ(WebSize(100, 100), layer->bounds());
  EXPECT_EQ(gfx::Size(100, 100), layer->layer()->bounds());
  EXPECT_EQ(gfx::Transform(), layer->layer()->transform());
}

gfx::Point3F TransformPoint(const gfx::Transform& transform,
                            const gfx::Point3F& point) {
  gfx::Point3F result = point;
  transform.TransformPoint(result);
  return result;
}

void CheckBoundsScaleSimple(WebLayerImplFixedBounds* layer,
                            const WebSize& bounds,
                            const gfx::Size& fixed_bounds) {
  layer->setBounds(bounds);
  layer->SetFixedBounds(fixed_bounds);

  EXPECT_EQ(bounds, layer->bounds());
  EXPECT_EQ(fixed_bounds, layer->layer()->bounds());
  EXPECT_TRUE(layer->transform().isIdentity());
  EXPECT_TRUE(layer->sublayerTransform().isIdentity());

  // An arbitrary point to check the scale and transforms.
  gfx::Point3F original_point(10, 20, 1);
  gfx::Point3F scaled_point(
      original_point.x() * bounds.width / fixed_bounds.width(),
      original_point.y() * bounds.height / fixed_bounds.height(),
      original_point.z());
  // Test if the bounds scale is correctly applied in transform and
  // sublayerTransform.
  EXPECT_POINT3F_EQ(scaled_point,
                    TransformPoint(layer->layer()->transform(),
                                   original_point));
  EXPECT_POINT3F_EQ(original_point,
                    TransformPoint(layer->layer()->sublayer_transform(),
                                   scaled_point));
}

TEST(WebLayerImplFixedBoundsTest, BoundsScaleSimple) {
  scoped_ptr<WebLayerImplFixedBounds> layer(new WebLayerImplFixedBounds());
  layer->setAnchorPoint(WebFloatPoint(0, 0));
  CheckBoundsScaleSimple(layer.get(), WebSize(100, 200), gfx::Size(150, 250));
  // Change fixed_bounds.
  CheckBoundsScaleSimple(layer.get(), WebSize(100, 200), gfx::Size(75, 100));
  // Change bounds.
  CheckBoundsScaleSimple(layer.get(), WebSize(300, 100), gfx::Size(75, 100));
}

void ExpectEqualLayerRectsInTarget(cc::Layer* layer1, cc::Layer* layer2) {
  gfx::RectF layer1_rect_in_target(layer1->content_bounds());
  layer1->draw_transform().TransformRect(&layer1_rect_in_target);

  gfx::RectF layer2_rect_in_target(layer2->content_bounds());
  layer2->draw_transform().TransformRect(&layer2_rect_in_target);

  EXPECT_FLOAT_RECT_EQ(layer1_rect_in_target, layer2_rect_in_target);
}

void CompareFixedBoundsLayerAndNormalLayer(
    const WebFloatPoint& anchor_point,
    const gfx::Transform& transform,
    const gfx::Transform& sublayer_transform) {
  const gfx::Size kDeviceViewportSize(800, 600);
  const float kDeviceScaleFactor = 2.f;
  const float kPageScaleFactor = 1.5f;
  const int kMaxTextureSize = 512;

  WebSize bounds(150, 200);
  WebFloatPoint position(20, 30);
  WebSize sublayer_bounds(88, 99);
  WebFloatPoint sublayer_position(50, 60);
  gfx::Size fixed_bounds(160, 70);

  scoped_ptr<WebLayerImplFixedBounds> root_layer(new WebLayerImplFixedBounds());

  WebLayerImplFixedBounds* fixed_bounds_layer =
      new WebLayerImplFixedBounds(cc::PictureImageLayer::Create());
  WebLayerImpl* sublayer_under_fixed_bounds_layer = new WebLayerImpl();
  sublayer_under_fixed_bounds_layer->setBounds(sublayer_bounds);
  sublayer_under_fixed_bounds_layer->setPosition(sublayer_position);
  fixed_bounds_layer->addChild(sublayer_under_fixed_bounds_layer);
  fixed_bounds_layer->setBounds(bounds);
  fixed_bounds_layer->SetFixedBounds(fixed_bounds);
  fixed_bounds_layer->setAnchorPoint(anchor_point);
  fixed_bounds_layer->setTransform(transform.matrix());
  fixed_bounds_layer->setSublayerTransform(sublayer_transform.matrix());
  fixed_bounds_layer->setPosition(position);
  root_layer->addChild(fixed_bounds_layer);

  WebLayerImpl* normal_layer(new WebLayerImpl(cc::PictureImageLayer::Create()));
  WebLayerImpl* sublayer_under_normal_layer = new WebLayerImpl();
  sublayer_under_normal_layer->setBounds(sublayer_bounds);
  sublayer_under_normal_layer->setPosition(sublayer_position);

  normal_layer->addChild(sublayer_under_normal_layer);
  normal_layer->setBounds(bounds);
  normal_layer->setAnchorPoint(anchor_point);
  normal_layer->setTransform(transform.matrix());
  normal_layer->setSublayerTransform(sublayer_transform.matrix());
  normal_layer->setPosition(position);
  root_layer->addChild(normal_layer);

  std::vector<scoped_refptr<cc::Layer> > render_surface_layer_list;
  cc::LayerTreeHostCommon::CalculateDrawProperties(
      root_layer->layer(),
      kDeviceViewportSize,
      kDeviceScaleFactor,
      kPageScaleFactor,
      kMaxTextureSize,
      false,
      &render_surface_layer_list);
  ExpectEqualLayerRectsInTarget(normal_layer->layer(),
                                fixed_bounds_layer->layer());
  ExpectEqualLayerRectsInTarget(sublayer_under_normal_layer->layer(),
                                sublayer_under_fixed_bounds_layer->layer());

  // Change of fixed bounds should not affect the target geometries.
  fixed_bounds_layer->SetFixedBounds(gfx::Size(fixed_bounds.width() / 2,
                                               fixed_bounds.height() * 2));
  cc::LayerTreeHostCommon::CalculateDrawProperties(
      root_layer->layer(),
      kDeviceViewportSize,
      kDeviceScaleFactor,
      kPageScaleFactor,
      kMaxTextureSize,
      false,
      &render_surface_layer_list);
  ExpectEqualLayerRectsInTarget(normal_layer->layer(),
                                fixed_bounds_layer->layer());
  ExpectEqualLayerRectsInTarget(sublayer_under_normal_layer->layer(),
                                sublayer_under_fixed_bounds_layer->layer());
}

// A black box test that ensures WebLayerImplFixedBounds won't change target
// geometries. Simple case: identity transforms and zero anchor point.
TEST(WebLayerImplFixedBoundsTest, CompareToWebLayerImplSimple) {
  CompareFixedBoundsLayerAndNormalLayer(WebFloatPoint(0, 0),
                                        gfx::Transform(),
                                        gfx::Transform());
}

// A black box test that ensures WebLayerImplFixedBounds won't change target
// geometries. Complex case: complex transforms and non-zero anchor point.
TEST(WebLayerImplFixedBoundsTest, CompareToWebLayerImplComplex) {
  gfx::Transform transform;
  // These are arbitrary values that should not affect the results.
  transform.Translate3d(50, 60, 70);
  transform.Scale3d(2, 3, 4);
  transform.RotateAbout(gfx::Vector3dF(33, 44, 55), 99);

  gfx::Transform sublayer_transform;
  // These are arbitrary values that should not affect the results.
  sublayer_transform.Scale3d(1.1, 2.2, 3.3);
  sublayer_transform.Translate3d(11, 22, 33);
  sublayer_transform.RotateAbout(gfx::Vector3dF(10, 30, 20), 88);

  CompareFixedBoundsLayerAndNormalLayer(WebFloatPoint(0, 0),
                                        transform,
                                        sublayer_transform);

  // With non-zero anchor point, WebLayerImplFixedBounds will fall back to
  // WebLayerImpl.
  CompareFixedBoundsLayerAndNormalLayer(WebFloatPoint(0.4f, 0.6f),
                                        transform,
                                        sublayer_transform);
}

}  // namespace
}  // namespace webkit
