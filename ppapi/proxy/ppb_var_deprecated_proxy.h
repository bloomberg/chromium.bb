// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_VAR_PROXY_H_
#define PPAPI_PPB_VAR_PROXY_H_

#include <vector>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Var_Deprecated;

namespace pp {
namespace proxy {

class SerializedVar;
class SerializedVarReceiveInput;
class SerializedVarVectorOutParam;
class SerializedVarVectorReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPB_Var_Deprecated_Proxy : public InterfaceProxy {
 public:
  PPB_Var_Deprecated_Proxy(Dispatcher* dispatcher,
                           const void* target_interface);
  virtual ~PPB_Var_Deprecated_Proxy();

  const PPB_Var_Deprecated* ppb_var_target() const {
    return reinterpret_cast<const PPB_Var_Deprecated*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual void OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgHasProperty(SerializedVarReceiveInput var,
                        SerializedVarReceiveInput name,
                        SerializedVarOutParam exception,
                        bool* result);
  void OnMsgHasMethodDeprecated(SerializedVarReceiveInput var,
                                SerializedVarReceiveInput name,
                                SerializedVarOutParam exception,
                                bool* result);
  void OnMsgGetProperty(SerializedVarReceiveInput var,
                        SerializedVarReceiveInput name,
                        SerializedVarOutParam exception,
                        SerializedVarReturnValue result);
  void OnMsgEnumerateProperties(
      SerializedVarReceiveInput var,
      SerializedVarVectorOutParam props,
      SerializedVarOutParam exception);
  void OnMsgSetPropertyDeprecated(SerializedVarReceiveInput var,
                                  SerializedVarReceiveInput name,
                                  SerializedVarReceiveInput value,
                                  SerializedVarOutParam exception);
  void OnMsgDeleteProperty(SerializedVarReceiveInput var,
                           SerializedVarReceiveInput name,
                           SerializedVarOutParam exception,
                           bool* result);
  void OnMsgCall(SerializedVarReceiveInput object,
                 SerializedVarReceiveInput this_object,
                 SerializedVarReceiveInput method_name,
                 SerializedVarVectorReceiveInput arg_vector,
                 SerializedVarOutParam exception,
                 SerializedVarReturnValue result);
  void OnMsgCallDeprecated(SerializedVarReceiveInput object,
                           SerializedVarReceiveInput method_name,
                           SerializedVarVectorReceiveInput arg_vector,
                           SerializedVarOutParam exception,
                           SerializedVarReturnValue result);
  void OnMsgConstruct(SerializedVarReceiveInput var,
                      SerializedVarVectorReceiveInput arg_vector,
                      SerializedVarOutParam exception,
                      SerializedVarReturnValue result);
  void OnMsgIsInstanceOfDeprecated(pp::proxy::SerializedVarReceiveInput var,
                                   int64 ppp_class,
                                   int64* ppp_class_data,
                                   bool* result);
  void OnMsgCreateObjectDeprecated(PP_Module module_id,
                                   int64 ppp_class,
                                   int64 ppp_class_data,
                                   SerializedVarReturnValue result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_VAR_PROXY_H_
