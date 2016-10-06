// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSession_h
#define MediaSession_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/mediasession/media_session.mojom-blink.h"
#include <memory>

namespace blink {

class MediaMetadata;
class ScriptState;

class MODULES_EXPORT MediaSession final
    : public GarbageCollectedFinalized<MediaSession>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaSession* create();

  void setMetadata(ScriptState*, MediaMetadata*);
  MediaMetadata* metadata(ScriptState*) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class MediaSessionTest;

  MediaSession();

  // Returns null when the ExecutionContext is not document.
  mojom::blink::MediaSessionService* getService(ScriptState*);

  Member<MediaMetadata> m_metadata;
  mojom::blink::MediaSessionServicePtr m_service;
};

}  // namespace blink

#endif  // MediaSession_h
