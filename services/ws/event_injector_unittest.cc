// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/event_injector.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "services/ws/event_injector.h"
#include "services/ws/injected_event_handler.h"
#include "services/ws/window_delegate_impl.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_test_helper.h"
#include "services/ws/window_service_test_setup.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_source.h"
#include "ui/events/test/test_event_rewriter.h"

namespace ws {
namespace {

std::unique_ptr<ui::Event> CreateMouseTestEvent() {
  return std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::Point(100, 100), gfx::Point(100, 100),
      base::TimeTicks::Now(), ui::EF_NONE, ui::EF_NONE);
}

std::unique_ptr<ui::Event> CreateTouchTestEvent() {
  return std::make_unique<ui::TouchEvent>(
      ui::ET_TOUCH_PRESSED, gfx::Point(100, 100), base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
}

gfx::Transform GetPrimaryDisplayRotationTransform(
    const display::Display::Rotation& rotation) {
  gfx::Transform rotate_transform;
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  switch (rotation) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      rotate_transform.Translate(display.bounds().height(), 0);
      rotate_transform.Rotate(90);
      break;
    case display::Display::ROTATE_180:
      rotate_transform.Translate(display.bounds().width(),
                                 display.bounds().height());
      rotate_transform.Rotate(180);
      break;
    case display::Display::ROTATE_270:
      rotate_transform.Translate(0, display.bounds().width());
      rotate_transform.Rotate(270);
      break;
  }
  return rotate_transform;
}

}  // namespace

TEST(EventInjectorTest, NoAck) {
  WindowServiceTestSetup test_setup;
  test_setup.service()->OnStart();
  auto* event_source = test_setup.root()->GetHost()->GetEventSource();
  ui::test::TestEventRewriter test_event_rewriter;
  event_source->AddEventRewriter(&test_event_rewriter);

  EventInjector* event_injector =
      WindowServiceTestHelper(test_setup.service()).event_injector();
  mojom::EventInjector* mojom_event_injector =
      static_cast<mojom::EventInjector*>(event_injector);
  auto display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  mojom_event_injector->InjectEventNoAck(display_id, CreateMouseTestEvent());
  EXPECT_EQ(1, test_event_rewriter.events_seen());
  EXPECT_FALSE(event_injector->HasQueuedEvents());
  test_event_rewriter.clear_events_seen();

  // Repeat, using the API that circumvents rewriters.
  mojom_event_injector->InjectEventNoAckNoRewriters(display_id,
                                                    CreateMouseTestEvent());
  EXPECT_EQ(0, test_event_rewriter.events_seen());
  EXPECT_FALSE(event_injector->HasQueuedEvents());
  event_source->RemoveEventRewriter(&test_event_rewriter);
}

// Test that the event's coordinates are set correctly when the screen is
// rotated at a degree of 90. 180 and 270.
TEST(EventInjectorTest, EventLocationRootTransformRotation) {
  WindowServiceTestSetup test_setup;
  test_setup.service()->OnStart();
  auto* event_source = test_setup.root()->GetHost()->GetEventSource();
  ui::test::TestEventRewriter test_event_rewriter;
  event_source->AddEventRewriter(&test_event_rewriter);

  EventInjector* event_injector =
      WindowServiceTestHelper(test_setup.service()).event_injector();
  mojom::EventInjector* mojom_event_injector =
      static_cast<mojom::EventInjector*>(event_injector);

  auto display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  // Inject a touch press event without rotating the screen.
  mojom_event_injector->InjectEventNoAck(display_id, CreateTouchTestEvent());
  EXPECT_EQ(1, test_event_rewriter.events_seen());
  const ui::Event* last_event = test_event_rewriter.last_event();
  ASSERT_TRUE(last_event);
  gfx::Point location(100, 100);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, last_event->type());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->location());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->root_location());
  test_event_rewriter.ResetLastEvent();

  // Set the screen rotation degreen to 90 and inject a touch press event.
  test_setup.root()->GetHost()->SetRootTransform(
      GetPrimaryDisplayRotationTransform(display::Display::ROTATE_90));
  mojom_event_injector->InjectEventNoAck(display_id, CreateTouchTestEvent());
  EXPECT_EQ(2, test_event_rewriter.events_seen());
  last_event = test_event_rewriter.last_event();
  ASSERT_TRUE(last_event);
  location = gfx::Point(500, 100);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, last_event->type());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->location());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->root_location());
  test_event_rewriter.ResetLastEvent();

  // Set the screen rotation degreen to 180 and inject a touch press event.
  test_setup.root()->GetHost()->SetRootTransform(
      GetPrimaryDisplayRotationTransform(display::Display::ROTATE_180));
  mojom_event_injector->InjectEventNoAck(display_id, CreateTouchTestEvent());
  EXPECT_EQ(3, test_event_rewriter.events_seen());
  last_event = test_event_rewriter.last_event();
  ASSERT_TRUE(last_event);
  location = gfx::Point(500, 700);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, last_event->type());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->location());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->root_location());
  test_event_rewriter.ResetLastEvent();

  // Set the screen rotation degreen to 270 and inject a mouse press event.
  test_setup.root()->GetHost()->SetRootTransform(
      GetPrimaryDisplayRotationTransform(display::Display::ROTATE_270));
  mojom_event_injector->InjectEventNoAck(display_id, CreateMouseTestEvent());
  EXPECT_EQ(4, test_event_rewriter.events_seen());
  last_event = test_event_rewriter.last_event();
  ASSERT_TRUE(last_event);
  location = gfx::Point(100, 700);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, last_event->type());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->location());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->root_location());
  test_event_rewriter.ResetLastEvent();
  event_source->RemoveEventRewriter(&test_event_rewriter);
}

// Test that the event's coordinates are set correctly when the screen is
// rotated at a degree of 90 and applied a scale factor of 2.
TEST(EventInjectorTest, EventLocationRootTransformRotationScale) {
  WindowServiceTestSetup test_setup;
  test_setup.service()->OnStart();
  auto* event_source = test_setup.root()->GetHost()->GetEventSource();
  ui::test::TestEventRewriter test_event_rewriter;
  event_source->AddEventRewriter(&test_event_rewriter);

  EventInjector* event_injector =
      WindowServiceTestHelper(test_setup.service()).event_injector();
  mojom::EventInjector* mojom_event_injector =
      static_cast<mojom::EventInjector*>(event_injector);

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();

  // Inject a touch press event without rotating the screen and scaling.
  mojom_event_injector->InjectEventNoAck(display.id(), CreateTouchTestEvent());
  EXPECT_EQ(1, test_event_rewriter.events_seen());
  const ui::Event* last_event = test_event_rewriter.last_event();
  ASSERT_TRUE(last_event);
  gfx::Point location(100, 100);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, last_event->type());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->location());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->root_location());
  test_event_rewriter.ResetLastEvent();

  // Inject a touch press event when the screen rotation degreen is 90 and the
  // scale factor is 2.
  gfx::Transform transform;
  transform.Scale(2, 2);
  transform.Translate(display.bounds().height(), 0);
  transform.Rotate(90);
  test_setup.root()->GetHost()->SetRootTransform(transform);
  mojom_event_injector->InjectEventNoAck(display.id(), CreateTouchTestEvent());
  EXPECT_EQ(2, test_event_rewriter.events_seen());
  last_event = test_event_rewriter.last_event();
  ASSERT_TRUE(last_event);
  location = gfx::Point(1000, 200);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, last_event->type());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->location());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->root_location());
  test_event_rewriter.ResetLastEvent();
  event_source->RemoveEventRewriter(&test_event_rewriter);
}

}  // namespace ws
