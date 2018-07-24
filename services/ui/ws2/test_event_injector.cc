// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/test_event_injector.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "services/ui/ws2/injected_event_handler.h"
#include "services/ui/ws2/test_event_injector_delegate.h"
#include "services/ui/ws2/window_service.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"

namespace ui {
namespace ws2 {

struct TestEventInjector::HandlerAndCallback {
  std::unique_ptr<InjectedEventHandler> handler;
  // |callback| is the callback supplied by the client.
  TestEventInjector::InjectEventCallback callback;
};

TestEventInjector::TestEventInjector(WindowService* window_service,
                                     TestEventInjectorDelegate* delegate)
    : window_service_(window_service), delegate_(delegate) {
  DCHECK(window_service_);
  DCHECK(delegate_);
}

TestEventInjector::~TestEventInjector() {
  for (auto& handler_and_callback : handlers_)
    std::move(handler_and_callback->callback).Run(false);
}

void TestEventInjector::AddBinding(mojom::TestEventInjectorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void TestEventInjector::OnEventDispatched(InjectedEventHandler* handler) {
  for (auto iter = handlers_.begin(); iter != handlers_.end(); ++iter) {
    auto& handler_and_callback = *iter;
    if (handler_and_callback->handler.get() == handler) {
      std::move(handler_and_callback->callback).Run(true);
      handlers_.erase(iter);
      return;
    }
  }
  NOTREACHED();
}

void TestEventInjector::InjectEvent(int64_t display_id,
                                    std::unique_ptr<ui::Event> event,
                                    InjectEventCallback cb) {
  aura::WindowTreeHost* window_tree_host =
      delegate_->GetWindowTreeHostForDisplayId(display_id);
  if (!window_tree_host) {
    DVLOG(1) << "InjectEvent(): invalid display " << display_id;
    std::move(cb).Run(false);
    return;
  }

  if (event->IsLocatedEvent()) {
    LocatedEvent* located_event = event->AsLocatedEvent();
    if (located_event->root_location_f() != located_event->location_f()) {
      DVLOG(1) << "InjectEvent(): root_location and location must match";
      std::move(cb).Run(false);
      return;
    }

    // NOTE: this does not correctly account for coordinates with capture
    // across displays. If needed, the implementation should match something
    // like:
    // https://chromium.googlesource.com/chromium/src/+/ae087c53f5ce4557bfb0b92a13651342336fe18a/services/ui/ws/event_injector.cc#22
  }

  // Map PointerEvents to Mouse/Touch event. This should be unnecessary.
  // TODO: https://crbug.com/865781
  std::unique_ptr<Event> event_to_dispatch;
  if (event->IsMousePointerEvent())
    event_to_dispatch = std::make_unique<MouseEvent>(*event->AsPointerEvent());
  else if (event->IsTouchPointerEvent())
    event_to_dispatch = std::make_unique<TouchEvent>(*event->AsPointerEvent());
  else
    event_to_dispatch = std::move(event);

  std::unique_ptr<HandlerAndCallback> handler_and_callback =
      std::make_unique<HandlerAndCallback>();
  handler_and_callback->callback = std::move(cb);
  handler_and_callback->handler =
      std::make_unique<InjectedEventHandler>(window_service_, window_tree_host);
  InjectedEventHandler* handler = handler_and_callback->handler.get();
  handlers_.push_back(std::move(handler_and_callback));
  auto callback = base::BindOnce(&TestEventInjector::OnEventDispatched,
                                 base::Unretained(this), handler);
  handler->Inject(std::move(event_to_dispatch), std::move(callback));
}

}  // namespace ws2
}  // namespace ui
