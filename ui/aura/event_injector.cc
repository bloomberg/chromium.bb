// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_injector.h"

#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace {
std::unique_ptr<ui::Event> MapEvent(const ui::Event& event) {
  if (event.IsScrollEvent()) {
    return std::make_unique<ui::PointerEvent>(
        ui::MouseWheelEvent(*event.AsScrollEvent()));
  }

  if (event.IsMouseEvent())
    return std::make_unique<ui::PointerEvent>(*event.AsMouseEvent());

  if (event.IsTouchEvent())
    return std::make_unique<ui::PointerEvent>(*event.AsTouchEvent());

  return ui::Event::Clone(event);
}

}  // namespace

namespace aura {

EventInjector::EventInjector() {}

EventInjector::~EventInjector() {}

ui::EventDispatchDetails EventInjector::Inject(WindowTreeHost* host,
                                               ui::Event* event) {
  Env* env = Env::GetInstance();
  DCHECK(env);
  DCHECK(host);
  DCHECK(event);

  if (env->mode() == Env::Mode::LOCAL)
    return host->event_sink()->OnEventFromSource(event);
  if (!window_server_ptr_) {
    env->window_tree_client_->connector()->BindInterface(
        ui::mojom::kServiceName, &window_server_ptr_);
  }
  display::Screen* screen = display::Screen::GetScreen();
  window_server_ptr_->DispatchEvent(
      screen->GetDisplayNearestWindow(host->window()).id(), MapEvent(*event),
      base::Bind([](bool result) { DCHECK(result); }));
  return ui::EventDispatchDetails();
}

}  // namespace aura
