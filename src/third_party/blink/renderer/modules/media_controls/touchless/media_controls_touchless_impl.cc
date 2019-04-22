// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"

#include <algorithm>

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_orientation_lock_delegate.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_text_track_manager.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_overlay_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_timeline_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_media_event_listener.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_resource_loader.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// When specified as trackIndex, disable text tracks.
constexpr int kTrackIndexOffValue = -1;

// Number of seconds to jump when press left/right arrow.
constexpr int kNumberOfSecondsToJumpForTouchless = 10;

// Amount of volume to change when press up/down arrow.
constexpr double kVolumeToChangeForTouchless = 0.05;

// Amount of time that media controls are visible.
constexpr WTF::TimeDelta kTimeToHideMediaControls =
    TimeDelta::FromMilliseconds(3000);

const char kTransparentCSSClass[] = "transparent";

}  // namespace

enum class MediaControlsTouchlessImpl::ArrowDirection {
  kUp,
  kDown,
  kLeft,
  kRight,
};

MediaControlsTouchlessImpl::MediaControlsTouchlessImpl(
    HTMLMediaElement& media_element)
    : HTMLDivElement(media_element.GetDocument()),
      MediaControls(media_element),
      media_event_listener_(
          MakeGarbageCollected<MediaControlsTouchlessMediaEventListener>(
              media_element)),
      text_track_manager_(
          MakeGarbageCollected<MediaControlsTextTrackManager>(media_element)),
      orientation_lock_delegate_(nullptr),
      hide_media_controls_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaControlsTouchlessImpl::HideMediaControlsTimerFired) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-touchless"));
  media_event_listener_->AddObserver(this);
}

MediaControlsTouchlessImpl* MediaControlsTouchlessImpl::Create(
    HTMLMediaElement& media_element,
    ShadowRoot& shadow_root) {
  MediaControlsTouchlessImpl* controls =
      MakeGarbageCollected<MediaControlsTouchlessImpl>(media_element);
  MediaControlsTouchlessOverlayElement* overlay_element =
      MakeGarbageCollected<MediaControlsTouchlessOverlayElement>(*controls);

  MediaControlsTouchlessTimeDisplayElement* time_display_element =
      MakeGarbageCollected<MediaControlsTouchlessTimeDisplayElement>(*controls);
  MediaControlsTouchlessTimelineElement* timeline_element =
      MakeGarbageCollected<MediaControlsTouchlessTimelineElement>(*controls);

  controls->ParserAppendChild(overlay_element);

  Element* bottom_container = MediaControlElementsHelper::CreateDiv(
      "-internal-media-controls-touchless-bottom-container", controls);
  bottom_container->ParserAppendChild(time_display_element);
  bottom_container->ParserAppendChild(timeline_element);

  // Controls start hidden.
  controls->MakeTransparent();

  if (RuntimeEnabledFeatures::VideoFullscreenOrientationLockEnabled() &&
      media_element.IsHTMLVideoElement()) {
    // Initialize the orientation lock when going fullscreen feature.
    controls->orientation_lock_delegate_ =
        MakeGarbageCollected<MediaControlsOrientationLockDelegate>(
            ToHTMLVideoElement(media_element));
  }

  MediaControlsTouchlessResourceLoader::
      InjectMediaControlsTouchlessUAStyleSheet();

  shadow_root.ParserAppendChild(controls);
  return controls;
}

Node::InsertionNotificationRequest MediaControlsTouchlessImpl::InsertedInto(
    ContainerNode& root) {
  media_event_listener_->Attach();

  if (orientation_lock_delegate_)
    orientation_lock_delegate_->Attach();

  return HTMLDivElement::InsertedInto(root);
}

void MediaControlsTouchlessImpl::RemovedFrom(ContainerNode& insertion_point) {
  HTMLDivElement::RemovedFrom(insertion_point);

  Hide();

  media_event_listener_->Detach();

  if (orientation_lock_delegate_)
    orientation_lock_delegate_->Detach();
}

void MediaControlsTouchlessImpl::MakeOpaque() {
  // show controls
  classList().Remove(kTransparentCSSClass);

  if (hide_media_controls_timer_.IsActive())
    StopHideMediaControlsTimer();
  StartHideMediaControlsTimer();
}

