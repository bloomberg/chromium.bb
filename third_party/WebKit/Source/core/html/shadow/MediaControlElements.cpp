/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/shadow/MediaControlElements.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/InputTypeNames.h"
#include "core/dom/ClientRect.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/TimeRanges.h"
#include "core/html/media/HTMLMediaElementControlsList.h"
#include "core/html/media/HTMLMediaSource.h"
#include "core/html/media/MediaControls.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/track/TextTrackList.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LayoutSliderItem.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"
#include "public/platform/UserMetricsAction.h"
#include "public/platform/WebScreenInfo.h"

namespace blink {

using namespace HTMLNames;

namespace {

bool IsUserInteractionEvent(Event* event) {
  const AtomicString& type = event->type();
  return type == EventTypeNames::mousedown || type == EventTypeNames::mouseup ||
         type == EventTypeNames::click || type == EventTypeNames::dblclick ||
         event->IsKeyboardEvent() || event->IsTouchEvent();
}

// Sliders (the volume control and timeline) need to capture some additional
// events used when dragging the thumb.
bool IsUserInteractionEventForSlider(Event* event,
                                     LayoutObject* layout_object) {
  // It is unclear if this can be converted to isUserInteractionEvent(), since
  // mouse* events seem to be eaten during a drag anyway.  crbug.com/516416 .
  if (IsUserInteractionEvent(event))
    return true;

  // Some events are only captured during a slider drag.
  LayoutSliderItem slider = LayoutSliderItem(ToLayoutSlider(layout_object));
  // TODO(crbug.com/695459#c1): LayoutSliderItem::inDragMode is incorrectly
  // false for drags that start from the track instead of the thumb.
  // Use SliderThumbElement::m_inDragMode and
  // SliderContainerElement::m_touchStarted instead.
  if (!slider.IsNull() && !slider.InDragMode())
    return false;

  const AtomicString& type = event->type();
  return type == EventTypeNames::mouseover ||
         type == EventTypeNames::mouseout ||
         type == EventTypeNames::mousemove ||
         type == EventTypeNames::pointerover ||
         type == EventTypeNames::pointerout ||
         type == EventTypeNames::pointermove;
}

Element* ElementFromCenter(Element& element) {
  ClientRect* client_rect = element.getBoundingClientRect();
  int center_x =
      static_cast<int>((client_rect->left() + client_rect->right()) / 2);
  int center_y =
      static_cast<int>((client_rect->top() + client_rect->bottom()) / 2);

  return element.GetDocument().ElementFromPoint(center_x, center_y);
}

}  // anonymous namespace

MediaControlPlayButtonElement::MediaControlPlayButtonElement(
    MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaPlayButton) {}

MediaControlPlayButtonElement* MediaControlPlayButtonElement::Create(
    MediaControls& media_controls) {
  MediaControlPlayButtonElement* button =
      new MediaControlPlayButtonElement(media_controls);
  button->EnsureUserAgentShadowRoot();
  button->setType(InputTypeNames::button);
  button->SetShadowPseudoId(AtomicString("-webkit-media-controls-play-button"));
  return button;
}

void MediaControlPlayButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (MediaElement().paused())
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.Play"));
    else
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.Pause"));

    // Allow play attempts for plain src= media to force a reload in the error
    // state. This allows potential recovery for transient network and decoder
    // resource issues.
    const String& url = MediaElement().currentSrc().GetString();
    if (MediaElement().error() && !HTMLMediaElement::IsMediaStreamURL(url) &&
        !HTMLMediaSource::Lookup(url))
      MediaElement().load();

