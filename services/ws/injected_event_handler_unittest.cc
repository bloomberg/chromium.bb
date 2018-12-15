// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/injected_event_handler.h"

#include <memory>

#include "base/bind.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/window_event_dispatcher_test_api.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ws {

TEST(InjectedEventHandlerTest, SyncDispatch) {
  WindowServiceTestSetup test_setup;

  bool was_callback_run = false;
  InjectedEventHandler injected_event_handler(test_setup.service(),
                                              test_setup.root()->GetHost());

  // Inject the event, the callback should be run immediately as the location is
  // not over a client's window.
  std::unique_ptr<ui::Event> event = std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), base::TimeTicks::Now(),
      /* flags */ 0, /*changed_button_flags*/ 0);
  injected_event_handler.Inject(
      std::move(event),
      base::BindOnce([](bool* foo) { *foo = true; }, &was_callback_run));
  EXPECT_TRUE(was_callback_run);
}

TEST(InjectedEventHandlerTest, OverRemoteWindow) {
  WindowServiceTestSetup test_setup;
  aura::Window* top_level =
      test_setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();

  bool was_callback_run = false;
  InjectedEventHandler injected_event_handler(test_setup.service(),
                                              test_setup.root()->GetHost());

  // Inject the event, the callback should not be run immediately as the
  // location is over a client's window.
  std::unique_ptr<ui::Event> event = std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::Point(10, 10), gfx::Point(10, 10),
      base::TimeTicks::Now(),
      /* flags */ 0, /*changed_button_flags*/ 0);
  injected_event_handler.Inject(
      std::move(event), base::BindOnce([](bool* was_run) { *was_run = true; },
                                       &was_callback_run));
  EXPECT_FALSE(was_callback_run);

  // Ack the event, which should trigger running the callback.
  test_setup.window_tree_client()->AckFirstEvent(test_setup.window_tree(),
                                                 mojom::EventResult::HANDLED);
  EXPECT_TRUE(was_callback_run);
}

TEST(InjectedEventHandlerTest, WindowTreeHostDeletedWhileWaiting) {
  WindowServiceTestSetup test_setup;

  // Create a new WindowTreeHost.
  std::unique_ptr<aura::WindowTreeHost> second_host =
      aura::WindowTreeHost::Create(
          ui::PlatformWindowInitProperties{gfx::Rect(20, 30, 100, 50)});
  second_host->InitHost();
  second_host->window()->Show();

  // Create a top-level in |second_host|.
  test_setup.delegate()->set_top_level_parent(second_host->window());
  aura::Window* top_level =
      test_setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();

  // Send an event to the top-level.
  bool was_callback_run = false;
  InjectedEventHandler injected_event_handler(test_setup.service(),
                                              second_host.get());

  // Inject the event, the callback should not be run immediately as the
  // location is over a client's window.
  std::unique_ptr<ui::Event> event = std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::Point(10, 10), gfx::Point(10, 10),
      base::TimeTicks::Now(),
      /* flags */ 0, /*changed_button_flags*/ 0);
  injected_event_handler.Inject(
      std::move(event), base::BindOnce([](bool* was_run) { *was_run = true; },
                                       &was_callback_run));
  EXPECT_FALSE(was_callback_run);

  // Deleting the host should trigger notifying running the callback.
  second_host.reset();
  EXPECT_TRUE(was_callback_run);
}

TEST(InjectedEventHandlerTest, HeldEvent) {
  WindowServiceTestSetup test_setup;
  aura::Window* top_level =
      test_setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();

  bool was_callback_run = false;
  InjectedEventHandler injected_event_handler(test_setup.service(),
                                              test_setup.root()->GetHost());

  test_setup.root()->GetHost()->dispatcher()->HoldPointerMoves();

  // Inject the event, the callback should not be run immediately as moves are
  // being held (ET_MOUSE_DRAGGED triggers the event holding logic in
  // WindowEventDispatcher).
  std::unique_ptr<ui::Event> event = std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_DRAGGED, gfx::Point(10, 10), gfx::Point(10, 10),
      base::TimeTicks::Now(),
      /* flags */ 0, /*changed_button_flags*/ 0);
  injected_event_handler.Inject(
      std::move(event), base::BindOnce([](bool* was_run) { *was_run = true; },
                                       &was_callback_run));
  EXPECT_FALSE(was_callback_run);

  // Releasing the move should run the callback.
  test_setup.root()->GetHost()->dispatcher()->ReleasePointerMoves();
  aura::test::WindowEventDispatcherTestApi(
      test_setup.root()->GetHost()->dispatcher())
      .WaitUntilPointerMovesDispatched();
  EXPECT_TRUE(was_callback_run);
}

class DiscardingEventRewriter : public ui::EventRewriter {
 public:
  DiscardingEventRewriter() = default;
  ~DiscardingEventRewriter() override = default;

  bool got_rewrite_event() const { return got_rewrite_event_; }

  // ui::EventRewriter:
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event) override {
    got_rewrite_event_ = true;
    return ui::EVENT_REWRITE_DISCARD;
  }
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override {
    return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
  }

 private:
  bool got_rewrite_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(DiscardingEventRewriter);
};

TEST(InjectedEventHandlerTest, RewriterDiscards) {
  // Create a single window and give it focus.
  DiscardingEventRewriter rewriter;
  WindowServiceTestSetup test_setup;
  test_setup.aura_test_helper()->root_window()->GetHost()->AddEventRewriter(
      &rewriter);
  aura::Window* top_level =
      test_setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();
  top_level->Focus();
  EXPECT_TRUE(top_level->HasFocus());

  // Dispatch an event. Because the rewriter discards the event, the callback
  // should be run immediately.
  bool was_callback_run = false;
  InjectedEventHandler injected_event_handler(test_setup.service(),
                                              test_setup.root()->GetHost());

  ui::KeyEvent char_event('A', ui::VKEY_A, ui::DomCode::NONE, 0);
  injected_event_handler.Inject(
      ui::Event::Clone(char_event),
      base::BindOnce([](bool* was_run) { *was_run = true; },
                     &was_callback_run));
  EXPECT_TRUE(was_callback_run);
  EXPECT_TRUE(rewriter.got_rewrite_event());
}

}  // namespace ws
