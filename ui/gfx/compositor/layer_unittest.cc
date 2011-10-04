// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/compositor_observer.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/test_compositor.h"
#include "ui/gfx/compositor/test_compositor_host.h"

namespace ui {

namespace {

// Use
// - LayerWithRealCompositorTest when a real compositor is required for testing.
//    - Slow because they bring up a window.
//
// - LayerWithDelegateTest for testing functionality of the LayerDelegate.
// - LayerWithNullDelegateTest for testing all other functionality.

class LayerWithRealCompositorTest : public testing::Test {
 public:
  LayerWithRealCompositorTest() {}
  virtual ~LayerWithRealCompositorTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    const gfx::Rect host_bounds(10, 10, 500, 500);
    window_.reset(TestCompositorHost::Create(host_bounds));
    window_->Show();
  }

  virtual void TearDown() OVERRIDE {
  }

  Compositor* GetCompositor() {
    return window_->GetCompositor();
  }

  Layer* CreateLayer(Layer::LayerType type) {
    return new Layer(GetCompositor(), type);
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_TEXTURE);
    layer->SetBounds(bounds);
    PaintColorToLayer(layer, color);
    return layer;
  }

  Layer* CreateNoTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_NO_TEXTURE);
    layer->SetBounds(bounds);
    return layer;
  }

  gfx::Canvas* CreateCanvasForLayer(const Layer* layer) {
    return gfx::Canvas::CreateCanvas(layer->bounds().width(),
                                     layer->bounds().height(),
                                     false);
  }

  void PaintColorToLayer(Layer* layer, SkColor color) {
    scoped_ptr<gfx::Canvas> canvas(CreateCanvasForLayer(layer));
    canvas->FillRectInt(color, 0, 0, layer->bounds().width(),
                        layer->bounds().height());
    layer->SetCanvas(*canvas->AsCanvasSkia(), layer->bounds().origin());
  }

  void DrawTree(Layer* root) {
    GetCompositor()->SetRootLayer(root);
    GetCompositor()->Draw(false);
  }

  void RunPendingMessages() {
    MessageLoopForUI::current()->RunAllPending();
  }

  // Invalidates the entire contents of the layer.
  void SchedulePaintForLayer(Layer* layer) {
    layer->SchedulePaint(
        gfx::Rect(0, 0, layer->bounds().width(), layer->bounds().height()));
  }

 private:
  MessageLoopForUI message_loop_;
  scoped_ptr<TestCompositorHost> window_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithRealCompositorTest);
};

// LayerDelegate that paints colors to the layer.
class TestLayerDelegate : public LayerDelegate {
 public:
  explicit TestLayerDelegate(Layer* owner) : owner_(owner), color_index_(0) {}
  virtual ~TestLayerDelegate() {}

  void AddColor(SkColor color) {
    colors_.push_back(color);
  }

  gfx::Size paint_size() const { return paint_size_; }
  int color_index() const { return color_index_; }

  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    SkBitmap contents = canvas->AsCanvasSkia()->ExtractBitmap();
    paint_size_ = gfx::Size(contents.width(), contents.height());
    canvas->FillRectInt(colors_.at(color_index_), 0, 0,
                        contents.width(),
                        contents.height());
    color_index_ = (color_index_ + 1) % static_cast<int>(colors_.size());
  }

 private:
  Layer* owner_;
  std::vector<SkColor> colors_;
  int color_index_;
  gfx::Size paint_size_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerDelegate);
};

// LayerDelegate that verifies that a layer was asked to update its canvas.
class DrawTreeLayerDelegate : public LayerDelegate {
 public:
  DrawTreeLayerDelegate() : painted_(false) {}
  virtual ~DrawTreeLayerDelegate() {}

  void Reset() {
    painted_ = false;
  }

  bool painted() const { return painted_; }

 private:
  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    painted_ = true;
  }

  bool painted_;

  DISALLOW_COPY_AND_ASSIGN(DrawTreeLayerDelegate);
};

