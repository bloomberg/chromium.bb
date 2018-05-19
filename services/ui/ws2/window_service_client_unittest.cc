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
#include "ui/aura/window_tracker.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/wm/core/capture_controller.h"

namespace ui {
namespace ws2 {
namespace {

// Embedding contains the object necessary for an embedding. This is created
// by way of WindowServiceTestHelper::CreateEmbededing().
//
// NOTE: destroying this object does not destroy the embedding.
struct Embedding {
  std::vector<Change>* changes() {
    return window_tree_client.tracker()->changes();
  }

  TestWindowTreeClient window_tree_client;

  // NOTE: this is owned by the WindowServiceClient that Embed() was called on.
  WindowServiceClient* window_service_client = nullptr;

  std::unique_ptr<WindowServiceClientTestHelper> helper;
};

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

  std::unique_ptr<Embedding> CreateEmbedding(aura::Window* embed_root,
                                             uint32_t flags = 0) {
    std::unique_ptr<Embedding> embedding = std::make_unique<Embedding>();
    embedding->window_service_client = helper_->Embed(
        embed_root, nullptr, &embedding->window_tree_client, flags);
    if (!embedding->window_service_client)
      return nullptr;
    embedding->helper = std::make_unique<WindowServiceClientTestHelper>(
        embedding->window_service_client);
    return embedding;
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
  aura::Window* window = helper.helper()->NewWindow(1);
  ASSERT_TRUE(window);
  EXPECT_EQ("ChangeCompleted id=1 sucess=true",
            SingleChangeToDescription(*helper.changes()));
}

TEST(WindowServiceClientTest, NewWindowWithProperties) {
  WindowServiceTestHelper helper;
  EXPECT_TRUE(helper.changes()->empty());
  aura::PropertyConverter::PrimitiveType value = true;
  std::vector<uint8_t> transport = mojo::ConvertTo<std::vector<uint8_t>>(value);
  aura::Window* window = helper.helper()->NewWindow(
      1, {{ui::mojom::WindowManager::kAlwaysOnTop_Property, transport}});
  ASSERT_TRUE(window);
  EXPECT_EQ("ChangeCompleted id=1 sucess=true",
            SingleChangeToDescription(*helper.changes()));
  EXPECT_TRUE(window->GetProperty(aura::client::kAlwaysOnTopKey));
}

TEST(WindowServiceClientTest, NewTopLevelWindow) {
  WindowServiceTestHelper helper;
  EXPECT_TRUE(helper.changes()->empty());
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  ASSERT_TRUE(top_level);
  EXPECT_EQ("TopLevelCreated id=1 window_id=0,1 drawn=false",
            SingleChangeToDescription(*helper.changes()));
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
}

TEST(WindowServiceClientTest, SetTopLevelWindowBounds) {
  WindowServiceTestHelper helper;
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  helper.changes()->clear();

  const gfx::Rect bounds_from_client = gfx::Rect(1, 2, 300, 400);
  helper.helper()->SetWindowBounds(top_level, bounds_from_client, 2);
  EXPECT_EQ(bounds_from_client, top_level->bounds());
  ASSERT_EQ(2u, helper.changes()->size());
  {
    const Change& change = (*helper.changes())[0];
    EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, change.type);
    EXPECT_EQ(top_level->bounds(), change.bounds2);
    EXPECT_TRUE(change.local_surface_id);
    helper.changes()->erase(helper.changes()->begin());
  }
  // See comments in WindowServiceClient::SetBoundsImpl() for why this returns
  // false.
  EXPECT_EQ("ChangeCompleted id=2 sucess=false",
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
}

// Tests the ability of the client to change properties on the server.
TEST(WindowServiceClientTest, SetTopLevelWindowProperty) {
  WindowServiceTestHelper helper;
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  helper.changes()->clear();

  EXPECT_FALSE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  aura::PropertyConverter::PrimitiveType client_value = true;
  std::vector<uint8_t> client_transport_value =
      mojo::ConvertTo<std::vector<uint8_t>>(client_value);
  helper.helper()->SetWindowProperty(
      top_level, ui::mojom::WindowManager::kAlwaysOnTop_Property,
      client_transport_value, 2);
  EXPECT_EQ("ChangeCompleted id=2 sucess=true",
            SingleChangeToDescription(*helper.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  helper.changes()->clear();

  top_level->SetProperty(aura::client::kAlwaysOnTopKey, false);
  EXPECT_EQ(
      "PropertyChanged window=0,1 key=prop:always_on_top "
      "value=0000000000000000",
      SingleChangeToDescription(*helper.changes()));
}

TEST(WindowServiceClientTest, WindowToWindowData) {
  WindowServiceTestHelper helper;
  aura::Window* window = helper.helper()->NewWindow(1);
  helper.changes()->clear();

  window->SetBounds(gfx::Rect(1, 2, 300, 400));
  window->SetProperty(aura::client::kAlwaysOnTopKey, true);
  window->Show();  // Called to make the window visible.
  mojom::WindowDataPtr data = helper.helper()->WindowToWindowData(window);
  EXPECT_EQ(gfx::Rect(1, 2, 300, 400), data->bounds);
  EXPECT_TRUE(data->visible);
  EXPECT_EQ(1u, data->properties.count(
                    ui::mojom::WindowManager::kAlwaysOnTop_Property));
  EXPECT_EQ(
      aura::PropertyConverter::PrimitiveType(true),
      mojo::ConvertTo<aura::PropertyConverter::PrimitiveType>(
          data->properties[ui::mojom::WindowManager::kAlwaysOnTop_Property]));
}

TEST(WindowServiceClientTest, MovePressDragRelease) {
  WindowServiceTestHelper helper;
  TestWindowTreeClient* window_tree_client = helper.window_tree_client();
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  test::EventGenerator event_generator(helper.root());
  {
    event_generator.MoveMouseTo(50, 50);
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_ENTERED, event->type());
    EXPECT_EQ(gfx::Point(40, 40), event->AsLocatedEvent()->location());
    event = window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_MOVED, event->type());
    EXPECT_EQ(gfx::Point(40, 40), event->AsLocatedEvent()->location());
  }

  {
    event_generator.PressLeftButton();
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_DOWN, event->type());
    EXPECT_EQ(gfx::Point(40, 40), event->AsLocatedEvent()->location());
  }

