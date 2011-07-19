// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_3D_DEVICE_DELEGATE_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_3D_DEVICE_DELEGATE_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace webkit {
namespace npapi {

// Interface for the NPAPI 3D device extension. This class implements "NOP"
// versions of all these functions so it can be used seamlessly by the
// "regular" plugin delegate while being overridden by the "pepper" one.
class WebPlugin3DDeviceDelegate {
 public:
  virtual NPError Device3DQueryCapability(int32 capability, int32* value);
  virtual NPError Device3DQueryConfig(const NPDeviceContext3DConfig* request,
                                      NPDeviceContext3DConfig* obtain);
  virtual NPError Device3DInitializeContext(
      const NPDeviceContext3DConfig* config,
      NPDeviceContext3D* context);
  virtual NPError Device3DSetStateContext(NPDeviceContext3D* context,
                                          int32 state,
                                          intptr_t value);
  virtual NPError Device3DGetStateContext(NPDeviceContext3D* context,
                                          int32 state,
                                          intptr_t* value);
  virtual NPError Device3DFlushContext(NPP id,
                                       NPDeviceContext3D* context,
                                       NPDeviceFlushContextCallbackPtr callback,
                                       void* user_data);
  virtual NPError Device3DDestroyContext(NPDeviceContext3D* context);
  virtual NPError Device3DCreateBuffer(NPDeviceContext3D* context,
                                       size_t size,
                                       int32* id);
  virtual NPError Device3DDestroyBuffer(NPDeviceContext3D* context,
                                        int32 id);
  virtual NPError Device3DMapBuffer(NPDeviceContext3D* context,
                                    int32 id,
                                    NPDeviceBuffer* buffer);
  virtual NPError Device3DGetNumConfigs(int32* num_configs);
  virtual NPError Device3DGetConfigAttribs(int32 config,
                                           int32* attrib_list);
  virtual NPError Device3DCreateContext(int32 config,
                                        const int32* attrib_list,
                                        NPDeviceContext3D** context);
  virtual NPError Device3DRegisterCallback(
      NPP id,
      NPDeviceContext3D* context,
      int32 callback_type,
      NPDeviceGenericCallbackPtr callback,
      void* callback_data);
  virtual NPError Device3DSynchronizeContext(
      NPP id,
      NPDeviceContext3D* context,
      NPDeviceSynchronizationMode mode,
      const int32* input_attrib_list,
      int32* output_attrib_list,
      NPDeviceSynchronizeContextCallbackPtr callback,
      void* callback_data);

 protected:
  WebPlugin3DDeviceDelegate() {}
  virtual ~WebPlugin3DDeviceDelegate() {}
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_3D_DEVICE_DELEGATE_H_