    MediaElement().TogglePlayState();
    UpdateDisplayType();
    event->SetDefaultHandled();
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

void MediaControlPlayButtonElement::UpdateDisplayType() {
  SetDisplayType(MediaElement().paused() ? kMediaPlayButton
                                         : kMediaPauseButton);
  UpdateOverflowString();
}

WebLocalizedString::Name
MediaControlPlayButtonElement::GetOverflowStringName() {
  if (MediaElement().paused())
    return WebLocalizedString::kOverflowMenuPlay;
  return WebLocalizedString::kOverflowMenuPause;
}

// ----------------------------

MediaControlOverlayPlayButtonElement::MediaControlOverlayPlayButtonElement(
    MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaOverlayPlayButton) {}

MediaControlOverlayPlayButtonElement*
MediaControlOverlayPlayButtonElement::Create(MediaControls& media_controls) {
  MediaControlOverlayPlayButtonElement* button =
      new MediaControlOverlayPlayButtonElement(media_controls);
  button->EnsureUserAgentShadowRoot();
  button->setType(InputTypeNames::button);
  button->SetShadowPseudoId(
      AtomicString("-webkit-media-controls-overlay-play-button"));
  return button;
}

void MediaControlOverlayPlayButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click && MediaElement().paused()) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.PlayOverlay"));
    MediaElement().Play();
    UpdateDisplayType();
    event->SetDefaultHandled();
  }
}

void MediaControlOverlayPlayButtonElement::UpdateDisplayType() {
  SetIsWanted(MediaElement().ShouldShowControls() && MediaElement().paused());
}

bool MediaControlOverlayPlayButtonElement::KeepEventInNode(Event* event) {
  return IsUserInteractionEvent(event);
}

// ----------------------------

MediaControlToggleClosedCaptionsButtonElement::
    MediaControlToggleClosedCaptionsButtonElement(MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaShowClosedCaptionsButton) {
}

MediaControlToggleClosedCaptionsButtonElement*
MediaControlToggleClosedCaptionsButtonElement::Create(
    MediaControls& media_controls) {
  MediaControlToggleClosedCaptionsButtonElement* button =
      new MediaControlToggleClosedCaptionsButtonElement(media_controls);
  button->EnsureUserAgentShadowRoot();
  button->setType(InputTypeNames::button);
  button->SetShadowPseudoId(
      AtomicString("-webkit-media-controls-toggle-closed-captions-button"));
  button->SetIsWanted(false);
  return button;
}

void MediaControlToggleClosedCaptionsButtonElement::UpdateDisplayType() {
  bool captions_visible = MediaElement().TextTracksVisible();
  SetDisplayType(captions_visible ? kMediaHideClosedCaptionsButton
                                  : kMediaShowClosedCaptionsButton);
}

