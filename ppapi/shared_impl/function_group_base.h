// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_
#define PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_

#include <stddef.h>  // For NULL.

#include "ppapi/shared_impl/ppapi_shared_export.h"

#define FOR_ALL_PPAPI_FUNCTION_APIS(F) \
  F(PPB_CharSet_FunctionAPI) \
  F(PPB_CursorControl_FunctionAPI) \
  F(PPB_Font_FunctionAPI) \
  F(PPB_Fullscreen_FunctionAPI) \
  F(PPB_Instance_FunctionAPI) \
  F(PPB_TextInput_FunctionAPI) \
  F(ResourceCreationAPI)

namespace ppapi {

// Forward declare all the function APIs.
namespace thunk {
#define DECLARE_FUNCTION_CLASS(FUNCTIONS) class FUNCTIONS;
FOR_ALL_PPAPI_FUNCTION_APIS(DECLARE_FUNCTION_CLASS)
#undef DECLARE_FUNCTION_CLASS
}  // namespace thunk

class PPAPI_SHARED_EXPORT FunctionGroupBase {
 public:
  virtual ~FunctionGroupBase();

  // Dynamic casting for this object. Returns the pointer to the given type if
  // Inheritance-based dynamic casting for this object. Returns the pointer to
  // the given type if it's supported. Derived classes override the functions
  // they support to return the interface.
  #define DEFINE_TYPE_GETTER(FUNCTIONS) \
    virtual thunk::FUNCTIONS* As##FUNCTIONS();
  FOR_ALL_PPAPI_FUNCTION_APIS(DEFINE_TYPE_GETTER)
  #undef DEFINE_TYPE_GETTER

  // Template-based dynamic casting. See specializations below.
  template <typename T> T* GetAs() { return NULL; }
};

// Template-based dynamic casting. These specializations forward to the
// AsXXX virtual functions to return whether the given type is supported.
#define DEFINE_FUNCTION_CAST(FUNCTIONS) \
  template<> inline thunk::FUNCTIONS* FunctionGroupBase::GetAs() { \
    return As##FUNCTIONS(); \
  }
FOR_ALL_PPAPI_FUNCTION_APIS(DEFINE_FUNCTION_CAST)
#undef DEFINE_FUNCTION_CAST

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_
