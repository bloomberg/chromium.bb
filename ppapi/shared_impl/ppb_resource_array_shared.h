// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_RESOURCE_ARRAY_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_RESOURCE_ARRAY_SHARED_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_resource_array_api.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_ResourceArray_Shared
    : public Resource,
      public thunk::PPB_ResourceArray_API {
 public:
  PPB_ResourceArray_Shared(ResourceObjectType type,
                           PP_Instance instance,
                           const PP_Resource elements[],
                           uint32_t size);
  virtual ~PPB_ResourceArray_Shared();

  // Resource overrides.
  virtual PPB_ResourceArray_API* AsPPB_ResourceArray_API() OVERRIDE;

  // PPB_ResourceArray_API implementation.
  virtual uint32_t GetSize() OVERRIDE;
  virtual PP_Resource GetAt(uint32_t index) OVERRIDE;

 private:
  std::vector<PP_Resource> resources_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PPB_ResourceArray_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_RESOURCE_ARRAY_SHARED_H_
