// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/video_capture/broadcasting_receiver.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace video_capture {

class PushVideoStreamSubscriptionImpl;

class VideoSourceImpl : public mojom::VideoSource {
 public:
  VideoSourceImpl(mojom::DeviceFactory* device_factory,
                  const std::string& device_id,
                  base::RepeatingClosure on_last_binding_closed_cb);
  ~VideoSourceImpl() override;

  void AddToBindingSet(mojom::VideoSourceRequest request);

  // mojom::VideoSource implementation.
  void CreatePushSubscription(
      mojom::ReceiverPtr subscriber,
      const media::VideoCaptureParams& requested_settings,
      bool force_reopen_with_new_settings,
      mojom::PushVideoStreamSubscriptionRequest subscription,
      CreatePushSubscriptionCallback callback) override;

 private:
  enum class DeviceStatus {
    kNotStarted,
    kStartingAsynchronously,
    kStarted,
  };

  void OnClientDisconnected();
  void OnCreateDeviceResponse(mojom::DeviceAccessResultCode result_code);
  void OnPushSubscriptionClosedOrDisconnectedOrDiscarded(
      PushVideoStreamSubscriptionImpl* subscription,
      base::OnceClosure done_cb);
  void StopDevice();

  mojom::DeviceFactory* const device_factory_;
  const std::string device_id_;
  mojo::BindingSet<mojom::VideoSource> bindings_;
  base::RepeatingClosure on_last_binding_closed_cb_;

  // We use the address of each instance as keys to itself.
  std::map<PushVideoStreamSubscriptionImpl*,
           std::unique_ptr<PushVideoStreamSubscriptionImpl>>
      push_subscriptions_;
  BroadcastingReceiver broadcaster_;
  std::unique_ptr<mojo::Binding<mojom::Receiver>> broadcaster_binding_;
  DeviceStatus device_status_;
  mojom::DevicePtr device_;
  media::VideoCaptureParams device_start_settings_;

  base::WeakPtrFactory<VideoSourceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoSourceImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_
