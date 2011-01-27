// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_IMAGE_DATA_PROXY_H_
#define PPAPI_PPB_IMAGE_DATA_PROXY_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_structs.h"

struct PPB_ImageData;

namespace pp {
namespace proxy {

class HostResource;

class PPB_ImageData_Proxy : public InterfaceProxy {
 public:
  PPB_ImageData_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_ImageData_Proxy();

  const PPB_ImageData* ppb_image_data_target() const {
    return static_cast<const PPB_ImageData*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgGetNativeImageDataFormat(int32* result);
  void OnMsgIsImageDataFormatSupported(int32 format, PP_Bool* result);
  void OnMsgCreate(PP_Instance instance,
                   int32_t format,
                   const PP_Size& size,
                   PP_Bool init_to_zero,
                   HostResource* result,
                   std::string* image_data_desc,
                   ImageHandle* result_image_handle);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_IMAGE_DATA_PROXY_H_