void MediaControlsTouchlessImpl::MakeTransparent() {
  // hide controls
  classList().Add(kTransparentCSSClass);
}

void MediaControlsTouchlessImpl::MaybeShow() {
  RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
}

void MediaControlsTouchlessImpl::Hide() {
  SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
}

MediaControlsTouchlessMediaEventListener&
MediaControlsTouchlessImpl::MediaEventListener() const {
  return *media_event_listener_;
}

void MediaControlsTouchlessImpl::HideMediaControlsTimerFired(TimerBase*) {
  MakeTransparent();
}

void MediaControlsTouchlessImpl::StartHideMediaControlsTimer() {
  hide_media_controls_timer_.StartOneShot(kTimeToHideMediaControls, FROM_HERE);
}

void MediaControlsTouchlessImpl::StopHideMediaControlsTimer() {
  hide_media_controls_timer_.Stop();
}

void MediaControlsTouchlessImpl::OnFocusIn() {
  if (MediaElement().ShouldShowControls())
    MakeOpaque();
}

void MediaControlsTouchlessImpl::OnKeyDown(KeyboardEvent* event) {
  bool handled = true;
  switch (event->keyCode()) {
    case VKEY_RETURN:
      MakeOpaque();
      MediaElement().TogglePlayState();
      break;
    case VKEY_LEFT:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kLeft));
      break;
    case VKEY_RIGHT:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kRight));
      break;
    case VKEY_UP:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kUp));
      break;
    case VKEY_DOWN:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kDown));
      break;
    default:
      handled = false;
      break;
  }

  if (handled)
    event->SetDefaultHandled();
}

void MediaControlsTouchlessImpl::EnsureMediaControlsMenuHost() {
  if (!media_controls_host_) {
    GetDocument().GetFrame()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&media_controls_host_,
                          GetExecutionContext()->GetTaskRunner(
                              blink::TaskType::kMediaElementEvent)));
    media_controls_host_.set_connection_error_handler(WTF::Bind(
        &MediaControlsTouchlessImpl::OnMediaControlsMenuHostConnectionError,
        WrapWeakPersistent(this)));
  }
}

mojom::blink::VideoStatePtr MediaControlsTouchlessImpl::GetVideoState() {
  mojom::blink::VideoStatePtr video_state = mojom::blink::VideoState::New();
  video_state->is_muted = MediaElement().muted();
  video_state->is_fullscreen = MediaElement().IsFullscreen();
  return video_state;
}

WTF::Vector<mojom::blink::TextTrackMetadataPtr>
MediaControlsTouchlessImpl::GetTextTracks() {
  WTF::Vector<mojom::blink::TextTrackMetadataPtr> text_tracks;
  TextTrackList* track_list = MediaElement().textTracks();
  for (unsigned i = 0; i < track_list->length(); i++) {
    TextTrack* track = track_list->AnonymousIndexedGetter(i);
    if (!track->CanBeRendered())
      continue;

    mojom::blink::TextTrackMetadataPtr text_track(
        mojom::blink::TextTrackMetadata::New());
    text_track->track_index = track->TrackIndex();
    text_track->label = text_track_manager_->GetTextTrackLabel(track);
    text_tracks.push_back(std::move(text_track));
  }

  if (!text_tracks.IsEmpty()) {
    mojom::blink::TextTrackMetadataPtr text_track(
        mojom::blink::TextTrackMetadata::New());
    text_track->track_index = kTrackIndexOffValue;
    text_track->label = text_track_manager_->GetTextTrackLabel(nullptr);
    text_tracks.push_front(std::move(text_track));
  }

  return text_tracks;
}

