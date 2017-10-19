// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioTrackList_h
#define AudioTrackList_h

#include "core/html/track/AudioTrack.h"
#include "core/html/track/TrackListBase.h"

namespace blink {

class CORE_EXPORT AudioTrackList final : public TrackListBase<AudioTrack> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioTrackList* Create(HTMLMediaElement&);

  ~AudioTrackList() override;

  bool HasEnabledTrack() const;

  // EventTarget
  const AtomicString& InterfaceName() const override;

  void Trace(blink::Visitor* visitor) {
    TrackListBase<AudioTrack>::Trace(visitor);
  }

 private:
  explicit AudioTrackList(HTMLMediaElement&);
};

}  // namespace blink

#endif  // AudioTrackList_h
