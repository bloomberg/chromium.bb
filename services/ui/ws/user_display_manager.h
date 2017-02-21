// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_USER_DISPLAY_MANAGER_H_
#define SERVICES_UI_WS_USER_DISPLAY_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/ui/public/interfaces/display_manager.mojom.h"
#include "services/ui/ws/user_id.h"

namespace display {
class Display;
}

namespace ui {
namespace ws {

class UserDisplayManagerDelegate;

// Provides per user display state.
class UserDisplayManager : public mojom::DisplayManager {
 public:
  UserDisplayManager(UserDisplayManagerDelegate* delegate,
                     const UserId& user_id);
  ~UserDisplayManager() override;

  // Called when the frame decorations for this user change.
  void OnFrameDecorationValuesChanged();

  void AddDisplayManagerBinding(
      mojo::InterfaceRequest<mojom::DisplayManager> request);

  // Called when something about the display (e.g. pixel-ratio, size) changes.
  void OnDisplayUpdate(const display::Display& display);

  // Called when |display_id| is being removed.
  void OnWillDestroyDisplay(int64_t display_id);

  // Called when the primary display changes.
  void OnPrimaryDisplayChanged(int64_t primary_display_id);

  // Overriden from mojom::DisplayManager:
  void AddObserver(mojom::DisplayManagerObserverPtr observer) override;

 private:
  // Called when a new observer is added. If frame decorations are available
  // notifies the observer immediately.
  void OnObserverAdded(mojom::DisplayManagerObserver* observer);

  // Fills in a WsDisplayPtr for |display|.
  mojom::WsDisplayPtr ToWsDisplayPtr(const display::Display& display);

  std::vector<mojom::WsDisplayPtr> GetAllDisplays();

  // Calls OnDisplays() on |observer|.
  void CallOnDisplays(mojom::DisplayManagerObserver* observer);

  UserDisplayManagerDelegate* delegate_;

  const UserId user_id_;

  // Set to true the first time at least one Display has valid frame values.
  bool got_valid_frame_decorations_;

  mojo::BindingSet<mojom::DisplayManager> display_manager_bindings_;

  // WARNING: only use these once |got_valid_frame_decorations_| is true.
  mojo::InterfacePtrSet<mojom::DisplayManagerObserver>
      display_manager_observers_;

  DISALLOW_COPY_AND_ASSIGN(UserDisplayManager);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_USER_DISPLAY_MANAGER_H_
