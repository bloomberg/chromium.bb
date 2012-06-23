// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_video_capture_proxy.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_video_capture_shared.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ppapi/thunk/ppb_buffer_trusted_api.h"
#include "ppapi/thunk/ppb_video_capture_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_BufferTrusted_API;
using ppapi::thunk::PPB_VideoCapture_API;

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreatePPPVideoCaptureProxy(Dispatcher* dispatcher) {
  return new PPP_VideoCapture_Proxy(dispatcher);
}

void OnDeviceInfo(PP_Instance instance,
                  PP_Resource resource,
                  const PP_VideoCaptureDeviceInfo_Dev* info,
                  uint32_t buffer_count,
                  const PP_Resource* resources) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }
  HostResource host_resource;
  host_resource.SetHostResource(instance, resource);
  std::vector<PPPVideoCapture_Buffer> buffers(buffer_count);
  const PPB_Core* core = static_cast<const PPB_Core*>(
      dispatcher->local_get_interface()(PPB_CORE_INTERFACE));
  DCHECK(core);
  for (uint32_t i = 0; i < buffer_count; ++i) {
    // We need to take a ref on the resource now. The browser may drop
    // references once we return from here, but we're sending an asynchronous
    // message. The plugin side takes ownership of that reference.
    core->AddRefResource(resources[i]);
    buffers[i].resource.SetHostResource(instance, resources[i]);
    {
      EnterResourceNoLock<PPB_Buffer_API> enter(resources[i], true);
      DCHECK(enter.succeeded());
      PP_Bool result = enter.object()->Describe(&buffers[i].size);
      DCHECK(result);
    }
    {
      EnterResourceNoLock<PPB_BufferTrusted_API> enter(resources[i], true);
      DCHECK(enter.succeeded());
      int handle;
      int32_t result = enter.object()->GetSharedMemory(&handle);
      DCHECK(result == PP_OK);
      // TODO(piman/brettw): Change trusted interface to return a PP_FileHandle,
      // those casts are ugly.
      base::PlatformFile platform_file =
#if defined(OS_WIN)
          reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
#elif defined(OS_POSIX)
          handle;
#else
#error Not implemented.
#endif
      buffers[i].handle =
          dispatcher->ShareHandleWithRemote(platform_file, false);
    }
  }
  dispatcher->Send(new PpapiMsg_PPPVideoCapture_OnDeviceInfo(
      API_ID_PPP_VIDEO_CAPTURE_DEV, host_resource, *info, buffers));
}

void OnStatus(PP_Instance instance, PP_Resource resource, uint32_t status) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }
  HostResource host_resource;
  host_resource.SetHostResource(instance, resource);
  dispatcher->Send(new PpapiMsg_PPPVideoCapture_OnStatus(
      API_ID_PPP_VIDEO_CAPTURE_DEV, host_resource, status));
}

void OnError(PP_Instance instance, PP_Resource resource, uint32_t error_code) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }
  HostResource host_resource;
  host_resource.SetHostResource(instance, resource);
  dispatcher->Send(new PpapiMsg_PPPVideoCapture_OnError(
      API_ID_PPP_VIDEO_CAPTURE_DEV, host_resource, error_code));
}

void OnBufferReady(PP_Instance instance,
                   PP_Resource resource,
                   uint32_t buffer) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }
  HostResource host_resource;
  host_resource.SetHostResource(instance, resource);
  dispatcher->Send(new PpapiMsg_PPPVideoCapture_OnBufferReady(
      API_ID_PPP_VIDEO_CAPTURE_DEV, host_resource, buffer));
}

PPP_VideoCapture_Dev ppp_video_capture = {
  OnDeviceInfo,
  OnStatus,
  OnError,
  OnBufferReady
};

}  // namespace

class VideoCapture : public PPB_VideoCapture_Shared {
 public:
  explicit VideoCapture(const HostResource& resource);
  virtual ~VideoCapture();

  bool OnStatus(PP_VideoCaptureStatus_Dev status);

  void set_status(PP_VideoCaptureStatus_Dev status) {
    SetStatus(status, true);
  }

  void SetBufferCount(size_t count) {
    buffer_in_use_ = std::vector<bool>(count);
  }

  void SetBufferInUse(uint32_t buffer) {
    DCHECK(buffer < buffer_in_use_.size());
    buffer_in_use_[buffer] = true;
  }

