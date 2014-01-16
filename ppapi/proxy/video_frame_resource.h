// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_
#define PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/media_stream_frame.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_video_frame_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT VideoFrameResource : public Resource,
                                              public thunk::PPB_VideoFrame_API {
 public:
  VideoFrameResource(PP_Instance instance,
                     int32_t index,
                     MediaStreamFrame* frame);

  virtual ~VideoFrameResource();

  // PluginResource overrides:
  virtual thunk::PPB_VideoFrame_API* AsPPB_VideoFrame_API() OVERRIDE;

  // PPB_VideoFrame_API overrides:
  virtual PP_TimeDelta GetTimestamp() OVERRIDE;
  virtual void SetTimestamp(PP_TimeDelta timestamp) OVERRIDE;
  virtual PP_VideoFrame_Format GetFormat() OVERRIDE;
  virtual PP_Bool GetSize(PP_Size* size) OVERRIDE;
  virtual void* GetDataBuffer() OVERRIDE;
  virtual uint32_t GetDataBufferSize() OVERRIDE;
  virtual MediaStreamFrame* GetFrameBuffer();
  virtual int32_t GetFrameBufferIndex();
  virtual void Invalidate();

  // Frame index
  int32_t index_;

  MediaStreamFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_VIDEO_FRAME_RESOURCE_H_
