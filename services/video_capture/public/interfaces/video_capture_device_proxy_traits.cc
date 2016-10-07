// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/interfaces/video_capture_device_proxy_traits.h"

namespace mojo {

// static
video_capture::mojom::ResolutionChangePolicy
EnumTraits<video_capture::mojom::ResolutionChangePolicy,
           media::ResolutionChangePolicy>::ToMojom(media::ResolutionChangePolicy
                                                       input) {
  switch (input) {
    case media::RESOLUTION_POLICY_FIXED_RESOLUTION:
      return video_capture::mojom::ResolutionChangePolicy::FIXED_RESOLUTION;
    case media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO:
      return video_capture::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
    case media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT:
      return video_capture::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  }
  NOTREACHED();
  return video_capture::mojom::ResolutionChangePolicy::FIXED_RESOLUTION;
}

// static
bool EnumTraits<video_capture::mojom::ResolutionChangePolicy,
                media::ResolutionChangePolicy>::
    FromMojom(video_capture::mojom::ResolutionChangePolicy input,
              media::ResolutionChangePolicy* output) {
  switch (input) {
    case video_capture::mojom::ResolutionChangePolicy::FIXED_RESOLUTION:
      *output = media::RESOLUTION_POLICY_FIXED_RESOLUTION;
      return true;
    case video_capture::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      *output = media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;
      return true;
    case video_capture::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      *output = media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
video_capture::mojom::PowerLineFrequency EnumTraits<
    video_capture::mojom::PowerLineFrequency,
    media::PowerLineFrequency>::ToMojom(media::PowerLineFrequency input) {
  switch (input) {
    case media::PowerLineFrequency::FREQUENCY_DEFAULT:
      return video_capture::mojom::PowerLineFrequency::DEFAULT;
    case media::PowerLineFrequency::FREQUENCY_50HZ:
      return video_capture::mojom::PowerLineFrequency::HZ_50;
    case media::PowerLineFrequency::FREQUENCY_60HZ:
      return video_capture::mojom::PowerLineFrequency::HZ_60;
  }
  NOTREACHED();
  return video_capture::mojom::PowerLineFrequency::DEFAULT;
}

// static
bool EnumTraits<video_capture::mojom::PowerLineFrequency,
                media::PowerLineFrequency>::
    FromMojom(video_capture::mojom::PowerLineFrequency input,
              media::PowerLineFrequency* output) {
  switch (input) {
    case video_capture::mojom::PowerLineFrequency::DEFAULT:
      *output = media::PowerLineFrequency::FREQUENCY_DEFAULT;
      return true;
    case video_capture::mojom::PowerLineFrequency::HZ_50:
      *output = media::PowerLineFrequency::FREQUENCY_50HZ;
      return true;
    case video_capture::mojom::PowerLineFrequency::HZ_60:
      *output = media::PowerLineFrequency::FREQUENCY_60HZ;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
