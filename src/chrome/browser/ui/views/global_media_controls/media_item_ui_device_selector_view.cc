// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_device_selector_delegate.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_observer.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom-shared.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

#include "components/media_router/common/mojom/media_route_provider_id.mojom.h"

using media_router::MediaRouterMetrics;
using media_router::mojom::MediaRouteProviderId;

namespace {

// Constants for the MediaItemUIDeviceSelectorView
constexpr gfx::Insets kExpandButtonStripInsets{6, 15};
constexpr gfx::Size kExpandButtonStripSize{400, 30};
constexpr gfx::Insets kExpandButtonBorderInsets{4, 8};

// Constant for DropdownButton
const int kDropdownButtonIconSize = 15;
const int kDropdownButtonBackgroundRadius = 15;
constexpr gfx::Insets kDropdownButtonBorderInsets{4};

// The maximum number of audio devices to count when recording the
// Media.GlobalMediaControls.NumberOfAvailableAudioDevices histogram. 30 was
// chosen because it would be very unlikely to see a user with 30+ audio
// devices.
const int kAudioDevicesCountHistogramMax = 30;

media_router::MediaRouterDialogOpenOrigin ConvertToOrigin(
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  switch (entry_point) {
    case global_media_controls::GlobalMediaControlsEntryPoint::kPresentation:
      return media_router::MediaRouterDialogOpenOrigin::PAGE;
    case global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray:
      return media_router::MediaRouterDialogOpenOrigin::SYSTEM_TRAY;
    case global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon:
      return media_router::MediaRouterDialogOpenOrigin::TOOLBAR;
  }
}

void RecordCastDeviceCountMetrics(
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    std::vector<CastDeviceEntryView*> entries) {
  MediaRouterMetrics::RecordDeviceCount(entries.size());

  std::map<MediaRouteProviderId, std::map<bool, int>> counts = {
      {MediaRouteProviderId::CAST, {{true, 0}, {false, 0}}},
      {MediaRouteProviderId::DIAL, {{true, 0}, {false, 0}}},
      {MediaRouteProviderId::WIRED_DISPLAY, {{true, 0}, {false, 0}}}};
  for (const CastDeviceEntryView* entry : entries) {
    if (entry->sink().provider != MediaRouteProviderId::TEST) {
      counts.at(entry->sink().provider).at(entry->GetEnabled())++;
    }
  }
  for (auto provider : {MediaRouteProviderId::CAST, MediaRouteProviderId::DIAL,
                        MediaRouteProviderId::WIRED_DISPLAY}) {
    for (bool is_available : {true, false}) {
      int count = counts.at(provider).at(is_available);
      MediaRouterMetrics::RecordGmcDeviceCount(ConvertToOrigin(entry_point),
                                               provider, is_available, count);
    }
  }
}

class ExpandDeviceSelectorLabel : public views::Label {
 public:
  explicit ExpandDeviceSelectorLabel(
      global_media_controls::GlobalMediaControlsEntryPoint entry_point);
  ~ExpandDeviceSelectorLabel() override = default;

  void OnColorsChanged(SkColor foreground_color, SkColor background_color);
};

class ExpandDeviceSelectorButton : public views::ToggleImageButton {
 public:
  explicit ExpandDeviceSelectorButton(PressedCallback callback,
                                      SkColor background_color);
  ~ExpandDeviceSelectorButton() override = default;

  void OnColorsChanged(SkColor foreground_color);
};

}  // namespace

ExpandDeviceSelectorLabel::ExpandDeviceSelectorLabel(
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  if (entry_point ==
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    SetText(l10n_util::GetStringUTF16(
        IDS_GLOBAL_MEDIA_CONTROLS_DEVICES_LABEL_WITH_COLON));
  } else {
    SetText(l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_DEVICES_LABEL));
  }
  auto size = GetPreferredSize();
  size.set_height(kExpandButtonStripSize.height());
  size.set_width(size.width() + kExpandButtonBorderInsets.width());
  SetPreferredSize(size);
}

void ExpandDeviceSelectorLabel::OnColorsChanged(SkColor foreground_color,
                                                SkColor background_color) {
  SetEnabledColor(foreground_color);
  SetBackgroundColor(background_color);
}

