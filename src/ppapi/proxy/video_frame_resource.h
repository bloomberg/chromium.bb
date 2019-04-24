// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_
#define PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_video_frame_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT VideoFrameResource : public Resource,
                                              public thunk::PPB_VideoFrame_API {
 public:
  VideoFrameResource(PP_Instance instance,
                     int32_t index,
                     MediaStreamBuffer* buffer);

  ~VideoFrameResource() override;

  // PluginResource overrides:
  thunk::PPB_VideoFrame_API* AsPPB_VideoFrame_API() override;

  // PPB_VideoFrame_API overrides:
  PP_TimeDelta GetTimestamp() override;
  void SetTimestamp(PP_TimeDelta timestamp) override;
  PP_VideoFrame_Format GetFormat() override;
  PP_Bool GetSize(PP_Size* size) override;
  void* GetDataBuffer() override;
  uint32_t GetDataBufferSize() override;
  MediaStreamBuffer* GetBuffer() override;
  int32_t GetBufferIndex() override;
  void Invalidate() override;

  // Frame index
  int32_t index_;

  MediaStreamBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_
