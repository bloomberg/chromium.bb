// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service_client.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <queue>

#include "base/test/scoped_task_environment.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/ws2/gpu_support.h"
#include "services/ui/ws2/test_window_service_delegate.h"
#include "services/ui/ws2/test_window_tree_client.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_client_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace ui {
namespace ws2 {
namespace {

// Sets up state needed for WindowService tests.
class WindowServiceTestHelper {
 public:
  WindowServiceTestHelper() {
    if (gl::GetGLImplementation() == gl::kGLImplementationNone)
      gl::GLSurfaceTestSupport::InitializeOneOff();

    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;
    const bool enable_pixel_output = false;
    ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                         &context_factory_private);
    aura_test_helper_.SetUp(context_factory, context_factory_private);
    service_ = std::make_unique<WindowService>(&delegate_, nullptr);
    delegate_.set_top_level_parent(aura_test_helper_.root_window());

    const bool intercepts_events = false;
    window_service_client_ = service_->CreateWindowServiceClient(
        &window_tree_client_, intercepts_events);
    window_service_client_->InitFromFactory();
    helper_ = std::make_unique<WindowServiceClientTestHelper>(
        window_service_client_.get());
  }

  ~WindowServiceTestHelper() {
    window_service_client_.reset();
    service_.reset();
    aura_test_helper_.TearDown();
    ui::TerminateContextFactoryForTests();
  }

  aura::Window* root() { return aura_test_helper_.root_window(); }
  TestWindowServiceDelegate* delegate() { return &delegate_; }
  TestWindowTreeClient* window_tree_client() { return &window_tree_client_; }
  WindowServiceClientTestHelper* helper() { return helper_.get(); }

  std::vector<Change>* changes() {
    return window_tree_client_.tracker()->changes();
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI};
  aura::test::AuraTestHelper aura_test_helper_;
  TestWindowServiceDelegate delegate_;
  std::unique_ptr<WindowService> service_;
  TestWindowTreeClient window_tree_client_;
  std::unique_ptr<WindowServiceClient> window_service_client_;
  std::unique_ptr<WindowServiceClientTestHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceTestHelper);
};

class TestLayoutManager : public aura::LayoutManager {
 public:
  TestLayoutManager() = default;
  ~TestLayoutManager() override = default;

  void set_next_bounds(const gfx::Rect& bounds) { next_bounds_ = bounds; }

  // aura::LayoutManager:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    if (next_bounds_) {
      SetChildBoundsDirect(child, *next_bounds_);
      next_bounds_.reset();
    } else {
      SetChildBoundsDirect(child, requested_bounds);
    }
  }

 private:
  base::Optional<gfx::Rect> next_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

TEST(WindowServiceClientTest, NewWindow) {
  WindowServiceTestHelper helper;
  EXPECT_TRUE(helper.changes()->empty());
  aura::Window* top_level = helper.helper()->NewWindow(1);
  ASSERT_TRUE(top_level);
  EXPECT_EQ("ChangeCompleted id=1 sucess=true",
            SingleChangeToDescription(*helper.changes()));
  helper.changes()->clear();
}

