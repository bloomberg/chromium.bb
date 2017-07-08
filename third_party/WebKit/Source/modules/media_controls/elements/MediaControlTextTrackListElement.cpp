// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlTextTrackListElement.h"

#include "core/InputTypeNames.h"
#include "core/events/Event.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLSpanElement.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackList.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

namespace {

// When specified as trackIndex, disable text tracks.
constexpr int kTrackIndexOffValue = -1;

const QualifiedName& TrackIndexAttrName() {
  // Save the track index in an attribute to avoid holding a pointer to the text
  // track.
  DEFINE_STATIC_LOCAL(QualifiedName, track_index_attr,
                      (g_null_atom, "data-track-index", g_null_atom));
  return track_index_attr;
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

MediaControlTextTrackListElement::MediaControlTextTrackListElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaTextTrackList) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-text-track-list"));
  SetIsWanted(false);
}

bool MediaControlTextTrackListElement::WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlTextTrackListElement::SetVisible(bool visible) {
  if (visible) {
    SetIsWanted(true);
    RefreshTextTrackListMenu();
  } else {
    SetIsWanted(false);
  }
}

void MediaControlTextTrackListElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::change) {
    // Identify which input element was selected and set track to showing
    Node* target = event->target()->ToNode();
    if (!target || !target->IsElementNode())
      return;

    static_cast<MediaControlsImpl&>(GetMediaControls())
        .DisableShowingTextTracks();
    int track_index =
        ToElement(target)->GetIntegralAttribute(TrackIndexAttrName());
    if (track_index != kTrackIndexOffValue) {
      DCHECK_GE(track_index, 0);
      static_cast<MediaControlsImpl&>(GetMediaControls())
          .ShowTextTrackAtIndex(track_index);
      MediaElement().DisableAutomaticTextTrackSelection();
    }

    event->SetDefaultHandled();
  }
  MediaControlDivElement::DefaultEventHandler(event);
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
      !MediaElement().TextTracksAreReady()) {
    return;
  }

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

}  // namespace blink
