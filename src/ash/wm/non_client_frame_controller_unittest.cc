// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/non_client_frame_controller.h"

#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/wm/top_level_window_factory.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/test_change_tracker.h"
#include "services/ws/test_window_tree_client.h"
#include "services/ws/window_tree_test_helper.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"

namespace ash {

using NonClientFrameControllerTest = AshTestBase;

TEST_F(NonClientFrameControllerTest, CallsRequestClose) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  ASSERT_TRUE(non_client_frame_controller);
  non_client_frame_controller->GetWidget()->Close();
  // Close should not have been scheduled on the widget yet (because the request
  // goes to the remote client).
  EXPECT_FALSE(non_client_frame_controller->GetWidget()->IsClosed());
  auto* changes = GetTestWindowTreeClient()->tracker()->changes();
  ASSERT_FALSE(changes->empty());
  // The remote client should have a request to close the window.
  EXPECT_EQ("RequestClose", ws::ChangeToDescription(changes->back()));
}

TEST_F(NonClientFrameControllerTest, WindowTitle) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  ASSERT_TRUE(non_client_frame_controller);
  EXPECT_TRUE(non_client_frame_controller->ShouldShowWindowTitle());
  EXPECT_TRUE(non_client_frame_controller->GetWindowTitle().empty());

  // Verify GetWindowTitle() mirrors window->SetTitle().
  const base::string16 title = base::ASCIIToUTF16("X");
  window->SetTitle(title);
  EXPECT_EQ(title, non_client_frame_controller->GetWindowTitle());

  // ShouldShowWindowTitle() mirrors |aura::client::kTitleShownKey|.
  window->SetProperty(aura::client::kTitleShownKey, false);
  EXPECT_FALSE(non_client_frame_controller->ShouldShowWindowTitle());
}

TEST_F(NonClientFrameControllerTest, ExposesChildTreeIdToAccessibility) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  const std::string ax_tree_id = "123";
  window->SetProperty(ui::kChildAXTreeID, new std::string(ax_tree_id));
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  views::View* contents_view = non_client_frame_controller->GetContentsView();
  ui::AXNodeData ax_node_data;
  contents_view->GetAccessibleNodeData(&ax_node_data);
  EXPECT_EQ(ax_tree_id, ax_node_data.GetStringAttribute(
                            ax::mojom::StringAttribute::kChildTreeId));
  EXPECT_EQ(ax::mojom::Role::kClient, ax_node_data.role);
}

TEST_F(NonClientFrameControllerTest, HonorsMinimumSize) {
  const gfx::Size min_size(201, 302);
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  // |window| takes ownership of the new size.
  window->SetProperty(aura::client::kMinimumSize, new gfx::Size(min_size));
  ASSERT_TRUE(window->delegate());
  EXPECT_EQ(min_size, window->delegate()->GetMinimumSize());
}

TEST_F(NonClientFrameControllerTest, HonorsMinimumSizeWithoutFrame) {
  // Variant of HonorsMinimumSize that removes the standard frame (the client
  // draws the non-client area).
  using TransportType = std::vector<uint8_t>;
  const gfx::Size min_size(201, 302);
  auto properties = CreatePropertiesForProxyWindow();
  properties[ws::mojom::WindowManager::kMinimumSize_Property] =
      mojo::ConvertTo<TransportType>(min_size);
  properties[ws::mojom::WindowManager::kClientProvidesFrame_InitProperty] =
      mojo::ConvertTo<TransportType>(true);
  std::unique_ptr<aura::Window> window(
      GetWindowTreeTestHelper()->NewTopLevelWindow(
          mojo::MapToFlatMap(properties)));
  ASSERT_TRUE(window->delegate());
  EXPECT_EQ(min_size, window->delegate()->GetMinimumSize());
}

TEST_F(NonClientFrameControllerTest, NonClientAreaShouldBeDraggable) {
  using TransportType = std::vector<uint8_t>;
  auto properties = CreatePropertiesForProxyWindow();
  properties[ws::mojom::WindowManager::kClientProvidesFrame_InitProperty] =
      mojo::ConvertTo<TransportType>(true);
  std::unique_ptr<aura::Window> window(
      GetWindowTreeTestHelper()->NewTopLevelWindow(
          mojo::MapToFlatMap(properties)));

  const gfx::Point point(10, 10);
  EXPECT_EQ(HTCLIENT, window->delegate()->GetNonClientComponent(point));
  EXPECT_EQ(HTTOPLEFT,
            window->delegate()->GetNonClientComponent(gfx::Point(-1, -1)));

  std::vector<gfx::Rect> additional_areas = {
      gfx::Rect(window->bounds().width() - 20, 0, 20, 20)};
  GetWindowTreeTestHelper()->SetClientArea(
      window.get(), gfx::Insets(20, 20, 20, 20), additional_areas);
  EXPECT_EQ(HTCAPTION, window->delegate()->GetNonClientComponent(point));
  EXPECT_EQ(HTTOPLEFT,
            window->delegate()->GetNonClientComponent(gfx::Point(-1, -1)));
  EXPECT_EQ(HTCLIENT,
            window->delegate()->GetNonClientComponent(gfx::Point(30, 30)));
  EXPECT_EQ(HTCLIENT, window->delegate()->GetNonClientComponent(
                          gfx::Point(window->bounds().width() - 10, 10)));
}

using NonClientFrameControllerSingleProcessMashTest = SingleProcessMashTestBase;

// Used to track whether in a window resize loop.
class ResizeLoopWidgetDelegate : public views::WidgetDelegateView {
 public:
  ResizeLoopWidgetDelegate() = default;
  ~ResizeLoopWidgetDelegate() override = default;

  bool in_resize_loop() const { return in_resize_loop_; }

  // views::WidgetDelegateView:
  void OnWindowBeginUserBoundsChange() override {
    EXPECT_FALSE(in_resize_loop_);
    in_resize_loop_ = true;
  }
  void OnWindowEndUserBoundsChange() override {
    EXPECT_TRUE(in_resize_loop_);
    in_resize_loop_ = false;
  }
  int32_t GetResizeBehavior() const override {
    return ws::mojom::kResizeBehaviorCanResize;
  }

 private:
  bool in_resize_loop_ = false;

  DISALLOW_COPY_AND_ASSIGN(ResizeLoopWidgetDelegate);
};

TEST_F(NonClientFrameControllerSingleProcessMashTest, ResizeLoop) {
  // Owned by |widget|.
  ResizeLoopWidgetDelegate* widget_delegate = new ResizeLoopWidgetDelegate;
  // Create a widget. This widget is backed by mus.
  views::Widget widget;
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.delegate = widget_delegate;
  params.native_widget =
      views::MusClient::Get()->CreateNativeWidget(params, &widget);
  widget.Init(params);
  widget.Show();

  // Should not initially be in a resize loop.
  EXPECT_FALSE(widget_delegate->in_resize_loop());

  // Flush all messages from the WindowTreeClient to ensure ash processes the
  // widget creation.
  aura::test::WaitForAllChangesToComplete();

  // The resize loop is entered once a possible resize is detected.
  GetEventGenerator()->MoveMouseTo(gfx::Point(5, 199));
  GetEventGenerator()->PressLeftButton();
  aura::test::WaitForAllChangesToComplete();
  EXPECT_TRUE(widget_delegate->in_resize_loop());

  // Releasing the button ends the loop.
  GetEventGenerator()->ReleaseLeftButton();
  aura::test::WaitForAllChangesToComplete();
  EXPECT_FALSE(widget_delegate->in_resize_loop());
}

}  // namespace ash