ExpandDeviceSelectorButton::ExpandDeviceSelectorButton(PressedCallback callback,
                                                       SkColor foreground_color)
    : ToggleImageButton(callback) {
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetBorder(views::CreateEmptyBorder(kDropdownButtonBorderInsets));

  SetHasInkDropActionOnClick(true);
  views::InstallFixedSizeCircleHighlightPathGenerator(
      this, kDropdownButtonBackgroundRadius);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(foreground_color);

  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_SHOW_DEVICE_LIST));
  SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_HIDE_DEVICE_LIST));
  OnColorsChanged(foreground_color);
}

void ExpandDeviceSelectorButton::OnColorsChanged(SkColor foreground_color) {
  // When the button is not toggled, the device list is collapsed and the arrow
  // is pointing up. Otherwise, the device list is expanded and the arrow is
  // pointing down.
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(vector_icons::kCaretDownIcon,
                                 kDropdownButtonIconSize, foreground_color));
  const auto caret_down_image = gfx::CreateVectorIcon(
      vector_icons::kCaretUpIcon, kDropdownButtonIconSize, foreground_color);
  SetToggledImage(views::Button::STATE_NORMAL, &caret_down_image);
  views::InkDrop::Get(this)->SetBaseColor(foreground_color);
}

MediaItemUIDeviceSelectorView::MediaItemUIDeviceSelectorView(
    const std::string& item_id,
    MediaItemUIDeviceSelectorDelegate* delegate,
    std::unique_ptr<media_router::CastDialogController> cast_controller,
    bool has_audio_output,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    bool show_expand_button)
    : item_id_(item_id), delegate_(delegate), entry_point_(entry_point) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  CreateExpandButtonStrip(show_expand_button);

  device_entry_views_container_ = AddChildView(std::make_unique<views::View>());
  device_entry_views_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  device_entry_views_container_->SetVisible(false);

  if (entry_point_ ==
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    ShowDevices();
  }
  SetBackground(views::CreateSolidBackground(background_color_));
  // Set the size of this view
  SetPreferredSize(kExpandButtonStripSize);
  Layout();

  // This view will become visible when devices are discovered.
  SetVisible(false);

  if (has_audio_output && base::FeatureList::IsEnabled(
                              media::kGlobalMediaControlsSeamlessTransfer)) {
    RegisterAudioDeviceCallbacks();
  }

  if (cast_controller) {
    cast_controller_ = std::move(cast_controller);
    cast_controller_->AddObserver(this);
  }
}

void MediaItemUIDeviceSelectorView::UpdateCurrentAudioDevice(
    const std::string& current_device_id) {
  if (current_audio_device_entry_view_)
    current_audio_device_entry_view_->SetHighlighted(false);

  // Find DeviceEntryView* from |device_entry_ui_map_| with |current_device_id|.
  auto it = base::ranges::find_if(
      device_entry_ui_map_, [&current_device_id](const auto& pair) {
        return pair.second->raw_device_id() == current_device_id;
      });

  if (it == device_entry_ui_map_.end()) {
    current_audio_device_entry_view_ = nullptr;
    current_device_id_ = "";
    return;
  }
  current_device_id_ = current_device_id;
  current_audio_device_entry_view_ =
      static_cast<AudioDeviceEntryView*>(it->second);
  current_audio_device_entry_view_->SetHighlighted(true);
  device_entry_views_container_->ReorderChildView(
      current_audio_device_entry_view_, 0);
  current_audio_device_entry_view_->Layout();
}

MediaItemUIDeviceSelectorView::~MediaItemUIDeviceSelectorView() {
  audio_device_subscription_ = {};

  // If this metric has not been recorded during the lifetime of this view, it
  // means that the device selector was never made available.
  if (!has_expand_button_been_shown_) {
    base::UmaHistogramBoolean(kDeviceSelectorAvailableHistogramName, false);
  } else if (!have_devices_been_shown_) {
    // Record if the device selector was available but never opened
    base::UmaHistogramBoolean(kDeviceSelectorOpenedHistogramName, false);
  }
}

