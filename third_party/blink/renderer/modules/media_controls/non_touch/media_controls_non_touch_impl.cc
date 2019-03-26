// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"

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
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_text_track_manager.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_overlay_element.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_media_event_listener.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// When specified as trackIndex, disable text tracks.
constexpr int kTrackIndexOffValue = -1;

// Number of seconds to jump when press left/right arrow.
constexpr int kNumberOfSecondsToJumpForNonTouch = 10;

// Amount of volume to change when press up/down arrow.
constexpr double kVolumeToChangeForNonTouch = 0.05;

// Amount of time that media controls are visible.
constexpr WTF::TimeDelta kTimeToHideMediaControls =
    TimeDelta::FromMilliseconds(3000);

const char kTransparentCSSClass[] = "transparent";

}  // namespace

enum class MediaControlsNonTouchImpl::ArrowDirection {
  kUp,
  kDown,
  kLeft,
  kRight,
};

MediaControlsNonTouchImpl::MediaControlsNonTouchImpl(
    HTMLMediaElement& media_element)
    : HTMLDivElement(media_element.GetDocument()),
      MediaControls(media_element),
      media_event_listener_(
          MakeGarbageCollected<MediaControlsNonTouchMediaEventListener>(
              media_element)),
      hide_media_controls_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaControlsNonTouchImpl::HideMediaControlsTimerFired),
      text_track_manager_(
          MakeGarbageCollected<MediaControlsTextTrackManager>(media_element)) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-non-touch"));
  media_event_listener_->AddObserver(this);
}

MediaControlsNonTouchImpl* MediaControlsNonTouchImpl::Create(
    HTMLMediaElement& media_element,
    ShadowRoot& shadow_root) {
  MediaControlsNonTouchImpl* controls =
      MakeGarbageCollected<MediaControlsNonTouchImpl>(media_element);
  MediaControlsNonTouchOverlayElement* overlay_element =
      MakeGarbageCollected<MediaControlsNonTouchOverlayElement>(*controls);

  controls->ParserAppendChild(overlay_element);

  // Controls start hidden.
  controls->MakeTransparent();

  MediaControlsResourceLoader::InjectMediaControlsUAStyleSheet();

  shadow_root.ParserAppendChild(controls);
  return controls;
}

Node::InsertionNotificationRequest MediaControlsNonTouchImpl::InsertedInto(
    ContainerNode& root) {
  media_event_listener_->Attach();
  return HTMLDivElement::InsertedInto(root);
}

void MediaControlsNonTouchImpl::RemovedFrom(ContainerNode& insertion_point) {
  HTMLDivElement::RemovedFrom(insertion_point);
  Hide();
  media_event_listener_->Detach();
}

void MediaControlsNonTouchImpl::MakeOpaque() {
  // show controls
  classList().Remove(kTransparentCSSClass);

  if (hide_media_controls_timer_.IsActive())
    StopHideMediaControlsTimer();
  StartHideMediaControlsTimer();
}

void MediaControlsNonTouchImpl::MakeTransparent() {
  // hide controls
  classList().Add(kTransparentCSSClass);
}

void MediaControlsNonTouchImpl::MaybeShow() {
  RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
}

void MediaControlsNonTouchImpl::Hide() {
  SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
}

MediaControlsNonTouchMediaEventListener&
MediaControlsNonTouchImpl::MediaEventListener() const {
  return *media_event_listener_;
}

void MediaControlsNonTouchImpl::HideMediaControlsTimerFired(TimerBase*) {
  MakeTransparent();
}

void MediaControlsNonTouchImpl::StartHideMediaControlsTimer() {
  hide_media_controls_timer_.StartOneShot(kTimeToHideMediaControls, FROM_HERE);
}

void MediaControlsNonTouchImpl::StopHideMediaControlsTimer() {
  hide_media_controls_timer_.Stop();
}

void MediaControlsNonTouchImpl::OnFocusIn() {
  if (MediaElement().ShouldShowControls())
    MakeOpaque();
}

void MediaControlsNonTouchImpl::OnKeyDown(KeyboardEvent* event) {
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

void MediaControlsNonTouchImpl::EnsureMediaControlsMenuHost() {
  if (!media_controls_host_) {
    GetDocument().GetFrame()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&media_controls_host_,
                          GetExecutionContext()->GetTaskRunner(
                              blink::TaskType::kMediaElementEvent)));
    media_controls_host_.set_connection_error_handler(WTF::Bind(
        &MediaControlsNonTouchImpl::OnMediaControlsMenuHostConnectionError,
        WrapWeakPersistent(this)));
  }
}

mojom::blink::VideoStatePtr MediaControlsNonTouchImpl::GetVideoState() {
  mojom::blink::VideoStatePtr video_state = mojom::blink::VideoState::New();
  video_state->is_muted = MediaElement().muted();
  video_state->is_fullscreen = MediaElement().IsFullscreen();
  return video_state;
}

WTF::Vector<mojom::blink::TextTrackMetadataPtr>
MediaControlsNonTouchImpl::GetTextTracks() {
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

void MediaControlsNonTouchImpl::ShowContextMenu() {
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
      WTF::Bind(&MediaControlsNonTouchImpl::OnMediaMenuResult,
                WrapWeakPersistent(this)));
}

void MediaControlsNonTouchImpl::OnMediaMenuResult(
    mojom::blink::MenuResponsePtr reponse) {}

void MediaControlsNonTouchImpl::OnMediaControlsMenuHostConnectionError() {
  media_controls_host_.reset();
}

MediaControlsNonTouchImpl::ArrowDirection
MediaControlsNonTouchImpl::OrientArrowPress(ArrowDirection direction) {
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

void MediaControlsNonTouchImpl::HandleOrientedArrowPress(
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

WebScreenOrientationType MediaControlsNonTouchImpl::GetOrientation() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return kWebScreenOrientationUndefined;

  return frame->GetChromeClient().GetScreenInfo().orientation_type;
}

void MediaControlsNonTouchImpl::HandleTopButtonPress() {
  MaybeChangeVolume(kVolumeToChangeForNonTouch);
}

void MediaControlsNonTouchImpl::HandleBottomButtonPress() {
  MaybeChangeVolume(kVolumeToChangeForNonTouch * -1);
}

void MediaControlsNonTouchImpl::HandleLeftButtonPress() {
  MaybeJump(kNumberOfSecondsToJumpForNonTouch * -1);
}

void MediaControlsNonTouchImpl::HandleRightButtonPress() {
  MaybeJump(kNumberOfSecondsToJumpForNonTouch);
}

void MediaControlsNonTouchImpl::MaybeChangeVolume(double volume_to_change) {
  double new_volume = std::max(0.0, MediaElement().volume() + volume_to_change);
  new_volume = std::min(new_volume, 1.0);
  MediaElement().setVolume(new_volume);
}

void MediaControlsNonTouchImpl::MaybeJump(int seconds) {
  double new_time = std::max(0.0, MediaElement().currentTime() + seconds);
  new_time = std::min(new_time, MediaElement().duration());
  MediaElement().setCurrentTime(new_time);
}

void MediaControlsNonTouchImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_event_listener_);
  visitor->Trace(text_track_manager_);
  MediaControls::Trace(visitor);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