 private:
  // PPB_VideoCapture_Shared implementation.
  virtual int32_t InternalEnumerateDevices(
      PP_Resource* devices,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t InternalOpen(
      const std::string& device_id,
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t InternalStartCapture() OVERRIDE;
  virtual int32_t InternalReuseBuffer(uint32_t buffer) OVERRIDE;
  virtual int32_t InternalStopCapture() OVERRIDE;
  virtual void InternalClose() OVERRIDE;
  virtual int32_t InternalStartCapture0_1(
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count) OVERRIDE;
  virtual const std::vector<DeviceRefData>&
      InternalGetDeviceRefData() const OVERRIDE;

  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  std::vector<bool> buffer_in_use_;

  DISALLOW_COPY_AND_ASSIGN(VideoCapture);
};

VideoCapture::VideoCapture(const HostResource& resource)
    : PPB_VideoCapture_Shared(resource) {
}

VideoCapture::~VideoCapture() {
  Close();
}

bool VideoCapture::OnStatus(PP_VideoCaptureStatus_Dev status) {
  switch (status) {
    case PP_VIDEO_CAPTURE_STATUS_STARTED:
    case PP_VIDEO_CAPTURE_STATUS_PAUSED:
    case PP_VIDEO_CAPTURE_STATUS_STOPPED:
      return SetStatus(status, false);
    case PP_VIDEO_CAPTURE_STATUS_STARTING:
    case PP_VIDEO_CAPTURE_STATUS_STOPPING:
      // Those states are not sent by the browser.
      break;
  }

  NOTREACHED();
  return false;
}

int32_t VideoCapture::InternalEnumerateDevices(
    PP_Resource* devices,
    scoped_refptr<TrackedCallback> callback) {
  devices_ = devices;
  enumerate_devices_callback_ = callback;
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_EnumerateDevices(
      API_ID_PPB_VIDEO_CAPTURE_DEV, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoCapture::InternalOpen(
    const std::string& device_id,
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count,
    scoped_refptr<TrackedCallback> callback) {
  open_callback_ = callback;
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_Open(
      API_ID_PPB_VIDEO_CAPTURE_DEV, host_resource(), device_id, requested_info,
      buffer_count));
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoCapture::InternalStartCapture() {
  buffer_in_use_.clear();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_StartCapture(
      API_ID_PPB_VIDEO_CAPTURE_DEV, host_resource()));
  return PP_OK;
}

int32_t VideoCapture::InternalReuseBuffer(uint32_t buffer) {
  if (buffer >= buffer_in_use_.size() || !buffer_in_use_[buffer])
    return PP_ERROR_BADARGUMENT;
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_ReuseBuffer(
      API_ID_PPB_VIDEO_CAPTURE_DEV, host_resource(), buffer));
  return PP_OK;
}

int32_t VideoCapture::InternalStopCapture() {
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_StopCapture(
      API_ID_PPB_VIDEO_CAPTURE_DEV, host_resource()));
  return PP_OK;
}

void VideoCapture::InternalClose() {
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_Close(
      API_ID_PPB_VIDEO_CAPTURE_DEV, host_resource()));
}

int32_t VideoCapture::InternalStartCapture0_1(
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count) {
  buffer_in_use_.clear();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_StartCapture0_1(
      API_ID_PPB_VIDEO_CAPTURE_DEV, host_resource(), requested_info,
      buffer_count));
  return PP_OK;
}

const std::vector<DeviceRefData>&
    VideoCapture::InternalGetDeviceRefData() const {
  // This should never be called at the plugin side.
  NOTREACHED();
  static std::vector<DeviceRefData> result;
  return result;
}

PPB_VideoCapture_Proxy::PPB_VideoCapture_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
}

PPB_VideoCapture_Proxy::~PPB_VideoCapture_Proxy() {
}

// static
PP_Resource PPB_VideoCapture_Proxy::CreateProxyResource(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBVideoCapture_Create(
      API_ID_PPB_VIDEO_CAPTURE_DEV, instance, &result));
  if (result.is_null())
    return 0;
  return (new VideoCapture(result))->GetReference();
}