void MediaItemUIDeviceSelectorView::UpdateAvailableAudioDevices(
    const media::AudioDeviceDescriptions& device_descriptions) {
  RemoveDevicesOfType(DeviceEntryUIType::kAudio);
  current_audio_device_entry_view_ = nullptr;

  bool current_device_still_exists = false;
  for (auto description : device_descriptions) {
    auto device_entry_view = std::make_unique<AudioDeviceEntryView>(
        base::BindRepeating(
            &MediaItemUIDeviceSelectorDelegate::OnAudioSinkChosen,
            base::Unretained(delegate_), item_id_, description.unique_id),
        foreground_color_, background_color_, description.unique_id,
        description.device_name);
    device_entry_view->set_tag(next_tag_++);
    device_entry_ui_map_[device_entry_view->tag()] = device_entry_view.get();
    device_entry_views_container_->AddChildView(std::move(device_entry_view));
    if (!current_device_still_exists &&
        description.unique_id == current_device_id_)
      current_device_still_exists = true;
  }

  // If the current device no longer exists, fallback to the default device
  UpdateCurrentAudioDevice(
      current_device_still_exists
          ? current_device_id_
          : media::AudioDeviceDescription::kDefaultDeviceId);

  UpdateVisibility();
  for (auto& observer : observers_)
    observer.OnMediaItemUIDeviceSelectorUpdated(device_entry_ui_map_);
}

void MediaItemUIDeviceSelectorView::SetMediaItemUIView(
    global_media_controls::MediaItemUIView* view) {
  media_item_ui_ = view;
}

void MediaItemUIDeviceSelectorView::OnColorsChanged(SkColor foreground_color,
                                                    SkColor background_color) {
  foreground_color_ = foreground_color;
  background_color_ = background_color;

  SetBackground(views::CreateSolidBackground(background_color_));
  for (auto it : device_entry_ui_map_) {
    it.second->OnColorsChanged(foreground_color_, background_color_);
  }

  expand_label_->OnColorsChanged(foreground_color_, background_color_);
  if (dropdown_button_)
    dropdown_button_->OnColorsChanged(foreground_color_);
  SchedulePaint();
}

SkColor
MediaItemUIDeviceSelectorView::GetIconLabelBubbleSurroundingForegroundColor()
    const {
  return foreground_color_;
}

SkColor MediaItemUIDeviceSelectorView::GetIconLabelBubbleBackgroundColor()
    const {
  return background_color_;
}

void MediaItemUIDeviceSelectorView::ShowDevices() {
  DCHECK(!is_expanded_);
  is_expanded_ = true;
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_SHOW_DEVICE_LIST));

  if (!have_devices_been_shown_) {
    base::UmaHistogramExactLinear(
        kAudioDevicesCountHistogramName,
        device_entry_views_container_->children().size(),
        kAudioDevicesCountHistogramMax);
    base::UmaHistogramBoolean(kDeviceSelectorOpenedHistogramName, true);
    RecordCastDeviceCountAfterDelay();
    have_devices_been_shown_ = true;
  }

  device_entry_views_container_->SetVisible(true);
  PreferredSizeChanged();
}

void MediaItemUIDeviceSelectorView::HideDevices() {
  DCHECK(is_expanded_);
  is_expanded_ = false;
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_HIDE_DEVICE_LIST));

  device_entry_views_container_->SetVisible(false);
  PreferredSizeChanged();
}

void MediaItemUIDeviceSelectorView::UpdateVisibility() {
  SetVisible(ShouldBeVisible());

  if (!has_expand_button_been_shown_ && GetVisible()) {
    base::UmaHistogramBoolean(kDeviceSelectorAvailableHistogramName, true);
    has_expand_button_been_shown_ = true;
  }

  if (media_item_ui_)
    media_item_ui_->OnDeviceSelectorViewSizeChanged();
}

bool MediaItemUIDeviceSelectorView::ShouldBeVisible() const {
  if (has_cast_device_)
    return true;
  if (!is_audio_device_switching_enabled_)
    return false;
  // The UI should be visible if there are more than one unique devices. That is
  // when:
  // * There are at least three devices
  // * Or, there are two devices and one of them has the default ID but not the
  // default name.
  if (device_entry_views_container_->children().size() == 2) {
    return base::ranges::any_of(
        device_entry_views_container_->children(), [this](views::View* view) {
          DeviceEntryUI* entry = GetDeviceEntryUI(view);
          return entry->raw_device_id() ==
                     media::AudioDeviceDescription::kDefaultDeviceId &&
                 entry->device_name() !=
                     media::AudioDeviceDescription::GetDefaultDeviceName();
        });
  }
  return device_entry_views_container_->children().size() > 2;
}

