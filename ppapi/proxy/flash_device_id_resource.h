// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FLASH_DEVICE_ID_RESOURCE_H_
#define PPAPI_PROXY_FLASH_DEVICE_ID_RESOURCE_H_

#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_flash_device_id_api.h"

namespace ppapi {
namespace proxy {

class FlashDeviceIDResource
    : public PluginResource,
      public thunk::PPB_Flash_DeviceID_API {
 public:
  FlashDeviceIDResource(Connection connection,
                        PP_Instance instance);
  virtual ~FlashDeviceIDResource();

  // Resource override.
  virtual thunk::PPB_Flash_DeviceID_API* AsPPB_Flash_DeviceID_API() OVERRIDE;

  // PPB_Flash_DeviceID_API implementation.
  virtual int32_t GetDeviceID(PP_Var* id,
                              scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  // PluginResource override;
  virtual void OnReplyReceived(const ResourceMessageReplyParams& params,
                               const IPC::Message& msg);

  // IPC message handler.
  void OnPluginMsgGetDeviceIDReply(const ResourceMessageReplyParams& params,
                                   const std::string& id);

  // Non-null when a callback is pending.
  PP_Var* dest_;

  scoped_refptr<TrackedCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(FlashDeviceIDResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FLASH_DEVICE_ID_RESOURCE_H_
