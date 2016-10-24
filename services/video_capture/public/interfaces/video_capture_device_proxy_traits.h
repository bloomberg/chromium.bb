// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_DEVICE_PROXY_TYPEMAP_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_DEVICE_PROXY_TYPEMAP_H_

#include "media/capture/video_capture_types.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "services/video_capture/public/interfaces/video_capture_device_proxy.mojom.h"

namespace mojo {

template <>
struct EnumTraits<video_capture::mojom::ResolutionChangePolicy,
                  media::ResolutionChangePolicy> {
  static video_capture::mojom::ResolutionChangePolicy ToMojom(
      media::ResolutionChangePolicy policy);

  static bool FromMojom(video_capture::mojom::ResolutionChangePolicy input,
                        media::ResolutionChangePolicy* out);
};

template <>
struct EnumTraits<video_capture::mojom::PowerLineFrequency,
                  media::PowerLineFrequency> {
  static video_capture::mojom::PowerLineFrequency ToMojom(
      media::PowerLineFrequency frequency);

  static bool FromMojom(video_capture::mojom::PowerLineFrequency input,
                        media::PowerLineFrequency* out);
};

}

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_DEVICE_PROXY_TYPEMAP_H_
