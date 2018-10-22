// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media_security/multi_profile_media_tray_item.h"

#include "ash/media_controller.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace tray {

class MultiProfileMediaTrayView : public TrayItemView,
                                  public MediaCaptureObserver {
 public:
  explicit MultiProfileMediaTrayView(SystemTrayItem* system_tray_item)
      : TrayItemView(system_tray_item) {
    CreateImageView();
    image_view()->SetImage(
        gfx::CreateVectorIcon(kSystemTrayRecordingIcon, kTrayIconColor));
    Shell::Get()->media_controller()->AddObserver(this);
    SetVisible(false);
    Shell::Get()->media_controller()->RequestCaptureState();
    set_id(VIEW_ID_MEDIA_TRAY_VIEW);
  }

  ~MultiProfileMediaTrayView() override {
    Shell::Get()->media_controller()->RemoveObserver(this);
  }

  // MediaCaptureObserver:
  void OnMediaCaptureChanged(
      const base::flat_map<AccountId, mojom::MediaCaptureState>& capture_states)
      override {
    // The user at 0 is the current active desktop user.
    const auto& current_account_id = Shell::Get()
                                         ->session_controller()
                                         ->GetUserSession(0)
                                         ->user_info->account_id;
    for (const auto& entry : capture_states) {
      if (entry.first == current_account_id)
        continue;
      if (entry.second != mojom::MediaCaptureState::NONE) {
        SetVisible(true);
        return;
      }
    }
    SetVisible(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiProfileMediaTrayView);
};

}  // namespace tray

MultiProfileMediaTrayItem::MultiProfileMediaTrayItem(SystemTray* system_tray)
    : SystemTrayItem(system_tray,
                     SystemTrayItemUmaType::UMA_MULTI_PROFILE_MEDIA) {}

MultiProfileMediaTrayItem::~MultiProfileMediaTrayItem() = default;

views::View* MultiProfileMediaTrayItem::CreateTrayView(LoginStatus status) {
  return new tray::MultiProfileMediaTrayView(this);
}

}  // namespace ash
