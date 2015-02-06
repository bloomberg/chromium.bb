// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VIDEO_ENCODER_RESOURCE_H_
#define PPAPI_PROXY_VIDEO_ENCODER_RESOURCE_H_

#include "base/memory/ref_counted.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_video_encoder_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT VideoEncoderResource
    : public PluginResource,
      public thunk::PPB_VideoEncoder_API {
 public:
  VideoEncoderResource(Connection connection, PP_Instance instance);
  ~VideoEncoderResource() override;

  thunk::PPB_VideoEncoder_API* AsPPB_VideoEncoder_API() override;

 private:
  // PPB_VideoEncoder_API implementation.
  int32_t GetSupportedProfiles(
      const PP_ArrayOutput& output,
      const scoped_refptr<TrackedCallback>& callback) override;
  int32_t Initialize(PP_VideoFrame_Format input_format,
                     const PP_Size* input_visible_size,
                     PP_VideoProfile output_profile,
                     uint32_t initial_bitrate,
                     PP_HardwareAcceleration acceleration,
                     const scoped_refptr<TrackedCallback>& callback) override;
  int32_t GetFramesRequired() override;
  int32_t GetFrameCodedSize(PP_Size* size) override;
  int32_t GetVideoFrame(
      PP_Resource* video_frame,
      const scoped_refptr<TrackedCallback>& callback) override;
  int32_t Encode(PP_Resource video_frame,
                 PP_Bool force_keyframe,
                 const scoped_refptr<TrackedCallback>& callback) override;
  int32_t GetBitstreamBuffer(
      PP_BitstreamBuffer* picture,
      const scoped_refptr<TrackedCallback>& callback) override;
  void RecycleBitstreamBuffer(const PP_BitstreamBuffer* picture) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Close() override;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_VIDEO_ENCODER_RESOURCE_H_
