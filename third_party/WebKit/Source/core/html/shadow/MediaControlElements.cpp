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
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLSpanElement.h"
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
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/Platform.h"
#include "public/platform/UserMetricsAction.h"
#include "public/platform/WebScreenInfo.h"

namespace blink {

using namespace HTMLNames;

namespace {

// This is the duration from mediaControls.css
const double kFadeOutDuration = 0.3;

const QualifiedName& TrackIndexAttrName() {
  // Save the track index in an attribute to avoid holding a pointer to the text
  // track.
  DEFINE_STATIC_LOCAL(QualifiedName, track_index_attr,
                      (g_null_atom, "data-track-index", g_null_atom));
  return track_index_attr;
}

// When specified as trackIndex, disable text tracks.
const int kTrackIndexOffValue = -1;

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

bool HasDuplicateLabel(TextTrack* current_track) {
  DCHECK(current_track);
  TextTrackList* track_list = current_track->TrackList();
  // The runtime of this method is quadratic but since there are usually very
  // few text tracks it won't affect the performance much.
  String current_track_label = current_track->label();
  for (unsigned i = 0; i < track_list->length(); i++) {
    TextTrack* track = track_list->AnonymousIndexedGetter(i);
    if (current_track != track && current_track_label == track->label())
      return true;
  }
  return false;
}

}  // anonymous namespace

MediaControlPanelElement::MediaControlPanelElement(
    MediaControls& media_controls)
    : MediaControlDivElement(media_controls, kMediaControlsPanel),
      is_displayed_(false),
      opaque_(true),
      transition_timer_(TaskRunnerHelper::Get(TaskType::kUnspecedTimer,
                                              &media_controls.OwnerDocument()),
                        this,
                        &MediaControlPanelElement::TransitionTimerFired) {}

MediaControlPanelElement* MediaControlPanelElement::Create(
    MediaControls& media_controls) {
  MediaControlPanelElement* panel =
      new MediaControlPanelElement(media_controls);
  panel->SetShadowPseudoId(AtomicString("-webkit-media-controls-panel"));
  return panel;
}

void MediaControlPanelElement::DefaultEventHandler(Event* event) {
  // Suppress the media element activation behavior (toggle play/pause) when
  // any part of the control panel is clicked.
  if (event->type() == EventTypeNames::click) {
    event->SetDefaultHandled();
    return;
  }
  HTMLDivElement::DefaultEventHandler(event);
}

void MediaControlPanelElement::StartTimer() {
  StopTimer();

  // The timer is required to set the property display:'none' on the panel,
  // such that captions are correctly displayed at the bottom of the video
  // at the end of the fadeout transition.
  // FIXME: Racing a transition with a setTimeout like this is wrong.
  transition_timer_.StartOneShot(kFadeOutDuration, BLINK_FROM_HERE);
}

void MediaControlPanelElement::StopTimer() {
  transition_timer_.Stop();
}

void MediaControlPanelElement::TransitionTimerFired(TimerBase*) {
  if (!opaque_)
    SetIsWanted(false);

  StopTimer();
}

void MediaControlPanelElement::DidBecomeVisible() {
  DCHECK(is_displayed_ && opaque_);
  MediaElement().MediaControlsDidBecomeVisible();
}

bool MediaControlPanelElement::IsOpaque() const {
  return opaque_;
}

void MediaControlPanelElement::MakeOpaque() {
  if (opaque_)
    return;

  SetInlineStyleProperty(CSSPropertyOpacity, 1.0,
                         CSSPrimitiveValue::UnitType::kNumber);
  opaque_ = true;

  if (is_displayed_) {
    SetIsWanted(true);
    DidBecomeVisible();
  }
}

void MediaControlPanelElement::MakeTransparent() {
  if (!opaque_)
    return;

  SetInlineStyleProperty(CSSPropertyOpacity, 0.0,
                         CSSPrimitiveValue::UnitType::kNumber);

  opaque_ = false;
  StartTimer();
}

void MediaControlPanelElement::SetIsDisplayed(bool is_displayed) {
  if (is_displayed_ == is_displayed)
    return;

  is_displayed_ = is_displayed;
  if (is_displayed_ && opaque_)
    DidBecomeVisible();
}

bool MediaControlPanelElement::KeepEventInNode(Event* event) {
  return IsUserInteractionEvent(event);
}

// ----------------------------

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

MediaControlTextTrackListElement::MediaControlTextTrackListElement(
    MediaControls& media_controls)
    : MediaControlDivElement(media_controls, kMediaTextTrackList) {}

MediaControlTextTrackListElement* MediaControlTextTrackListElement::Create(
    MediaControls& media_controls) {
  MediaControlTextTrackListElement* element =
      new MediaControlTextTrackListElement(media_controls);
  element->SetShadowPseudoId(
      AtomicString("-internal-media-controls-text-track-list"));
  element->SetIsWanted(false);
  return element;
}

void MediaControlTextTrackListElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::change) {
    // Identify which input element was selected and set track to showing
    Node* target = event->target()->ToNode();
    if (!target || !target->IsElementNode())
      return;

    GetMediaControls().DisableShowingTextTracks();
    int track_index =
        ToElement(target)->GetIntegralAttribute(TrackIndexAttrName());
    if (track_index != kTrackIndexOffValue) {
      DCHECK_GE(track_index, 0);
      GetMediaControls().ShowTextTrackAtIndex(track_index);
      MediaElement().DisableAutomaticTextTrackSelection();
    }

    event->SetDefaultHandled();
  }
  MediaControlDivElement::DefaultEventHandler(event);
}

