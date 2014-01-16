// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/video_frame_resource.h"

#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

VideoFrameResource::VideoFrameResource(PP_Instance instance,
                                       int32_t index,
                                       MediaStreamFrame* frame)
    : Resource(OBJECT_IS_PROXY, instance),
      index_(index),
      frame_(frame) {
  DCHECK_EQ(frame_->header.type, MediaStreamFrame::TYPE_VIDEO);
}

VideoFrameResource::~VideoFrameResource() {
  CHECK(!frame_) << "An unused (or unrecycled) frame is destroyed.";
}

thunk::PPB_VideoFrame_API* VideoFrameResource::AsPPB_VideoFrame_API() {
  return this;
}

PP_TimeDelta VideoFrameResource::GetTimestamp() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return 0.0;
  }
  return frame_->video.timestamp;
}

void VideoFrameResource::SetTimestamp(PP_TimeDelta timestamp) {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return;
  }
  frame_->video.timestamp = timestamp;
}

PP_VideoFrame_Format VideoFrameResource::GetFormat() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  }
  return frame_->video.format;
}

PP_Bool VideoFrameResource::GetSize(PP_Size* size) {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return PP_FALSE;
  }
  *size = frame_->video.size;
  return PP_TRUE;
}

void* VideoFrameResource::GetDataBuffer() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return NULL;
  }
  return frame_->video.data;
}

uint32_t VideoFrameResource::GetDataBufferSize() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return 0;
  }
  return frame_->video.data_size;
}

MediaStreamFrame* VideoFrameResource::GetFrameBuffer() {
  return frame_;
}

int32_t VideoFrameResource::GetFrameBufferIndex() {
  return index_;
}

void VideoFrameResource::Invalidate() {
  DCHECK(frame_);
  DCHECK_GE(index_, 0);
  frame_ = NULL;
  index_ = -1;
}

}  // namespace proxy
}  // namespace ppapi
