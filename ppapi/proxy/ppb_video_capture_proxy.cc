// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_video_capture_proxy.h"

#include <vector>

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
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ppapi/thunk/ppb_buffer_trusted_api.h"
#include "ppapi/thunk/ppb_video_capture_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_BufferTrusted_API;
using ppapi::thunk::PPB_VideoCapture_API;

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreatePPBVideoCaptureProxy(Dispatcher* dispatcher,
                                           const void* target_interface) {
  return new PPB_VideoCapture_Proxy(dispatcher, target_interface);
}

InterfaceProxy* CreatePPPVideoCaptureProxy(Dispatcher* dispatcher,
                                           const void* target_interface) {
  return new PPP_VideoCapture_Proxy(dispatcher, target_interface);
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
      dispatcher->GetLocalInterface(PPB_CORE_INTERFACE));
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
      INTERFACE_ID_PPP_VIDEO_CAPTURE_DEV, host_resource, *info, buffers));
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
      INTERFACE_ID_PPP_VIDEO_CAPTURE_DEV, host_resource, status));
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
      INTERFACE_ID_PPP_VIDEO_CAPTURE_DEV, host_resource, error_code));
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
      INTERFACE_ID_PPP_VIDEO_CAPTURE_DEV, host_resource, buffer));
}

PPP_VideoCapture_Dev ppp_video_capture = {
  OnDeviceInfo,
  OnStatus,
  OnError,
  OnBufferReady
};

}  // namespace

class VideoCapture : public ppapi::thunk::PPB_VideoCapture_API,
                     public Resource {
 public:
  VideoCapture(const HostResource& resource);
  virtual ~VideoCapture();

  // Resource overrides.
  virtual ppapi::thunk::PPB_VideoCapture_API* AsPPB_VideoCapture_API() OVERRIDE;

  // PPB_VideoCapture_API implementation.
  virtual int32_t StartCapture(
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count) {
    switch (status_) {
      case PP_VIDEO_CAPTURE_STATUS_STARTING:
      case PP_VIDEO_CAPTURE_STATUS_STARTED:
      case PP_VIDEO_CAPTURE_STATUS_PAUSED:
      default:
        return PP_ERROR_FAILED;
      case PP_VIDEO_CAPTURE_STATUS_STOPPED:
      case PP_VIDEO_CAPTURE_STATUS_STOPPING:
        break;
    }
    status_ = PP_VIDEO_CAPTURE_STATUS_STARTING;
    GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_StartCapture(
        INTERFACE_ID_PPB_VIDEO_CAPTURE_DEV, host_resource(),
        requested_info, buffer_count));
    return PP_OK;
  }

  virtual int32_t ReuseBuffer(uint32_t buffer) {
    if (buffer >= buffer_in_use_.size() || !buffer_in_use_[buffer])
      return PP_ERROR_BADARGUMENT;
    GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_ReuseBuffer(
        INTERFACE_ID_PPB_VIDEO_CAPTURE_DEV, host_resource(), buffer));
    return PP_OK;
  }

  virtual int32_t StopCapture() {
    switch (status_) {
      case PP_VIDEO_CAPTURE_STATUS_STOPPED:
      case PP_VIDEO_CAPTURE_STATUS_STOPPING:
      default:
        return PP_ERROR_FAILED;
      case PP_VIDEO_CAPTURE_STATUS_STARTING:
      case PP_VIDEO_CAPTURE_STATUS_STARTED:
      case PP_VIDEO_CAPTURE_STATUS_PAUSED:
        break;
    }
    buffer_in_use_.clear();
    status_ = PP_VIDEO_CAPTURE_STATUS_STOPPING;
    GetDispatcher()->Send(new PpapiHostMsg_PPBVideoCapture_StopCapture(
        INTERFACE_ID_PPB_VIDEO_CAPTURE_DEV, host_resource()));
    return PP_OK;
  }

  bool OnStatus(uint32_t status) {
    switch (status) {
      case PP_VIDEO_CAPTURE_STATUS_STARTING:
      case PP_VIDEO_CAPTURE_STATUS_STOPPING:
      default:
        // Those states are not sent by the browser.
        NOTREACHED();
        return false;
      case PP_VIDEO_CAPTURE_STATUS_STARTED:
        switch (status_) {
          case PP_VIDEO_CAPTURE_STATUS_STARTING:
          case PP_VIDEO_CAPTURE_STATUS_PAUSED:
            break;
          default:
            return false;
        }
        break;
      case PP_VIDEO_CAPTURE_STATUS_PAUSED:
        switch (status_) {
          case PP_VIDEO_CAPTURE_STATUS_STARTING:
          case PP_VIDEO_CAPTURE_STATUS_STARTED:
            break;
          default:
            return false;
        }
        break;
      case PP_VIDEO_CAPTURE_STATUS_STOPPED:
        if (status_ != PP_VIDEO_CAPTURE_STATUS_STOPPING)
          return false;
        break;
    }
    status_ = status;
    return true;
  }

  void set_status(uint32_t status) { status_ = status; }

  void SetBufferCount(size_t count) {
    buffer_in_use_ = std::vector<bool>(count);
  }
  void SetBufferInUse(uint32_t buffer) {
    DCHECK(buffer < buffer_in_use_.size());
    buffer_in_use_[buffer] = true;
  }

 private:
  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  uint32_t status_;
  std::vector<bool> buffer_in_use_;
  DISALLOW_COPY_AND_ASSIGN(VideoCapture);
};

