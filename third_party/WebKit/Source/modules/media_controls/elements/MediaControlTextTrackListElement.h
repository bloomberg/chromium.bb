// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlTextTrackListElement_h
#define MediaControlTextTrackListElement_h

#include "modules/media_controls/elements/MediaControlDivElement.h"

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

  // Returns the label for the track when a valid track is passed in and "Off"
  // when the parameter is null.
  String GetTextTrackLabel(TextTrack*);

  // Creates the track element in the list when a valid track is passed in and
  // the "Off" item when the parameter is null.
  Element* CreateTextTrackListItem(TextTrack*);
};

}  // namespace blink

#endif  // MediaControlTextTrackListElement_h