void MediaItemUIDeviceSelectorView::CreateExpandButtonStrip(
    bool show_expand_button) {
  expand_button_strip_ = AddChildView(std::make_unique<views::View>());
  auto* expand_button_strip_layout =
      expand_button_strip_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kExpandButtonStripInsets));
  expand_button_strip_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  expand_button_strip_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  expand_button_strip_->SetPreferredSize(kExpandButtonStripSize);

  expand_label_ = expand_button_strip_->AddChildView(
      std::make_unique<ExpandDeviceSelectorLabel>(entry_point_));

  // Show a button to show/hide the device list if dialog is opened from Cast
  // SDK.
  if (entry_point_ !=
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    dropdown_button_ = expand_button_strip_->AddChildView(
        std::make_unique<ExpandDeviceSelectorButton>(
            base::BindRepeating(
                &MediaItemUIDeviceSelectorView::ExpandButtonPressed,
                base::Unretained(this)),
            foreground_color_));
  }

  if (!show_expand_button)
    expand_button_strip_->SetVisible(false);
}

void MediaItemUIDeviceSelectorView::ExpandButtonPressed() {
  if (is_expanded_) {
    HideDevices();
  } else {
    ShowDevices();
  }
  dropdown_button_->SetToggled(is_expanded_);

  if (media_item_ui_)
    media_item_ui_->OnDeviceSelectorViewSizeChanged();
}

void MediaItemUIDeviceSelectorView::UpdateIsAudioDeviceSwitchingEnabled(
    bool enabled) {
  if (enabled == is_audio_device_switching_enabled_)
    return;

  is_audio_device_switching_enabled_ = enabled;
  UpdateVisibility();
}

void MediaItemUIDeviceSelectorView::RemoveDevicesOfType(
    DeviceEntryUIType type) {
  std::vector<views::View*> views_to_remove;
  for (auto* view : device_entry_views_container_->children()) {
    if (GetDeviceEntryUI(view)->GetType() == type) {
      views_to_remove.push_back(view);
    }
  }
  for (auto* view : views_to_remove) {
    device_entry_ui_map_.erase(static_cast<views::Button*>(view)->tag());
    device_entry_views_container_->RemoveChildView(view);
    delete view;
  }
}

DeviceEntryUI* MediaItemUIDeviceSelectorView::GetDeviceEntryUI(
    views::View* view) const {
  auto it = device_entry_ui_map_.find(static_cast<views::Button*>(view)->tag());
  DCHECK(it != device_entry_ui_map_.end());
  return it->second;
}

void MediaItemUIDeviceSelectorView::OnModelUpdated(
    const media_router::CastDialogModel& model) {
  RemoveDevicesOfType(DeviceEntryUIType::kCast);
  has_cast_device_ = false;
  for (auto sink : model.media_sinks()) {
    if (!base::Contains(sink.cast_modes,
                        media_router::MediaCastMode::PRESENTATION)) {
      continue;
    }
    has_cast_device_ = true;
    auto device_entry_view = std::make_unique<CastDeviceEntryView>(
        base::BindRepeating(&MediaItemUIDeviceSelectorView::StartCastSession,
                            base::Unretained(this)),
        foreground_color_, background_color_, sink);
    device_entry_view->set_tag(next_tag_++);
    device_entry_ui_map_[device_entry_view->tag()] = device_entry_view.get();
    auto* entry = device_entry_views_container_->AddChildView(
        std::move(device_entry_view));
    // After the |device_entry_view| is added, its icon color will change
    // according to the system theme. So we need to override the system color.
    entry->OnColorsChanged(foreground_color_, background_color_);
  }
  device_entry_views_container_->Layout();

  UpdateVisibility();
  for (auto& observer : observers_)
    observer.OnMediaItemUIDeviceSelectorUpdated(device_entry_ui_map_);
}

void MediaItemUIDeviceSelectorView::OnControllerInvalidated() {
  cast_controller_.reset();
}

void MediaItemUIDeviceSelectorView::OnDeviceSelected(int tag) {
  auto it = device_entry_ui_map_.find(tag);
  DCHECK(it != device_entry_ui_map_.end());

  if (it->second->GetType() == DeviceEntryUIType::kAudio)
    delegate_->OnAudioSinkChosen(item_id_, it->second->raw_device_id());
  else
    StartCastSession(static_cast<CastDeviceEntryView*>(it->second));
}

