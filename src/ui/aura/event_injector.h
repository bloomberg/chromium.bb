// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EVENT_INJECTOR_H_
#define UI_AURA_EVENT_INJECTOR_H_

#include "base/callback.h"
#include "services/ws/public/mojom/event_injector.mojom.h"
#include "ui/aura/aura_export.h"

namespace ui {
class Event;
struct EventDispatchDetails;
}

namespace aura {

class WindowTreeHost;

// Used to inject events into the system. In LOCAL mode, it directly injects
// events into the WindowTreeHost, but in MUS mode, it injects events into the
// window-server (over the mojom API).
class AURA_EXPORT EventInjector {
 public:
  EventInjector();
  ~EventInjector();

  // Inject |event| to |host|. |callback| is optional that gets invoked after
  // the event is processed by |host|. In LOCAL mode,  |callback| is invoked
  // synchronously. In MUS mode, it is invoked after a response is received
  // from window-server (via mojo).
  ui::EventDispatchDetails Inject(
      WindowTreeHost* host,
      ui::Event* event,
      base::OnceClosure callback = base::OnceClosure());

 private:
  ws::mojom::EventInjectorPtr event_injector_;
  bool has_pending_callback_ = false;

  DISALLOW_COPY_AND_ASSIGN(EventInjector);
};

}  // namespace aura

#endif  // UI_AURA_EVENT_INJECTOR_H_
