// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_client.h"

#include "components/viz/client/hit_test_data_provider.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {

namespace {

const int kMouseInset = -5;
const int kTouchInset = -10;

// Custom WindowTargeter that expands hit-test regions of child windows.
class TestWindowTargeter : public WindowTargeter {
 public:
  TestWindowTargeter() {
    SetInsets(gfx::Insets(kMouseInset), gfx::Insets(kTouchInset));
  }
  ~TestWindowTargeter() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowTargeter);
};

// Custom WindowTargeter that replaces hit-test area on a window with a frame
// rectangle and a hole in the middle 1/3.
//  ----------------------
// |   hit     hit        |
// |      ----------      |
// |     |          |     |
// |     |  No hit  | hit |
// |     |          |     |
// | hit |          |     |
// |      ----------      |
// |   hit        hit     |
//  ----------------------
class TestHoleWindowTargeter : public aura::WindowTargeter {
 public:
  TestHoleWindowTargeter() = default;
  ~TestHoleWindowTargeter() override {}

 private:
  // aura::WindowTargeter:
  std::unique_ptr<aura::WindowTargeter::HitTestRects> GetExtraHitTestShapeRects(
      aura::Window* target) const override {
    gfx::Rect bounds = target->bounds();
    int x0 = 0;
    int x1 = bounds.width() / 3;
    int x2 = bounds.width() - bounds.width() / 3;
    int x3 = bounds.width();
    int y0 = 0;
    int y1 = bounds.height() / 3;
    int y2 = bounds.height() - bounds.height() / 3;
    int y3 = bounds.height();
    auto shape_rects = base::MakeUnique<aura::WindowTargeter::HitTestRects>();
    shape_rects->emplace_back(x0, y0, bounds.width(), y1 - y0);
    shape_rects->emplace_back(x0, y1, x1 - x0, y2 - y1);
    shape_rects->emplace_back(x2, y1, x3 - x2, y2 - y1);
    shape_rects->emplace_back(x0, y2, bounds.width(), y3 - y2);
    return shape_rects;
  }

  DISALLOW_COPY_AND_ASSIGN(TestHoleWindowTargeter);
};

}  // namespace

// Creates a root window and child windows. Maintains a cc:LayerTreeFrameSink
// to help exercise its viz::HitTestDataProvider.
class HitTestDataProviderAuraTest : public test::AuraTestBaseMus {
 public:
  HitTestDataProviderAuraTest() {}
  ~HitTestDataProviderAuraTest() override {}

  void SetUp() override {
    test::AuraTestBaseMus::SetUp();

    root_ = base::MakeUnique<Window>(nullptr);
    root_->Init(ui::LAYER_NOT_DRAWN);
    root_->SetEventTargeter(base::MakeUnique<WindowTargeter>());
    root_->SetBounds(gfx::Rect(0, 0, 300, 200));

    window2_ = new Window(nullptr);
    window2_->Init(ui::LAYER_TEXTURED);
    window2_->SetBounds(gfx::Rect(20, 30, 40, 60));

    window3_ = new Window(nullptr);
    window3_->Init(ui::LAYER_TEXTURED);
    window3_->SetEventTargeter(base::MakeUnique<WindowTargeter>());
    window3_->SetBounds(gfx::Rect(50, 60, 100, 40));

    window4_ = new Window(nullptr);
    window4_->Init(ui::LAYER_TEXTURED);
    window4_->SetBounds(gfx::Rect(20, 10, 60, 30));

    window3_->AddChild(window4_);
    root_->AddChild(window2_);
    root_->AddChild(window3_);

    WindowPort* port = WindowPortMus::Get(root_.get());
    sink_ = port->CreateLayerTreeFrameSink();
  }

 protected:
  const viz::HitTestDataProvider* hit_test_data_provider() const {
    WindowPortMus* port = WindowPortMus::Get(root_.get());
    return port->local_layer_tree_frame_sink_->hit_test_data_provider();
  }

  Window* root() { return root_.get(); }
  Window* window2() { return window2_; }
  Window* window3() { return window3_; }
  Window* window4() { return window4_; }

 private:
  std::unique_ptr<cc::LayerTreeFrameSink> sink_;
  std::unique_ptr<Window> root_;
  Window* window2_;
  Window* window3_;
  Window* window4_;

  DISALLOW_COPY_AND_ASSIGN(HitTestDataProviderAuraTest);
};