void MediaItemUIDeviceSelectorView::OnDropdownButtonClicked() {
  ExpandButtonPressed();
}

bool MediaItemUIDeviceSelectorView::IsDeviceSelectorExpanded() {
  return is_expanded_;
}

bool MediaItemUIDeviceSelectorView::OnMousePressed(
    const ui::MouseEvent& event) {
  // Stop the mouse click event from bubbling to parent views.
  return true;
}

void MediaItemUIDeviceSelectorView::AddObserver(
    MediaItemUIDeviceSelectorObserver* observer) {
  observers_.AddObserver(observer);
}

views::Label*
MediaItemUIDeviceSelectorView::GetExpandDeviceSelectorLabelForTesting() {
  return expand_label_;
}

views::Button* MediaItemUIDeviceSelectorView::GetDropdownButtonForTesting() {
  return dropdown_button_;
}

std::string MediaItemUIDeviceSelectorView::GetEntryLabelForTesting(
    views::View* entry_view) {
  return GetDeviceEntryUI(entry_view)->device_name();
}

bool MediaItemUIDeviceSelectorView::GetEntryIsHighlightedForTesting(
    views::View* entry_view) {
  return GetDeviceEntryUI(entry_view)
      ->GetEntryIsHighlightedForTesting();  // IN-TEST
}

bool MediaItemUIDeviceSelectorView::GetDeviceEntryViewVisibilityForTesting() {
  return device_entry_views_container_->GetVisible();
}

std::vector<media_router::CastDialogSinkButton*>
MediaItemUIDeviceSelectorView::GetCastSinkButtonsForTesting() {
  std::vector<media_router::CastDialogSinkButton*> buttons;
  for (auto* view : device_entry_views_container_->children()) {
    if (GetDeviceEntryUI(view)->GetType() == DeviceEntryUIType::kCast) {
      buttons.push_back(static_cast<media_router::CastDialogSinkButton*>(view));
    }
  }
  return buttons;
}

void MediaItemUIDeviceSelectorView::StartCastSession(
    CastDeviceEntryView* entry) {
  if (!cast_controller_)
    return;
  const media_router::UIMediaSink& sink = entry->sink();
  // Clicking on the device entry with an issue will clear the issue without
  // starting casting.
  if (sink.issue) {
    cast_controller_->ClearIssue(sink.issue->id());
    return;
  }
  // When users click on a CONNECTED sink,
  // if it is a CAST sink, a new cast session will replace the existing cast
  // session.
  // if it is a DIAL sink, the existing session will be terminated and users
  // need to click on the sink again to start a new session.
  // TODO(crbug.com/1206830): implement "terminate existing route and start a
  // new session" in DIAL MRP.
  if (sink.state == media_router::UIMediaSinkState::AVAILABLE) {
    DoStartCastSession(sink);
  } else if (sink.state == media_router::UIMediaSinkState::CONNECTED) {
    // We record stopping casting here even if we are starting casting, because
    // the existing session is being stopped and replaced by a new session.
    RecordStopCastingMetrics();
    if (sink.provider == MediaRouteProviderId::DIAL) {
      DCHECK(sink.route);
      cast_controller_->StopCasting(sink.route->media_route_id());
    } else {
      DoStartCastSession(sink);
    }
  }
}

void MediaItemUIDeviceSelectorView::DoStartCastSession(
    media_router::UIMediaSink sink) {
  DCHECK(base::Contains(sink.cast_modes,
                        media_router::MediaCastMode::PRESENTATION));
  cast_controller_->StartCasting(sink.id,
                                 media_router::MediaCastMode::PRESENTATION);
  RecordStartCastingMetrics(sink.icon_type);
}

