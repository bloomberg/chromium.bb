// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_
#define PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
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

  virtual ~VideoFrameResource();

  // PluginResource overrides:
  virtual thunk::PPB_VideoFrame_API* AsPPB_VideoFrame_API() override;

  // PPB_VideoFrame_API overrides:
  virtual PP_TimeDelta GetTimestamp() override;
  virtual void SetTimestamp(PP_TimeDelta timestamp) override;
  virtual PP_VideoFrame_Format GetFormat() override;
  virtual PP_Bool GetSize(PP_Size* size) override;
  virtual void* GetDataBuffer() override;
  virtual uint32_t GetDataBufferSize() override;
  virtual MediaStreamBuffer* GetBuffer() override;
  virtual int32_t GetBufferIndex() override;
  virtual void Invalidate() override;

  // Frame index
  int32_t index_;

  MediaStreamBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_