TEST(WindowServiceClientTest, NewWindowWithProperties) {
  WindowServiceTestHelper helper;
  EXPECT_TRUE(helper.changes()->empty());
  aura::PropertyConverter::PrimitiveType value = true;
  std::vector<uint8_t> transport = mojo::ConvertTo<std::vector<uint8_t>>(value);
  aura::Window* top_level = helper.helper()->NewWindow(
      1, {{ui::mojom::WindowManager::kAlwaysOnTop_Property, transport}});
  ASSERT_TRUE(top_level);
  EXPECT_EQ("ChangeCompleted id=1 sucess=true",
            SingleChangeToDescription(*helper.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  helper.changes()->clear();
}

TEST(WindowServiceClientTest, NewTopLevelWindow) {
  WindowServiceTestHelper helper;
  EXPECT_TRUE(helper.changes()->empty());
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  ASSERT_TRUE(top_level);
  EXPECT_EQ("TopLevelCreated id=1 window_id=0,1 drawn=false",
            SingleChangeToDescription(*helper.changes()));
  helper.changes()->clear();
}

TEST(WindowServiceClientTest, NewTopLevelWindowWithProperties) {
  WindowServiceTestHelper helper;
  EXPECT_TRUE(helper.changes()->empty());
  aura::PropertyConverter::PrimitiveType value = true;
  std::vector<uint8_t> transport = mojo::ConvertTo<std::vector<uint8_t>>(value);
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(
      1, {{ui::mojom::WindowManager::kAlwaysOnTop_Property, transport}});
  ASSERT_TRUE(top_level);
  EXPECT_EQ("TopLevelCreated id=1 window_id=0,1 drawn=false",
            SingleChangeToDescription(*helper.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  helper.changes()->clear();
}

TEST(WindowServiceClientTest, SetWindowBounds) {
  WindowServiceTestHelper helper;
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  helper.changes()->clear();

  const gfx::Rect bounds_from_client = gfx::Rect(1, 2, 300, 400);
  helper.helper()->SetWindowBounds(top_level, bounds_from_client, 2);
  EXPECT_EQ(bounds_from_client, top_level->bounds());
  EXPECT_EQ("ChangeCompleted id=2 sucess=true",
            SingleChangeToDescription(*helper.changes()));
  helper.changes()->clear();

  const gfx::Rect bounds_from_server = gfx::Rect(101, 102, 103, 104);
  top_level->SetBounds(bounds_from_server);
  ASSERT_EQ(1u, helper.changes()->size());
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, (*helper.changes())[0].type);
  EXPECT_EQ(bounds_from_server, (*helper.changes())[0].bounds2);
  helper.changes()->clear();

  // Set a LayoutManager so that when the client requests a bounds change the
  // window is resized to a different bounds.
  // |layout_manager| is owned by top_level->parent();
  TestLayoutManager* layout_manager = new TestLayoutManager();
  const gfx::Rect restricted_bounds = gfx::Rect(401, 405, 406, 407);
  layout_manager->set_next_bounds(restricted_bounds);
  top_level->parent()->SetLayoutManager(layout_manager);
  helper.helper()->SetWindowBounds(top_level, bounds_from_client, 3);
  ASSERT_EQ(2u, helper.changes()->size());
  // The layout manager changes the bounds to a different value than the client
  // requested, so the client should get OnWindowBoundsChanged() with
  // |restricted_bounds|.
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, (*helper.changes())[0].type);
  EXPECT_EQ(restricted_bounds, (*helper.changes())[0].bounds2);

  // And because the layout manager changed the bounds the result is false.
  EXPECT_EQ("ChangeCompleted id=3 sucess=false",
            ChangeToDescription((*helper.changes())[1]));
  helper.changes()->clear();
}

TEST(WindowServiceClientTest, SetWindowProperty) {
  WindowServiceTestHelper helper;
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  helper.changes()->clear();

  EXPECT_FALSE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  aura::PropertyConverter::PrimitiveType value = true;
  std::vector<uint8_t> transport = mojo::ConvertTo<std::vector<uint8_t>>(value);
  helper.helper()->SetWindowProperty(
      top_level, ui::mojom::WindowManager::kAlwaysOnTop_Property, transport, 2);
  EXPECT_EQ("ChangeCompleted id=2 sucess=true",
            SingleChangeToDescription(*helper.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  helper.changes()->clear();
}

TEST(WindowServiceClientTest, PointerWatcher) {
  WindowServiceTestHelper helper;
  TestWindowTreeClient* window_tree_client = helper.window_tree_client();
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  ASSERT_TRUE(top_level);
  helper.helper()->SetEventTargetingPolicy(top_level,
                                           mojom::EventTargetingPolicy::NONE);
  EXPECT_EQ(mojom::EventTargetingPolicy::NONE,
            top_level->event_targeting_policy());
  // Start the pointer watcher only for pointer down/up.
  helper.helper()->window_tree()->StartPointerWatcher(false);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  test::EventGenerator event_generator(helper.root());
  event_generator.MoveMouseTo(50, 50);
  ASSERT_TRUE(window_tree_client->observed_pointer_events().empty());

  event_generator.MoveMouseTo(5, 6);
  ASSERT_TRUE(window_tree_client->observed_pointer_events().empty());

  event_generator.PressLeftButton();
  {
    ASSERT_EQ(1u, window_tree_client->observed_pointer_events().size());
    auto event = std::move(window_tree_client->PopObservedPointerEvent().event);
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_DOWN, event->type());
    EXPECT_EQ(gfx::Point(5, 6), event->AsLocatedEvent()->location());
  }

  event_generator.ReleaseLeftButton();
  {
    ASSERT_EQ(1u, window_tree_client->observed_pointer_events().size());
    auto event = std::move(window_tree_client->PopObservedPointerEvent().event);
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_UP, event->type());
    EXPECT_EQ(gfx::Point(5, 6), event->AsLocatedEvent()->location());
  }

  // Enable observing move events.
  helper.helper()->window_tree()->StartPointerWatcher(true);
  event_generator.MoveMouseTo(8, 9);
  {
    ASSERT_EQ(1u, window_tree_client->observed_pointer_events().size());
    auto event = std::move(window_tree_client->PopObservedPointerEvent().event);
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_MOVED, event->type());
    EXPECT_EQ(gfx::Point(8, 9), event->AsLocatedEvent()->location());
  }

  const int kTouchId = 11;
  event_generator.MoveTouchId(gfx::Point(2, 3), kTouchId);
  {
    ASSERT_EQ(1u, window_tree_client->observed_pointer_events().size());
    auto event = std::move(window_tree_client->PopObservedPointerEvent().event);
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_MOVED, event->type());
    EXPECT_EQ(gfx::Point(2, 3), event->AsLocatedEvent()->location());
  }
}

}  // namespace
}  // namespace ws2
}  // namespace ui
