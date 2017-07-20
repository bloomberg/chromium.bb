// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/user_display_manager.h"

#include <utility>

#include "services/ui/display/screen_manager.h"
#include "services/ui/ws/user_display_manager_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/display/types/display_constants.h"

namespace ui {
namespace ws {

UserDisplayManager::UserDisplayManager(UserDisplayManagerDelegate* delegate,
                                       const UserId& user_id)
    : delegate_(delegate),
      user_id_(user_id),
      got_valid_frame_decorations_(
          delegate->GetFrameDecorationsForUser(user_id, nullptr)) {}

UserDisplayManager::~UserDisplayManager() {}

void UserDisplayManager::DisableAutomaticNotification() {
  DCHECK(notify_automatically_);
  notify_automatically_ = false;
}

void UserDisplayManager::CallOnDisplaysChanged() {
  display_manager_observers_.ForAllPtrs(
      [this](mojom::DisplayManagerObserver* observer) {
        CallOnDisplaysChanged(observer);
      });
}

void UserDisplayManager::OnFrameDecorationValuesChanged() {
  got_valid_frame_decorations_ = true;
  CallOnDisplaysChangedIfNecessary();
}

void UserDisplayManager::AddDisplayManagerBinding(
    mojo::InterfaceRequest<mojom::DisplayManager> request) {
  display_manager_bindings_.AddBinding(this, std::move(request));
}

void UserDisplayManager::OnDisplayUpdated(const display::Display& display) {
  CallOnDisplaysChangedIfNecessary();
}

void UserDisplayManager::OnDisplayDestroyed(int64_t display_id) {
  CallOnDisplaysChangedIfNecessary();
}

void UserDisplayManager::OnPrimaryDisplayChanged(int64_t primary_display_id) {
  CallOnDisplaysChangedIfNecessary();
}

void UserDisplayManager::AddObserver(
    mojom::DisplayManagerObserverPtr observer) {
  mojom::DisplayManagerObserver* observer_impl = observer.get();
  display_manager_observers_.AddPtr(std::move(observer));
  OnObserverAdded(observer_impl);
}

void UserDisplayManager::OnObserverAdded(
    mojom::DisplayManagerObserver* observer) {
  // Many clients key off the frame decorations to size widgets. Wait for frame
  // decorations before notifying so that we don't have to worry about clients
  // resizing appropriately.
  if (!ShouldCallOnDisplaysChanged())
    return;

  CallOnDisplaysChanged(observer);
}

mojom::WsDisplayPtr UserDisplayManager::ToWsDisplayPtr(
    const display::Display& display) {
  mojom::WsDisplayPtr ws_display = mojom::WsDisplay::New();
  ws_display->display = display;
  delegate_->GetFrameDecorationsForUser(user_id_,
                                        &ws_display->frame_decoration_values);
  return ws_display;
}

std::vector<mojom::WsDisplayPtr> UserDisplayManager::GetAllDisplays() {
  const auto& displays =
      display::ScreenManager::GetInstance()->GetScreen()->GetAllDisplays();

  std::vector<mojom::WsDisplayPtr> ws_display;
  ws_display.reserve(displays.size());

  for (const auto& display : displays)
    ws_display.push_back(ToWsDisplayPtr(display));

  return ws_display;
}

bool UserDisplayManager::ShouldCallOnDisplaysChanged() const {
  return got_valid_frame_decorations_ && !display::ScreenManager::GetInstance()
                                              ->GetScreen()
                                              ->GetAllDisplays()
                                              .empty();
}

void UserDisplayManager::CallOnDisplaysChangedIfNecessary() {
  if (!notify_automatically_ || !ShouldCallOnDisplaysChanged())
    return;

  CallOnDisplaysChanged();
}

void UserDisplayManager::CallOnDisplaysChanged(
    mojom::DisplayManagerObserver* observer) {
  observer->OnDisplaysChanged(GetAllDisplays(),
                              display::ScreenManager::GetInstance()
                                  ->GetScreen()
                                  ->GetPrimaryDisplay()
                                  .id(),
                              delegate_->GetInternalDisplayId());
}

}  // namespace ws
}  // namespace ui
