// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageCapture_h
#define ImageCapture_h

#include <memory>
#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "media/capture/mojo/image_capture.mojom-blink.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/mediastream/MediaTrackCapabilities.h"
#include "modules/mediastream/MediaTrackConstraintSet.h"
#include "platform/AsyncMethodRunner.h"

namespace blink {

class ExceptionState;
class MediaStreamTrack;
class MediaTrackSettings;
class PhotoSettings;
class ScriptPromiseResolver;
class WebImageCaptureFrameGrabber;

// TODO(mcasas): Consideradding a LayoutTest checking that this class is not
// garbage collected while it has event listeners.
class MODULES_EXPORT ImageCapture final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<ImageCapture>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ImageCapture);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageCapture* create(ExecutionContext*,
                              MediaStreamTrack*,
                              ExceptionState&);
  ~ImageCapture() override;

  // EventTarget implementation.
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  // ScriptWrappable implementation.
  bool hasPendingActivity() const final;

  // ContextLifecycleObserver
  void contextDestroyed(ExecutionContext*) override;

  MediaStreamTrack* videoStreamTrack() const { return m_streamTrack.get(); }

  ScriptPromise getPhotoCapabilities(ScriptState*);

  ScriptPromise setOptions(ScriptState*, const PhotoSettings&);

  ScriptPromise takePhoto(ScriptState*);

  ScriptPromise grabFrame(ScriptState*);

  MediaTrackCapabilities& getMediaTrackCapabilities();
  void setMediaTrackConstraints(ScriptPromiseResolver*,
                                const MediaTrackConstraintSet&);
  const MediaTrackConstraintSet& getMediaTrackConstraints() const;
  void getMediaTrackSettings(MediaTrackSettings&) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  ImageCapture(ExecutionContext*, MediaStreamTrack*);

  void onPhotoCapabilities(ScriptPromiseResolver*,
                           media::mojom::blink::PhotoCapabilitiesPtr);
  void onSetOptions(ScriptPromiseResolver*, bool);
  void onTakePhoto(ScriptPromiseResolver*, media::mojom::blink::BlobPtr);
  void onCapabilitiesUpdate(media::mojom::blink::PhotoCapabilitiesPtr);
  void onServiceConnectionError();

  Member<MediaStreamTrack> m_streamTrack;
  std::unique_ptr<WebImageCaptureFrameGrabber> m_frameGrabber;
  media::mojom::blink::ImageCapturePtr m_service;

  MediaTrackCapabilities m_capabilities;
  MediaTrackConstraintSet m_currentConstraints;

  HeapHashSet<Member<ScriptPromiseResolver>> m_serviceRequests;
};

}  // namespace blink

#endif  // ImageCapture_h
