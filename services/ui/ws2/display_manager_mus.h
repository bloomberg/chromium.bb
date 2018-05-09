// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_DISPLAY_MANAGER_MUS_H_
#define SERVICES_UI_WS2_DISPLAY_MANAGER_MUS_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/ui/public/interfaces/display_manager.mojom.h"
#include "ui/display/display_observer.h"

namespace ui {
namespace ws2 {

// Provides information about displays to window service clients.
// display::Screen must outlive this object. Exported for test.
// TODO(jamescook): Rename to DisplayManagerWs or whatever we end up calling the
// window server library. It is named Mus to match ScreenMus.
class COMPONENT_EXPORT(WINDOW_SERVICE) DisplayManagerMus
    : public mojom::DisplayManager,
      public display::DisplayObserver {
 public:
  DisplayManagerMus();
  ~DisplayManagerMus() override;

  void AddBinding(mojom::DisplayManagerRequest request);

  // mojom::DisplayManager:
  void AddObserver(mojom::DisplayManagerObserverPtr observer) override;

  // display::DisplayObserver:
  void OnDidProcessDisplayChanges() override;

 private:
  std::vector<mojom::WsDisplayPtr> GetAllDisplays();

  void NotifyAllObservers();

  void NotifyObserver(mojom::DisplayManagerObserver* observer);

  mojo::BindingSet<mojom::DisplayManager> bindings_;

  mojo::InterfacePtrSet<mojom::DisplayManagerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManagerMus);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_DISPLAY_MANAGER_MUS_H_
