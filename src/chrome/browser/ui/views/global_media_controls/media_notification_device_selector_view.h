// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/views/global_media_controls/global_media_controls_types.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_footer_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "components/media_router/common/media_sink.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace {
class ExpandDeviceSelectorButton;
const char kAudioDevicesCountHistogramName[] =
    "Media.GlobalMediaControls.NumberOfAvailableAudioDevices";
const char kCastDeviceCountHistogramName[] =
    "Media.GlobalMediaControls.CastDeviceCount";
const char kDeviceSelectorAvailableHistogramName[] =
    "Media.GlobalMediaControls.DeviceSelectorAvailable";
const char kDeviceSelectorOpenedHistogramName[] =
    "Media.GlobalMediaControls.DeviceSelectorOpened";
}  // anonymous namespace

namespace media_router {
class CastDialogSinkButton;
}
class MediaNotificationDeviceSelectorViewDelegate;
class MediaNotificationDeviceSelectorObserver;

class MediaNotificationDeviceSelectorView
    : public views::View,
      public IconLabelBubbleView::Delegate,
      public media_router::CastDialogController::Observer,
      public MediaNotificationFooterView::Delegate {
 public:
  METADATA_HEADER(MediaNotificationDeviceSelectorView);
  MediaNotificationDeviceSelectorView(
      MediaNotificationDeviceSelectorViewDelegate* delegate,
      std::unique_ptr<media_router::CastDialogController> cast_controller,
      bool has_audio_output,
      const std::string& current_device_id,
      const SkColor& foreground_color,
      const SkColor& background_color,
      GlobalMediaControlsEntryPoint entry_point,
      bool show_expand_button = true);
  ~MediaNotificationDeviceSelectorView() override;

  // Called when audio output devices are discovered.
  void UpdateAvailableAudioDevices(
      const media::AudioDeviceDescriptions& device_descriptions);
  // Called when an audio device switch has occurred
  void UpdateCurrentAudioDevice(const std::string& current_device_id);
  void OnColorsChanged(const SkColor& foreground_color,
                       const SkColor& background_color);

  // Called when the audio device switching has become enabled or disabled.
  void UpdateIsAudioDeviceSwitchingEnabled(bool enabled);

  // IconLabelBubbleView::Delegate
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  //  media_router::CastDialogController::Observer
  void OnModelUpdated(const media_router::CastDialogModel& model) override;
  void OnControllerInvalidated() override;

  // MediaNotificationFooterview::Delegate
  void OnDeviceSelected(int tag) override;
  void OnDropdownButtonClicked() override;
  bool IsDeviceSelectorExpanded() override;

  void AddObserver(MediaNotificationDeviceSelectorObserver* observer);

  views::Button* GetExpandButtonForTesting();
  std::string GetEntryLabelForTesting(views::View* entry_view);
  bool GetEntryIsHighlightedForTesting(views::View* entry_view);
  std::vector<media_router::CastDialogSinkButton*>
  GetCastSinkButtonsForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceButtonsCreated);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           ExpandButtonOpensEntryContainer);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceEntryContainerVisibility);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           AudioDeviceButtonClickNotifiesContainer);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           CurrentAudioDeviceHighlighted);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           AudioDeviceHighlightedOnChange);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           AudioDeviceButtonsChange);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           AudioDevicesCountHistogramRecorded);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceSelectorOpenedHistogramRecorded);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           CastDeviceButtonClickStartsCasting);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           CastDeviceButtonClickClearsIssue);

  void UpdateVisibility();
  bool ShouldBeVisible() const;
  void ExpandButtonPressed();
  void ShowDevices();
  void HideDevices();
  void RemoveDevicesOfType(DeviceEntryUIType type);
  void StartCastSession(CastDeviceEntryView* entry);
  void DoStartCastSession(media_router::UIMediaSink sink);
  void RecordStartCastingMetrics(media_router::SinkIconType sink_icon_type);
  void RecordStartCastingWithCastAndDialPresent(
      media_router::SinkIconType sink_icon_type);
  void RecordStopCastingMetrics();
  void RecordCastDeviceCountAfterDelay();
  void RecordCastDeviceCount();
  DeviceEntryUI* GetDeviceEntryUI(views::View* view) const;
  void RegisterAudioDeviceCallbacks();

  bool has_expand_button_been_shown_ = false;
  bool have_devices_been_shown_ = false;

  bool is_expanded_ = false;
  bool is_audio_device_switching_enabled_ = false;
  bool has_cast_device_ = false;
  MediaNotificationDeviceSelectorViewDelegate* const delegate_;
  std::string current_device_id_;
  SkColor foreground_color_, background_color_;
  GlobalMediaControlsEntryPoint const entry_point_;

  // Child views
  AudioDeviceEntryView* current_audio_device_entry_view_ = nullptr;
  views::View* expand_button_strip_ = nullptr;
  ExpandDeviceSelectorButton* expand_button_ = nullptr;
  views::View* device_entry_views_container_ = nullptr;

  base::CallbackListSubscription audio_device_subscription_;
  base::CallbackListSubscription is_device_switching_enabled_subscription_;

  std::unique_ptr<media_router::CastDialogController> cast_controller_;

  base::ObserverList<MediaNotificationDeviceSelectorObserver> observers_;

  // Each button has a unique tag, which is used to look up DeviceEntryUI* in
  // |device_entry_ui_map_|.
  int next_tag_ = 0;
  std::map<int, DeviceEntryUI*> device_entry_ui_map_;

  base::WeakPtrFactory<MediaNotificationDeviceSelectorView> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_
