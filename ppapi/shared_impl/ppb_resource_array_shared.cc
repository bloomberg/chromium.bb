// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_resource_array_shared.h"

#include "base/logging.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"

using ppapi::thunk::PPB_ResourceArray_API;

namespace ppapi {

PPB_ResourceArray_Shared::PPB_ResourceArray_Shared(ResourceObjectType type,
                                                   PP_Instance instance,
                                                   const PP_Resource elements[],
                                                   uint32_t size)
    : Resource(type, instance) {
  DCHECK(resources_.empty());

  resources_.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    PP_Resource element = elements[index];
    if (element)
      PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(element);
    resources_.push_back(element);
  }
}

PPB_ResourceArray_Shared::~PPB_ResourceArray_Shared() {
  for (std::vector<PP_Resource>::iterator iter = resources_.begin();
       iter != resources_.end(); ++iter) {
    if (*iter)
      PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(*iter);
  }
}

PPB_ResourceArray_API* PPB_ResourceArray_Shared::AsPPB_ResourceArray_API() {
  return this;
}

uint32_t PPB_ResourceArray_Shared::GetSize() {
  return static_cast<uint32_t>(resources_.size());
}

PP_Resource PPB_ResourceArray_Shared::GetAt(uint32_t index) {
  return index < resources_.size() ? resources_[index] : 0;
}

}  // namespace ppapi
