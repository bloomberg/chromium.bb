// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/resource_array_dev.h"

#include "ppapi/c/dev/ppb_resource_array_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_ResourceArray_Dev>() {
  return PPB_RESOURCEARRAY_DEV_INTERFACE;
}

}  // namespace

ResourceArray_Dev::ResourceArray_Dev() {
}

ResourceArray_Dev::ResourceArray_Dev(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

ResourceArray_Dev::ResourceArray_Dev(const ResourceArray_Dev& other)
    : Resource(other) {
}

ResourceArray_Dev::ResourceArray_Dev(const InstanceHandle& instance,
                                     const PP_Resource elements[],
                                     uint32_t size) {
  if (has_interface<PPB_ResourceArray_Dev>()) {
    PassRefFromConstructor(get_interface<PPB_ResourceArray_Dev>()->Create(
        instance.pp_instance(), elements, size));
  }
}

ResourceArray_Dev::~ResourceArray_Dev() {
}

ResourceArray_Dev& ResourceArray_Dev::operator=(
    const ResourceArray_Dev& other) {
  Resource::operator=(other);
  return *this;
}

uint32_t ResourceArray_Dev::size() const {
  if (!has_interface<PPB_ResourceArray_Dev>())
    return 0;
  return get_interface<PPB_ResourceArray_Dev>()->GetSize(pp_resource());
}

PP_Resource ResourceArray_Dev::operator[](uint32_t index) const {
  if (!has_interface<PPB_ResourceArray_Dev>())
    return 0;
  return get_interface<PPB_ResourceArray_Dev>()->GetAt(pp_resource(), index);
}

// static
void ResourceArray_Dev::ArrayOutputCallbackConverter(void* user_data,
                                                     int32_t result) {
  ArrayOutputCallbackData* data =
      static_cast<ArrayOutputCallbackData*>(user_data);

  // data->resource_array_output should remain 0 if the call failed.
  ResourceArray_Dev resources(PASS_REF, data->resource_array_output);
  PP_DCHECK(resources.is_null() || result == PP_OK);

  // Need to issue the "GetDataBuffer" even for error cases and when the number
  // of items is 0.
  PP_Resource* output_buf = static_cast<PP_Resource*>(
      data->output.GetDataBuffer(
          data->output.user_data, resources.is_null() ? 0 : resources.size(),
          sizeof(PP_Resource)));
  if (output_buf) {
    for (uint32_t index = 0; index < resources.size(); ++index) {
      output_buf[index] = resources[index];
      Module::Get()->core()->AddRefResource(output_buf[index]);
    }
  }

  PP_RunCompletionCallback(&data->original_callback, result);
  delete data;
}

}  // namespace pp
