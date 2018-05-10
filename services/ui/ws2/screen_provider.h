// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_SCREEN_PROVIDER_H_
#define SERVICES_UI_WS2_SCREEN_PROVIDER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/ui/public/interfaces/screen_provider.mojom.h"
#include "ui/display/display_observer.h"

namespace ui {
namespace ws2 {

// Provides information about displays to window service clients.
// display::Screen must outlive this object. Exported for test.
class COMPONENT_EXPORT(WINDOW_SERVICE) ScreenProvider
    : public mojom::ScreenProvider,
      public display::DisplayObserver {
 public:
  ScreenProvider();
  ~ScreenProvider() override;

  void AddBinding(mojom::ScreenProviderRequest request);

  // mojom::ScreenProvider:
  void AddObserver(mojom::ScreenProviderObserverPtr observer) override;

  // display::DisplayObserver:
  void OnDidProcessDisplayChanges() override;

 private:
  std::vector<mojom::WsDisplayPtr> GetAllDisplays();

  void NotifyAllObservers();

  void NotifyObserver(mojom::ScreenProviderObserver* observer);

  mojo::BindingSet<mojom::ScreenProvider> bindings_;

  mojo::InterfacePtrSet<mojom::ScreenProviderObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenProvider);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_SCREEN_PROVIDER_H_