// The simplest possible layer delegate. Does nothing.
class NullLayerDelegate : public LayerDelegate {
 public:
  NullLayerDelegate() {}
  virtual ~NullLayerDelegate() {}

 private:
  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(NullLayerDelegate);
};

}

TEST_F(LayerWithRealCompositorTest, Draw) {
  scoped_ptr<Layer> layer(CreateColorLayer(SK_ColorRED,
                                           gfx::Rect(20, 20, 50, 50)));
  DrawTree(layer.get());
}

// Create this hierarchy:
// L1 - red
// +-- L2 - blue
// |   +-- L3 - yellow
// +-- L4 - magenta
//
TEST_F(LayerWithRealCompositorTest, Hierarchy) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(5, 5, 25, 25)));
  scoped_ptr<Layer> l4(CreateColorLayer(SK_ColorMAGENTA,
                                        gfx::Rect(300, 300, 100, 100)));

  l1->Add(l2.get());
  l1->Add(l4.get());
  l2->Add(l3.get());

  DrawTree(l1.get());
}

class LayerWithDelegateTest : public testing::Test {
 public:
  LayerWithDelegateTest() {}
  virtual ~LayerWithDelegateTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    compositor_ = new TestCompositor();
  }

  virtual void TearDown() OVERRIDE {
  }

  Compositor* compositor() { return compositor_.get(); }

  virtual Layer* CreateLayer(Layer::LayerType type) {
    return new Layer(compositor(), type);
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_TEXTURE);
    layer->SetBounds(bounds);
    PaintColorToLayer(layer, color);
    return layer;
  }

  virtual Layer* CreateNoTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_NO_TEXTURE);
    layer->SetBounds(bounds);
    return layer;
  }

  gfx::Canvas* CreateCanvasForLayer(const Layer* layer) {
    return gfx::Canvas::CreateCanvas(layer->bounds().width(),
                                     layer->bounds().height(),
                                     false);
  }

  void PaintColorToLayer(Layer* layer, SkColor color) {
    scoped_ptr<gfx::Canvas> canvas(CreateCanvasForLayer(layer));
    canvas->FillRectInt(color, 0, 0, layer->bounds().width(),
                        layer->bounds().height());
    layer->SetCanvas(*canvas->AsCanvasSkia(), layer->bounds().origin());
  }

  void DrawTree(Layer* root) {
    compositor()->SetRootLayer(root);
    compositor()->Draw(false);
  }

  // Invalidates the entire contents of the layer.
  void SchedulePaintForLayer(Layer* layer) {
    layer->SchedulePaint(
        gfx::Rect(0, 0, layer->bounds().width(), layer->bounds().height()));
  }

  // Invokes DrawTree on the compositor.
  void Draw() {
    compositor_->root_layer()->DrawTree();
  }

 private:
  scoped_refptr<TestCompositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithDelegateTest);
};

// L1
//  +-- L2
TEST_F(LayerWithDelegateTest, ConvertPointToLayer_Simple) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  l1->Add(l2.get());
  DrawTree(l1.get());

  gfx::Point point1_in_l2_coords(5, 5);
  Layer::ConvertPointToLayer(l2.get(), l1.get(), &point1_in_l2_coords);
  gfx::Point point1_in_l1_coords(15, 15);
  EXPECT_EQ(point1_in_l1_coords, point1_in_l2_coords);

  gfx::Point point2_in_l1_coords(5, 5);
  Layer::ConvertPointToLayer(l1.get(), l2.get(), &point2_in_l1_coords);
  gfx::Point point2_in_l2_coords(-5, -5);
  EXPECT_EQ(point2_in_l2_coords, point2_in_l1_coords);
}

