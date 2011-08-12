// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_video_decoder_proxy.h"

#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_context_3d_proxy.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Context3D_API;
using ppapi::thunk::PPB_VideoDecoder_API;

namespace pp {
namespace proxy {

class VideoDecoder : public PluginResource,
                     public ::ppapi::VideoDecoderImpl {
 public:
  virtual ~VideoDecoder();

  static VideoDecoder* Create(const HostResource& resource,
                              PP_Resource context3d_id,
                              const PP_VideoConfigElement* config);

  // ResourceObjectBase overrides.
  virtual PPB_VideoDecoder_API* AsPPB_VideoDecoder_API() OVERRIDE;

  // PPB_VideoDecoder_API implementation.
  virtual int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                         PP_CompletionCallback callback) OVERRIDE;
  virtual void AssignPictureBuffers(
      uint32_t no_of_buffers, const PP_PictureBuffer_Dev* buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32_t picture_buffer_id) OVERRIDE;
  virtual int32_t Flush(PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Reset(PP_CompletionCallback callback) OVERRIDE;
  virtual void Destroy() OVERRIDE;

 protected:
  virtual void AddRefResource(PP_Resource resource) OVERRIDE;
  virtual void UnrefResource(PP_Resource resource) OVERRIDE;

 private:
  friend class PPB_VideoDecoder_Proxy;
  explicit VideoDecoder(const HostResource& resource);

  // Run the callbacks that were passed into the plugin interface.
  void FlushACK(int32_t result);
  void ResetACK(int32_t result);
  void EndOfBitstreamACK(int32_t buffer_id, int32_t result);

  DISALLOW_COPY_AND_ASSIGN(VideoDecoder);
};

VideoDecoder::VideoDecoder(const HostResource& decoder)
    : PluginResource(decoder) {
}

VideoDecoder* VideoDecoder::Create(const HostResource& resource,
                                   PP_Resource context3d_id,
                                   const PP_VideoConfigElement* config) {
  if (!context3d_id)
    return NULL;

  EnterResourceNoLock<PPB_Context3D_API> enter_context(context3d_id, true);
  if (enter_context.failed())
    return NULL;

  scoped_refptr<VideoDecoder> decoder(new VideoDecoder(resource));
  if (decoder->Init(context3d_id, enter_context.object(), config))
    return decoder.release();
  return NULL;
}

VideoDecoder::~VideoDecoder() {
}

::ppapi::thunk::PPB_VideoDecoder_API* VideoDecoder::AsPPB_VideoDecoder_API() {
  return this;
}

int32_t VideoDecoder::Decode(
    const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
    PP_CompletionCallback callback) {
  ppapi::thunk::EnterResourceNoLock<PPB_Buffer_API>
      enter_buffer(bitstream_buffer->data, true);
  if (enter_buffer.failed())
    return PP_ERROR_BADRESOURCE;

  if (!SetBitstreamBufferCallback(bitstream_buffer->id, callback))
    return PP_ERROR_BADARGUMENT;

  Buffer* ppb_buffer =
      static_cast<Buffer*>(enter_buffer.object());
  HostResource host_buffer = ppb_buffer->host_resource();

  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_Decode(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, host_resource(),
      host_buffer, bitstream_buffer->id,
      bitstream_buffer->size));
  return PP_OK_COMPLETIONPENDING;
}

void VideoDecoder::AssignPictureBuffers(uint32_t no_of_buffers,
                                        const PP_PictureBuffer_Dev* buffers) {
  std::vector<PP_PictureBuffer_Dev> buffer_list(
      buffers, buffers + no_of_buffers);
  FlushCommandBuffer();
  GetDispatcher()->Send(
      new PpapiHostMsg_PPBVideoDecoder_AssignPictureBuffers(
          INTERFACE_ID_PPB_VIDEO_DECODER_DEV, host_resource(), buffer_list));
}

void VideoDecoder::ReusePictureBuffer(int32_t picture_buffer_id) {
  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_ReusePictureBuffer(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, host_resource(), picture_buffer_id));
}

