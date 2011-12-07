// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_VIDEO_DECODER_IMPL_H_
#define PPAPI_SHARED_IMPL_VIDEO_DECODER_IMPL_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_video_decoder_api.h"

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace ppapi {

// Implements the logic to set and run callbacks for various video decoder
// events. Both the proxy and the renderer implementation share this code.
class PPAPI_SHARED_EXPORT VideoDecoderImpl
    : NON_EXPORTED_BASE(public thunk::PPB_VideoDecoder_API) {
 public:
  VideoDecoderImpl();
  virtual ~VideoDecoderImpl();

  // PPB_VideoDecoder_API implementation.
  virtual void Destroy() OVERRIDE;

 protected:
  bool SetFlushCallback(PP_CompletionCallback callback);
  bool SetResetCallback(PP_CompletionCallback callback);
  bool SetBitstreamBufferCallback(
      int32 bitstream_buffer_id, PP_CompletionCallback callback);

  void RunFlushCallback(int32 result);
  void RunResetCallback(int32 result);
  void RunBitstreamBufferCallback(int32 bitstream_buffer_id, int32 result);

  // Tell command buffer to process all commands it has received so far.
  void FlushCommandBuffer();

  // Initialize the underlying decoder.
  void InitCommon(PP_Resource graphics_context,
                  gpu::gles2::GLES2Implementation* gles2_impl);

 private:
  // Key: bitstream_buffer_id, value: callback to run when bitstream decode is
  // done.
  typedef std::map<int32, PP_CompletionCallback> CallbackById;

  PP_CompletionCallback flush_callback_;
  PP_CompletionCallback reset_callback_;
  CallbackById bitstream_buffer_callbacks_;

  // The resource ID of the underlying Graphics3D object being used.  Used only
  // for reference counting to keep it alive for the lifetime of |*this|.
  PP_Resource graphics_context_;

  // Reference to the GLES2Implementation owned by |graphics_context_|.
  // Graphics3D is guaranteed to be alive for the lifetime of this class.
  // In the out-of-process case, Graphics3D's gles2_impl() exists in the plugin
  // process only, so gles2_impl_ is NULL in that case.
  gpu::gles2::GLES2Implementation* gles2_impl_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_VIDEO_DECODER_IMPL_H_