// L1
//  +-- L2
//       +-- L3
TEST_F(LayerWithDelegateTest, ConvertPointToLayer_Medium) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(10, 10, 100, 100)));
  l1->Add(l2.get());
  l2->Add(l3.get());
  DrawTree(l1.get());

  gfx::Point point1_in_l3_coords(5, 5);
  Layer::ConvertPointToLayer(l3.get(), l1.get(), &point1_in_l3_coords);
  gfx::Point point1_in_l1_coords(25, 25);
  EXPECT_EQ(point1_in_l1_coords, point1_in_l3_coords);

  gfx::Point point2_in_l1_coords(5, 5);
  Layer::ConvertPointToLayer(l1.get(), l3.get(), &point2_in_l1_coords);
  gfx::Point point2_in_l3_coords(-15, -15);
  EXPECT_EQ(point2_in_l3_coords, point2_in_l1_coords);
}

TEST_F(LayerWithDelegateTest, Delegate) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorBLACK,
                                        gfx::Rect(20, 20, 400, 400)));
  TestLayerDelegate delegate(l1.get());
  l1->set_delegate(&delegate);
  delegate.AddColor(SK_ColorWHITE);
  delegate.AddColor(SK_ColorYELLOW);
  delegate.AddColor(SK_ColorGREEN);

  compositor()->SetRootLayer(l1.get());

  l1->SchedulePaint(gfx::Rect(0, 0, 400, 400));
  Draw();
  EXPECT_EQ(delegate.color_index(), 1);
  EXPECT_EQ(delegate.paint_size(), l1->bounds().size());

  l1->SchedulePaint(gfx::Rect(10, 10, 200, 200));
  Draw();
  EXPECT_EQ(delegate.color_index(), 2);
  EXPECT_EQ(delegate.paint_size(), gfx::Size(200, 200));

  l1->SchedulePaint(gfx::Rect(5, 5, 50, 50));
  Draw();
  EXPECT_EQ(delegate.color_index(), 0);
  EXPECT_EQ(delegate.paint_size(), gfx::Size(50, 50));
}

TEST_F(LayerWithDelegateTest, DrawTree) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(10, 10, 100, 100)));
  l1->Add(l2.get());
  l2->Add(l3.get());

  DrawTreeLayerDelegate d1;
  l1->set_delegate(&d1);
  DrawTreeLayerDelegate d2;
  l2->set_delegate(&d2);
  DrawTreeLayerDelegate d3;
  l3->set_delegate(&d3);

  compositor()->SetRootLayer(l1.get());

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  Draw();
  EXPECT_FALSE(d1.painted());
  EXPECT_TRUE(d2.painted());
  EXPECT_FALSE(d3.painted());
}

// Tests no-texture Layers.
// Create this hierarchy:
// L1 - red
// +-- L2 - NO TEXTURE
// |   +-- L3 - yellow
// +-- L4 - magenta
//
TEST_F(LayerWithDelegateTest, HierarchyNoTexture) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateNoTextureLayer(gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(5, 5, 25, 25)));
  scoped_ptr<Layer> l4(CreateColorLayer(SK_ColorMAGENTA,
                                        gfx::Rect(300, 300, 100, 100)));

  l1->Add(l2.get());
  l1->Add(l4.get());
  l2->Add(l3.get());

  DrawTreeLayerDelegate d2;
  l2->set_delegate(&d2);
  DrawTreeLayerDelegate d3;
  l3->set_delegate(&d3);

  compositor()->SetRootLayer(l1.get());

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  l3->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  Draw();

  // |d2| should not have received a paint notification since it has no texture.
  EXPECT_FALSE(d2.painted());
  // |d3| should have received a paint notification.
  EXPECT_TRUE(d3.painted());
}

