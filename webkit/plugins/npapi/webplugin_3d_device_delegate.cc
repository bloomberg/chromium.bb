// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugin_3d_device_delegate.h"

namespace webkit {
namespace npapi {

NPError WebPlugin3DDeviceDelegate::Device3DQueryCapability(int32 capability,
                                                           int32* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DQueryConfig(
    const NPDeviceContext3DConfig* request,
    NPDeviceContext3DConfig* obtain) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DInitializeContext(
    const NPDeviceContext3DConfig* config,
    NPDeviceContext3D* context) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DSetStateContext(
    NPDeviceContext3D* context,
    int32 state,
    intptr_t value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DGetStateContext(
    NPDeviceContext3D* context,
    int32 state,
    intptr_t* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DFlushContext(
    NPP id,
    NPDeviceContext3D* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* user_data) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DDestroyContext(
    NPDeviceContext3D* context) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DCreateBuffer(
    NPDeviceContext3D* context,
    size_t size,
    int32* id) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DDestroyBuffer(
    NPDeviceContext3D* context,
    int32 id) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DMapBuffer(NPDeviceContext3D* context,
                                                     int32 id,
                                                     NPDeviceBuffer* buffer) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DGetNumConfigs(int32* num_configs) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DGetConfigAttribs(
    int32 config,
    int32* attrib_list) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DCreateContext(
    int32 config,
    const int32* attrib_list,
    NPDeviceContext3D** context) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DRegisterCallback(
    NPP id,
    NPDeviceContext3D* context,
    int32 callback_type,
    NPDeviceGenericCallbackPtr callback,
    void* callback_data) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPlugin3DDeviceDelegate::Device3DSynchronizeContext(
    NPP id,
    NPDeviceContext3D* context,
    NPDeviceSynchronizationMode mode,
    const int32* input_attrib_list,
    int32* output_attrib_list,
    NPDeviceSynchronizeContextCallbackPtr callback,
    void* callback_data) {
  return NPERR_GENERIC_ERROR;
}

}  // namespace npapi
}  // namespace webkit

