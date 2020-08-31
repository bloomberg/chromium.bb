// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_devices_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
blink::mojom::MediaDeviceType
EnumTraits<blink::mojom::MediaDeviceType, blink::MediaDeviceType>::ToMojom(
    blink::MediaDeviceType type) {
  switch (type) {
    case blink::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return blink::mojom::MediaDeviceType::MEDIA_AUDIO_INPUT;
    case blink::MediaDeviceType::MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return blink::mojom::MediaDeviceType::MEDIA_VIDEO_INPUT;
    case blink::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      return blink::mojom::MediaDeviceType::MEDIA_AUDIO_OUTPUT;
    default:
      break;
  }
  NOTREACHED();
  return blink::mojom::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES;
}

// static
bool EnumTraits<blink::mojom::MediaDeviceType, blink::MediaDeviceType>::
    FromMojom(blink::mojom::MediaDeviceType input,
              blink::MediaDeviceType* out) {
  switch (input) {
    case blink::mojom::MediaDeviceType::MEDIA_AUDIO_INPUT:
      *out = blink::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_INPUT;
      return true;
    case blink::mojom::MediaDeviceType::MEDIA_VIDEO_INPUT:
      *out = blink::MediaDeviceType::MEDIA_DEVICE_TYPE_VIDEO_INPUT;
      return true;
    case blink::mojom::MediaDeviceType::MEDIA_AUDIO_OUTPUT:
      *out = blink::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
      return true;
    default:
      break;
  }
  NOTREACHED();
  return false;
}

// static
blink::mojom::FacingMode
EnumTraits<blink::mojom::FacingMode, media::VideoFacingMode>::ToMojom(
    media::VideoFacingMode facing_mode) {
  switch (facing_mode) {
    case media::MEDIA_VIDEO_FACING_NONE:
      return blink::mojom::FacingMode::NONE;
    case media::MEDIA_VIDEO_FACING_USER:
      return blink::mojom::FacingMode::USER;
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return blink::mojom::FacingMode::ENVIRONMENT;
    default:
      break;
  }
  NOTREACHED();
  return blink::mojom::FacingMode::NONE;
}

// static
bool EnumTraits<blink::mojom::FacingMode, media::VideoFacingMode>::FromMojom(
    blink::mojom::FacingMode input,
    media::VideoFacingMode* out) {
  switch (input) {
    case blink::mojom::FacingMode::NONE:
      *out = media::MEDIA_VIDEO_FACING_NONE;
      return true;
    case blink::mojom::FacingMode::USER:
      *out = media::MEDIA_VIDEO_FACING_USER;
      return true;
    case blink::mojom::FacingMode::ENVIRONMENT:
      *out = media::MEDIA_VIDEO_FACING_ENVIRONMENT;
      return true;
    default:
      break;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<blink::mojom::MediaDeviceInfoDataView,
                  blink::WebMediaDeviceInfo>::
    Read(blink::mojom::MediaDeviceInfoDataView input,
         blink::WebMediaDeviceInfo* out) {
  if (!input.ReadDeviceId(&out->device_id))
    return false;
  if (!input.ReadLabel(&out->label))
    return false;
  if (!input.ReadGroupId(&out->group_id))
    return false;
  return true;
}

}  // namespace mojo
