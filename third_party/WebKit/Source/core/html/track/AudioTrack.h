// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioTrack_h
#define AudioTrack_h

#include "core/CoreExport.h"
#include "core/html/track/TrackBase.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CORE_EXPORT AudioTrack final
    : public GarbageCollectedFinalized<AudioTrack>,
      public TrackBase,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(AudioTrack);

 public:
  static AudioTrack* Create(const String& id,
                            const AtomicString& kind,
                            const AtomicString& label,
                            const AtomicString& language,
                            bool enabled) {
    return new AudioTrack(id, IsValidKindKeyword(kind) ? kind : g_empty_atom,
                          label, language, enabled);
  }

  ~AudioTrack() override;
  virtual void Trace(blink::Visitor*);

  bool enabled() const { return enabled_; }
  void setEnabled(bool);

  // Valid kind keywords.
  static const AtomicString& AlternativeKeyword();
  static const AtomicString& DescriptionsKeyword();
  static const AtomicString& MainKeyword();
  static const AtomicString& MainDescriptionsKeyword();
  static const AtomicString& TranslationKeyword();
  static const AtomicString& CommentaryKeyword();

  static bool IsValidKindKeyword(const String&);

 private:
  AudioTrack(const String& id,
             const AtomicString& kind,
             const AtomicString& label,
             const AtomicString& language,
             bool enabled);

  bool enabled_;
};

DEFINE_TRACK_TYPE_CASTS(AudioTrack, WebMediaPlayer::kAudioTrack);

}  // namespace blink

#endif  // AudioTrack_h
