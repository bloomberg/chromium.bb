// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugin_2d_device_delegate.h"

namespace webkit {
namespace npapi {

NPError WebPlugin2DDeviceDelegate::Device2DQueryCapability(int32 capability,
                                                           int32* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin2DDeviceDelegate::Device2DQueryConfig(
    const NPDeviceContext2DConfig* request,
    NPDeviceContext2DConfig* obtain) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin2DDeviceDelegate::Device2DInitializeContext(
    const NPDeviceContext2DConfig* config,
    NPDeviceContext2D* context) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin2DDeviceDelegate::Device2DSetStateContext(
    NPDeviceContext2D* context,
    int32 state,
    intptr_t value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin2DDeviceDelegate::Device2DGetStateContext(
    NPDeviceContext2D* context,
    int32 state,
    intptr_t* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin2DDeviceDelegate::Device2DFlushContext(
    NPP id,
    NPDeviceContext2D* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* user_data) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin2DDeviceDelegate::Device2DDestroyContext(
    NPDeviceContext2D* context) {
  return NPERR_GENERIC_ERROR;
}

}  // namespace npapi
}  // namespace webkit