// Tests that the order of reported hit-test regions matches windows Z-order.
TEST_F(HitTestDataProviderAuraTest, Stacking) {
  const auto hit_test_data_1 = hit_test_data_provider()->GetHitTestData();
  Window* expected_order_1[] = {window3(), window4(), window2()};
  EXPECT_EQ(hit_test_data_1->regions.size(), arraysize(expected_order_1));
  int i = 0;
  for (const auto& region : hit_test_data_1->regions) {
    EXPECT_EQ(region->flags, viz::mojom::kHitTestMine |
                                 viz::mojom::kHitTestMouse |
                                 viz::mojom::kHitTestTouch);
    viz::SurfaceId surface_id(
        WindowPortMus::Get(expected_order_1[i])->frame_sink_id(),
        WindowMus::Get(expected_order_1[i])->GetLocalSurfaceId());
    EXPECT_EQ(region->surface_id, surface_id);
    EXPECT_EQ(region->rect.ToString(),
              expected_order_1[i]->bounds().ToString());
    i++;
  }

  root()->StackChildAbove(window2(), window3());
  const auto hit_test_data_2 = hit_test_data_provider()->GetHitTestData();

  Window* expected_order_2[] = {window2(), window3(), window4()};
  EXPECT_EQ(hit_test_data_2->regions.size(), arraysize(expected_order_2));
  i = 0;
  for (const auto& region : hit_test_data_2->regions) {
    EXPECT_EQ(region->flags, viz::mojom::kHitTestMine |
                                 viz::mojom::kHitTestMouse |
                                 viz::mojom::kHitTestTouch);
    viz::SurfaceId surface_id(
        WindowPortMus::Get(expected_order_2[i])->frame_sink_id(),
        WindowMus::Get(expected_order_2[i])->GetLocalSurfaceId());
    EXPECT_EQ(region->surface_id, surface_id);
    EXPECT_EQ(region->rect.ToString(),
              expected_order_2[i]->bounds().ToString());
    i++;
  }
}

// Tests that the hit-test regions get expanded with a custom event targeter.
TEST_F(HitTestDataProviderAuraTest, CustomTargeter) {
  window3()->SetEventTargeter(base::MakeUnique<TestWindowTargeter>());
  const auto hit_test_data = hit_test_data_provider()->GetHitTestData();

  // Children of a container that has the custom targeter installed will get
  // reported twice, once with hit-test bounds optimized for mouse events and
  // another time with bounds expanded more for touch input.
  Window* expected_windows[] = {window3(), window4(), window4(), window2()};
  uint32_t expected_flags[] = {
      viz::mojom::kHitTestMine | viz::mojom::kHitTestMouse |
          viz::mojom::kHitTestTouch,
      viz::mojom::kHitTestMine | viz::mojom::kHitTestMouse,
      viz::mojom::kHitTestMine | viz::mojom::kHitTestTouch,
      viz::mojom::kHitTestMine | viz::mojom::kHitTestMouse |
          viz::mojom::kHitTestTouch};
  int expected_insets[] = {0, kMouseInset, kTouchInset, 0};
  ASSERT_EQ(hit_test_data->regions.size(), arraysize(expected_windows));
  ASSERT_EQ(hit_test_data->regions.size(), arraysize(expected_flags));
  ASSERT_EQ(hit_test_data->regions.size(), arraysize(expected_insets));
  int i = 0;
  for (const auto& region : hit_test_data->regions) {
    viz::SurfaceId surface_id(
        WindowPortMus::Get(expected_windows[i])->frame_sink_id(),
        WindowMus::Get(expected_windows[i])->GetLocalSurfaceId());
    EXPECT_EQ(region->surface_id, surface_id);
    EXPECT_EQ(region->flags, expected_flags[i]);
    gfx::Rect expected_bounds = expected_windows[i]->bounds();
    expected_bounds.Inset(gfx::Insets(expected_insets[i]));
    EXPECT_EQ(region->rect.ToString(), expected_bounds.ToString());
    i++;
  }
}

// Tests that the complex hit-test shape can be set with a custom targeter.
TEST_F(HitTestDataProviderAuraTest, HoleTargeter) {
  window3()->SetEventTargeter(base::MakeUnique<TestHoleWindowTargeter>());
  const auto hit_test_data = hit_test_data_provider()->GetHitTestData();

  // Children of a container that has the custom targeter installed will get
  // reported 4 times for each of the hit test regions defined by the custom
  // targeter.
  Window* expected_windows[] = {window3(), window4(), window4(),
                                window4(), window4(), window2()};
  uint32_t expected_flags = viz::mojom::kHitTestMine |
                            viz::mojom::kHitTestMouse |
                            viz::mojom::kHitTestTouch;
  std::vector<gfx::Rect> expected_bounds;
  expected_bounds.push_back(window3()->bounds());

  // original window4 is at gfx::Rect(20, 10, 60, 30).
  expected_bounds.emplace_back(20, 10, 60, 10);
  expected_bounds.emplace_back(20, 20, 20, 10);
  expected_bounds.emplace_back(60, 20, 20, 10);
  expected_bounds.emplace_back(20, 30, 60, 10);

  expected_bounds.push_back(window2()->bounds());

  ASSERT_EQ(hit_test_data->regions.size(), arraysize(expected_windows));
  ASSERT_EQ(hit_test_data->regions.size(), expected_bounds.size());
  int i = 0;
  for (const auto& region : hit_test_data->regions) {
    viz::SurfaceId surface_id(
        WindowPortMus::Get(expected_windows[i])->frame_sink_id(),
        WindowMus::Get(expected_windows[i])->GetLocalSurfaceId());
    EXPECT_EQ(region->surface_id, surface_id);
    EXPECT_EQ(region->flags, expected_flags);
    EXPECT_EQ(region->rect.ToString(), expected_bounds[i].ToString());
    i++;
  }
}

}  // namespace aura
