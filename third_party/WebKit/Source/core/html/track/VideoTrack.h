// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoTrack_h
#define VideoTrack_h

#include "core/CoreExport.h"
#include "core/html/track/TrackBase.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CORE_EXPORT VideoTrack final
    : public GarbageCollectedFinalized<VideoTrack>,
      public TrackBase,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(VideoTrack);

 public:
  static VideoTrack* Create(const String& id,
                            const AtomicString& kind,
                            const AtomicString& label,
                            const AtomicString& language,
                            bool selected) {
    return new VideoTrack(id, IsValidKindKeyword(kind) ? kind : g_empty_atom,
                          label, language, selected);
  }

  ~VideoTrack() override;
  virtual void Trace(blink::Visitor*);

  bool selected() const { return selected_; }
  void setSelected(bool);

  // Set selected to false without notifying the owner media element. Used when
  // another video track is selected, implicitly deselecting this one.
  void ClearSelected() { selected_ = false; }

  // Valid kind keywords.
  static const AtomicString& AlternativeKeyword();
  static const AtomicString& CaptionsKeyword();
  static const AtomicString& MainKeyword();
  static const AtomicString& SignKeyword();
  static const AtomicString& SubtitlesKeyword();
  static const AtomicString& CommentaryKeyword();

  static bool IsValidKindKeyword(const String&);

 private:
  VideoTrack(const String& id,
             const AtomicString& kind,
             const AtomicString& label,
             const AtomicString& language,
             bool selected);

  bool selected_;
};

DEFINE_TRACK_TYPE_CASTS(VideoTrack, WebMediaPlayer::kVideoTrack);

}  // namespace blink

#endif  // VideoTrack_h
