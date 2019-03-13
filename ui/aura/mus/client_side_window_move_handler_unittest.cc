// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/window_tree_client_test_observer.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
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

  // Makes the window behave as if fullscreened: the entire window is HTCLIENT,
  // and the flag to enable drags from the top of the screen/window is enabled.
  void Fullscreen() {
    fullscreen_ = true;
    window_->GetRootWindow()->SetProperty(
        aura::client::kGestureDragFromClientAreaTopMovesWindow, true);
  }

  void set_window(Window* window) { window_ = window; }

 private:
  int GetNonClientComponent(const gfx::Point& point) const override {
    if (fullscreen_)
      return HTCLIENT;

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

  bool fullscreen_ = false;
  Window* window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowMoveTestDelegate);
};

class WindowMoveObserver : public WindowTreeClientTestObserver {
 public:
  WindowMoveObserver(WindowTreeClient* client) : client_(client) {
    client_->AddTestObserver(this);
  }
  ~WindowMoveObserver() override { client_->RemoveTestObserver(this); }

  bool in_window_move() const { return window_move_count_ > 0; }
  int window_move_count() const { return window_move_count_; }

 private:
  // WindowTreeClientTestObserver:
  void OnChangeStarted(uint32_t change_id, ChangeType type) override {
    if (type == ChangeType::MOVE_LOOP)
      window_move_count_++;
  }
  void OnChangeCompleted(uint32_t change_id,
                         ChangeType type,
                         bool success) override {
    if (type == ChangeType::MOVE_LOOP)
      window_move_count_--;
  }

  WindowTreeClient* client_;
  int window_move_count_ = 0;

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

  void ReleaseInput() {
    if (IsInputMouse())
      event_generator_->ReleaseLeftButton();
    else
      event_generator_->ReleaseTouch();
  }

  gfx::Rect GetWindowBounds() { return test_window_->GetBoundsInScreen(); }

  WindowMoveTestDelegate test_delegate_;
  std::unique_ptr<Window> test_window_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

 private:
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
  EXPECT_EQ(HTNOWHERE, window_tree()->last_window_resize_shadow());
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
  EXPECT_EQ(HTBOTTOMRIGHT, window_tree()->last_window_resize_shadow());
  window_tree()->AckAllChanges();
  EXPECT_FALSE(observer.in_window_move());
}

TEST_P(ClientSideWindowMoveHandlerTest, ClientAreaDoesntStartMove) {
  WindowMoveObserver observer(window_tree_client_impl());

  MoveInputTo(GetWindowBounds().CenterPoint());

  PressInput();
  MoveInputBy(20, 20);
  EXPECT_FALSE(observer.in_window_move());
  window_tree()->AckAllChanges();

  // Simulate fullscreen; the events still don't trigger a drag because the y
  // coordinate is too great.
  test_delegate_.Fullscreen();
  MoveInputTo(GetWindowBounds().CenterPoint());

  PressInput();
  MoveInputBy(20, 20);
  EXPECT_FALSE(observer.in_window_move());
  window_tree()->AckAllChanges();
}

TEST_P(ClientSideWindowMoveHandlerTest, ClientAreaCanStartMove) {
  WindowMoveObserver observer(window_tree_client_impl());
  test_delegate_.Fullscreen();

  MoveInputTo(GetWindowBounds().top_center());

  PressInput();
  MoveInputBy(0, 20);
  // The window will be moved for a gesture sequence but not a mouse sequence.
  EXPECT_NE(IsInputMouse(), observer.in_window_move());
  window_tree()->AckAllChanges();
}

TEST_P(ClientSideWindowMoveHandlerTest, MouseExitDoesNotCancelResize) {
  // Create another window-tree-host and sets the capture there.
  auto host2 = std::make_unique<WindowTreeHostMus>(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  host2->InitHost();
  gfx::Rect host2_bounds = host()->GetBoundsInPixels();
  host2_bounds.Offset(30, 30);
  static_cast<WindowTreeHost*>(host2.get())->SetBoundsInPixels(host2_bounds);
  host2->window()->Show();

  test::TestWindowDelegate test_delegate;
  std::unique_ptr<Window> window2(
      CreateNormalWindow(12, host2->window(), &test_delegate));
  window2->SetCapture();
  window_tree()->AckAllChanges();

  WindowMoveObserver observer(window_tree_client_impl());

  // Makes the mouse event to cause window resize; since |window2| has the
  // capture, this should cause MOUSE_EXITED on |window2|, but that shouldn't
  // affect the behavior of resizing on the target window.
  MoveInputTo(GetWindowBounds().origin() + gfx::Vector2d(1, 1));
  PressInput();
  MoveInputBy(-10, -10);
  EXPECT_TRUE(observer.in_window_move());
  EXPECT_EQ(HTTOPLEFT, window_tree()->last_move_hit_test());
  EXPECT_EQ(HTTOPLEFT, window_tree()->last_window_resize_shadow());
  window_tree()->AckAllChanges();
  EXPECT_FALSE(observer.in_window_move());
}

TEST_P(ClientSideWindowMoveHandlerTest,
       AdditionalEventsShouldntTriggerAnotherMove) {
  WindowMoveObserver observer(window_tree_client_impl());

  MoveInputTo(GetWindowBounds().origin() + gfx::Vector2d(10, 10));

  PressInput();
  MoveInputBy(10, 10);
  EXPECT_EQ(1, observer.window_move_count());
  EXPECT_EQ(HTCAPTION, window_tree()->last_move_hit_test());
  EXPECT_EQ(HTNOWHERE, window_tree()->last_window_resize_shadow());

  // Nothing should happen for additional move events.
  MoveInputBy(10, 10);
  EXPECT_EQ(1, observer.window_move_count());
  EXPECT_EQ(HTCAPTION, window_tree()->last_move_hit_test());
  EXPECT_EQ(HTNOWHERE, window_tree()->last_window_resize_shadow());
  window_tree()->AckAllChanges();
  EXPECT_FALSE(observer.in_window_move());
  ReleaseInput();

  // Restarting a move should start a new move.
  MoveInputTo(GetWindowBounds().origin() + gfx::Vector2d(10, 10));
  PressInput();
  MoveInputBy(10, 10);
  EXPECT_EQ(1, observer.window_move_count());
  EXPECT_EQ(HTCAPTION, window_tree()->last_move_hit_test());
  EXPECT_EQ(HTNOWHERE, window_tree()->last_window_resize_shadow());
  window_tree()->AckAllChanges();
  EXPECT_FALSE(observer.in_window_move());
}

INSTANTIATE_TEST_SUITE_P(,
                         ClientSideWindowMoveHandlerTest,
                         ::testing::Values("mouse", "touch"));
}  // namespace aura
