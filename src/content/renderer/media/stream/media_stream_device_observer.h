// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_DEVICE_OBSERVER_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_DEVICE_OBSERVER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace blink {
class MediaStreamDispatcherEventHandler;
}

namespace content {

// This class implements a Mojo object that receives device stopped
// notifications and forwards them to blink::MediaStreamDispatcherEventHandler.
class CONTENT_EXPORT MediaStreamDeviceObserver
    : public RenderFrameObserver,
      public blink::mojom::MediaStreamDeviceObserver {
 public:
  explicit MediaStreamDeviceObserver(RenderFrame* render_frame);
  ~MediaStreamDeviceObserver() override;

  // Get all the media devices of video capture, e.g. webcam. This is the set
  // of devices that should be suspended when the content frame is no longer
  // being shown to the user.
  blink::MediaStreamDevices GetNonScreenCaptureDevices();

  void AddStream(const std::string& label,
                 const blink::MediaStreamDevices& audio_devices,
                 const blink::MediaStreamDevices& video_devices,
                 const base::WeakPtr<blink::MediaStreamDispatcherEventHandler>&
                     event_handler);
  void AddStream(const std::string& label,
                 const blink::MediaStreamDevice& device);
  bool RemoveStream(const std::string& label);
  void RemoveStreamDevice(const blink::MediaStreamDevice& device);

  // Get the video session_id given a label. The label identifies a stream.
  int video_session_id(const std::string& label);
  // Returns an audio session_id given a label.
  int audio_session_id(const std::string& label);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDeviceObserverTest,
                           GetNonScreenCaptureDevices);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDeviceObserverTest, OnDeviceStopped);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDeviceObserverTest, OnDeviceChanged);

  // Private class for keeping track of opened devices and who have
  // opened it.
  struct Stream;

  // RenderFrameObserver override.
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void OnDestruct() override;

  // mojom::MediaStreamDeviceObserver implementation.
  void OnDeviceStopped(const std::string& label,
                       const blink::MediaStreamDevice& device) override;
  void OnDeviceChanged(const std::string& label,
                       const blink::MediaStreamDevice& old_device,
                       const blink::MediaStreamDevice& new_device) override;

  void BindMediaStreamDeviceObserverRequest(
      blink::mojom::MediaStreamDeviceObserverRequest request);

  mojo::Binding<blink::mojom::MediaStreamDeviceObserver> binding_;

  // Used for DCHECKs so methods calls won't execute in the wrong thread.
  THREAD_CHECKER(thread_checker_);

  using LabelStreamMap = std::map<std::string, Stream>;
  LabelStreamMap label_stream_map_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDeviceObserver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_DEVICE_OBSERVER_H_
