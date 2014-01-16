// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_MEDIA_STREAM_TRACK_RESOURCE_BASE_H_
#define PPAPI_PROXY_MEDIA_STREAM_TRACK_RESOURCE_BASE_H_

#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/media_stream_frame_buffer.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT MediaStreamTrackResourceBase
  : public PluginResource,
    public MediaStreamFrameBuffer::Delegate {
 protected:
  MediaStreamTrackResourceBase(Connection connection,
                               PP_Instance instance,
                               int pending_renderer_id,
                               const std::string& id);

  virtual ~MediaStreamTrackResourceBase();

  std::string id() const { return id_; }

  bool has_ended() const { return has_ended_; }

  MediaStreamFrameBuffer* frame_buffer() { return &frame_buffer_; }

  void CloseInternal();

  // Sends a frame index to the corresponding PepperMediaStreamTrackHostBase
  // via an IPC message. The host adds the frame index into its
  // |frame_buffer_| for reading or writing. Also see |MediaStreamFrameBuffer|.
  void SendEnqueueFrameMessageToHost(int32_t index);

  // PluginResource overrides:
  virtual void OnReplyReceived(const ResourceMessageReplyParams& params,
                               const IPC::Message& msg) OVERRIDE;

 private:
  // Message handlers:
  void OnPluginMsgInitFrames(const ResourceMessageReplyParams& params,
                             int32_t number_of_frames,
                             int32_t frame_size);
  void OnPluginMsgEnqueueFrame(const ResourceMessageReplyParams& params,
                               int32_t index);

  MediaStreamFrameBuffer frame_buffer_;

  std::string id_;

  bool has_ended_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamTrackResourceBase);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_MEDIA_STREAM_TRACK_RESOURCE_BASE_H_
