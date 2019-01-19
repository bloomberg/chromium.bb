// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_BROADCASTING_RECEIVER_H_
#define SERVICES_VIDEO_CAPTURE_BROADCASTING_RECEIVER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/capture/video/video_frame_receiver.h"
#include "services/video_capture/public/mojom/receiver.mojom.h"

namespace video_capture {

// Implementation of mojom::VideoFrameReceiver that distributes frames to
// potentially multiple clients.
class BroadcastingReceiver : public mojom::Receiver {
 public:
  BroadcastingReceiver();
  ~BroadcastingReceiver() override;

  // Returns a client_id that can be used for a call to RemoveClient().
  int32_t AddClient(mojom::ReceiverPtr client);
  // Returns ownership of the client back to the caller.
  mojom::ReceiverPtr RemoveClient(int32_t client_id);

  // video_capture::mojom::Receiver:
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      int32_t frame_feedback_id,
      mojom::ScopedAccessPermissionPtr access_permission,
      media::mojom::VideoFrameInfoPtr frame_info) override;
  void OnBufferRetired(int32_t buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;

 private:
  enum class Status {
    kOnStartedHasNotYetBeenCalled,
    kOnStartedHasBeenCalled,
    kOnStartedUsingGpuDecodeHasBeenCalled,
    kOnErrorHasBeenCalled,
  };

  // Combines ownership of a media::mojom::VideoBufferHandlePtr with context
  // needed for sharing the buffer as well as temporary access permission to
  // its contents with potentially multiple consumers.
  class BufferContext {
   public:
    BufferContext(int32_t buffer_id,
                  media::mojom::VideoBufferHandlePtr buffer_handle);
    ~BufferContext();
    BufferContext(BufferContext&& other);
    BufferContext& operator=(BufferContext&& other);
    int32_t buffer_id() const { return buffer_id_; }
    void set_access_permission(
        mojom::ScopedAccessPermissionPtr access_permission) {
      access_permission_ = std::move(access_permission);
    }
    void IncreaseConsumerCount();
    void DecreaseConsumerCount();
    bool IsStillBeingConsumed() const;
    media::mojom::VideoBufferHandlePtr CloneBufferHandle();

   private:
    int32_t buffer_id_;
    media::mojom::VideoBufferHandlePtr buffer_handle_;
    // Indicates how many consumers are currently relying on
    // |access_permission_|.
    int32_t consumer_hold_count_;
    mojom::ScopedAccessPermissionPtr access_permission_;
  };

  void OnClientFinishedConsumingFrame(int32_t buffer_id);
  void OnClientDisconnected(int32_t client_id);
  BufferContext& LookupBufferContextFromBufferId(int32_t buffer_id);

  SEQUENCE_CHECKER(sequence_checker_);
  std::map<int32_t /*client_id*/, mojom::ReceiverPtr> clients_;
  std::vector<BufferContext> buffer_contexts_;
  Status status_;

  // Keeps track of the last VideoCaptureError that arrived via OnError().
  // This is used for relaying the error event to clients that connect after the
  // OnError() call has been received.
  media::VideoCaptureError error_;

  // Keeps track of the id value for the next client to be added. This member is
  // incremented each time a client is added and represents a unique identifier
  // for each client.
  int32_t next_client_id_;

  base::WeakPtrFactory<BroadcastingReceiver> weak_factory_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_BROADCASTING_RECEIVER_H_
