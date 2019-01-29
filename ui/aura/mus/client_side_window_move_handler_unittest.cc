// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/window_tree_client_test_observer.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace aura {

namespace {

const int kBorderThickness = 5;
const int kClientAreaRadius = 10;

class WindowMoveTestDelegate : public test::TestWindowDelegate {
 public:
  WindowMoveTestDelegate() = default;
  ~WindowMoveTestDelegate() override = default;

  void set_window(Window* window) { window_ = window; }

 private:
  int GetNonClientComponent(const gfx::Point& point) const override {
    gfx::Size size = window_->bounds().size();
    if (point.y() < kBorderThickness) {
      if (point.x() < kBorderThickness)
        return HTTOPLEFT;
      if (point.x() > size.width() - kBorderThickness)
        return HTTOPRIGHT;
      return HTTOP;
    }
    if (point.y() > size.height() - kBorderThickness) {
      if (point.x() < kBorderThickness)
        return HTBOTTOMLEFT;
      if (point.x() > size.width() - kBorderThickness)
        return HTBOTTOMRIGHT;
      return HTBOTTOM;
    }
    if (point.x() < kBorderThickness)
      return HTLEFT;
    if (point.x() > size.width() - kBorderThickness)
      return HTRIGHT;

    if ((point - (gfx::Rect(size).CenterPoint())).LengthSquared() <
        kClientAreaRadius * kClientAreaRadius) {
      return HTCLIENT;
    }
    return HTCAPTION;
  }

  Window* window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowMoveTestDelegate);
};

class WindowMoveObserver : public WindowTreeClientTestObserver {
 public:
  WindowMoveObserver(WindowTreeClient* client) : client_(client) {
    client_->AddTestObserver(this);
  }
  ~WindowMoveObserver() override { client_->RemoveTestObserver(this); }

  bool in_window_move() const { return in_window_move_; }

 private:
  // WindowTreeClientTestObserver:
  void OnChangeStarted(uint32_t change_id, ChangeType type) override {
    if (type == ChangeType::MOVE_LOOP)
      in_window_move_ = true;
  }
  void OnChangeCompleted(uint32_t change_id,
                         ChangeType type,
                         bool success) override {
    if (type == ChangeType::MOVE_LOOP)
      in_window_move_ = false;
  }

  WindowTreeClient* client_;
  bool in_window_move_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowMoveObserver);
};

}  // namespace

class ClientSideWindowMoveHandlerTest
    : public test::AuraMusClientTestBase,
      public ::testing::WithParamInterface<const char*> {
 public:
  ClientSideWindowMoveHandlerTest() = default;
  ~ClientSideWindowMoveHandlerTest() override = default;

  void SetUp() override {
    test::AuraMusClientTestBase::SetUp();
    test_window_.reset(CreateNormalWindow(10, root_window(), &test_delegate_));
    test_delegate_.set_window(test_window_.get());
    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(root_window());
  }

  void TearDown() override {
    test_window_.reset();
    test_delegate_.set_window(nullptr);
    event_generator_.reset();
    test::AuraMusClientTestBase::TearDown();
  }

 protected:
  bool IsInputMouse() { return GetParam() == std::string("mouse"); }

  void MoveInputTo(const gfx::Point& point) {
    if (IsInputMouse())
      event_generator_->MoveMouseTo(point);
    else
      event_generator_->MoveTouch(point);
  }

  void MoveInputBy(int x, int y) {
    if (IsInputMouse())
      event_generator_->MoveMouseBy(x, y);
    else
      event_generator_->MoveTouchBy(x, y);
  }

  void PressInput() {
    if (IsInputMouse())
      event_generator_->PressLeftButton();
    else
      event_generator_->PressTouch();
  }

  gfx::Rect GetWindowBounds() { return test_window_->GetBoundsInScreen(); }

 private:
  WindowMoveTestDelegate test_delegate_;
  std::unique_ptr<Window> test_window_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideWindowMoveHandlerTest);
};

TEST_P(ClientSideWindowMoveHandlerTest, ResizeShadow) {
  if (!IsInputMouse())
    return;

  gfx::Rect bounds = GetWindowBounds();
  MoveInputTo(bounds.origin());

  EXPECT_EQ(1u, window_tree()->get_and_clear_window_resize_shadow_count());
  EXPECT_EQ(HTTOPLEFT, window_tree()->last_window_resize_shadow());

  MoveInputBy(1, 1);
  EXPECT_EQ(0u, window_tree()->get_and_clear_window_resize_shadow_count());

  MoveInputBy(10, 0);
  EXPECT_EQ(1u, window_tree()->get_and_clear_window_resize_shadow_count());
  EXPECT_EQ(HTTOP, window_tree()->last_window_resize_shadow());

  MoveInputBy(0, 10);
  EXPECT_EQ(1u, window_tree()->get_and_clear_window_resize_shadow_count());
  EXPECT_EQ(HTNOWHERE, window_tree()->last_window_resize_shadow());

  MoveInputTo(bounds.CenterPoint());
  EXPECT_EQ(0u, window_tree()->get_and_clear_window_resize_shadow_count());

  // Moves the input outside of the window bounds.
  MoveInputTo(bounds.bottom_right() + gfx::Vector2d(-2, -2));
  EXPECT_EQ(1u, window_tree()->get_and_clear_window_resize_shadow_count());
  EXPECT_EQ(HTBOTTOMRIGHT, window_tree()->last_window_resize_shadow());

  MoveInputBy(10, 10);
  EXPECT_EQ(1u, window_tree()->get_and_clear_window_resize_shadow_count());
  EXPECT_EQ(HTNOWHERE, window_tree()->last_window_resize_shadow());
}

TEST_P(ClientSideWindowMoveHandlerTest, PerformWindowMove) {
  WindowMoveObserver observer(window_tree_client_impl());

  MoveInputTo(GetWindowBounds().origin() + gfx::Vector2d(10, 10));

  PressInput();
  MoveInputBy(10, 10);
  EXPECT_TRUE(observer.in_window_move());
  EXPECT_EQ(HTCAPTION, window_tree()->last_move_hit_test());
  window_tree()->AckAllChanges();
  EXPECT_FALSE(observer.in_window_move());
}

TEST_P(ClientSideWindowMoveHandlerTest, WindowResize) {
  WindowMoveObserver observer(window_tree_client_impl());

  MoveInputTo(GetWindowBounds().bottom_right() + gfx::Vector2d(-3, -3));

  PressInput();
  MoveInputBy(-10, -10);
  EXPECT_TRUE(observer.in_window_move());
  EXPECT_EQ(HTBOTTOMRIGHT, window_tree()->last_move_hit_test());
  window_tree()->AckAllChanges();
  EXPECT_FALSE(observer.in_window_move());
}

TEST_P(ClientSideWindowMoveHandlerTest, ClientAreaDontStartMove) {
  WindowMoveObserver observer(window_tree_client_impl());

  MoveInputTo(GetWindowBounds().CenterPoint());

  PressInput();
  MoveInputBy(20, 20);
  EXPECT_FALSE(observer.in_window_move());
  window_tree()->AckAllChanges();
}

INSTANTIATE_TEST_CASE_P(,
                        ClientSideWindowMoveHandlerTest,
                        ::testing::Values("mouse", "touch"));
}  // namespace aura
