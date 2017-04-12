// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutomaticTrackSelection_h
#define AutomaticTrackSelection_h

#include "core/html/track/TextTrackKindUserPreference.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class TextTrackList;
class TrackGroup;

class AutomaticTrackSelection {
  STACK_ALLOCATED();

 public:
  struct Configuration {
    DISALLOW_NEW();
    Configuration()
        : disable_currently_enabled_tracks(false),
          force_enable_subtitle_or_caption_track(false),
          text_track_kind_user_preference(
              TextTrackKindUserPreference::kDefault) {}

    bool disable_currently_enabled_tracks;
    bool force_enable_subtitle_or_caption_track;
    TextTrackKindUserPreference text_track_kind_user_preference;
  };

  AutomaticTrackSelection(const Configuration&);

  void Perform(TextTrackList&);

 private:
  void PerformAutomaticTextTrackSelection(const TrackGroup&);
  void EnableDefaultMetadataTextTracks(const TrackGroup&);
  const AtomicString& PreferredTrackKind() const;

  const Configuration configuration_;
};

}  // namespace blink

#endif  // AutomaticTrackSelection_h
