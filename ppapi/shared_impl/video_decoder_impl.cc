// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/video_decoder_impl.h"

#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {

VideoDecoderImpl::VideoDecoderImpl()
    : flush_callback_(PP_MakeCompletionCallback(NULL, NULL)),
      reset_callback_(PP_MakeCompletionCallback(NULL, NULL)),
      graphics_context_(0),
      gles2_impl_(NULL) {
}

VideoDecoderImpl::~VideoDecoderImpl() {
}

void VideoDecoderImpl::InitCommon(
    PP_Resource graphics_context,
    gpu::gles2::GLES2Implementation* gles2_impl) {
  DCHECK(graphics_context);
  DCHECK(!gles2_impl_ && !graphics_context_);
  gles2_impl_ = gles2_impl;
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(graphics_context);
  graphics_context_ = graphics_context;
}

void VideoDecoderImpl::Destroy() {
  graphics_context_ = 0;
  gles2_impl_ = NULL;
  PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(graphics_context_);
}

bool VideoDecoderImpl::SetFlushCallback(PP_CompletionCallback callback) {
  CHECK(callback.func);
  if (flush_callback_.func)
    return false;
  flush_callback_ = callback;
  return true;
}

bool VideoDecoderImpl::SetResetCallback(PP_CompletionCallback callback) {
  CHECK(callback.func);
  if (reset_callback_.func)
    return false;
  reset_callback_ = callback;
  return true;
}

bool VideoDecoderImpl::SetBitstreamBufferCallback(
    int32 bitstream_buffer_id, PP_CompletionCallback callback) {
  return bitstream_buffer_callbacks_.insert(
      std::make_pair(bitstream_buffer_id, callback)).second;
}

void VideoDecoderImpl::RunFlushCallback(int32 result) {
  DCHECK(flush_callback_.func);
  PP_RunAndClearCompletionCallback(&flush_callback_, result);
}

void VideoDecoderImpl::RunResetCallback(int32 result) {
  DCHECK(reset_callback_.func);
  PP_RunAndClearCompletionCallback(&reset_callback_, result);
}

void VideoDecoderImpl::RunBitstreamBufferCallback(
    int32 bitstream_buffer_id, int32 result) {
  CallbackById::iterator it =
      bitstream_buffer_callbacks_.find(bitstream_buffer_id);
  DCHECK(it != bitstream_buffer_callbacks_.end());
  PP_CompletionCallback cc = it->second;
  bitstream_buffer_callbacks_.erase(it);
  PP_RunCompletionCallback(&cc, PP_OK);
}

void VideoDecoderImpl::FlushCommandBuffer() {
  if (gles2_impl_)
    gles2_impl_->Flush();
}

}  // namespace ppapi
