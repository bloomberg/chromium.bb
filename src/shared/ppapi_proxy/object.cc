/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <string.h>
#include <map>
#include <string>

#include "native_client/src/shared/ppapi_proxy/object.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/pp_var.h"

namespace ppapi_proxy {

namespace {

ppapi_proxy::Object* VoidPtrToObjectPtr(void* object) {
  return reinterpret_cast<Object*>(object);
}

bool HasPropertyThunk(void* object,
                      PP_Var name,
                      PP_Var* exception) {
  return VoidPtrToObjectPtr(object)->HasProperty(name, exception);
}

bool HasMethodThunk(void* object,
                    PP_Var name,
                    PP_Var* exception) {
  return VoidPtrToObjectPtr(object)->HasMethod(name, exception);
}

PP_Var GetPropertyThunk(void* object,
                        PP_Var name,
                        PP_Var* exception) {
  return VoidPtrToObjectPtr(object)->GetProperty(name, exception);
}

void GetAllPropertyNamesThunk(void* object,
                              uint32_t* property_count,
                              PP_Var** properties,
                              PP_Var* exception) {
  VoidPtrToObjectPtr(object)->GetAllPropertyNames(property_count,
                                                  properties,
                                                  exception);
}

void SetPropertyThunk(void* object,
                      PP_Var name,
                      PP_Var value,
                      PP_Var* exception) {
  return VoidPtrToObjectPtr(object)->SetProperty(name, value, exception);
}

void RemovePropertyThunk(void* object,
                         PP_Var name,
                         PP_Var* exception) {
  VoidPtrToObjectPtr(object)->RemoveProperty(name, exception);
}

PP_Var CallThunk(void* object,
                 PP_Var method_name,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  return VoidPtrToObjectPtr(object)->Call(method_name, argc, argv, exception);
}

PP_Var ConstructThunk(void* object,
                      uint32_t argc,
                      PP_Var* argv,
                      PP_Var* exception) {
  return VoidPtrToObjectPtr(object)->Construct(argc, argv, exception);
}

void DeallocateThunk(void* object) {
  VoidPtrToObjectPtr(object)->Deallocate();
}

}  // namespace

const PPP_Class_Deprecated Object::object_class = {
  HasPropertyThunk,
  HasMethodThunk,
  GetPropertyThunk,
  GetAllPropertyNamesThunk,
  SetPropertyThunk,
  RemovePropertyThunk,
  CallThunk,
  ConstructThunk,
  DeallocateThunk
};

}  // namespace ppapi_proxy