void MediaControlsTouchlessImpl::ShowContextMenu() {
  EnsureMediaControlsMenuHost();

  mojom::blink::VideoStatePtr video_state = GetVideoState();
  WTF::Vector<mojom::blink::TextTrackMetadataPtr> text_tracks = GetTextTracks();

  WTF::Vector<mojom::blink::MenuItem> menu_items;

  // TODO(jazzhsu, https://crbug.com/942577): Populate fullscreen list entry
  // properly.
  menu_items.push_back(mojom::blink::MenuItem::FULLSCREEN);

  if (MediaElement().HasAudio())
    menu_items.push_back(mojom::blink::MenuItem::MUTE);

  if (MediaElement().SupportsSave())
    menu_items.push_back(mojom::blink::MenuItem::DOWNLOAD);

  if (!text_tracks.IsEmpty())
    menu_items.push_back(mojom::blink::MenuItem::CAPTIONS);

  media_controls_host_->ShowMediaMenu(
      std::move(menu_items), std::move(video_state), std::move(text_tracks),
      WTF::Bind(&MediaControlsTouchlessImpl::OnMediaMenuResult,
                WrapWeakPersistent(this)));
}

void MediaControlsTouchlessImpl::OnMediaMenuResult(
    mojom::blink::MenuResponsePtr reponse) {}

void MediaControlsTouchlessImpl::OnMediaControlsMenuHostConnectionError() {
  media_controls_host_.reset();
}

MediaControlsTouchlessImpl::ArrowDirection
MediaControlsTouchlessImpl::OrientArrowPress(ArrowDirection direction) {
  switch (GetOrientation()) {
    case kWebScreenOrientationUndefined:
    case kWebScreenOrientationPortraitPrimary:
      return direction;
    case kWebScreenOrientationPortraitSecondary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kDown;
        case ArrowDirection::kDown:
          return ArrowDirection::kUp;
        case ArrowDirection::kLeft:
          return ArrowDirection::kRight;
        case ArrowDirection::kRight:
          return ArrowDirection::kLeft;
      }
    case kWebScreenOrientationLandscapePrimary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kLeft;
        case ArrowDirection::kDown:
          return ArrowDirection::kRight;
        case ArrowDirection::kLeft:
          return ArrowDirection::kDown;
        case ArrowDirection::kRight:
          return ArrowDirection::kUp;
      }
    case kWebScreenOrientationLandscapeSecondary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kRight;
        case ArrowDirection::kDown:
          return ArrowDirection::kLeft;
        case ArrowDirection::kLeft:
          return ArrowDirection::kUp;
        case ArrowDirection::kRight:
          return ArrowDirection::kDown;
      }
  }
}

void MediaControlsTouchlessImpl::HandleOrientedArrowPress(
    ArrowDirection direction) {
  switch (direction) {
    case ArrowDirection::kUp:
      HandleTopButtonPress();
      break;
    case ArrowDirection::kDown:
      HandleBottomButtonPress();
      break;
    case ArrowDirection::kLeft:
      HandleLeftButtonPress();
      break;
    case ArrowDirection::kRight:
      HandleRightButtonPress();
      break;
  }
}

WebScreenOrientationType MediaControlsTouchlessImpl::GetOrientation() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return kWebScreenOrientationUndefined;

  return frame->GetChromeClient().GetScreenInfo().orientation_type;
}

void MediaControlsTouchlessImpl::HandleTopButtonPress() {
  MaybeChangeVolume(kVolumeToChangeForTouchless);
}

void MediaControlsTouchlessImpl::HandleBottomButtonPress() {
  MaybeChangeVolume(kVolumeToChangeForTouchless * -1);
}

void MediaControlsTouchlessImpl::HandleLeftButtonPress() {
  MaybeJump(kNumberOfSecondsToJumpForTouchless * -1);
}

void MediaControlsTouchlessImpl::HandleRightButtonPress() {
  MaybeJump(kNumberOfSecondsToJumpForTouchless);
}

void MediaControlsTouchlessImpl::MaybeChangeVolume(double volume_to_change) {
  double new_volume = std::max(0.0, MediaElement().volume() + volume_to_change);
  new_volume = std::min(new_volume, 1.0);
  MediaElement().setVolume(new_volume);
}

void MediaControlsTouchlessImpl::MaybeJump(int seconds) {
  double new_time = std::max(0.0, MediaElement().currentTime() + seconds);
  new_time = std::min(new_time, MediaElement().duration());
  MediaElement().setCurrentTime(new_time);
}

void MediaControlsTouchlessImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_event_listener_);
  visitor->Trace(text_track_manager_);
  visitor->Trace(orientation_lock_delegate_);
  MediaControls::Trace(visitor);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
