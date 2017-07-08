// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlCastButtonElement.h"

#include "core/InputTypeNames.h"
#include "core/dom/ClientRect.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"
#include "modules/remoteplayback/RemotePlayback.h"
#include "platform/Histogram.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

Element* ElementFromCenter(Element& element) {
  ClientRect* client_rect = element.getBoundingClientRect();
  int center_x =
      static_cast<int>((client_rect->left() + client_rect->right()) / 2);
  int center_y =
      static_cast<int>((client_rect->top() + client_rect->bottom()) / 2);

  return element.GetDocument().ElementFromPoint(center_x, center_y);
}

}  // anonymous namespace

MediaControlCastButtonElement::MediaControlCastButtonElement(
    MediaControlsImpl& media_controls,
    bool is_overlay_button)
    : MediaControlInputElement(media_controls, kMediaCastOnButton),
      is_overlay_button_(is_overlay_button) {
  EnsureUserAgentShadowRoot();
  SetShadowPseudoId(is_overlay_button
                        ? "-internal-media-controls-overlay-cast-button"
                        : "-internal-media-controls-cast-button");
  setType(InputTypeNames::button);

  if (is_overlay_button_)
    RecordMetrics(CastOverlayMetrics::kCreated);
  UpdateDisplayType();
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

void MediaControlCastButtonElement::UpdateDisplayType() {
  if (IsPlayingRemotely()) {
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

bool MediaControlCastButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

WebLocalizedString::Name MediaControlCastButtonElement::GetOverflowStringName()
    const {
  if (IsPlayingRemotely())
    return WebLocalizedString::kOverflowMenuStopCast;
  return WebLocalizedString::kOverflowMenuCast;
}

bool MediaControlCastButtonElement::HasOverflowButton() const {
  return true;
}

void MediaControlCastButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (is_overlay_button_) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.CastOverlay"));
      Platform::Current()->RecordRapporURL("Media.Controls.CastOverlay",
                                           WebURL(GetDocument().Url()));
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.Cast"));
      Platform::Current()->RecordRapporURL("Media.Controls.Cast",
                                           WebURL(GetDocument().Url()));
    }

    if (is_overlay_button_ && !click_use_counted_) {
      click_use_counted_ = true;
      RecordMetrics(CastOverlayMetrics::kClicked);
    }
    RemotePlayback* remote =
        HTMLMediaElementRemotePlayback::remote(MediaElement());
    if (remote)
      remote->PromptInternal();
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

bool MediaControlCastButtonElement::KeepEventInNode(Event* event) {
  return MediaControlElementsHelper::IsUserInteractionEvent(event);
}

void MediaControlCastButtonElement::RecordMetrics(CastOverlayMetrics metric) {
  DCHECK(is_overlay_button_);
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, overlay_histogram,
      ("Cast.Sender.Overlay", static_cast<int>(CastOverlayMetrics::kCount)));
  overlay_histogram.Count(static_cast<int>(metric));
}

bool MediaControlCastButtonElement::IsPlayingRemotely() const {
  RemotePlayback* remote =
      HTMLMediaElementRemotePlayback::remote(MediaElement());
  return remote && remote->GetState() != WebRemotePlaybackState::kDisconnected;
}

}  // namespace blink
