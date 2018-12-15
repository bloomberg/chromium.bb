// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/injected_event_handler.h"

#include <memory>

#include "services/ws/event_injector.h"
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

std::unique_ptr<ui::Event> CreateTestEvent() {
  return std::make_unique<ui::MouseEvent>(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                          gfx::Point(), base::TimeTicks::Now(),
                                          ui::EF_NONE, ui::EF_NONE);
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
  mojom_event_injector->InjectEventNoAck(display_id, CreateTestEvent());
  EXPECT_EQ(1, test_event_rewriter.events_seen());
  EXPECT_FALSE(event_injector->HasQueuedEvents());
  test_event_rewriter.clear_events_seen();

  // Repeat, using the API that circumvents rewriters.
  mojom_event_injector->InjectEventNoAckNoRewriters(display_id,
                                                    CreateTestEvent());
  EXPECT_EQ(0, test_event_rewriter.events_seen());
  EXPECT_FALSE(event_injector->HasQueuedEvents());
  event_source->RemoveEventRewriter(&test_event_rewriter);
}

}  // namespace ws