class LayerWithNullDelegateTest : public LayerWithDelegateTest {
 public:
  LayerWithNullDelegateTest() {}
  virtual ~LayerWithNullDelegateTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    LayerWithDelegateTest::SetUp();
    default_layer_delegate_.reset(new NullLayerDelegate());
  }

  virtual void TearDown() OVERRIDE {
  }

  Layer* CreateLayer (Layer::LayerType type) OVERRIDE {
    Layer* layer = new Layer(compositor(), type);
    layer->set_delegate(default_layer_delegate_.get());
    return layer;
  }

  Layer* CreateTextureRootLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateTextureLayer(bounds);
    compositor()->SetRootLayer(layer);
    return layer;
  }

  Layer* CreateTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_TEXTURE);
    layer->SetBounds(bounds);
    return layer;
  }

  Layer* CreateNoTextureLayer(const gfx::Rect& bounds) OVERRIDE {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_NO_TEXTURE);
    layer->SetBounds(bounds);
    return layer;
  }

 private:
  scoped_ptr<NullLayerDelegate> default_layer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithNullDelegateTest);
};

// Verifies that a layer which is set never to have a texture does not
// get a texture when SetFillsBoundsOpaquely is called.
TEST_F(LayerWithNullDelegateTest, LayerNoTextureSetFillsBoundsOpaquely) {
  scoped_ptr<Layer> parent(CreateNoTextureLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateNoTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  compositor()->SetRootLayer(parent.get());
  Draw();
  EXPECT_TRUE(child->texture() == NULL);
  EXPECT_TRUE(parent->texture() == NULL);
}

// Verifies that a layer does not have a texture when the hole is the size
// of the parent layer.
TEST_F(LayerWithNullDelegateTest, LayerNoTextureHoleSizeOfLayer) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  Draw();
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100), parent->hole_rect());
  EXPECT_TRUE(parent->texture() != NULL);

  child->SetBounds(gfx::Rect(0, 0, 400, 400));
  Draw();
  EXPECT_TRUE(parent->texture() == NULL);
}

// Verifies that a layer which has opacity == 0 does not have a texture.
TEST_F(LayerWithNullDelegateTest, LayerNoTextureTransparent) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  parent->SetOpacity(0.0f);
  child->SetOpacity(0.0f);
  Draw();
  EXPECT_TRUE(parent->texture() == NULL);
  EXPECT_TRUE(child->texture() == NULL);

  parent->SetOpacity(1.0f);
  Draw();
  EXPECT_TRUE(parent->texture() != NULL);
  EXPECT_TRUE(child->texture() == NULL);

  child->SetOpacity(1.0f);
  Draw();
  EXPECT_TRUE(parent->texture() != NULL);
  EXPECT_TRUE(child->texture() != NULL);
}

// Verifies that no texture is created for a layer with empty bounds.
TEST_F(LayerWithNullDelegateTest, LayerTextureNonEmptySchedulePaint) {
  scoped_ptr<Layer> layer(CreateTextureRootLayer(gfx::Rect(0, 0, 0, 0)));
  Draw();
  EXPECT_TRUE(layer->texture() == NULL);

  layer->SetBounds(gfx::Rect(0, 0, 400, 400));
  Draw();
  EXPECT_TRUE(layer->texture() != NULL);
}

// Verifies that a hole in a layer does not intersect non completely-opaque
// layers.
TEST_F(LayerWithNullDelegateTest, HoleDoesNotIntersectNonOpaque) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child1(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child1.get());

  scoped_ptr<Layer> child2(CreateLayer(Layer::LAYER_HAS_NO_TEXTURE));
  child2->SetFillsBoundsOpaquely(false);
  parent->Add(child2.get());
  Draw();

  // Ensure largest non intersecting hole picked.
  // largest fragment on bottom.
  child2->SetBounds(gfx::Rect(20, 20, 60, 80));
  EXPECT_EQ(gfx::Rect(50, 100, 100, 50), parent->hole_rect());

  // largest fragment on right.
  child2->SetBounds(gfx::Rect(20, 20, 60, 200));
  EXPECT_EQ(gfx::Rect(80, 50, 70, 100), parent->hole_rect());

  // largest fragment on top.
  child2->SetBounds(gfx::Rect(130, 110, 50, 50));
  EXPECT_EQ(gfx::Rect(50, 50, 100, 60), parent->hole_rect());

  // largest fragment on left.
  child2->SetBounds(gfx::Rect(130, 20, 50, 200));
  EXPECT_EQ(gfx::Rect(50, 50, 80, 100), parent->hole_rect());
}

