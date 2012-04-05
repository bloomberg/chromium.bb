// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_RESOURCE_ARRAY_DEV_H_
#define PPAPI_CPP_DEV_RESOURCE_ARRAY_DEV_H_

#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class InstanceHandle;

class ResourceArray_Dev : public Resource {
 public:
  // Heap-allocated data passed to ArrayOutputCallbackConverter(). Please see
  // the comment of ArrayOutputCallbackConverter() for more details.
  struct ArrayOutputCallbackData {
    ArrayOutputCallbackData(const PP_ArrayOutput& array_output,
                            const PP_CompletionCallback& callback)
        : resource_array_output(0),
          output(array_output),
          original_callback(callback) {
    }

    PP_Resource resource_array_output;
    PP_ArrayOutput output;
    PP_CompletionCallback original_callback;
  };

  ResourceArray_Dev();

  ResourceArray_Dev(PassRef, PP_Resource resource);

  ResourceArray_Dev(const ResourceArray_Dev& other);

  ResourceArray_Dev(const InstanceHandle& instance,
                    const PP_Resource elements[],
                    uint32_t size);

  virtual ~ResourceArray_Dev();

  ResourceArray_Dev& operator=(const ResourceArray_Dev& other);

  uint32_t size() const;

  PP_Resource operator[](uint32_t index) const;

  // This is an adapter for backward compatibility. It works with those APIs
  // which take a PPB_ResourceArray_Dev resource as output parameter, and
  // returns results using PP_ArrayOutput. For example:
  //
  //   ResourceArray_Dev::ArrayOutputCallbackData* data =
  //       new ResourceArray_Dev::ArrayOutputCallbackData(
  //           pp_array_output, callback);
  //   ppb_foo->Bar(
  //       foo_resource, &data->resource_array_output,
  //       PP_MakeCompletionCallback(
  //           &ResourceArray_Dev::ArrayOutputCallbackConverter, data));
  //
  // It takes a heap-allocated ArrayOutputCallbackData struct passed as the user
  // data, and deletes it when the call completes.
  static void ArrayOutputCallbackConverter(void* user_data, int32_t result);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_RESOURCE_ARRAY_DEV_H_
