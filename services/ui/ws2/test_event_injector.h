// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_TEST_EVENT_INJECTOR_H_
#define SERVICES_UI_WS2_TEST_EVENT_INJECTOR_H_

#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/public/interfaces/test_event_injector.mojom.h"

namespace ui {
namespace ws2 {

class InjectedEventHandler;
class TestEventInjectorDelegate;
class WindowService;

// See description in mojom for details on this. This is *not* part of the
// window-service proper as it is intended only for tests.
class TestEventInjector : public mojom::TestEventInjector {
 public:
  TestEventInjector(WindowService* window_service,
                    TestEventInjectorDelegate* delegate);
  ~TestEventInjector() override;

  void AddBinding(mojom::TestEventInjectorRequest request);

 private:
  struct HandlerAndCallback;

  void OnEventDispatched(InjectedEventHandler* handler);

  // mojom::TestEventInjector:
  void InjectEvent(int64_t display_id,
                   std::unique_ptr<ui::Event> event,
                   InjectEventCallback cb) override;

  WindowService* window_service_;

  TestEventInjectorDelegate* delegate_;

  // Each call to InjectEvent() results in creating and adding a new handler to
  // |handlers_|. The callback in HandlerAndCallback is the callback supplied by
  // the client. The handlers are removed once the event is processed.
  std::vector<std::unique_ptr<HandlerAndCallback>> handlers_;

  mojo::BindingSet<mojom::TestEventInjector> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestEventInjector);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_TEST_EVENT_INJECTOR_H_
