// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_ui.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "media/base/media_switches.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/focus/focus_manager.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace views {
class ImageButton;
class SlideOutController;
}  // namespace views

namespace global_media_controls {

class MediaItemUIObserver;

// MediaItemUIView holds a media notification for display
// within the MediaDialogView. The media notification shows metadata for a media
// session and can control playback.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIView
    : public views::Button,
      public media_message_center::MediaNotificationContainer,
      public global_media_controls::MediaItemUI,
      public views::SlideOutControllerDelegate,
      public views::FocusChangeListener {
 public:
  METADATA_HEADER(MediaItemUIView);

  MediaItemUIView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      std::unique_ptr<MediaItemUIFooter> footer_view,
      std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
      absl::optional<media_message_center::NotificationTheme> theme =
          absl::nullopt);
  MediaItemUIView(const MediaItemUIView&) = delete;
  MediaItemUIView& operator=(const MediaItemUIView&) = delete;
  ~MediaItemUIView() override;

  // views::Button:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // media_message_center::MediaNotificationContainer:
  void OnExpanded(bool expanded) override;
  void OnMediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override;
  void OnMediaSessionMetadataChanged(
      const media_session::MediaMetadata& metadata) override;
  void OnVisibleActionsChanged(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override;
  void OnMediaArtworkChanged(const gfx::ImageSkia& image) override;
  void OnColorsChanged(SkColor foreground, SkColor background) override;
  void OnHeaderClicked() override;

  // views::SlideOutControllerDelegate:
  ui::Layer* GetSlideOutLayer() override;
  void OnSlideStarted() override {}
  void OnSlideChanged(bool in_progress) override {}
  void OnSlideOut() override;

  // global_media_controls::MediaItemUI:
  void AddObserver(
      global_media_controls::MediaItemUIObserver* observer) override;
  void RemoveObserver(
      global_media_controls::MediaItemUIObserver* observer) override;

  void OnDeviceSelectorViewSizeChanged();

  const std::u16string& GetTitle() const;

  views::ImageButton* GetDismissButtonForTesting();

  media_message_center::MediaNotificationViewImpl* view_for_testing() {
    DCHECK(!base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI));
    return static_cast<media_message_center::MediaNotificationViewImpl*>(view_);
  }
  MediaItemUIDeviceSelector* device_selector_view_for_testing() {
    return device_selector_view_;
  }

  bool is_playing_for_testing() { return is_playing_; }
  bool is_expanded_for_testing() { return is_expanded_; }

 private:
  class DismissButton;

  void UpdateDismissButtonIcon();
  void UpdateDismissButtonBackground();
  void UpdateDismissButtonVisibility();
  void DismissNotification();
  // Updates the forced expanded state of |view_|.
  void ForceExpandedState();
  // Notify observers that we've been clicked.
  void ContainerClicked();
  void OnSizeChanged();

  const std::string id_;
  raw_ptr<views::View> swipeable_container_ = nullptr;

  std::u16string title_;

  // Always "visible" so that it reserves space in the header so that the
  // dismiss button can appear without forcing things to shift.
  raw_ptr<views::View> dismiss_button_placeholder_ = nullptr;

  // Shows the colored circle background behind the dismiss button to give it
  // proper contrast against the artwork. The background can't be on the dismiss
  // button itself because it messes up the ink drop.
  raw_ptr<views::View> dismiss_button_container_ = nullptr;

  raw_ptr<DismissButton> dismiss_button_ = nullptr;
  raw_ptr<media_message_center::MediaNotificationView> view_ = nullptr;

  const raw_ptr<MediaItemUIFooter> footer_view_;

  raw_ptr<MediaItemUIDeviceSelector> device_selector_view_ = nullptr;

  SkColor foreground_color_;
  SkColor background_color_;

  bool has_artwork_ = false;
  bool has_many_actions_ = false;

  bool is_playing_ = false;

  bool is_expanded_ = false;

  base::ObserverList<global_media_controls::MediaItemUIObserver> observers_;

  // Handles gesture events for swiping to dismiss notifications.
  std::unique_ptr<views::SlideOutController> slide_out_controller_;

  const bool is_cros_;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_
