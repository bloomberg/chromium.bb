// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_CLIENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/mediastream/apply_constraints_request.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_processor.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class ApplyConstraintsProcessor;
class LocalFrame;

// UserMediaClient handles requests coming from the Blink MediaDevices
// object. This includes getUserMedia and enumerateDevices. It must be created,
// called and destroyed on the render thread.
class MODULES_EXPORT UserMediaClient
    : public GarbageCollected<UserMediaClient> {
 public:
  // TODO(guidou): Make all constructors private and replace with Create methods
  // that return a std::unique_ptr. This class is intended for instantiation on
  // the free store. https://crbug.com/764293
  // |frame| and its respective RenderFrame must outlive this instance.
  UserMediaClient(LocalFrame* frame,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  UserMediaClient(LocalFrame* frame,
                  UserMediaProcessor* user_media_processor,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  UserMediaClient(const UserMediaClient&) = delete;
  UserMediaClient& operator=(const UserMediaClient&) = delete;

  virtual ~UserMediaClient();

  void RequestUserMedia(UserMediaRequest* user_media_request);
  void CancelUserMediaRequest(UserMediaRequest* user_media_request);
  void ApplyConstraints(blink::ApplyConstraintsRequest* user_media_request);
  void StopTrack(MediaStreamComponent* track);
  void ContextDestroyed();

  bool IsCapturing();

#if !BUILDFLAG(IS_ANDROID)
  void FocusCapturedSurface(const String& label, bool focus);
#endif

  void Trace(Visitor*) const;

  void SetMediaDevicesDispatcherForTesting(
      mojo::PendingRemote<blink::mojom::blink::MediaDevicesDispatcherHost>
          media_devices_dispatcher);

 private:
  class Request final : public GarbageCollected<Request> {
   public:
    explicit Request(UserMediaRequest* request);
    explicit Request(blink::ApplyConstraintsRequest* request);
    explicit Request(MediaStreamComponent* request);

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    ~Request();

    UserMediaRequest* MoveUserMediaRequest();

    UserMediaRequest* user_media_request() const { return user_media_request_; }
    blink::ApplyConstraintsRequest* apply_constraints_request() const {
      return apply_constraints_request_;
    }
    MediaStreamComponent* track_to_stop() const { return track_to_stop_; }

    bool IsUserMedia() const { return !!user_media_request_; }
    bool IsApplyConstraints() const { return !!apply_constraints_request_; }
    bool IsStopTrack() const { return !!track_to_stop_; }

    void Trace(Visitor* visitor) const {
      visitor->Trace(user_media_request_);
      visitor->Trace(apply_constraints_request_);
      visitor->Trace(track_to_stop_);
    }

   private:
    Member<UserMediaRequest> user_media_request_;
    Member<blink::ApplyConstraintsRequest> apply_constraints_request_;
    Member<MediaStreamComponent> track_to_stop_;
  };

  void MaybeProcessNextRequestInfo();
  void CurrentRequestCompleted();

  void DeleteAllUserMediaRequests();

  blink::mojom::blink::MediaDevicesDispatcherHost* GetMediaDevicesDispatcher();

  // LocalFrame instance associated with the UserMediaController that
  // own this UserMediaClient.
  WeakMember<LocalFrame> frame_;

  // |user_media_processor_| is a unique_ptr for testing purposes.
  Member<UserMediaProcessor> user_media_processor_;

  // |user_media_processor_| is a unique_ptr in order to avoid compilation
  // problems in builds that do not include WebRTC.
  Member<ApplyConstraintsProcessor> apply_constraints_processor_;

  HeapMojoRemote<blink::mojom::blink::MediaDevicesDispatcherHost>
      media_devices_dispatcher_;

  // UserMedia requests are processed sequentially. |is_processing_request_|
  // is a flag that indicates if a request is being processed at a given time,
  // and |pending_request_infos_| is a list of queued requests.
  bool is_processing_request_ = false;

  HeapDeque<Member<Request>> pending_request_infos_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_CLIENT_H_
