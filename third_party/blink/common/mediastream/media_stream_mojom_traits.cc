// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_stream_mojom_traits.h"

#include "base/logging.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/capture/mojom/video_capture_types_mojom_traits.h"
#include "media/mojo/interfaces/display_media_information.mojom.h"

namespace mojo {

// static
blink::mojom::MediaStreamType
EnumTraits<blink::mojom::MediaStreamType, blink::MediaStreamType>::ToMojom(
    blink::MediaStreamType type) {
  switch (type) {
    case blink::MediaStreamType::MEDIA_NO_SERVICE:
      return blink::mojom::MediaStreamType::MEDIA_NO_SERVICE;
    case blink::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE:
      return blink::mojom::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE;
    case blink::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE:
      return blink::mojom::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE;
    case blink::MediaStreamType::MEDIA_GUM_TAB_AUDIO_CAPTURE:
      return blink::mojom::MediaStreamType::MEDIA_GUM_TAB_AUDIO_CAPTURE;
    case blink::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE:
      return blink::mojom::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE;
    case blink::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE:
      return blink::mojom::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE;
    case blink::MediaStreamType::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE:
      return blink::mojom::MediaStreamType::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE;
    case blink::MediaStreamType::MEDIA_DISPLAY_VIDEO_CAPTURE:
      return blink::mojom::MediaStreamType::MEDIA_DISPLAY_VIDEO_CAPTURE;
    case blink::MediaStreamType::MEDIA_DISPLAY_AUDIO_CAPTURE:
      return blink::mojom::MediaStreamType::MEDIA_DISPLAY_AUDIO_CAPTURE;
    case blink::MediaStreamType::NUM_MEDIA_TYPES:
      return blink::mojom::MediaStreamType::NUM_MEDIA_TYPES;
  }
  NOTREACHED();
  return blink::mojom::MediaStreamType::MEDIA_NO_SERVICE;
}

// static
bool EnumTraits<blink::mojom::MediaStreamType, blink::MediaStreamType>::
    FromMojom(blink::mojom::MediaStreamType input,
              blink::MediaStreamType* out) {
  switch (input) {
    case blink::mojom::MediaStreamType::MEDIA_NO_SERVICE:
      *out = blink::MediaStreamType::MEDIA_NO_SERVICE;
      return true;
    case blink::mojom::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE:
      *out = blink::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE;
      return true;
    case blink::mojom::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE:
      *out = blink::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE;
      return true;
    case blink::mojom::MediaStreamType::MEDIA_GUM_TAB_AUDIO_CAPTURE:
      *out = blink::MediaStreamType::MEDIA_GUM_TAB_AUDIO_CAPTURE;
      return true;
    case blink::mojom::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE:
      *out = blink::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE;
      return true;
    case blink::mojom::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE:
      *out = blink::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE;
      return true;
    case blink::mojom::MediaStreamType::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE:
      *out = blink::MediaStreamType::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE;
      return true;
    case blink::mojom::MediaStreamType::MEDIA_DISPLAY_VIDEO_CAPTURE:
      *out = blink::MediaStreamType::MEDIA_DISPLAY_VIDEO_CAPTURE;
      return true;
    case blink::mojom::MediaStreamType::MEDIA_DISPLAY_AUDIO_CAPTURE:
      *out = blink::MediaStreamType::MEDIA_DISPLAY_AUDIO_CAPTURE;
      return true;
    case blink::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      *out = blink::MediaStreamType::NUM_MEDIA_TYPES;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<blink::mojom::MediaStreamDeviceDataView,
                  blink::MediaStreamDevice>::
    Read(blink::mojom::MediaStreamDeviceDataView input,
         blink::MediaStreamDevice* out) {
  if (!input.ReadType(&out->type))
    return false;
  if (!input.ReadId(&out->id))
    return false;
  if (!input.ReadVideoFacing(&out->video_facing))
    return false;
  if (!input.ReadGroupId(&out->group_id))
    return false;
  if (!input.ReadMatchedOutputDeviceId(&out->matched_output_device_id))
    return false;
  if (!input.ReadName(&out->name))
    return false;
  if (!input.ReadInput(&out->input))
    return false;
  out->session_id = input.session_id();
  if (!input.ReadDisplayMediaInfo(&out->display_media_info))
    return false;
  return true;
}

// static
bool StructTraits<blink::mojom::TrackControlsDataView, blink::TrackControls>::
    Read(blink::mojom::TrackControlsDataView input, blink::TrackControls* out) {
  out->requested = input.requested();
  if (!input.ReadStreamType(&out->stream_type))
    return false;
  if (!input.ReadDeviceId(&out->device_id))
    return false;
  return true;
}

// static
bool StructTraits<blink::mojom::StreamControlsDataView, blink::StreamControls>::
    Read(blink::mojom::StreamControlsDataView input,
         blink::StreamControls* out) {
  if (!input.ReadAudio(&out->audio))
    return false;
  if (!input.ReadVideo(&out->video))
    return false;
#if DCHECK_IS_ON()
  if (input.hotword_enabled() || input.disable_local_echo())
    DCHECK(out->audio.requested);
#endif
  out->hotword_enabled = input.hotword_enabled();
  out->disable_local_echo = input.disable_local_echo();
  return true;
}

}  // namespace mojo
