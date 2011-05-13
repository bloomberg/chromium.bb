// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/shared_impl/image_data_impl.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_ImageDataFormat GetNativeImageDataFormat() {
  return ppapi::ImageDataImpl::GetNativeImageDataFormat();
}

PP_Bool IsImageDataFormatSupported(PP_ImageDataFormat format) {
  return ppapi::ImageDataImpl::IsImageDataFormatSupported(format)
      ? PP_TRUE : PP_FALSE;
}

PP_Resource Create(PP_Instance instance,
                   PP_ImageDataFormat format,
                   const PP_Size* size,
                   PP_Bool init_to_zero) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateImageData(instance, format,
                                            *size, init_to_zero);
}

PP_Bool IsImageData(PP_Resource resource) {
  EnterResource<PPB_ImageData_API> enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_Bool Describe(PP_Resource resource, PP_ImageDataDesc* desc) {
  // Give predictable values on failure.
  memset(desc, 0, sizeof(PP_ImageDataDesc));

  EnterResource<PPB_ImageData_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->Describe(desc);
}

void* Map(PP_Resource resource) {
  EnterResource<PPB_ImageData_API> enter(resource, true);
  if (enter.failed())
    return NULL;
  return enter.object()->Map();
}

void Unmap(PP_Resource resource) {
  EnterResource<PPB_ImageData_API> enter(resource, true);
  if (enter.failed())
    return;
  enter.object()->Unmap();
}

const PPB_ImageData g_ppb_image_data_thunk = {
  &GetNativeImageDataFormat,
  &IsImageDataFormatSupported,
  &Create,
  &IsImageData,
  &Describe,
  &Map,
  &Unmap,
};

}  // namespace

const PPB_ImageData* GetPPB_ImageData_Thunk() {
  return &g_ppb_image_data_thunk;
}

}  // namespace thunk
}  // namespace ppapi
