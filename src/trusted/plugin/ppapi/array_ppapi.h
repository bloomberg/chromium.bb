/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_ARRAY_PPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_ARRAY_PPAPI_H_

#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/scriptable_object.h"
#include "ppapi/cpp/var.h"

namespace plugin {

// Wraps a JavaScript array where each element is an indexed property.
// Can be used to represent single arguments and return values of array type
// as well as multiple return values from invoking SRPC methods.
class ArrayPpapi : public pp::ScriptableObject {
 public:
  explicit ArrayPpapi(PluginPpapi* instance);
  virtual ~ArrayPpapi() {}

  virtual bool HasProperty(const pp::Var& name, pp::Var* exception) {
    return js_array_.HasProperty(name, exception);
  }
  virtual pp::Var GetProperty(const pp::Var& name, pp::Var* exception) {
    return js_array_.GetProperty(name, exception);
  }
  virtual void GetAllPropertyNames(std::vector<pp::Var>* properties,
                                   pp::Var* exception) {
    js_array_.GetAllPropertyNames(properties, exception);
  }
  virtual void SetProperty(const pp::Var& name,
                           const pp::Var& value, pp::Var* exception) {
    js_array_.SetProperty(name, value, exception);
  }
  virtual void RemoveProperty(const pp::Var& name, pp::Var* exception) {
    js_array_.RemoveProperty(name, exception);
  }
  virtual pp::Var Call(const pp::Var& method_name,
                       const std::vector<pp::Var>& args, pp::Var* exception) {
    size_t argc = args.size();
    pp::Var* argv = const_cast<pp::Var*>(&args[0]);  // elements are contiguous
    return js_array_.Call(method_name, argc, argv, exception);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ArrayPpapi);

  pp::Var js_array_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_ARRAY_PPAPI_H_