void MediaControlToggleClosedCaptionsButtonElement::DefaultEventHandler(
    Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (MediaElement().textTracks()->length() == 1) {
      // If only one track exists, toggle it on/off
      if (MediaElement().textTracks()->HasShowingTracks()) {
        GetMediaControls().DisableShowingTextTracks();
      } else {
        GetMediaControls().ShowTextTrackAtIndex(0);
      }
    } else {
      GetMediaControls().ToggleTextTrackList();
    }

    UpdateDisplayType();
    event->SetDefaultHandled();
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

WebLocalizedString::Name
MediaControlToggleClosedCaptionsButtonElement::GetOverflowStringName() {
  return WebLocalizedString::kOverflowMenuCaptions;
}

// ----------------------------

MediaControlOverflowMenuButtonElement::MediaControlOverflowMenuButtonElement(
    MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaOverflowButton) {}

MediaControlOverflowMenuButtonElement*
MediaControlOverflowMenuButtonElement::Create(MediaControls& media_controls) {
  MediaControlOverflowMenuButtonElement* button =
      new MediaControlOverflowMenuButtonElement(media_controls);
  button->EnsureUserAgentShadowRoot();
  button->setType(InputTypeNames::button);
  button->SetShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-button"));
  button->SetIsWanted(false);
  return button;
}

void MediaControlOverflowMenuButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (GetMediaControls().OverflowMenuVisible())
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.OverflowClose"));
    else
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.OverflowOpen"));

    GetMediaControls().ToggleOverflowMenu();
    event->SetDefaultHandled();
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

// ----------------------------
MediaControlDownloadButtonElement::MediaControlDownloadButtonElement(
    MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaDownloadButton) {}

MediaControlDownloadButtonElement* MediaControlDownloadButtonElement::Create(
    MediaControls& media_controls) {
  MediaControlDownloadButtonElement* button =
      new MediaControlDownloadButtonElement(media_controls);
  button->EnsureUserAgentShadowRoot();
  button->setType(InputTypeNames::button);
  button->SetShadowPseudoId(
      AtomicString("-internal-media-controls-download-button"));
  button->SetIsWanted(false);
  return button;
}

WebLocalizedString::Name
MediaControlDownloadButtonElement::GetOverflowStringName() {
  return WebLocalizedString::kOverflowMenuDownload;
}

bool MediaControlDownloadButtonElement::ShouldDisplayDownloadButton() {
  const KURL& url = MediaElement().currentSrc();

  // Check page settings to see if download is disabled.
  if (GetDocument().GetPage() &&
      GetDocument().GetPage()->GetSettings().GetHideDownloadUI())
    return false;

  // URLs that lead to nowhere are ignored.
  if (url.IsNull() || url.IsEmpty())
    return false;

  // If we have no source, we can't download.
  if (MediaElement().getNetworkState() == HTMLMediaElement::kNetworkEmpty ||
      MediaElement().getNetworkState() == HTMLMediaElement::kNetworkNoSource) {
    return false;
  }

  // Local files and blobs (including MSE) should not have a download button.
  if (url.IsLocalFile() || url.ProtocolIs("blob"))
    return false;

  // MediaStream can't be downloaded.
  if (HTMLMediaElement::IsMediaStreamURL(url.GetString()))
    return false;

  // MediaSource can't be downloaded.
  if (HTMLMediaSource::Lookup(url))
    return false;

  // HLS stream shouldn't have a download button.
  if (HTMLMediaElement::IsHLSURL(url))
    return false;

  // Infinite streams don't have a clear end at which to finish the download
  // (would require adding UI to prompt for the duration to download).
  if (MediaElement().duration() == std::numeric_limits<double>::infinity())
    return false;

  // The attribute disables the download button.
  if (MediaElement().ControlsListInternal()->ShouldHideDownload()) {
    UseCounter::Count(MediaElement().GetDocument(),
                      UseCounter::kHTMLMediaElementControlsListNoDownload);
    return false;
  }

  return true;
}

void MediaControlDownloadButtonElement::SetIsWanted(bool wanted) {
  MediaControlElement::SetIsWanted(wanted);

  if (!IsWanted())
    return;

  DCHECK(IsWanted());
  if (!show_use_counted_) {
    show_use_counted_ = true;
    RecordMetrics(DownloadActionMetrics::kShown);
  }
}

void MediaControlDownloadButtonElement::DefaultEventHandler(Event* event) {
  const KURL& url = MediaElement().currentSrc();
  if (event->type() == EventTypeNames::click &&
      !(url.IsNull() || url.IsEmpty())) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.Download"));
    if (!click_use_counted_) {
      click_use_counted_ = true;
      RecordMetrics(DownloadActionMetrics::kClicked);
    }
    if (!anchor_) {
      HTMLAnchorElement* anchor = HTMLAnchorElement::Create(GetDocument());
      anchor->setAttribute(HTMLNames::downloadAttr, "");
      anchor_ = anchor;
    }
    anchor_->SetURL(url);
    anchor_->DispatchSimulatedClick(event);
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

DEFINE_TRACE(MediaControlDownloadButtonElement) {
  visitor->Trace(anchor_);
  MediaControlInputElement::Trace(visitor);
}

void MediaControlDownloadButtonElement::RecordMetrics(
    DownloadActionMetrics metric) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, download_action_histogram,
                      ("Media.Controls.Download",
                       static_cast<int>(DownloadActionMetrics::kCount)));
  download_action_histogram.Count(static_cast<int>(metric));
}

