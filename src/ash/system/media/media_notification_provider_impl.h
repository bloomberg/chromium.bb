// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
#define ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/system/media/media_notification_provider.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace global_media_controls {
class MediaItemManager;
class MediaItemUIListView;
class MediaSessionItemProducer;
}  // namespace global_media_controls

namespace media_session {
class MediaSessionService;
}  // namespace media_session

namespace ash {

class ASH_EXPORT MediaNotificationProviderImpl
    : public MediaNotificationProvider,
      public global_media_controls::MediaDialogDelegate,
      public global_media_controls::MediaItemManagerObserver,
      public global_media_controls::MediaItemUIObserver {
 public:
  explicit MediaNotificationProviderImpl(
      media_session::MediaSessionService* service);
  ~MediaNotificationProviderImpl() override;

  // MediaNotificationProvider:
  void AddObserver(MediaNotificationProviderObserver* observer) override;
  void RemoveObserver(MediaNotificationProviderObserver* observer) override;
  bool HasActiveNotifications() override;
  bool HasFrozenNotifications() override;
  std::unique_ptr<views::View> GetMediaNotificationListView(
      int separator_thickness) override;
  std::unique_ptr<views::View> GetActiveMediaNotificationView() override;
  void OnBubbleClosing() override;
  void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) override;

  // global_media_controls::MediaDialogDelegate:
  global_media_controls::MediaItemUI* ShowMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaItem(const std::string& id) override;
  void HideMediaDialog() override {}
  void Focus() override {}

  // global_media_controls::MediaItemManagerObserver:
  void OnItemListChanged() override;
  void OnMediaDialogOpened() override {}
  void OnMediaDialogClosed() override {}

  // global_media_controls::MediaItemUIObserver:
  void OnMediaItemUISizeChanged() override;
  void OnMediaItemUIDestroyed(const std::string& id) override;

  global_media_controls::MediaSessionItemProducer*
  media_session_item_producer_for_testing() {
    return media_session_item_producer_.get();
  }

 private:
  base::ObserverList<MediaNotificationProviderObserver> observers_;

  global_media_controls::MediaItemUIListView* active_session_view_ = nullptr;

  std::unique_ptr<global_media_controls::MediaItemManager> item_manager_;

  std::unique_ptr<global_media_controls::MediaSessionItemProducer>
      media_session_item_producer_;

  std::map<const std::string, global_media_controls::MediaItemUI*>
      observed_item_uis_;

  absl::optional<media_message_center::NotificationTheme> color_theme_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
