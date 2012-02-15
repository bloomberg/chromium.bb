// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_VIDEO_CAPTURE_PROXY_H_
#define PPAPI_PROXY_PPB_VIDEO_CAPTURE_PROXY_H_

#include <string>
#include <vector>

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/utility/completion_callback_factory.h"

struct PPP_VideoCapture_Dev;
struct PP_VideoCaptureDeviceInfo_Dev;

namespace ppapi {

class HostResource;

namespace proxy {

class PPB_VideoCapture_Proxy : public InterfaceProxy {
 public:
  explicit PPB_VideoCapture_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_VideoCapture_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_VIDEO_CAPTURE_DEV;

 private:
  // Message handlers in the renderer process.
  void OnMsgCreate(PP_Instance instance, ppapi::HostResource* result_resource);
  void OnMsgEnumerateDevices(const ppapi::HostResource& resource);
  void OnMsgOpen(const ppapi::HostResource& resource,
                 const std::string& device_id,
                 const PP_VideoCaptureDeviceInfo_Dev& info,
                 uint32_t buffers);
  void OnMsgStartCapture(const ppapi::HostResource& resource);
  void OnMsgReuseBuffer(const ppapi::HostResource& resource,
                        uint32_t buffer);
  void OnMsgStopCapture(const ppapi::HostResource& resource);
  void OnMsgClose(const ppapi::HostResource& resource);
  void OnMsgStartCapture0_1(const ppapi::HostResource& resource,
                            const PP_VideoCaptureDeviceInfo_Dev& info,
                            uint32_t buffers);

  // Message handlers in the plugin process.
  void OnMsgEnumerateDevicesACK(
      const ppapi::HostResource& resource,
      int32_t result,
      const std::vector<ppapi::DeviceRefData>& devices);
  void OnMsgOpenACK(const ppapi::HostResource& resource, int32_t result);

  void EnumerateDevicesACKInHost(int32_t result,
                                 const ppapi::HostResource& resource);
  void OpenACKInHost(int32_t result, const ppapi::HostResource& resource);

  pp::CompletionCallbackFactory<PPB_VideoCapture_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoCapture_Proxy);
};

class PPP_VideoCapture_Proxy : public InterfaceProxy {
 public:
  explicit PPP_VideoCapture_Proxy(Dispatcher* dispatcher);
  virtual ~PPP_VideoCapture_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPP_VIDEO_CAPTURE_DEV;

 private:
  // Message handlers.
  void OnMsgOnDeviceInfo(const ppapi::HostResource& video_capture,
                         const PP_VideoCaptureDeviceInfo_Dev& info,
                         const std::vector<PPPVideoCapture_Buffer>& buffers);
  void OnMsgOnStatus(const ppapi::HostResource& video_capture,
                     uint32_t status);
  void OnMsgOnError(const ppapi::HostResource& video_capture,
                    uint32_t error_code);
  void OnMsgOnBufferReady(const ppapi::HostResource& video_capture,
                          uint32_t buffer);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_VideoCapture_Dev* ppp_video_capture_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPP_VideoCapture_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_VIDEO_CAPTURE_PROXY_H_
