// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/media/media_notification_item.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace ash {

class MediaNotificationItem;
class MediaNotificationView;

// MediaNotificationController will show/hide media notifications when media
// sessions are active. These notifications will show metadata and playback
// controls.
class ASH_EXPORT MediaNotificationController
    : public media_session::mojom::AudioFocusObserver {
 public:
  explicit MediaNotificationController(service_manager::Connector* connector);
  ~MediaNotificationController() override;

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnActiveSessionChanged(
      media_session::mojom::AudioFocusRequestStatePtr session) override {}

  void SetView(const std::string& id, MediaNotificationView* view);

  MediaNotificationItem* GetItem(const std::string& id) {
    auto it = notifications_.find(id);
    DCHECK(it != notifications_.end());
    return &it->second;
  }

 private:
  media_session::mojom::MediaControllerManagerPtr controller_manager_ptr_;

  mojo::Binding<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_binding_{this};

  // Stores a |MediaNotificationItem| for each media session keyed by its
  // |request_id| in string format.
  std::map<const std::string, MediaNotificationItem> notifications_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationController);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_
