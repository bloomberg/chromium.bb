// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/user_display_manager.h"

#include <utility>

#include "services/ui/ws/user_display_manager_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ui {
namespace ws {
namespace {

std::vector<mojom::WsDisplayPtr> CloneDisplays(
    const std::vector<mojom::WsDisplayPtr>& input) {
  std::vector<mojom::WsDisplayPtr> result;
  result.reserve(input.size());
  for (const auto& display : input) {
    result.push_back(display.Clone());
  }
  return result;
}

int64_t GetInternalDisplayId() {
  if (!display::Display::HasInternalDisplay())
    return display::kInvalidDisplayId;

  return display::Display::InternalDisplayId();
}

}  // namespace

UserDisplayManager::UserDisplayManager(UserDisplayManagerDelegate* delegate,
                                       const UserId& user_id)
    : delegate_(delegate),
      user_id_(user_id),
      got_valid_frame_decorations_(
          delegate->GetFrameDecorationsForUser(user_id, nullptr)) {}

UserDisplayManager::~UserDisplayManager() {}

void UserDisplayManager::OnFrameDecorationValuesChanged() {
  if (!got_valid_frame_decorations_) {
    got_valid_frame_decorations_ = true;
    display_manager_observers_.ForAllPtrs([this](
        mojom::DisplayManagerObserver* observer) { CallOnDisplays(observer); });
    return;
  }

  std::vector<mojom::WsDisplayPtr> displays = GetAllDisplays();
  display_manager_observers_.ForAllPtrs(
      [&displays](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplaysChanged(CloneDisplays(displays));
      });
}

void UserDisplayManager::AddDisplayManagerBinding(
    mojo::InterfaceRequest<mojom::DisplayManager> request) {
  display_manager_bindings_.AddBinding(this, std::move(request));
}

void UserDisplayManager::OnDisplayUpdate(const display::Display& display) {
  if (!got_valid_frame_decorations_)
    return;

  std::vector<mojom::WsDisplayPtr> displays(1);
  displays[0] = ToWsDisplayPtr(display);

  display_manager_observers_.ForAllPtrs(
      [&displays](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplaysChanged(CloneDisplays(displays));
      });
}

void UserDisplayManager::OnWillDestroyDisplay(int64_t display_id) {
  if (!got_valid_frame_decorations_)
    return;

  display_manager_observers_.ForAllPtrs(
      [display_id](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplayRemoved(display_id);
      });
}

void UserDisplayManager::OnPrimaryDisplayChanged(int64_t primary_display_id) {
  if (!got_valid_frame_decorations_)
    return;

  display_manager_observers_.ForAllPtrs(
      [primary_display_id](mojom::DisplayManagerObserver* observer) {
        observer->OnPrimaryDisplayChanged(primary_display_id);
      });
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
  if (!got_valid_frame_decorations_)
    return;

  CallOnDisplays(observer);
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
  const auto& displays = display::Screen::GetScreen()->GetAllDisplays();

  std::vector<mojom::WsDisplayPtr> ws_display;
  ws_display.reserve(displays.size());

  for (const auto& display : displays)
    ws_display.push_back(ToWsDisplayPtr(display));

  return ws_display;
}

void UserDisplayManager::CallOnDisplays(
    mojom::DisplayManagerObserver* observer) {
  observer->OnDisplays(GetAllDisplays(),
                       display::Screen::GetScreen()->GetPrimaryDisplay().id(),
                       GetInternalDisplayId());
}

}  // namespace ws
}  // namespace ui