// ----------------------------

MediaControlTimelineElement::MediaControlTimelineElement(
    MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaSlider) {}

MediaControlTimelineElement* MediaControlTimelineElement::Create(
    MediaControls& media_controls) {
  MediaControlTimelineElement* timeline =
      new MediaControlTimelineElement(media_controls);
  timeline->EnsureUserAgentShadowRoot();
  timeline->setType(InputTypeNames::range);
  timeline->setAttribute(stepAttr, "any");
  timeline->SetShadowPseudoId(AtomicString("-webkit-media-controls-timeline"));
  return timeline;
}

void MediaControlTimelineElement::DefaultEventHandler(Event* event) {
  if (event->IsMouseEvent() &&
      ToMouseEvent(event)->button() !=
          static_cast<short>(WebPointerProperties::Button::kLeft))
    return;

  if (!isConnected() || !GetDocument().IsActive())
    return;

  // TODO(crbug.com/706504): These should listen for pointerdown/up.
  if (event->type() == EventTypeNames::mousedown)
    GetMediaControls().BeginScrubbing();
  if (event->type() == EventTypeNames::mouseup)
    GetMediaControls().EndScrubbing();

  // Only respond to main button of primary pointer(s).
  if (event->IsPointerEvent() && ToPointerEvent(event)->isPrimary() &&
      ToPointerEvent(event)->button() ==
          static_cast<short>(WebPointerProperties::Button::kLeft)) {
    if (event->type() == EventTypeNames::pointerdown) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.ScrubbingBegin"));
      GetMediaControls().BeginScrubbing();
      Element* thumb = UserAgentShadowRoot()->GetElementById(
          ShadowElementNames::SliderThumb());
      bool started_from_thumb = thumb && thumb == event->target()->ToNode();
      metrics_.StartGesture(started_from_thumb);
    }
    if (event->type() == EventTypeNames::pointerup) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.ScrubbingEnd"));
      GetMediaControls().EndScrubbing();
      metrics_.RecordEndGesture(TimelineWidth(), MediaElement().duration());
    }
  }

  if (event->type() == EventTypeNames::keydown) {
    metrics_.StartKey();
  }
  if (event->type() == EventTypeNames::keyup && event->IsKeyboardEvent()) {
    metrics_.RecordEndKey(TimelineWidth(), ToKeyboardEvent(event)->keyCode());
  }

  MediaControlInputElement::DefaultEventHandler(event);

  if (event->type() != EventTypeNames::input)
    return;

  double time = value().ToDouble();

  double duration = MediaElement().duration();
  // Workaround for floating point error - it's possible for this element's max
  // attribute to be rounded to a value slightly higher than the duration. If
  // this happens and scrubber is dragged near the max, seek to duration.
  if (time > duration)
    time = duration;

  metrics_.OnInput(MediaElement().currentTime(), time);

  // FIXME: This will need to take the timeline offset into consideration
  // once that concept is supported, see https://crbug.com/312699
  if (MediaElement().seekable()->Contain(time))
    MediaElement().setCurrentTime(time);

  // Provide immediate feedback (without waiting for media to seek) to make it
  // easier for user to seek to a precise time.
  GetMediaControls().UpdateCurrentTimeDisplay();
}

bool MediaControlTimelineElement::WillRespondToMouseClickEvents() {
  return isConnected() && GetDocument().IsActive();
}