void MediaItemUIDeviceSelectorView::RecordStartCastingMetrics(
    media_router::SinkIconType sink_icon_type) {
  MediaRouterMetrics::RecordMediaSinkTypeForGlobalMediaControls(sink_icon_type);
  RecordStartCastingWithCastAndDialPresent(sink_icon_type);

  global_media_controls::GlobalMediaControlsCastActionAndEntryPoint action;
  switch (entry_point_) {
    case global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon:
      action = global_media_controls::
          GlobalMediaControlsCastActionAndEntryPoint::kStartViaToolbarIcon;
      break;
    case global_media_controls::GlobalMediaControlsEntryPoint::kPresentation:
      action = global_media_controls::
          GlobalMediaControlsCastActionAndEntryPoint::kStartViaPresentation;
      break;
    case global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray:
      action = global_media_controls::
          GlobalMediaControlsCastActionAndEntryPoint::kStartViaSystemTray;
      break;
  }
  base::UmaHistogramEnumeration(
      media_message_center::MediaNotificationItem::kCastStartStopHistogramName,
      action);
}

void MediaItemUIDeviceSelectorView::RecordStartCastingWithCastAndDialPresent(
    media_router::SinkIconType type) {
  bool has_cast = false;
  bool has_dial = false;
  for (views::View* view : device_entry_views_container_->children()) {
    DeviceEntryUI* entry = GetDeviceEntryUI(view);
    if (entry->GetType() != DeviceEntryUIType::kCast) {
      continue;
    }
    const auto* cast_entry = static_cast<CastDeviceEntryView*>(entry);
    // A sink gets disabled while we're trying to connect to it, but we consider
    // those sinks available.
    if (!cast_entry->GetEnabled() &&
        cast_entry->sink().state !=
            media_router::UIMediaSinkState::CONNECTING) {
      continue;
    }
    if (cast_entry->sink().provider == MediaRouteProviderId::CAST) {
      has_cast = true;
    } else if (cast_entry->sink().provider == MediaRouteProviderId::DIAL) {
      has_dial = true;
    }
    if (has_cast && has_dial) {
      MediaRouterMetrics::RecordMediaSinkTypeWhenCastAndDialPresent(
          type, media_router::UiType::kGlobalMediaControls);
      return;
    }
  }
}

void MediaItemUIDeviceSelectorView::RecordStopCastingMetrics() {
  global_media_controls::GlobalMediaControlsCastActionAndEntryPoint action;
  switch (entry_point_) {
    case global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon:
      action = global_media_controls::
          GlobalMediaControlsCastActionAndEntryPoint::kStopViaToolbarIcon;
      break;
    case global_media_controls::GlobalMediaControlsEntryPoint::kPresentation:
      action = global_media_controls::
          GlobalMediaControlsCastActionAndEntryPoint::kStopViaPresentation;
      break;
    case global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray:
      action = global_media_controls::
          GlobalMediaControlsCastActionAndEntryPoint::kStopViaSystemTray;
      break;
  }
  base::UmaHistogramEnumeration(
      media_message_center::MediaNotificationItem::kCastStartStopHistogramName,
      action);
}

void MediaItemUIDeviceSelectorView::RecordCastDeviceCountAfterDelay() {
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MediaItemUIDeviceSelectorView::RecordCastDeviceCount,
                     weak_ptr_factory_.GetWeakPtr()),
      MediaRouterMetrics::kDeviceCountMetricDelay);
}

void MediaItemUIDeviceSelectorView::RecordCastDeviceCount() {
  std::vector<CastDeviceEntryView*> entries;
  for (views::View* view : device_entry_views_container_->children()) {
    DeviceEntryUI* entry = GetDeviceEntryUI(view);
    if (entry->GetType() == DeviceEntryUIType::kCast) {
      entries.push_back(static_cast<CastDeviceEntryView*>(entry));
    }
  }
  RecordCastDeviceCountMetrics(entry_point_, entries);
}

void MediaItemUIDeviceSelectorView::RegisterAudioDeviceCallbacks() {
  // Get a list of the connected audio output devices.
  audio_device_subscription_ =
      delegate_->RegisterAudioOutputDeviceDescriptionsCallback(
          base::BindRepeating(
              &MediaItemUIDeviceSelectorView::UpdateAvailableAudioDevices,
              weak_ptr_factory_.GetWeakPtr()));

  // Get the availability of audio output device switching.
  is_device_switching_enabled_subscription_ =
      delegate_->RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
          item_id_, base::BindRepeating(&MediaItemUIDeviceSelectorView::
                                            UpdateIsAudioDeviceSwitchingEnabled,
                                        weak_ptr_factory_.GetWeakPtr()));
}

BEGIN_METADATA(MediaItemUIDeviceSelectorView, views::View)
END_METADATA
