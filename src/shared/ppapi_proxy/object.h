/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_H_

#include <map>
#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_class_deprecated.h"
#include "native_client/src/third_party/ppapi/c/pp_var.h"

namespace ppapi_proxy {

// Implements a scriptable object in PPAPI.
// This class is used in both the browser and plugin.
// These methods are in one to one correspondence with the methods exported by
// the interface in ppapi/c/dev/ppp_class_deprecated.h.
class Object {
 public:
  Object() {}
  virtual ~Object() {}

  // The bindings for the methods invoked by the PPAPI interface.
  virtual bool HasProperty(PP_Var name,
                           PP_Var* exception) = 0;
  virtual bool HasMethod(PP_Var name,
                         PP_Var* exception) = 0;
  virtual PP_Var GetProperty(PP_Var name,
                             PP_Var* exception) = 0;
  virtual void GetAllPropertyNames(uint32_t* property_count,
                                   PP_Var** properties,
                                   PP_Var* exception) = 0;
  virtual void SetProperty(PP_Var name,
                           PP_Var value,
                           PP_Var* exception) = 0;
  virtual void RemoveProperty(PP_Var name,
                              PP_Var* exception) = 0;
  virtual PP_Var Call(PP_Var method_name,
                      uint32_t argc,
                      PP_Var* argv,
                      PP_Var* exception) = 0;
  virtual PP_Var Construct(uint32_t argc,
                           PP_Var* argv,
                           PP_Var* exception) = 0;
  virtual void Deallocate() = 0;

  // For use by derived classes in constructing ObjectProxies.
  static const PPP_Class_Deprecated object_class;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Object);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_H_
