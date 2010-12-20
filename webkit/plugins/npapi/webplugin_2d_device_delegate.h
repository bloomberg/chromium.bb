// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_2D_DEVICE_DELEGATE_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_2D_DEVICE_DELEGATE_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace webkit {
namespace npapi {

// Interface for the NPAPI 2D device extension. This class implements "NOP"
// versions of all these functions so it can be used seamlessly by the
// "regular" plugin delegate while being overridden by the "pepper" one.
class WebPlugin2DDeviceDelegate {
 public:
  virtual NPError Device2DQueryCapability(int32 capability, int32* value);
  virtual NPError Device2DQueryConfig(const NPDeviceContext2DConfig* request,
                                      NPDeviceContext2DConfig* obtain);
  virtual NPError Device2DInitializeContext(
      const NPDeviceContext2DConfig* config,
      NPDeviceContext2D* context);
  virtual NPError Device2DSetStateContext(NPDeviceContext2D* context,
                                          int32 state,
                                          intptr_t value);
  virtual NPError Device2DGetStateContext(NPDeviceContext2D* context,
                                          int32 state,
                                          intptr_t* value);
  virtual NPError Device2DFlushContext(NPP id,
                                       NPDeviceContext2D* context,
                                       NPDeviceFlushContextCallbackPtr callback,
                                       void* user_data);
  virtual NPError Device2DDestroyContext(NPDeviceContext2D* context);

 protected:
  WebPlugin2DDeviceDelegate() {}
  virtual ~WebPlugin2DDeviceDelegate() {}
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_2D_DEVICE_DELEGATE_H_