int32_t VideoDecoder::Flush(PP_CompletionCallback callback) {
  if (!SetFlushCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_Flush(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoDecoder::Reset(PP_CompletionCallback callback) {
  if (!SetResetCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_Reset(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

void VideoDecoder::Destroy() {
  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_Destroy(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, host_resource()));
  ::ppapi::VideoDecoderImpl::Destroy();
}

void VideoDecoder::AddRefResource(PP_Resource resource) {
  PluginResourceTracker::GetInstance()->AddRefResource(resource);
}

void VideoDecoder::UnrefResource(PP_Resource resource) {
  PluginResourceTracker::GetInstance()->ReleaseResource(resource);
}

void VideoDecoder::ResetACK(int32_t result) {
  RunResetCallback(result);
}

void VideoDecoder::FlushACK(int32_t result) {
  RunFlushCallback(result);
}

void VideoDecoder::EndOfBitstreamACK(
    int32_t bitstream_buffer_id, int32_t result) {
  RunBitstreamBufferCallback(bitstream_buffer_id, result);
}

namespace {

InterfaceProxy* CreateVideoDecoderProxy(Dispatcher* dispatcher,
                                        const void* target_interface) {
  return new PPB_VideoDecoder_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_VideoDecoder_Proxy::PPB_VideoDecoder_Proxy(Dispatcher* dispatcher,
                                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_VideoDecoder_Proxy::~PPB_VideoDecoder_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_VideoDecoder_Proxy::GetInfo() {
  static const Info info = {
    ::ppapi::thunk::GetPPB_VideoDecoder_Thunk(),
    PPB_VIDEODECODER_DEV_INTERFACE,
    INTERFACE_ID_PPB_VIDEO_DECODER_DEV,
    false,
    &CreateVideoDecoderProxy,
  };
  return &info;
}

bool PPB_VideoDecoder_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_VideoDecoder_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Decode, OnMsgDecode)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_AssignPictureBuffers,
                        OnMsgAssignPictureBuffers)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_ReusePictureBuffer,
                        OnMsgReusePictureBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Flush, OnMsgFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Reset, OnMsgReset)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Destroy, OnMsgDestroy)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBVideoDecoder_ResetACK, OnMsgResetACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBVideoDecoder_EndOfBitstreamACK,
                        OnMsgEndOfBitstreamACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBVideoDecoder_FlushACK, OnMsgFlushACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

PP_Resource PPB_VideoDecoder_Proxy::CreateProxyResource(
    PP_Instance instance, PP_Resource context3d_id,
    const PP_VideoConfigElement* config) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  // Dispatcher is null if it cannot find the instance passed to it (i.e. if the
  // client passes in an invalid instance).
  if (!dispatcher)
    return 0;

  std::vector<PP_VideoConfigElement> copied;
  if (!ppapi::VideoDecoderImpl::CopyConfigsToVector(config, &copied))
    return 0;

  ppapi::thunk::EnterResourceNoLock<PPB_Context3D_API>
      enter_context(context3d_id, true);
  if (enter_context.failed())
    return 0;
  Context3D* ppb_context =
      static_cast<Context3D*>(enter_context.object());
  HostResource host_context = ppb_context->host_resource();

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBVideoDecoder_Create(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, instance,
      host_context, copied, &result));
  if (result.is_null())
    return 0;

  return PluginResourceTracker::GetInstance()->AddResource(
      VideoDecoder::Create(result, context3d_id, config));
}

void PPB_VideoDecoder_Proxy::OnMsgCreate(
    PP_Instance instance, const HostResource& context3d_id,
    const std::vector<PP_VideoConfigElement>& config,
    HostResource* result) {
  ::ppapi::thunk::EnterFunction< ::ppapi::thunk::ResourceCreationAPI>
      resource_creation(instance, true);
  if (resource_creation.failed())
    return;

  std::vector<PP_VideoConfigElement> copied = config;
  copied.push_back(PP_VIDEOATTR_DICTIONARY_TERMINATOR);

  // Make the resource and get the API pointer to its interface.
  result->SetHostResource(
      instance, resource_creation.functions()->CreateVideoDecoder(
          instance, context3d_id.host_resource(), &copied.front()));
}

void PPB_VideoDecoder_Proxy::OnMsgDecode(
    const HostResource& decoder,
    const HostResource& buffer, int32 id, int32 size) {
  CompletionCallback callback = callback_factory_.NewRequiredCallback(
      &PPB_VideoDecoder_Proxy::SendMsgEndOfBitstreamACKToPlugin, decoder, id);

  PP_VideoBitstreamBuffer_Dev bitstream = { id, buffer.host_resource(), size };
  ppb_video_decoder_target()->Decode(
     decoder.host_resource(), &bitstream, callback.pp_completion_callback());
}

void PPB_VideoDecoder_Proxy::OnMsgAssignPictureBuffers(
    const HostResource& decoder,
    const std::vector<PP_PictureBuffer_Dev>& buffers) {
  DCHECK(!buffers.empty());
  const PP_PictureBuffer_Dev* buffer_array = &buffers.front();

  ppb_video_decoder_target()->AssignPictureBuffers(
     decoder.host_resource(), buffers.size(), buffer_array);
}

void PPB_VideoDecoder_Proxy::OnMsgReusePictureBuffer(
    const HostResource& decoder, int32 picture_buffer_id) {
  ppb_video_decoder_target()->ReusePictureBuffer(
      decoder.host_resource(), picture_buffer_id);
}

void PPB_VideoDecoder_Proxy::OnMsgFlush(const HostResource& decoder) {
  CompletionCallback callback = callback_factory_.NewRequiredCallback(
      &PPB_VideoDecoder_Proxy::SendMsgFlushACKToPlugin, decoder);
  ppb_video_decoder_target()->Flush(
      decoder.host_resource(), callback.pp_completion_callback());
}

void PPB_VideoDecoder_Proxy::OnMsgReset(const HostResource& decoder) {
  CompletionCallback callback = callback_factory_.NewRequiredCallback(
      &PPB_VideoDecoder_Proxy::SendMsgResetACKToPlugin, decoder);
  ppb_video_decoder_target()->Reset(
      decoder.host_resource(), callback.pp_completion_callback());
}

void PPB_VideoDecoder_Proxy::OnMsgDestroy(const HostResource& decoder) {
  ppb_video_decoder_target()->Destroy(decoder.host_resource());
}

void PPB_VideoDecoder_Proxy::SendMsgEndOfBitstreamACKToPlugin(
    int32_t result, const HostResource& decoder, int32 id) {
  dispatcher()->Send(new PpapiMsg_PPBVideoDecoder_EndOfBitstreamACK(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, decoder, id, result));
}

void PPB_VideoDecoder_Proxy::SendMsgFlushACKToPlugin(
    int32_t result, const HostResource& decoder) {
  dispatcher()->Send(new PpapiMsg_PPBVideoDecoder_FlushACK(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, decoder, result));
}

void PPB_VideoDecoder_Proxy::SendMsgResetACKToPlugin(
    int32_t result, const HostResource& decoder) {
  dispatcher()->Send(new PpapiMsg_PPBVideoDecoder_ResetACK(
      INTERFACE_ID_PPB_VIDEO_DECODER_DEV, decoder, result));
}

void PPB_VideoDecoder_Proxy::OnMsgEndOfBitstreamACK(
    const HostResource& decoder, int32_t id, int32_t result) {
  EnterPluginFromHostResource<PPB_VideoDecoder_API> enter(decoder);
  if (enter.succeeded())
    static_cast<VideoDecoder*>(enter.object())->EndOfBitstreamACK(id, result);
}

void PPB_VideoDecoder_Proxy::OnMsgFlushACK(
    const HostResource& decoder, int32_t result) {
  EnterPluginFromHostResource<PPB_VideoDecoder_API> enter(decoder);
  if (enter.succeeded())
    static_cast<VideoDecoder*>(enter.object())->FlushACK(result);
}

void PPB_VideoDecoder_Proxy::OnMsgResetACK(
    const HostResource& decoder, int32_t result) {
  EnterPluginFromHostResource<PPB_VideoDecoder_API> enter(decoder);
  if (enter.succeeded())
    static_cast<VideoDecoder*>(enter.object())->ResetACK(result);
}

}  // namespace proxy
}  // namespace pp