VideoCapture::VideoCapture(const HostResource& resource)
    : Resource(resource),
      status_(PP_VIDEO_CAPTURE_STATUS_STOPPED) {
}

VideoCapture::~VideoCapture() {
}

ppapi::thunk::PPB_VideoCapture_API* VideoCapture::AsPPB_VideoCapture_API() {
  return this;
}

PPB_VideoCapture_Proxy::PPB_VideoCapture_Proxy(Dispatcher* dispatcher,
                                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_VideoCapture_Proxy::~PPB_VideoCapture_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_VideoCapture_Proxy::GetInfo() {
  static const Info info = {
    ppapi::thunk::GetPPB_VideoCapture_Thunk(),
    PPB_VIDEO_CAPTURE_DEV_INTERFACE,
    INTERFACE_ID_PPB_VIDEO_CAPTURE_DEV,
    false,
    &CreatePPBVideoCaptureProxy,
  };
  return &info;
}

// static
PP_Resource PPB_VideoCapture_Proxy::CreateProxyResource(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBVideoCapture_Create(
      INTERFACE_ID_PPB_VIDEO_CAPTURE_DEV, instance, &result));
  if (result.is_null())
    return 0;
  return (new VideoCapture(result))->GetReference();
}

bool PPB_VideoCapture_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_VideoCapture_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_StartCapture,
                        OnMsgStartCapture)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_ReuseBuffer,
                        OnMsgReuseBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoCapture_StopCapture,
                        OnMsgStopCapture)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_VideoCapture_Proxy::OnMsgCreate(PP_Instance instance,
                                         HostResource* result_resource) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  PP_Resource resource = ppb_video_capture_target()->Create(instance);
  result_resource->SetHostResource(instance, resource);
}

void PPB_VideoCapture_Proxy::OnMsgStartCapture(
    const HostResource& resource,
    const PP_VideoCaptureDeviceInfo_Dev& info,
    uint32_t buffers) {
  EnterHostFromHostResource<PPB_VideoCapture_API> enter(resource);
  if (enter.succeeded())
    enter.object()->StartCapture(info, buffers);
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

PPP_VideoCapture_Proxy::PPP_VideoCapture_Proxy(Dispatcher* dispatcher,
                                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPP_VideoCapture_Proxy::~PPP_VideoCapture_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_VideoCapture_Proxy::GetInfo() {
  static const Info info = {
    &ppp_video_capture,
    PPP_VIDEO_CAPTURE_DEV_INTERFACE,
    INTERFACE_ID_PPP_VIDEO_CAPTURE_DEV,
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
  PluginResourceTracker* tracker = PluginResourceTracker::GetInstance();
  PP_Resource resource = tracker->PluginResourceForHostResource(host_resource);
  if (!resource || !ppp_video_capture_target() || enter.failed())
    return;

  scoped_array<PP_Resource> resources(new PP_Resource[buffers.size()]);
  for (size_t i = 0; i < buffers.size(); ++i) {
    // We assume that the browser created a new set of resources.
    DCHECK(!tracker->PluginResourceForHostResource(buffers[i].resource));
    resources[i] = PPB_Buffer_Proxy::AddProxyResource(buffers[i].resource,
                                                      buffers[i].handle,
                                                      buffers[i].size);
  }
  static_cast<VideoCapture*>(enter.object())->SetBufferCount(buffers.size());
  ppp_video_capture_target()->OnDeviceInfo(
      host_resource.instance(),
      resource,
      &info,
      buffers.size(),
      resources.get());
  for (size_t i = 0; i < buffers.size(); ++i)
    tracker->ReleaseResource(resources[i]);
}

void PPP_VideoCapture_Proxy::OnMsgOnStatus(const HostResource& host_resource,
                                           uint32_t status) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(host_resource);
  PluginResourceTracker* tracker = PluginResourceTracker::GetInstance();
  PP_Resource resource = tracker->PluginResourceForHostResource(host_resource);
  if (!resource || !ppp_video_capture_target() || enter.failed())
    return;

  if (!static_cast<VideoCapture*>(enter.object())->OnStatus(status))
    return;
  ppp_video_capture_target()->OnStatus(
      host_resource.instance(), resource, status);
}

void PPP_VideoCapture_Proxy::OnMsgOnError(const HostResource& host_resource,
                                          uint32_t error_code) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(host_resource);
  PluginResourceTracker* tracker = PluginResourceTracker::GetInstance();
  PP_Resource resource = tracker->PluginResourceForHostResource(host_resource);
  if (!resource || !ppp_video_capture_target() || enter.failed())
    return;
  static_cast<VideoCapture*>(enter.object())->set_status(
      PP_VIDEO_CAPTURE_STATUS_STOPPED);
  ppp_video_capture_target()->OnError(
      host_resource.instance(), resource, error_code);
}

void PPP_VideoCapture_Proxy::OnMsgOnBufferReady(
    const HostResource& host_resource, uint32_t buffer) {
  EnterPluginFromHostResource<PPB_VideoCapture_API> enter(host_resource);
  PluginResourceTracker* tracker = PluginResourceTracker::GetInstance();
  PP_Resource resource = tracker->PluginResourceForHostResource(host_resource);
  if (!resource || !ppp_video_capture_target() || enter.failed())
    return;
  static_cast<VideoCapture*>(enter.object())->SetBufferInUse(buffer);
  ppp_video_capture_target()->OnBufferReady(
      host_resource.instance(), resource, buffer);
}

}  // namespace proxy
}  // namespace ppapi
