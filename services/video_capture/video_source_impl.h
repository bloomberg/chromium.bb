// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace video_capture {

class VideoSourceImpl : public mojom::VideoSource {
 public:
  VideoSourceImpl(mojom::DeviceFactory* device_factory,
                  const std::string& device_id,
                  base::RepeatingClosure on_last_binding_closed_cb);
  ~VideoSourceImpl() override;

  void AddToBindingSet(mojom::VideoSourceRequest request);

 private:
  void OnClientDisconnected();

  mojom::DeviceFactory* const device_factory_;
  const std::string device_id_;
  mojo::BindingSet<mojom::VideoSource> bindings_;
  base::RepeatingClosure on_last_binding_closed_cb_;

  base::WeakPtrFactory<VideoSourceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoSourceImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_