  {
    event_generator.MoveMouseTo(0, 0);
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_MOVED, event->type());
    EXPECT_EQ(gfx::Point(-10, -10), event->AsLocatedEvent()->location());
  }

  {
    event_generator.ReleaseLeftButton();
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_UP, event->type());
    EXPECT_EQ(gfx::Point(-10, -10), event->AsLocatedEvent()->location());
  }
}

class EventRecordingWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  EventRecordingWindowDelegate() = default;
  ~EventRecordingWindowDelegate() override = default;

  std::queue<std::unique_ptr<ui::Event>>& events() { return events_; }

  std::unique_ptr<Event> PopEvent() {
    if (events_.empty())
      return nullptr;
    auto event = std::move(events_.front());
    events_.pop();
    return event;
  }

  void ClearEvents() {
    std::queue<std::unique_ptr<ui::Event>> events;
    std::swap(events_, events);
  }

  // aura::test::TestWindowDelegate:
  void OnEvent(ui::Event* event) override {
    events_.push(ui::Event::Clone(*event));
  }

 private:
  std::queue<std::unique_ptr<ui::Event>> events_;

  DISALLOW_COPY_AND_ASSIGN(EventRecordingWindowDelegate);
};

TEST(WindowServiceClientTest, MoveFromClientToNonClient) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestHelper helper;
  TestWindowTreeClient* window_tree_client = helper.window_tree_client();
  helper.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(1);
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  helper.helper()->SetClientArea(top_level, gfx::Insets(10, 0, 0, 0));

  window_delegate.ClearEvents();

  test::EventGenerator event_generator(helper.root());
  {
    event_generator.MoveMouseTo(50, 50);
    // Move generates both an enter and move.
    std::unique_ptr<Event> enter_event =
        window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(enter_event);
    EXPECT_EQ(ET_POINTER_ENTERED, enter_event->type());
    EXPECT_EQ(gfx::Point(40, 40), enter_event->AsLocatedEvent()->location());
    std::unique_ptr<Event> move_event =
        window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(move_event);
    EXPECT_EQ(ET_POINTER_MOVED, move_event->type());
    EXPECT_EQ(gfx::Point(40, 40), move_event->AsLocatedEvent()->location());

    // The delegate should see the same events.
    ASSERT_EQ(2u, window_delegate.events().size());
    enter_event = window_delegate.PopEvent();
    EXPECT_EQ(ET_MOUSE_ENTERED, enter_event->type());
    EXPECT_EQ(gfx::Point(40, 40), enter_event->AsLocatedEvent()->location());
    move_event = window_delegate.PopEvent();
    EXPECT_EQ(ET_MOUSE_MOVED, move_event->type());
    EXPECT_EQ(gfx::Point(40, 40), move_event->AsLocatedEvent()->location());
  }

  // Move the mouse over the non-client area.
  // The event is still sent to the client, and the delegate.
  {
    event_generator.MoveMouseTo(15, 16);
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_MOVED, event->type());
    EXPECT_EQ(gfx::Point(5, 6), event->AsLocatedEvent()->location());

    // Delegate should also get the events.
    event = window_delegate.PopEvent();
    EXPECT_EQ(ET_MOUSE_MOVED, event->type());
    EXPECT_EQ(gfx::Point(5, 6), event->AsLocatedEvent()->location());
  }

  // Only the delegate should get the press in this case.
  {
    event_generator.PressLeftButton();
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_FALSE(event);

    event = window_delegate.PopEvent();
    EXPECT_EQ(ET_MOUSE_PRESSED, event->type());
    EXPECT_EQ(gfx::Point(5, 6), event->AsLocatedEvent()->location());
  }

  // Move mouse into client area, only the delegate should get the move (drag).
  {
    event_generator.MoveMouseTo(35, 51);
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_FALSE(event);

    event = window_delegate.PopEvent();
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_MOUSE_DRAGGED, event->type());
    EXPECT_EQ(gfx::Point(25, 41), event->AsLocatedEvent()->location());
  }

  // Release over client area, again only delegate should get it.
  {
    event_generator.ReleaseLeftButton();
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_FALSE(event);

    event = window_delegate.PopEvent();
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
  }

  {
    event_generator.MoveMouseTo(26, 50);
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_MOVED, event->type());
    EXPECT_EQ(gfx::Point(16, 40), event->AsLocatedEvent()->location());

    // Delegate should also get the events.
    event = window_delegate.PopEvent();
    EXPECT_EQ(ET_MOUSE_MOVED, event->type());
    EXPECT_EQ(gfx::Point(16, 40), event->AsLocatedEvent()->location());
  }

  // Press in client area. Only the client should get the event.
  {
    event_generator.PressLeftButton();
    std::unique_ptr<Event> event = window_tree_client->PopInputEvent().event;
    ASSERT_TRUE(event);
    EXPECT_EQ(ET_POINTER_DOWN, event->type());
    EXPECT_EQ(gfx::Point(16, 40), event->AsLocatedEvent()->location());

    event = window_delegate.PopEvent();
    ASSERT_FALSE(event);
  }
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

