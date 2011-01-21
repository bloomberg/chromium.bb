// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_VAR_SERIALIZATION_RULES_H_
#define PPAPI_PROXY_PLUGIN_VAR_SERIALIZATION_RULES_H_

#include "base/basictypes.h"
#include "ppapi/proxy/var_serialization_rules.h"

namespace pp {
namespace proxy {

class PluginVarTracker;

// Implementation of the VarSerialization interface for the plugin.
class PluginVarSerializationRules : public VarSerializationRules {
 public:
  // This class will use the given non-owning pointer to the var tracker to
  // handle object refcounting and string conversion.
  PluginVarSerializationRules();
  ~PluginVarSerializationRules();

  // VarSerialization implementation.
  virtual PP_Var SendCallerOwned(const PP_Var& var, std::string* str_val);
  virtual PP_Var BeginReceiveCallerOwned(const PP_Var& var,
                                         const std::string* str_val,
                                         Dispatcher* dispatcher);
  virtual void EndReceiveCallerOwned(const PP_Var& var);
  virtual PP_Var ReceivePassRef(const PP_Var& var,
                                const std::string& str_val,
                                Dispatcher* dispatcher);
  virtual PP_Var BeginSendPassRef(const PP_Var& var, std::string* str_val);
  virtual void EndSendPassRef(const PP_Var& var);
  virtual void ReleaseObjectRef(const PP_Var& var);

 private:
  PluginVarTracker* var_tracker_;

  DISALLOW_COPY_AND_ASSIGN(PluginVarSerializationRules);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_VAR_SERIALIZATION_RULES_H_