// Verifies that when there are many potential holes, the largest one is picked.
TEST_F(LayerWithNullDelegateTest, LargestHole) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));

  scoped_ptr<Layer> child1(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child1.get());

  scoped_ptr<Layer> child2(CreateTextureLayer(gfx::Rect(75, 75, 200, 200)));
  parent->Add(child2.get());

  EXPECT_EQ(gfx::Rect(75, 75, 200, 200), parent->hole_rect());
}

// Verifies that there is no hole present when one of the child layers has a
// transform.
TEST_F(LayerWithNullDelegateTest, NoHoleWithTransform) {
  scoped_ptr<Layer> parent(CreateTextureLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  EXPECT_TRUE(!parent->hole_rect().IsEmpty());

  ui::Transform t;
  t.SetRotate(90.0f);
  child->SetTransform(t);

  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), parent->hole_rect());
}

// Create this hierarchy:
// L1 (no texture)
//  +- L11 (texture)
//  +- L12 (no texture) (added after L1 is already set as root-layer)
//    +- L121 (texture)
//    +- L122 (texture)
TEST_F(LayerWithNullDelegateTest, NoCompositor) {
  scoped_ptr<Layer> l1(CreateLayer(Layer::LAYER_HAS_NO_TEXTURE));
  scoped_ptr<Layer> l11(CreateLayer(Layer::LAYER_HAS_TEXTURE));
  scoped_ptr<Layer> l12(CreateLayer(Layer::LAYER_HAS_NO_TEXTURE));
  scoped_ptr<Layer> l121(CreateLayer(Layer::LAYER_HAS_TEXTURE));
  scoped_ptr<Layer> l122(CreateLayer(Layer::LAYER_HAS_TEXTURE));

  EXPECT_EQ(NULL, l1->texture());
  EXPECT_EQ(NULL, l11->texture());
  EXPECT_EQ(NULL, l12->texture());
  EXPECT_EQ(NULL, l121->texture());
  EXPECT_EQ(NULL, l122->texture());

  l1->Add(l11.get());
  l1->SetBounds(gfx::Rect(0, 0, 500, 500));
  l11->SetBounds(gfx::Rect(5, 5, 490, 490));

  EXPECT_EQ(NULL, l11->texture());

  compositor()->SetRootLayer(l1.get());

  EXPECT_EQ(NULL, l1->texture());

  // Despite having type LAYER_HAS_TEXTURE, l11 will not have one set yet
  // because it has never been asked to draw.
  EXPECT_EQ(NULL, l11->texture());

  l12->Add(l121.get());
  l12->Add(l122.get());
  l12->SetBounds(gfx::Rect(5, 5, 480, 480));
  l121->SetBounds(gfx::Rect(5, 5, 100, 100));
  l122->SetBounds(gfx::Rect(110, 110, 100, 100));

  EXPECT_EQ(NULL, l121->texture());
  EXPECT_EQ(NULL, l122->texture());

  l1->Add(l12.get());

  // By asking l121 and l122 to paint, we cause them to generate a texture.
  SchedulePaintForLayer(l121.get());
  SchedulePaintForLayer(l122.get());
  Draw();

  EXPECT_EQ(NULL, l12->texture());
  EXPECT_TRUE(NULL != l121->texture());
  EXPECT_TRUE(NULL != l122->texture());

  // Toggling l2's visibility should drop all sub-layer textures.
  l12->SetVisible(false);
  EXPECT_EQ(NULL, l12->texture());
  EXPECT_EQ(NULL, l121->texture());
  EXPECT_EQ(NULL, l122->texture());
}

} // namespace ui