TEST(WindowServiceClientTest, Capture) {
  WindowServiceTestHelper helper;
  aura::Window* window = helper.helper()->NewWindow(1);

  // Setting capture on |window| should fail as it's not visible.
  EXPECT_FALSE(helper.helper()->SetCapture(window));

  aura::Window* top_level = helper.helper()->NewTopLevelWindow(2);
  ASSERT_TRUE(top_level);
  EXPECT_FALSE(helper.helper()->SetCapture(top_level));
  top_level->Show();
  EXPECT_TRUE(helper.helper()->SetCapture(top_level));

  EXPECT_FALSE(helper.helper()->ReleaseCapture(window));
  EXPECT_TRUE(helper.helper()->ReleaseCapture(top_level));

  top_level->AddChild(window);
  window->Show();
  EXPECT_TRUE(helper.helper()->SetCapture(window));
  EXPECT_TRUE(helper.helper()->ReleaseCapture(window));
}

TEST(WindowServiceClientTest, CaptureNotification) {
  WindowServiceTestHelper helper;
  aura::Window* window = helper.helper()->NewWindow(1);
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(2);
  top_level->AddChild(window);
  ASSERT_TRUE(top_level);
  top_level->Show();
  window->Show();
  helper.changes()->clear();
  EXPECT_TRUE(helper.helper()->SetCapture(window));
  EXPECT_TRUE(helper.changes()->empty());

  wm::CaptureController::Get()->ReleaseCapture(window);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(helper.changes())));
}

