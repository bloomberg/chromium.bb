// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/shared/ppapi_proxy/ppp_instance_combined.h"

#include <stdlib.h>
#include <string.h>

namespace ppapi_proxy {

PPP_Instance_Combined::PPP_Instance_Combined()
    : initialized_(false),
      did_change_view_1_0_(NULL) {
  memset(&instance_1_1_, 0, sizeof(instance_1_1_));
}

void PPP_Instance_Combined::Init1_0(const PPP_Instance_1_0* instance_if) {
  initialized_ = true;
  did_change_view_1_0_ = instance_if->DidChangeView;
  instance_1_1_.DidCreate = instance_if->DidCreate;
  instance_1_1_.DidDestroy = instance_if->DidDestroy;
  instance_1_1_.DidChangeView = NULL;
  instance_1_1_.DidChangeFocus = instance_if->DidChangeFocus;
  instance_1_1_.HandleDocumentLoad = instance_if->HandleDocumentLoad;
}

void PPP_Instance_Combined::Init1_1(const PPP_Instance_1_1* instance_if) {
  initialized_ = true;
  instance_1_1_ = *instance_if;
}

PP_Bool PPP_Instance_Combined::DidCreate(PP_Instance instance,
                                         uint32_t argc,
                                         const char* argn[],
                                         const char* argv[]) {
  return instance_1_1_.DidCreate(instance, argc, argn, argv);
}

void PPP_Instance_Combined::DidDestroy(PP_Instance instance) {
  return instance_1_1_.DidDestroy(instance);
}

void PPP_Instance_Combined::DidChangeView(PP_Instance instance,
                                          PP_Resource view_resource,
                                          const struct PP_Rect* position,
                                          const struct PP_Rect* clip) {
  if (instance_1_1_.DidChangeView)
    instance_1_1_.DidChangeView(instance, view_resource);
  else
    did_change_view_1_0_(instance, position, clip);
}

void PPP_Instance_Combined::DidChangeFocus(PP_Instance instance,
                                           PP_Bool has_focus) {
  instance_1_1_.DidChangeFocus(instance, has_focus);
}

PP_Bool PPP_Instance_Combined::HandleDocumentLoad(PP_Instance instance,
                                                  PP_Resource url_loader) {
  return instance_1_1_.HandleDocumentLoad(instance, url_loader);
}

}  // namespace ppapi_proxy

