/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_OBJECT_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_OBJECT_H_

#include <map>
#include <string>
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_class.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/object.h"

namespace fake_browser_ppapi {

// Implements a scriptable object in PPAPI.
class Object : public ppapi_proxy::Object {
 public:
  virtual ~Object() {}
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

  typedef std::map<std::string, PP_Var*> PropertyMap;
  typedef PP_Var (*Method)(Object* object,
                           uint32_t argc,
                           PP_Var* argv,
                           PP_Var* exception);
  typedef std::map<std::string, Method> MethodMap;

  // Create a PP_Var object with a set of initial properties.
  static PP_Var New(const PropertyMap& properties, const MethodMap& methods);

  // Construct using a map of initial properties.
  Object(const PropertyMap& properties, const MethodMap& methods);

  PropertyMap* properties() { return &properties_; }
  MethodMap* methods() { return &methods_; }

 private:
  // Maintains the list of properties for Set/Get/Remove.
  PropertyMap properties_;
  // Maintains the list of methods that can be invoked by Call.
  MethodMap methods_;

  NACL_DISALLOW_COPY_AND_ASSIGN(Object);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_OBJECT_H_
