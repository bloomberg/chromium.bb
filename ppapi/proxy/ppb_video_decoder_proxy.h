// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_VIDEO_DECODER_PROXY_H_
#define PPAPI_PROXY_PPB_VIDEO_DECODER_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/shared_impl/video_decoder_impl.h"
#include "ppapi/thunk/ppb_video_decoder_api.h"

namespace ppapi {
namespace proxy {

class PPB_VideoDecoder_Proxy : public InterfaceProxy {
 public:
  PPB_VideoDecoder_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_VideoDecoder_Proxy();

  static const Info* GetInfo();

  // Creates a VideoDecoder object in the plugin process.
  static PP_Resource CreateProxyResource(
      PP_Instance instance,
      PP_Resource graphics_context,
      PP_VideoDecoder_Profile profile);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  const PPB_VideoDecoder_Dev* ppb_video_decoder_target() const {
    return static_cast<const PPB_VideoDecoder_Dev*>(target_interface());
  }

 private:
  // Message handlers in the renderer process to receive messages from the
  // plugin process.
  void OnMsgCreate(PP_Instance instance,
                   const ppapi::HostResource& graphics_context,
                   PP_VideoDecoder_Profile profile,
                   ppapi::HostResource* result);
  void OnMsgDecode(
      const ppapi::HostResource& decoder,
      const ppapi::HostResource& buffer, int32 id, int32 size);
  void OnMsgAssignPictureBuffers(
      const ppapi::HostResource& decoder,
      const std::vector<PP_PictureBuffer_Dev>& buffers);
  void OnMsgReusePictureBuffer(
      const ppapi::HostResource& decoder,
      int32 picture_buffer_id);
  void OnMsgFlush(const ppapi::HostResource& decoder);
  void OnMsgReset(const ppapi::HostResource& decoder);
  void OnMsgDestroy(const ppapi::HostResource& decoder);

  // Send a message from the renderer process to the plugin process to tell it
  // to run its callback.
  void SendMsgEndOfBitstreamACKToPlugin(
      int32_t result, const ppapi::HostResource& decoder, int32 id);
  void SendMsgFlushACKToPlugin(
      int32_t result, const ppapi::HostResource& decoder);
  void SendMsgResetACKToPlugin(
      int32_t result, const ppapi::HostResource& decoder);

  // Message handlers in the plugin process to receive messages from the
  // renderer process.
  void OnMsgEndOfBitstreamACK(const ppapi::HostResource& decoder,
                              int32_t id, int32_t result);
  void OnMsgFlushACK(const ppapi::HostResource& decoder, int32_t result);
  void OnMsgResetACK(const ppapi::HostResource& decoder, int32_t result);

  pp::CompletionCallbackFactory<PPB_VideoDecoder_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoDecoder_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_VIDEO_DECODER_PROXY_H_