bool PPB_VideoCapture_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_VideoCapture_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_EnumerateDevices,
                        OnMsgEnumerateDevices)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_Open, OnMsgOpen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_StartCapture,
                        OnMsgStartCapture)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_ReuseBuffer,
                        OnMsgReuseBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_StopCapture,
                        OnMsgStopCapture)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_Close, OnMsgClose)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_StartCapture0_1,
                        OnMsgStartCapture0_1)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBVideoCapture_EnumerateDevicesACK,
                        OnMsgEnumerateDevicesACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBVideoCapture_OpenACK,
                        OnMsgOpenACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_VideoCapture_Proxy::OnMsgCreate(PP_Instance instance,
                                         HostResource* result_resource) {
  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result_resource->SetHostResource(
        instance,
        enter.functions()->CreateVideoCapture(instance));
  }
}

void PPB_VideoCapture_Proxy::OnMsgEnumerateDevices(
    const HostResource& resource) {
  EnterHostFromHostResourceForceCallback<PPB_VideoCapture_API> enter(
      resource, callback_factory_,
      &PPB_VideoCapture_Proxy::EnumerateDevicesACKInHost, resource);

  if (enter.succeeded())
    enter.SetResult(enter.object()->EnumerateDevices(NULL, enter.callback()));
}

void PPB_VideoCapture_Proxy::OnMsgOpen(
    const ppapi::HostResource& resource,
    const std::string& device_id,
    const PP_VideoCaptureDeviceInfo_Dev& info,
    uint32_t buffers) {
  EnterHostFromHostResourceForceCallback<PPB_VideoCapture_API> enter(
      resource, callback_factory_, &PPB_VideoCapture_Proxy::OpenACKInHost,
      resource);

  if (enter.succeeded()) {
    enter.SetResult(enter.object()->Open(device_id, info, buffers,
                                         enter.callback()));
  }
}

void PPB_VideoCapture_Proxy::OnMsgStartCapture(const HostResource& resource) {
  EnterHostFromHostResource<PPB_VideoCapture_API> enter(resource);
  if (enter.succeeded())
    enter.object()->StartCapture();
}

void PPB_VideoCapture_Proxy::OnMsgReuseBuffer(const HostResource& resource,
                                              uint32_t buffer) {
  EnterHostFromHostResource<PPB_VideoCapture_API> enter(resource);
  if (enter.succeeded())
    enter.object()->ReuseBuffer(buffer);
}

void PPB_VideoCapture_Proxy::OnMsgStopCapture(const HostResource& resource) {
  EnterHostFromHostResource<PPB_VideoCapture_API> enter(resource);
  if (enter.succeeded())
    enter.object()->StopCapture();
}

void PPB_VideoCapture_Proxy::OnMsgClose(const HostResource& resource) {
  EnterHostFromHostResource<PPB_VideoCapture_API> enter(resource);
  if (enter.succeeded())
    enter.object()->Close();
}

void PPB_VideoCapture_Proxy::OnMsgStartCapture0_1(
    const HostResource& resource,
    const PP_VideoCaptureDeviceInfo_Dev& info,
    uint32_t buffers) {
  EnterHostFromHostResource<PPB_VideoCapture_API> enter(resource);
  if (enter.succeeded())
    enter.object()->StartCapture0_1(info, buffers);
}

void PPB_VideoCapture_Proxy::OnMsgEnumerateDevicesACK(
    const HostResource& resource,
    int32_t result,
    const std::vector<ppapi::DeviceRefData>& devices) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(resource);
  if (enter.succeeded()) {
    static_cast<VideoCapture*>(enter.object())->OnEnumerateDevicesComplete(
        result, devices);
  }
}

void PPB_VideoCapture_Proxy::OnMsgOpenACK(
    const HostResource& resource,
    int32_t result) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(resource);
  if (enter.succeeded())
    static_cast<VideoCapture*>(enter.object())->OnOpenComplete(result);
}

void PPB_VideoCapture_Proxy::EnumerateDevicesACKInHost(
    int32_t result,
    const HostResource& resource) {
  EnterHostFromHostResource<PPB_VideoCapture_API> enter(resource);
  dispatcher()->Send(new PpapiMsg_PPBVideoCapture_EnumerateDevicesACK(
      API_ID_PPB_VIDEO_CAPTURE_DEV, resource, result,
      enter.succeeded() && result == PP_OK ?
          enter.object()->GetDeviceRefData() : std::vector<DeviceRefData>()));
}