void MediaControlTextTrackListElement::SetVisible(bool visible) {
  if (visible) {
    SetIsWanted(true);
    RefreshTextTrackListMenu();
  } else {
    SetIsWanted(false);
  }
}

String MediaControlTextTrackListElement::GetTextTrackLabel(TextTrack* track) {
  if (!track) {
    return MediaElement().GetLocale().QueryString(
        WebLocalizedString::kTextTracksOff);
  }

  String track_label = track->label();

  if (track_label.IsEmpty())
    track_label = track->language();

  if (track_label.IsEmpty()) {
    track_label = String(MediaElement().GetLocale().QueryString(
        WebLocalizedString::kTextTracksNoLabel,
        String::Number(track->TrackIndex() + 1)));
  }

  return track_label;
}

// TextTrack parameter when passed in as a nullptr, creates the "Off" list item
// in the track list.
Element* MediaControlTextTrackListElement::CreateTextTrackListItem(
    TextTrack* track) {
  int track_index = track ? track->TrackIndex() : kTrackIndexOffValue;
  HTMLLabelElement* track_item = HTMLLabelElement::Create(GetDocument());
  track_item->SetShadowPseudoId(
      AtomicString("-internal-media-controls-text-track-list-item"));
  HTMLInputElement* track_item_input =
      HTMLInputElement::Create(GetDocument(), false);
  track_item_input->SetShadowPseudoId(
      AtomicString("-internal-media-controls-text-track-list-item-input"));
  track_item_input->setType(InputTypeNames::checkbox);
  track_item_input->SetIntegralAttribute(TrackIndexAttrName(), track_index);
  if (!MediaElement().TextTracksVisible()) {
    if (!track)
      track_item_input->setChecked(true);
  } else {
    // If there are multiple text tracks set to showing, they must all have
    // checkmarks displayed.
    if (track && track->mode() == TextTrack::ShowingKeyword())
      track_item_input->setChecked(true);
  }

  track_item->AppendChild(track_item_input);
  String track_label = GetTextTrackLabel(track);
  track_item->AppendChild(Text::Create(GetDocument(), track_label));
  // Add a track kind marker icon if there are multiple tracks with the same
  // label or if the track has no label.
  if (track && (track->label().IsEmpty() || HasDuplicateLabel(track))) {
    HTMLSpanElement* track_kind_marker = HTMLSpanElement::Create(GetDocument());
    if (track->kind() == track->CaptionsKeyword()) {
      track_kind_marker->SetShadowPseudoId(AtomicString(
          "-internal-media-controls-text-track-list-kind-captions"));
    } else {
      DCHECK_EQ(track->kind(), track->SubtitlesKeyword());
      track_kind_marker->SetShadowPseudoId(AtomicString(
          "-internal-media-controls-text-track-list-kind-subtitles"));
    }
    track_item->AppendChild(track_kind_marker);
  }
  return track_item;
}

void MediaControlTextTrackListElement::RefreshTextTrackListMenu() {
  if (!MediaElement().HasClosedCaptions() ||
      !MediaElement().TextTracksAreReady())
    return;

  EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
  RemoveChildren(kOmitSubtreeModifiedEvent);

  // Construct a menu for subtitles and captions.  Pass in a nullptr to
  // createTextTrackListItem to create the "Off" track item.
  AppendChild(CreateTextTrackListItem(nullptr));

  TextTrackList* track_list = MediaElement().textTracks();
  for (unsigned i = 0; i < track_list->length(); i++) {
    TextTrack* track = track_list->AnonymousIndexedGetter(i);
    if (!track->CanBeRendered())
      continue;
    AppendChild(CreateTextTrackListItem(track));
  }
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
MediaControlOverflowMenuListElement::MediaControlOverflowMenuListElement(
    MediaControls& media_controls)
    : MediaControlDivElement(media_controls, kMediaOverflowList) {}

MediaControlOverflowMenuListElement*
MediaControlOverflowMenuListElement::Create(MediaControls& media_controls) {
  MediaControlOverflowMenuListElement* element =
      new MediaControlOverflowMenuListElement(media_controls);
  element->SetIsWanted(false);
  element->SetShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-menu-list"));
  return element;
}

void MediaControlOverflowMenuListElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click)
    event->SetDefaultHandled();

  MediaControlDivElement::DefaultEventHandler(event);
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
