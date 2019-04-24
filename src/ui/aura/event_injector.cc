// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_injector.h"

#include <utility>

#include "base/bind.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace aura {

namespace {

void RunCallback(base::OnceClosure callback, bool processed) {
  if (!callback)
    return;

  std::move(callback).Run();
}

}  // namespace

EventInjector::EventInjector() {}

EventInjector::~EventInjector() {
  // |event_injector_| should not be waiting for responses. Otherwise, the
  // pending callback would not happen because the mojo channel is closed.
  DCHECK(!has_pending_callback_ || !event_injector_.IsExpectingResponse());
}

ui::EventDispatchDetails EventInjector::Inject(WindowTreeHost* host,
                                               ui::Event* event,
                                               base::OnceClosure callback) {
  DCHECK(host);
  Env* env = host->window()->env();
  DCHECK(env);
  DCHECK(event);

  if (env->mode() == Env::Mode::LOCAL) {
    ui::EventDispatchDetails details =
        host->event_sink()->OnEventFromSource(event);
    RunCallback(std::move(callback), /*processed=*/true);
    return details;
  }

  has_pending_callback_ |= !callback.is_null();

  if (event->IsLocatedEvent()) {
    // The ui-service expects events coming in to have a location matching the
    // root location. The non-ui-service code does this by way of
    // OnEventFromSource() ending up in LocatedEvent::UpdateForRootTransform(),
    // which reset the root_location to match the location.
    event->AsLocatedEvent()->set_root_location_f(
        event->AsLocatedEvent()->location_f());
  }

  if (!event_injector_) {
    env->window_tree_client_->connector()->BindInterface(
        ws::mojom::kServiceName, &event_injector_);
  }
  event_injector_->InjectEvent(
      host->GetDisplayId(), ui::Event::Clone(*event),
      base::BindOnce(&RunCallback, std::move(callback)));
  return ui::EventDispatchDetails();
}

}  // namespace aura
