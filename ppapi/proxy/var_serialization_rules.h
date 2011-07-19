// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VAR_SERIALIZATION_RULES_H_
#define PPAPI_PROXY_VAR_SERIALIZATION_RULES_H_

#include "ppapi/c/pp_var.h"

#include <string>

namespace pp {
namespace proxy {

class Dispatcher;

// Encapsulates the rules for serializing and deserializing vars to and from
// the local process. The renderer and the plugin process each have separate
// bookkeeping rules.
class VarSerializationRules {
 public:
  virtual ~VarSerializationRules() {}

  // Caller-owned calls --------------------------------------------------------
  //
  // A caller-owned call is when doing a function call with a "normal" input
  // argument. The caller has a reference to the var, and the caller is
  // responsible for freeing that reference.

  // Prepares the given var for sending to the callee. If the var is a string,
  // the value of that string will be placed in *str_val. If the var is not
  // a string, str_val will be untouched and may be NULL. The return value will
  // be the var valid for the host process.
  virtual PP_Var SendCallerOwned(const PP_Var& var, std::string* str_val) = 0;

  // When receiving a caller-owned variable, normally we don't have to do
  // anything. However, in the case of strings, we need to deserialize the
  // string from IPC, create a new PP_Var string in the local process, call the
  // function, and then destroy the temporary string. These two functions
  // handle that process
  //
  // BeginReceiveCallerOwned takes a var from IPC and an optional pointer to
  // the deserialized string (which will be used only when var is a
  // VARTYPE_STRING and may be NULL otherwise) and returns a new var
  // representing the input in the local process.
  //
  // EndReceiveCallerOwned destroys the string created by Begin* and does
  // nothing otherwise. It should be called with the result of Begin*.
  virtual PP_Var BeginReceiveCallerOwned(const PP_Var& var,
                                         const std::string* str_val,
                                         Dispatcher* dispatcher) = 0;
  virtual void EndReceiveCallerOwned(const PP_Var& var) = 0;

  // Passinag refs -------------------------------------------------------------
  //
  // A pass-ref transfer is when ownership of a reference is passed from
  // onen side to the other. Normally, this happens via return values and
  // output arguments, as for exceptions. The code generating the value
  // (the function returning it in the case of a return value) will AddRef
  // the var on behalf of the consumer of the value. Responsibility for
  // Release is on the consumer (the caller of the function in the case of a
  // return value).

  // Creates a var in the context of the local process from the given
  // deserialized var and deserialized string (which will be used only when var
  // is a VARTYPE_STRING and may be NULL otherwise). The input var/string
  // should be the result of calling SendPassRef in the remote process. The
  // return value is the var valid in the plugin process.
  virtual PP_Var ReceivePassRef(const PP_Var& var,
                                const std::string& str_val,
                                Dispatcher* dispatcher) = 0;

  // Prepares a var to be sent to the remote side. One local reference will
  // be passed to the remote side. Call Begin* before doing the send and End*
  // after doing the send. See SendCallerOwned for a description of the string.
  //
  // The return value from BeginSendPassRef will be the var valid for the host
  // process. This same value must be passed to EndSendPassRef.
  virtual PP_Var BeginSendPassRef(const PP_Var& var, std::string* str_val) = 0;
  virtual void EndSendPassRef(const PP_Var& var, Dispatcher* dispatcher) = 0;

  // ---------------------------------------------------------------------------

  virtual void ReleaseObjectRef(const PP_Var& var) = 0;

 protected:
  VarSerializationRules() {}
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_VAR_SERIALIZATION_RULES_H_
