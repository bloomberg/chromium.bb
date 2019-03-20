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
#include "ui/aura/mus/window_tree_host_mus.h"
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

// Updates the location of |event| to be relative to the display |host| is
// on. It is assumed at the time this is called the location of the event is
// relative to |host|.
void ConvertHostLocationToDisplayLocation(WindowTreeHost* host,
                                          ui::LocatedEvent* event) {
  WindowTreeHostMus* host_mus = WindowTreeHostMus::ForWindow(host->window());
  DCHECK(host_mus);
  // WindowTreeHostMus's bounds are in screen coordinates, convert to display
  // relative.
  gfx::PointF location =
      gfx::PointF(host_mus->bounds_in_dip().origin() -
                  host_mus->GetDisplay().bounds().origin().OffsetFromOrigin());
  // And then add the event location.
  location += event->root_location_f().OffsetFromOrigin();
  event->set_root_location_f(location);
  event->set_location_f(location);
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
    if (event->IsLocatedEvent()) {
      ui::LocatedEvent* located_event = event->AsLocatedEvent();
      // Transforming the coordinate to the root will apply the screen scale
      // factor to the event's location and also the screen rotation degree.
      located_event->UpdateForRootTransform(
          host->GetRootTransform(),
          host->GetRootTransformForLocalEventCoordinates());
    }

    ui::EventDispatchDetails details =
        host->event_sink()->OnEventFromSource(event);
    RunCallback(std::move(callback), /*processed=*/true);
    return details;
  }

  has_pending_callback_ |= !callback.is_null();

  if (event->IsLocatedEvent()) {
    // The window-service expects events coming in to have a location matching
    // the root location.
    event->AsLocatedEvent()->set_root_location_f(
        event->AsLocatedEvent()->location_f());

    // Convert the location to be display relative. This is *not* needed in the
    // classic case as the WindowTreeHost and the display are the same.
    ConvertHostLocationToDisplayLocation(host, event->AsLocatedEvent());
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
