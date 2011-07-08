/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_PROXY_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_PROXY_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/object.h"
#include "native_client/src/shared/ppapi_proxy/object_capability.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_class_deprecated.h"
#include "native_client/src/third_party/ppapi/c/pp_var.h"

struct NaClSrpcChannel;

namespace ppapi_proxy {

// Implements a scriptable object in PPAPI.
// This class is used in both the browser and plugin.
// These methods are in one to one correspondence with the methods exported by
// the interface in ppapi/c/dev/ppp_class_deprecated.h.
class ObjectProxy : public Object {
 public:
  ObjectProxy(const ObjectCapability& capability,
              NaClSrpcChannel* channel,
              bool is_instance_object)
      : capability_(capability),
        channel_(channel),
        is_instance_object_(is_instance_object) {}
  virtual ~ObjectProxy() {}

  // The bindings for the methods invoked by the PPAPI interface.
  virtual bool HasProperty(PP_Var name,
                           PP_Var* exception);
  virtual bool HasMethod(PP_Var name,
                         PP_Var* exception);
  virtual PP_Var GetProperty(PP_Var name,
                             PP_Var* exception);
  virtual void GetAllPropertyNames(uint32_t* property_count,
                                   PP_Var** properties,
                                   PP_Var* exception);
  virtual void SetProperty(PP_Var name,
                           PP_Var value,
                           PP_Var* exception);
  virtual void RemoveProperty(PP_Var name,
                              PP_Var* exception);
  virtual PP_Var Call(PP_Var method_name,
                      uint32_t argc,
                      PP_Var* argv,
                      PP_Var* exception);
  virtual PP_Var Construct(uint32_t argc,
                           PP_Var* argv,
                           PP_Var* exception);
  virtual void Deallocate();

  static PP_Var New(const ObjectCapability& capability,
                    NaClSrpcChannel* channel,
                    bool is_instance_object);

 private:
  ObjectCapability capability_;
  // TODO(sehr): this should be a scoped_refptr.
  NaClSrpcChannel* channel_;
  bool is_instance_object_;
  NACL_DISALLOW_COPY_AND_ASSIGN(ObjectProxy);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_PROXY_H_
