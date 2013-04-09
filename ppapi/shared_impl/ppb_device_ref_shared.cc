// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_device_ref_shared.h"

#include "base/memory/scoped_ptr.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_resource_array_shared.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"

using ppapi::thunk::PPB_DeviceRef_API;

namespace ppapi {

DeviceRefData::DeviceRefData()
    : type(PP_DEVICETYPE_DEV_INVALID) {
}

PPB_DeviceRef_Shared::PPB_DeviceRef_Shared(ResourceObjectType type,
                                           PP_Instance instance,
                                           const DeviceRefData& data)
    : Resource(type, instance),
      data_(data) {
}

PPB_DeviceRef_API* PPB_DeviceRef_Shared::AsPPB_DeviceRef_API() {
  return this;
}

const DeviceRefData& PPB_DeviceRef_Shared::GetDeviceRefData() const {
  return data_;
}

PP_DeviceType_Dev PPB_DeviceRef_Shared::GetType() {
  return data_.type;
}

PP_Var PPB_DeviceRef_Shared::GetName() {
  return StringVar::StringToPPVar(data_.name);
}

// static
PP_Resource PPB_DeviceRef_Shared::CreateResourceArray(
    ResourceObjectType type,
    PP_Instance instance,
    const std::vector<DeviceRefData>& devices) {
  scoped_ptr<PP_Resource[]> elements;
  size_t size = devices.size();
  if (size > 0) {
    elements.reset(new PP_Resource[size]);
    for (size_t index = 0; index < size; ++index) {
      PPB_DeviceRef_Shared* device_object =
          new PPB_DeviceRef_Shared(type, instance, devices[index]);
      elements[index] = device_object->GetReference();
    }
  }
  PPB_ResourceArray_Shared* array_object =
      new PPB_ResourceArray_Shared(type, instance, elements.get(),
                                   static_cast<uint32_t>(size));

  for (size_t index = 0; index < size; ++index)
    PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(elements[index]);

  return array_object->GetReference();
}

}  // namespace ppapi
