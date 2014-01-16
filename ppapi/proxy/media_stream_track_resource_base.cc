// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/media_stream_track_resource_base.h"

#include "base/logging.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

MediaStreamTrackResourceBase::MediaStreamTrackResourceBase(
    Connection connection,
    PP_Instance instance,
    int pending_renderer_id,
    const std::string& id)
    : PluginResource(connection, instance),
      frame_buffer_(this),
      id_(id),
      has_ended_(false) {
  AttachToPendingHost(RENDERER, pending_renderer_id);
}

MediaStreamTrackResourceBase::~MediaStreamTrackResourceBase() {
}

void MediaStreamTrackResourceBase::SendEnqueueFrameMessageToHost(
    int32_t index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, frame_buffer()->number_of_frames());
  Post(RENDERER, PpapiHostMsg_MediaStreamTrack_EnqueueFrame(index));
}

void MediaStreamTrackResourceBase::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(MediaStreamTrackResourceBase, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_MediaStreamTrack_InitFrames, OnPluginMsgInitFrames)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_MediaStreamTrack_EnqueueFrame, OnPluginMsgEnqueueFrame)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(
        PluginResource::OnReplyReceived(params, msg))
  IPC_END_MESSAGE_MAP()
}

void MediaStreamTrackResourceBase::CloseInternal() {
  if (!has_ended_) {
    Post(RENDERER, PpapiHostMsg_MediaStreamTrack_Close());
    has_ended_ = true;
  }
}

void MediaStreamTrackResourceBase::OnPluginMsgInitFrames(
    const ResourceMessageReplyParams& params,
    int32_t number_of_frames,
    int32_t frame_size) {
  base::SharedMemoryHandle shm_handle = base::SharedMemory::NULLHandle();
  params.TakeSharedMemoryHandleAtIndex(0, &shm_handle);
  frame_buffer_.SetFrames(number_of_frames, frame_size,
      scoped_ptr<base::SharedMemory>(new base::SharedMemory(shm_handle, true)),
      false);
}

void MediaStreamTrackResourceBase::OnPluginMsgEnqueueFrame(
    const ResourceMessageReplyParams& params,
    int32_t index) {
  frame_buffer_.EnqueueFrame(index);
}

}  // namespace proxy
}  // namespace ppapi
