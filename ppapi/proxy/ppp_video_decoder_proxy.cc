// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_video_decoder_proxy.h"

#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_video_decoder_proxy.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_video_decoder_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_VideoDecoder_API;

namespace ppapi {
namespace proxy {

namespace {

void ProvidePictureBuffers(PP_Instance instance, PP_Resource decoder,
                           uint32_t req_num_of_bufs, PP_Size dimensions) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPVideoDecoder_ProvidePictureBuffers(
          INTERFACE_ID_PPP_VIDEO_DECODER_DEV,
          decoder_resource, req_num_of_bufs, dimensions));
}

void DismissPictureBuffer(PP_Instance instance, PP_Resource decoder,
                          int32_t picture_buffer_id) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPVideoDecoder_DismissPictureBuffer(
          INTERFACE_ID_PPP_VIDEO_DECODER_DEV,
          decoder_resource, picture_buffer_id));
}

void PictureReady(PP_Instance instance, PP_Resource decoder,
                  PP_Picture_Dev picture) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPVideoDecoder_PictureReady(
          INTERFACE_ID_PPP_VIDEO_DECODER_DEV, decoder_resource, picture));
}

void EndOfStream(PP_Instance instance, PP_Resource decoder) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPVideoDecoder_NotifyEndOfStream(
          INTERFACE_ID_PPP_VIDEO_DECODER_DEV, decoder_resource));
}

void NotifyError(PP_Instance instance, PP_Resource decoder,
                 PP_VideoDecodeError_Dev error) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPVideoDecoder_NotifyError(
          INTERFACE_ID_PPP_VIDEO_DECODER_DEV, decoder_resource, error));
}

static const PPP_VideoDecoder_Dev video_decoder_interface = {
  &ProvidePictureBuffers,
  &DismissPictureBuffer,
  &PictureReady,
  &EndOfStream,
  &NotifyError
};

InterfaceProxy* CreateVideoDecoderPPPProxy(Dispatcher* dispatcher) {
  return new PPP_VideoDecoder_Proxy(dispatcher);
}

}  // namespace

PPP_VideoDecoder_Proxy::PPP_VideoDecoder_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_video_decoder_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_video_decoder_impl_ = static_cast<const PPP_VideoDecoder_Dev*>(
        dispatcher->local_get_interface()(PPP_VIDEODECODER_DEV_INTERFACE));
  }
}

PPP_VideoDecoder_Proxy::~PPP_VideoDecoder_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_VideoDecoder_Proxy::GetInfo() {
  static const Info info = {
    &video_decoder_interface,
    PPP_VIDEODECODER_DEV_INTERFACE,
    INTERFACE_ID_PPP_VIDEO_DECODER_DEV,
    false,
    &CreateVideoDecoderPPPProxy,
  };
  return &info;
}

bool PPP_VideoDecoder_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_VideoDecoder_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_ProvidePictureBuffers,
                        OnMsgProvidePictureBuffers)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_DismissPictureBuffer,
                        OnMsgDismissPictureBuffer)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_PictureReady,
                        OnMsgPictureReady)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_NotifyEndOfStream,
                        OnMsgNotifyEndOfStream)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_NotifyError,
                        OnMsgNotifyError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PPP_VideoDecoder_Proxy::OnMsgProvidePictureBuffers(
    const HostResource& decoder, uint32_t req_num_of_bufs,
    const PP_Size& dimensions) {
  PP_Resource plugin_decoder = PluginResourceTracker::GetInstance()->
      PluginResourceForHostResource(decoder);
  ppp_video_decoder_impl_->ProvidePictureBuffers(
      decoder.instance(), plugin_decoder, req_num_of_bufs, dimensions);
}

void PPP_VideoDecoder_Proxy::OnMsgDismissPictureBuffer(
    const HostResource& decoder, int32_t picture_id) {
  PP_Resource plugin_decoder = PluginResourceTracker::GetInstance()->
      PluginResourceForHostResource(decoder);
  ppp_video_decoder_impl_->DismissPictureBuffer(
      decoder.instance(), plugin_decoder, picture_id);
}

void PPP_VideoDecoder_Proxy::OnMsgPictureReady(
    const HostResource& decoder, const PP_Picture_Dev& picture) {
  PP_Resource plugin_decoder = PluginResourceTracker::GetInstance()->
      PluginResourceForHostResource(decoder);
  ppp_video_decoder_impl_->PictureReady(
      decoder.instance(), plugin_decoder, picture);
}

void PPP_VideoDecoder_Proxy::OnMsgNotifyEndOfStream(
    const HostResource& decoder) {
  PP_Resource plugin_decoder = PluginResourceTracker::GetInstance()->
      PluginResourceForHostResource(decoder);
  ppp_video_decoder_impl_->EndOfStream(decoder.instance(),
                                          plugin_decoder);
}

void PPP_VideoDecoder_Proxy::OnMsgNotifyError(
    const HostResource& decoder, PP_VideoDecodeError_Dev error) {
  PP_Resource plugin_decoder = PluginResourceTracker::GetInstance()->
      PluginResourceForHostResource(decoder);
  ppp_video_decoder_impl_->NotifyError(
      decoder.instance(), plugin_decoder, error);
}

}  // namespace proxy
}  // namespace ppapi
