// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPP_INSTANCE_COMBINED_H_
#define PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPP_INSTANCE_COMBINED_H_

#include "ppapi/c/ppp_instance.h"

namespace ppapi_proxy {

// This exposes the 1.1 interface and forwards it to the 1.0 interface is
// necessary.
struct  PPP_Instance_Combined {
 public:
  // You must call one of the Init functions after the constructor.
  PPP_Instance_Combined();

  void Init1_0(const PPP_Instance_1_0* instance_if);
  void Init1_1(const PPP_Instance_1_1* instance_if);

  bool initialized() const { return initialized_; }

  PP_Bool DidCreate(PP_Instance instance,
                    uint32_t argc,
                    const char* argn[],
                    const char* argv[]);
  void DidDestroy(PP_Instance instance);

  // This version of DidChangeView encapsulates all arguments for both 1.0
  // and 1.1 versions of this function. Conversion from 1.1 -> 1.0 is easy,
  // but this class doesn't have the necessary context (resource interfaces)
  // to do the conversion, so the caller must do it.
  void DidChangeView(PP_Instance instance,
                     PP_Resource view_resource,
                     const struct PP_Rect* position,
                     const struct PP_Rect* clip);

  void DidChangeFocus(PP_Instance instance, PP_Bool has_focus);
  PP_Bool HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader);

 private:
  bool initialized_;

  // For version 1.0, DidChangeView will be NULL, and DidChangeView_1_0 will
  // be set below.
  PPP_Instance_1_1 instance_1_1_;

  // Non-NULL when Instance 1.0 is used.
  void (*did_change_view_1_0_)(PP_Instance instance,
                               const struct PP_Rect* position,
                               const struct PP_Rect* clip);
};

}  // namespace ppapi_proxy

#endif  // PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPP_INSTANCE_COMBINED_H_

