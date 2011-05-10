// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_RESOURCE_CREATION_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_RESOURCE_CREATION_IMPL_H_

#include "base/basictypes.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace webkit {
namespace ppapi {

class ResourceCreationImpl : public ::ppapi::shared_impl::FunctionGroupBase,
                             public ::ppapi::thunk::ResourceCreationAPI {
 public:
  ResourceCreationImpl();
  virtual ~ResourceCreationImpl();

  // FunctionGroupBase implementation.
  virtual ::ppapi::thunk::ResourceCreationAPI* AsResourceCreation();

  // ResourceCreationAPI implementation.
  virtual PP_Resource CreateFontObject(
      PP_Instance instance,
      const PP_FontDescription_Dev* description);
  virtual PP_Resource CreateGraphics2D(PP_Instance pp_instance,
                                       const PP_Size& size,
                                       PP_Bool is_always_opaque);
  virtual PP_Resource CreateImageData(PP_Instance instance,
                                      PP_ImageDataFormat format,
                                      const PP_Size& size,
                                      PP_Bool init_to_zero);

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceCreationImpl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_RESOURCE_CREATION_IMPL_H_
