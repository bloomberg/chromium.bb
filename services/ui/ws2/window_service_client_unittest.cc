// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service_client.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "services/ui/ws2/gpu_support.h"
#include "services/ui/ws2/test_window_service_delegate.h"
#include "services/ui/ws2/test_window_tree_client.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_client_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace ui {
namespace ws2 {
namespace {

// Sets up state needed for WindowService tests.
class WindowServiceTestHelper {
 public:
  WindowServiceTestHelper() {
    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;
    const bool enable_pixel_output = false;
    gl::GLSurfaceTestSupport::InitializeOneOff();
    ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                         &context_factory_private);
    aura_test_helper_.SetUp(context_factory, context_factory_private);
    service_ = std::make_unique<WindowService>(&delegate_, nullptr);
    delegate_.set_top_level_parent(root());
  }
  ~WindowServiceTestHelper() {
    aura_test_helper_.TearDown();
    ui::TerminateContextFactoryForTests();
  }

  WindowService* service() { return service_.get(); }
  aura::Window* root() { return aura_test_helper_.root_window(); }

 private:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI};
  aura::test::AuraTestHelper aura_test_helper_;
  TestWindowServiceDelegate delegate_;
  std::unique_ptr<WindowService> service_;

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

TEST(WindowServiceClientTest, SetBounds) {
  WindowServiceTestHelper helper;
  TestWindowTreeClient window_tree_client;
  const bool intercepts_events = false;
  std::unique_ptr<WindowServiceClient> window_service_client =
      helper.service()->CreateWindowServiceClient(&window_tree_client,
                                                  intercepts_events);
  window_service_client->InitFromFactory();
  std::vector<Change>* changes = window_tree_client.tracker()->changes();
  EXPECT_TRUE(changes->empty());
  WindowServiceClientTestHelper window_service_client_helper(
      window_service_client.get());
  aura::Window* top_level = window_service_client_helper.NewTopLevelWindow(1);
  ASSERT_TRUE(top_level);
  EXPECT_EQ("TopLevelCreated id=1 window_id=0,1 drawn=false",
            SingleChangeToDescription(*changes));
  changes->clear();

  const gfx::Rect bounds_from_client = gfx::Rect(1, 2, 300, 400);
  window_service_client_helper.SetWindowBounds(top_level, bounds_from_client,
                                               2);
  EXPECT_EQ(bounds_from_client, top_level->bounds());
  EXPECT_EQ("ChangeCompleted id=2 sucess=true",
            SingleChangeToDescription(*changes));
  changes->clear();

  const gfx::Rect bounds_from_server = gfx::Rect(101, 102, 103, 104);
  top_level->SetBounds(bounds_from_server);
  ASSERT_EQ(1u, changes->size());
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, (*changes)[0].type);
  EXPECT_EQ(bounds_from_server, (*changes)[0].bounds2);
  changes->clear();

  // Set a LayoutManager so that when the client requests a bounds change the
  // window is resized to a different bounds.
  // |layout_manager| is owned by top_level->parent();
  TestLayoutManager* layout_manager = new TestLayoutManager();
  const gfx::Rect restricted_bounds = gfx::Rect(401, 405, 406, 407);
  layout_manager->set_next_bounds(restricted_bounds);
  top_level->parent()->SetLayoutManager(layout_manager);
  window_service_client_helper.SetWindowBounds(top_level, bounds_from_client,
                                               3);
  ASSERT_EQ(2u, changes->size());
  // The layout manager changes the bounds to a different value than the client
  // requested, so the client should get OnWindowBoundsChanged() with
  // |restricted_bounds|.
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, (*changes)[0].type);
  EXPECT_EQ(restricted_bounds, (*changes)[0].bounds2);

  // And because the layout manager changed the bounds the result is false.
  EXPECT_EQ("ChangeCompleted id=3 sucess=false",
            ChangeToDescription((*changes)[1]));
  changes->clear();
}

}  // namespace
}  // namespace ws2
}  // namespace ui