void MediaControlTimelineElement::SetPosition(double current_time) {
  setValue(String::Number(current_time));

  if (LayoutObject* layout_object = this->GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

void MediaControlTimelineElement::SetDuration(double duration) {
  SetFloatingPointAttribute(maxAttr, std::isfinite(duration) ? duration : 0);

  if (LayoutObject* layout_object = this->GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

void MediaControlTimelineElement::OnPlaying() {
  Frame* frame = GetDocument().GetFrame();
  if (!frame)
    return;
  metrics_.RecordPlaying(
      frame->GetChromeClient().GetScreenInfo().orientation_type,
      MediaElement().IsFullscreen(), TimelineWidth());
}

bool MediaControlTimelineElement::KeepEventInNode(Event* event) {
  return IsUserInteractionEventForSlider(event, GetLayoutObject());
}

int MediaControlTimelineElement::TimelineWidth() {
  if (LayoutBoxModelObject* box = GetLayoutBoxModelObject())
    return box->OffsetWidth().Round();
  return 0;
}

// ----------------------------

MediaControlVolumeSliderElement::MediaControlVolumeSliderElement(
    MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaVolumeSlider) {}

MediaControlVolumeSliderElement* MediaControlVolumeSliderElement::Create(
    MediaControls& media_controls) {
  MediaControlVolumeSliderElement* slider =
      new MediaControlVolumeSliderElement(media_controls);
  slider->EnsureUserAgentShadowRoot();
  slider->setType(InputTypeNames::range);
  slider->setAttribute(stepAttr, "any");
  slider->setAttribute(maxAttr, "1");
  slider->SetShadowPseudoId(
      AtomicString("-webkit-media-controls-volume-slider"));
  return slider;
}

void MediaControlVolumeSliderElement::DefaultEventHandler(Event* event) {
  if (!isConnected() || !GetDocument().IsActive())
    return;

  MediaControlInputElement::DefaultEventHandler(event);

  if (event->type() == EventTypeNames::mousedown)
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.VolumeChangeBegin"));

  if (event->type() == EventTypeNames::mouseup)
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.VolumeChangeEnd"));

  if (event->type() == EventTypeNames::input) {
    double volume = value().ToDouble();
    MediaElement().setVolume(volume);
    MediaElement().setMuted(false);
    if (LayoutObject* layout_object = this->GetLayoutObject())
      layout_object->SetShouldDoFullPaintInvalidation();
  }
}

bool MediaControlVolumeSliderElement::WillRespondToMouseMoveEvents() {
  if (!isConnected() || !GetDocument().IsActive())
    return false;

  return MediaControlInputElement::WillRespondToMouseMoveEvents();
}

bool MediaControlVolumeSliderElement::WillRespondToMouseClickEvents() {
  if (!isConnected() || !GetDocument().IsActive())
    return false;

  return MediaControlInputElement::WillRespondToMouseClickEvents();
}

void MediaControlVolumeSliderElement::SetVolume(double volume) {
  if (value().ToDouble() == volume)
    return;

  setValue(String::Number(volume));
  if (LayoutObject* layout_object = this->GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

bool MediaControlVolumeSliderElement::KeepEventInNode(Event* event) {
  return IsUserInteractionEventForSlider(event, GetLayoutObject());
}

// ----------------------------

MediaControlFullscreenButtonElement::MediaControlFullscreenButtonElement(
    MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaEnterFullscreenButton) {}

MediaControlFullscreenButtonElement*
MediaControlFullscreenButtonElement::Create(MediaControls& media_controls) {
  MediaControlFullscreenButtonElement* button =
      new MediaControlFullscreenButtonElement(media_controls);
  button->EnsureUserAgentShadowRoot();
  button->setType(InputTypeNames::button);
  button->SetShadowPseudoId(
      AtomicString("-webkit-media-controls-fullscreen-button"));
  button->SetIsFullscreen(media_controls.MediaElement().IsFullscreen());
  button->SetIsWanted(false);
  return button;
}

void MediaControlFullscreenButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    bool is_embedded_experience_enabled =
        GetDocument().GetSettings() &&
        GetDocument().GetSettings()->GetEmbeddedMediaExperienceEnabled();
    if (MediaElement().IsFullscreen()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.ExitFullscreen"));
      if (is_embedded_experience_enabled) {
        Platform::Current()->RecordAction(UserMetricsAction(
            "Media.Controls.ExitFullscreen.EmbeddedExperience"));
      }
      GetMediaControls().ExitFullscreen();
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.EnterFullscreen"));
      if (is_embedded_experience_enabled) {
        Platform::Current()->RecordAction(UserMetricsAction(
            "Media.Controls.EnterFullscreen.EmbeddedExperience"));
      }
      GetMediaControls().EnterFullscreen();
    }
    event->SetDefaultHandled();
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

void MediaControlFullscreenButtonElement::SetIsFullscreen(bool is_fullscreen) {
  SetDisplayType(is_fullscreen ? kMediaExitFullscreenButton
                               : kMediaEnterFullscreenButton);
}

WebLocalizedString::Name
MediaControlFullscreenButtonElement::GetOverflowStringName() {
  if (MediaElement().IsFullscreen())
    return WebLocalizedString::kOverflowMenuExitFullscreen;
  return WebLocalizedString::kOverflowMenuEnterFullscreen;
}

// ----------------------------

MediaControlCastButtonElement::MediaControlCastButtonElement(
    MediaControls& media_controls,
    bool is_overlay_button)
    : MediaControlInputElement(media_controls, kMediaCastOnButton),
      is_overlay_button_(is_overlay_button) {
  if (is_overlay_button_)
    RecordMetrics(CastOverlayMetrics::kCreated);
  SetIsPlayingRemotely(false);
}

MediaControlCastButtonElement* MediaControlCastButtonElement::Create(
    MediaControls& media_controls,
    bool is_overlay_button) {
  MediaControlCastButtonElement* button =
      new MediaControlCastButtonElement(media_controls, is_overlay_button);
  button->EnsureUserAgentShadowRoot();
  button->SetShadowPseudoId(is_overlay_button
                                ? "-internal-media-controls-overlay-cast-button"
                                : "-internal-media-controls-cast-button");
  button->setType(InputTypeNames::button);
  return button;
}

void MediaControlCastButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (is_overlay_button_)
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.CastOverlay"));
    else
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.Cast"));

    if (is_overlay_button_ && !click_use_counted_) {
      click_use_counted_ = true;
      RecordMetrics(CastOverlayMetrics::kClicked);
    }
    if (MediaElement().IsPlayingRemotely()) {
      MediaElement().RequestRemotePlaybackControl();
    } else {
      MediaElement().RequestRemotePlayback();
    }
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

void MediaControlCastButtonElement::SetIsPlayingRemotely(
    bool is_playing_remotely) {
  if (is_playing_remotely) {
    if (is_overlay_button_) {
      SetDisplayType(kMediaOverlayCastOnButton);
    } else {
      SetDisplayType(kMediaCastOnButton);
    }
  } else {
    if (is_overlay_button_) {
      SetDisplayType(kMediaOverlayCastOffButton);
    } else {
      SetDisplayType(kMediaCastOffButton);
    }
  }
  UpdateOverflowString();
}

WebLocalizedString::Name
MediaControlCastButtonElement::GetOverflowStringName() {
  if (MediaElement().IsPlayingRemotely())
    return WebLocalizedString::kOverflowMenuStopCast;
  return WebLocalizedString::kOverflowMenuCast;
}

void MediaControlCastButtonElement::TryShowOverlay() {
  DCHECK(is_overlay_button_);

  SetIsWanted(true);
  if (ElementFromCenter(*this) != &MediaElement()) {
    SetIsWanted(false);
    return;
  }

  DCHECK(IsWanted());
  if (!show_use_counted_) {
    show_use_counted_ = true;
    RecordMetrics(CastOverlayMetrics::kShown);
  }
}

bool MediaControlCastButtonElement::KeepEventInNode(Event* event) {
  return IsUserInteractionEvent(event);
}

void MediaControlCastButtonElement::RecordMetrics(CastOverlayMetrics metric) {
  DCHECK(is_overlay_button_);
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, overlay_histogram,
      ("Cast.Sender.Overlay", static_cast<int>(CastOverlayMetrics::kCount)));
  overlay_histogram.Count(static_cast<int>(metric));
}

}  // namespace blink
