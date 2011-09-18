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
#include "ui/gfx/compositor/test_compositor_host.h"

namespace ui {

namespace {

// Simple class that exits the current message loop when compositing finishes.
class CompositingEndedObserver : public CompositorObserver {
 public:
  CompositingEndedObserver() {}
  virtual ~CompositingEndedObserver() {}

  // Overridden from CompositorObserver:
  virtual void OnCompositingEnded() OVERRIDE {
    MessageLoop::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositingEndedObserver);
};

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
    color_index_ = ++color_index_ % colors_.size();
  }

private:
  Layer* owner_;
  std::vector<SkColor> colors_;
  int color_index_;
  gfx::Size paint_size_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerDelegate);
};

class LayerTest : public testing::Test {
 public:
  LayerTest() {}
  virtual ~LayerTest() {}

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

  Layer* CreateLayer() {
    return new Layer(GetCompositor());
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = CreateLayer();
    layer->SetBounds(bounds);
    PaintColorToLayer(layer, color);
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
    GetCompositor()->set_root_layer(root);
    GetCompositor()->Draw(false);
  }

  void RunPendingMessages() {
    MessageLoopForUI::current()->Run(NULL);
  }

 private:
  MessageLoopForUI message_loop_;
  scoped_ptr<TestCompositorHost> window_;

  DISALLOW_COPY_AND_ASSIGN(LayerTest);
};

class LayerQuitOnCompositedTest : public LayerTest {
 public:
  LayerQuitOnCompositedTest() {}
  virtual ~LayerQuitOnCompositedTest() {}

  // Overridden from LayerTest:
  virtual void SetUp() OVERRIDE {
    LayerTest::SetUp();
    GetCompositor()->AddObserver(&compositor_observer_);
  }

  virtual void TearDown() OVERRIDE {
    LayerTest::TearDown();
    GetCompositor()->RemoveObserver(&compositor_observer_);
  }

 private:
  CompositingEndedObserver compositor_observer_;

  DISALLOW_COPY_AND_ASSIGN(LayerQuitOnCompositedTest);
};

}

TEST_F(LayerTest, Draw) {
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
TEST_F(LayerTest, Hierarchy) {
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

// L1
//  +-- L2
TEST_F(LayerTest, ConvertPointToLayer_Simple) {
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
TEST_F(LayerTest, ConvertPointToLayer_Medium) {
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

TEST_F(LayerQuitOnCompositedTest, Delegate) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorBLACK,
                                        gfx::Rect(20, 20, 400, 400)));
  TestLayerDelegate delegate(l1.get());
  l1->set_delegate(&delegate);
  delegate.AddColor(SK_ColorWHITE);
  delegate.AddColor(SK_ColorYELLOW);
  delegate.AddColor(SK_ColorGREEN);

  GetCompositor()->set_root_layer(l1.get());

  l1->SchedulePaint(gfx::Rect(0, 0, 400, 400));
  RunPendingMessages();
  EXPECT_EQ(delegate.color_index(), 1);
  EXPECT_EQ(delegate.paint_size(), l1->bounds().size());

  l1->SchedulePaint(gfx::Rect(10, 10, 200, 200));
  RunPendingMessages();
  EXPECT_EQ(delegate.color_index(), 2);
  EXPECT_EQ(delegate.paint_size(), gfx::Size(200, 200));

  l1->SchedulePaint(gfx::Rect(5, 5, 50, 50));
  RunPendingMessages();
  EXPECT_EQ(delegate.color_index(), 0);
  EXPECT_EQ(delegate.paint_size(), gfx::Size(50, 50));
}

namespace {

class DrawTreeLayerDelegate : public LayerDelegate {
 public:
  DrawTreeLayerDelegate() : painted_(false) {}
  virtual ~DrawTreeLayerDelegate() {}

  void Reset() {
    painted_ = false;
  }

  bool painted() const { return painted_; }

  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    painted_ = true;
  }

 private:
  bool painted_;

  DISALLOW_COPY_AND_ASSIGN(DrawTreeLayerDelegate);
};

}  // namespace

TEST_F(LayerQuitOnCompositedTest, DrawTree) {
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

  GetCompositor()->set_root_layer(l1.get());

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  RunPendingMessages();
  EXPECT_FALSE(d1.painted());
  EXPECT_TRUE(d2.painted());
  EXPECT_FALSE(d3.painted());
}

}  // namespace ui

