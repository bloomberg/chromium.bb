// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PROXY_OBJECT_VAR_H_
#define PPAPI_PROXY_PROXY_OBJECT_VAR_H_

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

namespace proxy {
class PluginDispatcher;
}  // namespace proxy

// Tracks a reference to an object var in the plugin side of the proxy. This
// just stores the dispatcher and host var ID, and provides the interface for
// integrating this with PP_Var creation.
class ProxyObjectVar : public Var {
 public:
  ProxyObjectVar(proxy::PluginDispatcher* dispatcher,
                 int32 host_var_id);

  virtual ~ProxyObjectVar();

  // Var overrides.
  virtual ProxyObjectVar* AsProxyObjectVar() OVERRIDE;
  virtual PP_Var GetPPVar() OVERRIDE;
  virtual PP_VarType GetType() const OVERRIDE;

  proxy::PluginDispatcher* dispatcher() const { return dispatcher_; }
  int32 host_var_id() const { return host_var_id_; }

  // Expose AssignVarID on Var so the PluginResourceTracker can call us when
  // it's creating IDs.
  void AssignVarID(int32 id);

 private:
  proxy::PluginDispatcher* dispatcher_;
  int32 host_var_id_;

  DISALLOW_COPY_AND_ASSIGN(ProxyObjectVar);
};

}  // namespace ppapi

#endif  // PPAPI_PROXY_PROXY_OBJECT_VAR_H_
