// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageCapture_h
#define ImageCapture_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "media/capture/mojo/image_capture.mojom-blink.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/imagecapture/PhotoSettings.h"
#include "modules/mediastream/MediaTrackCapabilities.h"
#include "modules/mediastream/MediaTrackConstraintSet.h"
#include "modules/mediastream/MediaTrackSettings.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/bindings/ActiveScriptWrappable.h"

namespace blink {

class ExceptionState;
class MediaStreamTrack;
class MediaTrackConstraints;
class PhotoCapabilities;
class ScriptPromiseResolver;
class WebImageCaptureFrameGrabber;

// TODO(mcasas): Consider adding a LayoutTest checking that this class is not
// garbage collected while it has event listeners.
class MODULES_EXPORT ImageCapture final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<ImageCapture>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ImageCapture);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageCapture* Create(ExecutionContext*,
                              MediaStreamTrack*,
                              ExceptionState&);
  ~ImageCapture() override;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  MediaStreamTrack* videoStreamTrack() const { return stream_track_.Get(); }

  ScriptPromise getPhotoCapabilities(ScriptState*);
  ScriptPromise getPhotoSettings(ScriptState*);

  ScriptPromise setOptions(ScriptState*,
                           const PhotoSettings&,
                           bool trigger_take_photo = false);

  ScriptPromise takePhoto(ScriptState*);
  ScriptPromise takePhoto(ScriptState*, const PhotoSettings&);

  ScriptPromise grabFrame(ScriptState*);

  MediaTrackCapabilities& GetMediaTrackCapabilities();
  void SetMediaTrackConstraints(ScriptPromiseResolver*,
                                const HeapVector<MediaTrackConstraintSet>&);
  const MediaTrackConstraintSet& GetMediaTrackConstraints() const;
  void ClearMediaTrackConstraints(ScriptPromiseResolver*);
  void GetMediaTrackSettings(MediaTrackSettings&) const;

  // TODO(mcasas): Remove this service method, https://crbug.com/338503.
  bool HasNonImageCaptureConstraints(const MediaTrackConstraints&) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  using PromiseResolverFunction = Function<void(ScriptPromiseResolver*)>;

  ImageCapture(ExecutionContext*, MediaStreamTrack*);

  void OnMojoGetPhotoState(ScriptPromiseResolver*,
                           PromiseResolverFunction,
                           bool trigger_take_photo,
                           media::mojom::blink::PhotoStatePtr);
  void OnMojoSetOptions(ScriptPromiseResolver*,
                        PromiseResolverFunction,
                        bool trigger_take_photo,
                        bool result);
  void OnMojoTakePhoto(ScriptPromiseResolver*, media::mojom::blink::BlobPtr);

  void UpdateMediaTrackCapabilities(media::mojom::blink::PhotoStatePtr);
  void OnServiceConnectionError();

  void ResolveWithNothing(ScriptPromiseResolver*);
  void ResolveWithPhotoSettings(ScriptPromiseResolver*);
  void ResolveWithPhotoCapabilities(ScriptPromiseResolver*);
  void ResolveWithMediaTrackConstraints(MediaTrackConstraints,
                                        ScriptPromiseResolver*);

  Member<MediaStreamTrack> stream_track_;
  std::unique_ptr<WebImageCaptureFrameGrabber> frame_grabber_;
  media::mojom::blink::ImageCapturePtr service_;

  MediaTrackCapabilities capabilities_;
  MediaTrackSettings settings_;
  MediaTrackConstraintSet current_constraints_;
  PhotoSettings photo_settings_;

  Member<PhotoCapabilities> photo_capabilities_;

  HeapHashSet<Member<ScriptPromiseResolver>> service_requests_;
};

}  // namespace blink

#endif  // ImageCapture_h