void PPB_VideoCapture_Proxy::OpenACKInHost(int32_t result,
                                           const HostResource& resource) {
  dispatcher()->Send(new PpapiMsg_PPBVideoCapture_OpenACK(
      API_ID_PPB_VIDEO_CAPTURE_DEV, resource, result));
}

PPP_VideoCapture_Proxy::PPP_VideoCapture_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_video_capture_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_video_capture_impl_ = static_cast<const PPP_VideoCapture_Dev*>(
        dispatcher->local_get_interface()(PPP_VIDEO_CAPTURE_DEV_INTERFACE));
  }
}

PPP_VideoCapture_Proxy::~PPP_VideoCapture_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_VideoCapture_Proxy::GetInfo() {
  static const Info info = {
    &ppp_video_capture,
    PPP_VIDEO_CAPTURE_DEV_INTERFACE,
    API_ID_PPP_VIDEO_CAPTURE_DEV,
    false,
    &CreatePPPVideoCaptureProxy,
  };
  return &info;
}

bool PPP_VideoCapture_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_VideoCapture_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoCapture_OnDeviceInfo,
                        OnMsgOnDeviceInfo)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoCapture_OnStatus, OnMsgOnStatus)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoCapture_OnError, OnMsgOnError)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoCapture_OnBufferReady,
                        OnMsgOnBufferReady)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPP_VideoCapture_Proxy::OnMsgOnDeviceInfo(
    const HostResource& host_resource,
    const PP_VideoCaptureDeviceInfo_Dev& info,
    const std::vector<PPPVideoCapture_Buffer>& buffers) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(host_resource);
  if (enter.failed() || !ppp_video_capture_impl_)
    return;

  PluginResourceTracker* tracker =
      PluginGlobals::Get()->plugin_resource_tracker();
  scoped_array<PP_Resource> resources(new PP_Resource[buffers.size()]);
  for (size_t i = 0; i < buffers.size(); ++i) {
    // We assume that the browser created a new set of resources.
    DCHECK(!tracker->PluginResourceForHostResource(buffers[i].resource));
    resources[i] = PPB_Buffer_Proxy::AddProxyResource(buffers[i].resource,
                                                      buffers[i].handle,
                                                      buffers[i].size);
  }

  VideoCapture* capture = static_cast<VideoCapture*>(enter.object());
  capture->SetBufferCount(buffers.size());
  ppp_video_capture_impl_->OnDeviceInfo(
      host_resource.instance(),
      capture->pp_resource(),
      &info,
      buffers.size(),
      resources.get());
  for (size_t i = 0; i < buffers.size(); ++i)
    tracker->ReleaseResource(resources[i]);
}

void PPP_VideoCapture_Proxy::OnMsgOnStatus(const HostResource& host_resource,
                                           uint32_t status) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(host_resource);
  if (enter.failed() || !ppp_video_capture_impl_)
    return;

  VideoCapture* capture = static_cast<VideoCapture*>(enter.object());
  if (!capture->OnStatus(static_cast<PP_VideoCaptureStatus_Dev>(status)))
    return;
  ppp_video_capture_impl_->OnStatus(
      host_resource.instance(), capture->pp_resource(), status);
}

void PPP_VideoCapture_Proxy::OnMsgOnError(const HostResource& host_resource,
                                          uint32_t error_code) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(host_resource);
  if (enter.failed() || !ppp_video_capture_impl_)
    return;

  VideoCapture* capture = static_cast<VideoCapture*>(enter.object());
  capture->set_status(PP_VIDEO_CAPTURE_STATUS_STOPPED);
  ppp_video_capture_impl_->OnError(
      host_resource.instance(), capture->pp_resource(), error_code);
}

void PPP_VideoCapture_Proxy::OnMsgOnBufferReady(
    const HostResource& host_resource, uint32_t buffer) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(host_resource);
  if (enter.failed() || !ppp_video_capture_impl_)
    return;

  VideoCapture* capture = static_cast<VideoCapture*>(enter.object());
  capture->SetBufferInUse(buffer);
  ppp_video_capture_impl_->OnBufferReady(
      host_resource.instance(), capture->pp_resource(), buffer);
}

}  // namespace proxy
}  // namespace ppapi
