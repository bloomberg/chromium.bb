// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSession_h
#define MediaSession_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/mediasession/media_session.mojom-blink.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class ExecutionContext;
class MediaMetadata;
class MediaSessionActionHandler;

class MODULES_EXPORT MediaSession final
    : public GarbageCollectedFinalized<MediaSession>,
      public ContextClient,
      public ScriptWrappable,
      blink::mojom::blink::MediaSessionClient {
  USING_GARBAGE_COLLECTED_MIXIN(MediaSession);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(MediaSession, dispose);

 public:
  static MediaSession* create(ExecutionContext*);

  void dispose();

  void setPlaybackState(const String&);
  String playbackState();

  void setMetadata(MediaMetadata*);
  MediaMetadata* metadata() const;

  void setActionHandler(const String& action, MediaSessionActionHandler*);

  // Called by the MediaMetadata owned by |this| when it has updates. Also used
  // internally when a new MediaMetadata object is set.
  void onMetadataChanged();

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  friend class V8MediaSession;
  friend class MediaSessionTest;

  enum class ActionChangeType {
    ActionEnabled,
    ActionDisabled,
  };

  explicit MediaSession(ExecutionContext*);

  void notifyActionChange(const String& action, ActionChangeType);

  // blink::mojom::blink::MediaSessionClient implementation.
  void DidReceiveAction(blink::mojom::blink::MediaSessionAction) override;

  // Returns null when the ExecutionContext is not document.
  mojom::blink::MediaSessionService* getService();

  mojom::blink::MediaSessionPlaybackState m_playbackState;
  Member<MediaMetadata> m_metadata;
  HeapHashMap<String, TraceWrapperMember<MediaSessionActionHandler>>
      m_actionHandlers;
  mojom::blink::MediaSessionServicePtr m_service;
  mojo::Binding<blink::mojom::blink::MediaSessionClient> m_clientBinding;
};

}  // namespace blink

#endif  // MediaSession_h
