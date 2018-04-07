// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_TEXT_TRACK_LIST_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_TEXT_TRACK_LIST_ELEMENT_H_

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_div_element.h"

namespace blink {

class Event;
class MediaControlsImpl;
class TextTrack;

class MediaControlTextTrackListElement final : public MediaControlDivElement {
 public:
  explicit MediaControlTextTrackListElement(MediaControlsImpl&);

  // Node interface.
  bool WillRespondToMouseClickEvents() override;

  // Toggle visibility of the list.
  void SetVisible(bool);

 private:
  void DefaultEventHandler(Event*) override;

  void RefreshTextTrackListMenu();

  // Creates the track element in the list when a valid track is passed in and
  // the "Off" item when the parameter is null.
  Element* CreateTextTrackListItem(TextTrack*);

  // Creates the header element of the text track list (modern only).
  Element* CreateTextTrackHeaderItem();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_TEXT_TRACK_LIST_ELEMENT_H_
