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
blink::mojom::MediaStreamRequestResult EnumTraits<
    blink::mojom::MediaStreamRequestResult,
    blink::MediaStreamRequestResult>::ToMojom(blink::MediaStreamRequestResult
                                                  result) {
  switch (result) {
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_OK:
      return blink::mojom::MediaStreamRequestResult::OK;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_PERMISSION_DENIED:
      return blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_PERMISSION_DISMISSED:
      return blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_INVALID_STATE:
      return blink::mojom::MediaStreamRequestResult::INVALID_STATE;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_NO_HARDWARE:
      return blink::mojom::MediaStreamRequestResult::NO_HARDWARE;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_INVALID_SECURITY_ORIGIN:
      return blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      return blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      return blink::mojom::MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_CAPTURE_FAILURE:
      return blink::mojom::MediaStreamRequestResult::CAPTURE_FAILURE;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED:
      return blink::mojom::MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
    case blink::MediaStreamRequestResult::
        MEDIA_DEVICE_TRACK_START_FAILURE_AUDIO:
      return blink::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO;
    case blink::MediaStreamRequestResult::
        MEDIA_DEVICE_TRACK_START_FAILURE_VIDEO:
      return blink::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_NOT_SUPPORTED:
      return blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN:
      return blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_KILL_SWITCH_ON:
      return blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON;
    case blink::MediaStreamRequestResult::MEDIA_DEVICE_SYSTEM_PERMISSION_DENIED:
      return blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED;
    default:
      break;
  }
  NOTREACHED();
  return blink::mojom::MediaStreamRequestResult::OK;
}

// static
bool EnumTraits<blink::mojom::MediaStreamRequestResult,
                blink::MediaStreamRequestResult>::
    FromMojom(blink::mojom::MediaStreamRequestResult input,
              blink::MediaStreamRequestResult* out) {
  switch (input) {
    case blink::mojom::MediaStreamRequestResult::OK:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_OK;
      return true;
    case blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_PERMISSION_DENIED;
      return true;
    case blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_PERMISSION_DISMISSED;
      return true;
    case blink::mojom::MediaStreamRequestResult::INVALID_STATE:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_INVALID_STATE;
      return true;
    case blink::mojom::MediaStreamRequestResult::NO_HARDWARE:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_NO_HARDWARE;
      return true;
    case blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      *out =
          blink::MediaStreamRequestResult::MEDIA_DEVICE_INVALID_SECURITY_ORIGIN;
      return true;
    case blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_TAB_CAPTURE_FAILURE;
      return true;
    case blink::mojom::MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      *out =
          blink::MediaStreamRequestResult::MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE;
      return true;
    case blink::mojom::MediaStreamRequestResult::CAPTURE_FAILURE:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_CAPTURE_FAILURE;
      return true;
    case blink::mojom::MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      *out = blink::MediaStreamRequestResult::
          MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
      return true;
    case blink::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      *out = blink::MediaStreamRequestResult::
          MEDIA_DEVICE_TRACK_START_FAILURE_AUDIO;
      return true;
    case blink::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      *out = blink::MediaStreamRequestResult::
          MEDIA_DEVICE_TRACK_START_FAILURE_VIDEO;
      return true;
    case blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_NOT_SUPPORTED;
      return true;
    case blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      *out =
          blink::MediaStreamRequestResult::MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN;
      return true;
    case blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON:
      *out = blink::MediaStreamRequestResult::MEDIA_DEVICE_KILL_SWITCH_ON;
      return true;
    case blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      *out = blink::MediaStreamRequestResult::
          MEDIA_DEVICE_SYSTEM_PERMISSION_DENIED;
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
