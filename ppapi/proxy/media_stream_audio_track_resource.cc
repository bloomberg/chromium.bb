// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/media_stream_audio_track_resource.h"

#include "ppapi/proxy/audio_frame_resource.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

MediaStreamAudioTrackResource::MediaStreamAudioTrackResource(
    Connection connection,
    PP_Instance instance,
    int pending_renderer_id,
    const std::string& id)
    : MediaStreamTrackResourceBase(
        connection, instance, pending_renderer_id, id),
      get_frame_output_(NULL) {
}

MediaStreamAudioTrackResource::~MediaStreamAudioTrackResource() {
  Close();
}

thunk::PPB_MediaStreamAudioTrack_API*
MediaStreamAudioTrackResource::AsPPB_MediaStreamAudioTrack_API() {
  return this;
}

PP_Var MediaStreamAudioTrackResource::GetId() {
  return StringVar::StringToPPVar(id());
}

PP_Bool MediaStreamAudioTrackResource::HasEnded() {
  return PP_FromBool(has_ended());
}

int32_t MediaStreamAudioTrackResource::Configure(
    const int32_t attrib_list[],
    scoped_refptr<TrackedCallback> callback) {
  // TODO(penghuang): Implement this function.
  return PP_ERROR_NOTSUPPORTED;
}

int32_t MediaStreamAudioTrackResource::GetAttrib(
    PP_MediaStreamAudioTrack_Attrib attrib,
    int32_t* value) {
  // TODO(penghuang): Implement this function.
  return PP_ERROR_NOTSUPPORTED;
}

int32_t MediaStreamAudioTrackResource::GetFrame(
    PP_Resource* frame,
    scoped_refptr<TrackedCallback> callback) {
  if (has_ended())
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(get_frame_callback_))
    return PP_ERROR_INPROGRESS;

  *frame = GetAudioFrame();
  if (*frame)
    return PP_OK;

  // TODO(penghuang): Use the callback as hints to determine which thread will
  // use the resource, so we could deliver frames to the target thread directly
  // for better performance.
  get_frame_output_ = frame;
  get_frame_callback_ = callback;
  return PP_OK_COMPLETIONPENDING;
}

int32_t MediaStreamAudioTrackResource::RecycleFrame(PP_Resource frame) {
  FrameMap::iterator it = frames_.find(frame);
  if (it == frames_.end())
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<AudioFrameResource> frame_resource = it->second;
  frames_.erase(it);

  if (has_ended())
    return PP_OK;

  DCHECK_GE(frame_resource->GetBufferIndex(), 0);

  SendEnqueueBufferMessageToHost(frame_resource->GetBufferIndex());
  frame_resource->Invalidate();
  return PP_OK;
}

void MediaStreamAudioTrackResource::Close() {
  if (has_ended())
    return;

  if (TrackedCallback::IsPending(get_frame_callback_)) {
    *get_frame_output_ = 0;
    get_frame_callback_->PostAbort();
    get_frame_callback_ = NULL;
    get_frame_output_ = 0;
  }

  ReleaseFrames();
  MediaStreamTrackResourceBase::CloseInternal();
}

void MediaStreamAudioTrackResource::OnNewBufferEnqueued() {
  if (!TrackedCallback::IsPending(get_frame_callback_))
    return;

  *get_frame_output_ = GetAudioFrame();
  int32_t result = *get_frame_output_ ? PP_OK : PP_ERROR_FAILED;
  get_frame_output_ = NULL;
  scoped_refptr<TrackedCallback> callback;
  callback.swap(get_frame_callback_);
  callback->Run(result);
}

PP_Resource MediaStreamAudioTrackResource::GetAudioFrame() {
  int32_t index = buffer_manager()->DequeueBuffer();
  if (index < 0)
      return 0;

  MediaStreamBuffer* buffer = buffer_manager()->GetBufferPointer(index);
  DCHECK(buffer);
  scoped_refptr<AudioFrameResource> resource =
      new AudioFrameResource(pp_instance(), index, buffer);
  // Add |pp_resource()| and |resource| into |frames_|.
  // |frames_| uses scoped_ptr<> to hold a ref of |resource|. It keeps the
  // resource alive.
  frames_.insert(FrameMap::value_type(resource->pp_resource(), resource));
  return resource->GetReference();
}

void MediaStreamAudioTrackResource::ReleaseFrames() {
  FrameMap::iterator it = frames_.begin();
  while (it != frames_.end()) {
    // Just invalidate and release VideoFrameResorce, but keep PP_Resource.
    // So plugin can still use |RecycleFrame()|.
    it->second->Invalidate();
    it->second = NULL;
  }
}

}  // namespace proxy
}  // namespace ppapi