TEST(WindowServiceClientTest, CaptureNotificationForEmbedRoot) {
  WindowServiceTestHelper helper;
  aura::Window* window = helper.helper()->NewWindow(1);
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(2);
  top_level->AddChild(window);
  ASSERT_TRUE(top_level);
  top_level->Show();
  window->Show();
  helper.changes()->clear();
  EXPECT_TRUE(helper.helper()->SetCapture(window));
  EXPECT_TRUE(helper.changes()->empty());

  // Set capture on the embed-root from the embedded client. The embedder
  // should be notified.
  std::unique_ptr<Embedding> embedding = helper.CreateEmbedding(window);
  ASSERT_TRUE(embedding);
  helper.changes()->clear();
  embedding->changes()->clear();
  EXPECT_TRUE(embedding->helper->SetCapture(window));
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(helper.changes())));
  helper.changes()->clear();
  EXPECT_TRUE(embedding->changes()->empty());

  // Set capture from the embedder. This triggers the embedded client to lose
  // capture.
  EXPECT_TRUE(helper.helper()->SetCapture(window));
  EXPECT_TRUE(helper.changes()->empty());
  // NOTE: the '2' is because the embedded client sees the high order bits of
  // the root.
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=2,1",
            SingleChangeToDescription(*(embedding->changes())));
  embedding->changes()->clear();

  // And release capture locally.
  wm::CaptureController::Get()->ReleaseCapture(window);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(helper.changes())));
  EXPECT_TRUE(embedding->changes()->empty());
}

TEST(WindowServiceClientTest, CaptureNotificationForTopLevel) {
  WindowServiceTestHelper helper;
  aura::Window* top_level = helper.helper()->NewTopLevelWindow(11);
  ASSERT_TRUE(top_level);
  top_level->Show();
  helper.changes()->clear();
  EXPECT_TRUE(helper.helper()->SetCapture(top_level));
  EXPECT_TRUE(helper.changes()->empty());

  // Release capture locally.
  wm::CaptureController* capture_controller = wm::CaptureController::Get();
  capture_controller->ReleaseCapture(top_level);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,11",
            SingleChangeToDescription(*(helper.changes())));
  helper.changes()->clear();

  // Set capture locally.
  capture_controller->SetCapture(top_level);
  EXPECT_TRUE(helper.changes()->empty());

  // Set capture from client.
  EXPECT_TRUE(helper.helper()->SetCapture(top_level));
  EXPECT_TRUE(helper.changes()->empty());

  // Release locally.
  capture_controller->ReleaseCapture(top_level);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,11",
            SingleChangeToDescription(*(helper.changes())));
}

TEST(WindowServiceClientTest, DeleteWindow) {
  WindowServiceTestHelper helper;
  aura::Window* window = helper.helper()->NewWindow(1);
  ASSERT_TRUE(window);
  aura::WindowTracker tracker;
  tracker.Add(window);
  helper.changes()->clear();
  helper.helper()->DeleteWindow(window);
  EXPECT_TRUE(tracker.windows().empty());
  EXPECT_EQ("ChangeCompleted id=1 sucess=true",
            SingleChangeToDescription(*helper.changes()));
}

TEST(WindowServiceClientTest, ExternalDeleteWindow) {
  WindowServiceTestHelper helper;
  aura::Window* window = helper.helper()->NewWindow(1);
  ASSERT_TRUE(window);
  helper.changes()->clear();
  delete window;
  EXPECT_EQ("WindowDeleted window=0,1",
            SingleChangeToDescription(*helper.changes()));
}

}  // namespace
}  // namespace ws2
}  // namespace ui
