// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_RESOURCE_ARRAY_DEV_H_
#define PPAPI_CPP_DEV_RESOURCE_ARRAY_DEV_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

class ResourceArray_Dev : public Resource {
 public:
  ResourceArray_Dev();

  struct PassRef {};

  ResourceArray_Dev(PassRef, PP_Resource resource);

  ResourceArray_Dev(const ResourceArray_Dev& other);

  ResourceArray_Dev(Instance* instance,
                    const PP_Resource elements[],
                    uint32_t size);

  virtual ~ResourceArray_Dev();

  ResourceArray_Dev& operator=(const ResourceArray_Dev& other);

  uint32_t size() const;

  PP_Resource operator[](uint32_t index) const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_RESOURCE_ARRAY_DEV_H_
