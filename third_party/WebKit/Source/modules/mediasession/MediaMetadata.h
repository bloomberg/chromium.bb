// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaMetadata_h
#define MediaMetadata_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class MediaImage;
class MediaMetadataInit;
class MediaSession;

// Implementation of MediaMetadata interface from the Media Session API.
// The MediaMetadata object is linked to a MediaSession that owns it. When one
// of its properties are updated, the object will notify its MediaSession if
// any. The notification will be made asynchronously in order to combine changes
// made inside the same event loop. When a MediaMetadata is created and assigned
// to a MediaSession, the MediaSession will automatically update.
class MODULES_EXPORT MediaMetadata final
    : public GarbageCollectedFinalized<MediaMetadata>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaMetadata* create(ExecutionContext*, const MediaMetadataInit&);

  String title() const;
  String artist() const;
  String album() const;
  const HeapVector<Member<MediaImage>>& artwork() const;

  void setTitle(const String&);
  void setArtist(const String&);
  void setAlbum(const String&);
  void setArtwork(const HeapVector<Member<MediaImage>>&);

  // Called by MediaSession to associate or de-associate itself.
  void setSession(MediaSession*);

  DECLARE_VIRTUAL_TRACE();

 private:
  MediaMetadata(ExecutionContext*, const MediaMetadataInit&);

  // Called when one of the metadata fields is updated from script. It will
  // notify the session asynchronously in order to bundle multiple call in one
  // notification.
  void notifySessionAsync();

  // Called asynchronously after at least one field of MediaMetadata has been
  // modified.
  void notifySessionTimerFired(TimerBase*);

  String m_title;
  String m_artist;
  String m_album;
  HeapVector<Member<MediaImage>> m_artwork;

  Member<MediaSession> m_session;
  Timer<MediaMetadata> m_notifySessionTimer;
};

}  // namespace blink

#endif  // MediaMetadata_h
