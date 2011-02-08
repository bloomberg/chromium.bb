// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_var_serialization_rules.h"

#include "base/logging.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_var_tracker.h"

namespace pp {
namespace proxy {

PluginVarSerializationRules::PluginVarSerializationRules()
    : var_tracker_(PluginVarTracker::GetInstance()) {
}

PluginVarSerializationRules::~PluginVarSerializationRules() {
}

PP_Var PluginVarSerializationRules::SendCallerOwned(const PP_Var& var,
                                                    std::string* str_val) {
  // Objects need special translations to get the IDs valid in the host.
  if (var.type == PP_VARTYPE_OBJECT)
    return var_tracker_->GetHostObject(var);

  // Nothing to do since we manage the refcount, other than retrieve the string
  // to use for IPC.
  if (var.type == PP_VARTYPE_STRING)
    *str_val = var_tracker_->GetString(var);
  return var;
}

PP_Var PluginVarSerializationRules::BeginReceiveCallerOwned(
    const PP_Var& var,
    const std::string* str_val,
    Dispatcher* dispatcher) {
  if (var.type == PP_VARTYPE_STRING) {
    // Convert the string to the context of the current process.
    PP_Var ret;
    ret.type = PP_VARTYPE_STRING;
    ret.value.as_id = var_tracker_->MakeString(*str_val);
    return ret;
  }

  if (var.type == PP_VARTYPE_OBJECT) {
    DCHECK(dispatcher->IsPlugin());
    return var_tracker_->TrackObjectWithNoReference(
        var, static_cast<PluginDispatcher*>(dispatcher));
  }

  return var;
}

void PluginVarSerializationRules::EndReceiveCallerOwned(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    // Destroy the string BeginReceiveCallerOwned created above.
    var_tracker_->Release(var);
  } else if (var.type == PP_VARTYPE_OBJECT) {
    var_tracker_->StopTrackingObjectWithNoReference(var);
  }
}

PP_Var PluginVarSerializationRules::ReceivePassRef(const PP_Var& var,
                                                   const std::string& str_val,
                                                   Dispatcher* dispatcher) {
  if (var.type == PP_VARTYPE_STRING) {
    // Convert the string to the context of the current process.
    PP_Var ret;
    ret.type = PP_VARTYPE_STRING;
    ret.value.as_id = var_tracker_->MakeString(str_val);
    return ret;
  }

  // Overview of sending an object with "pass ref" from the browser to the
  // plugin:
  //                                  Example 1             Example 2
  //                               Plugin   Browser      Plugin   Browser
  // Before send                      3        2            0        1
  // Browser calls BeginSendPassRef   3        2            0        1
  // Plugin calls ReceivePassRef      4        1            1        1
  // Browser calls EndSendPassRef     4        1            1        1
  //
  // In example 1 before the send, the plugin has 3 refs which are represented
  // as one ref in the browser (since the plugin only tells the browser when
  // it's refcount goes from 1 -> 0). The initial state is that the browser
  // plugin code started to return a value, which means it gets another ref
  // on behalf of the caller. This needs to be transferred to the plugin and
  // folded in to its set of refs it maintains (with one ref representing all
  // fo them in the browser).
  if (var.type == PP_VARTYPE_OBJECT) {
    DCHECK(dispatcher->IsPlugin());
    return var_tracker_->ReceiveObjectPassRef(
        var, static_cast<PluginDispatcher*>(dispatcher));
  }

  // Other types are unchanged.
  return var;
}

PP_Var PluginVarSerializationRules::BeginSendPassRef(const PP_Var& var,
                                                     std::string* str_val) {
  // Overview of sending an object with "pass ref" from the plugin to the
  // browser:
  //                                  Example 1             Example 2
  //                               Plugin   Browser      Plugin   Browser
  // Before send                      3        1            1        1
  // Plugin calls BeginSendPassRef    3        1            1        1
  // Browser calls ReceivePassRef     3        2            1        2
  // Plugin calls EndSendPassRef      2        2            0        1
  //
  // The plugin maintains one ref count in the browser on behalf of the
  // entire ref count in the plugin. When the plugin refcount goes to 0, it
  // will call the browser to deref the object. This is why in example 2
  // transferring the object ref to the browser involves no net change in the
  // browser's refcount.

  // Objects need special translations to get the IDs valid in the host.
  if (var.type == PP_VARTYPE_OBJECT)
    return var_tracker_->GetHostObject(var);

  if (var.type == PP_VARTYPE_STRING)
    *str_val = var_tracker_->GetString(var);
  return var;
}

void PluginVarSerializationRules::EndSendPassRef(const PP_Var& var,
                                                 Dispatcher* dispatcher) {
  // See BeginSendPassRef for an example of why we release our ref here.
  // The var we have in our inner class has been converted to a host object
  // by BeginSendPassRef. This means it's not a normal var valid in the plugin,
  // so we need to use the special ReleaseHostObject.
  if (var.type == PP_VARTYPE_OBJECT) {
    var_tracker_->ReleaseHostObject(
        static_cast<PluginDispatcher*>(dispatcher), var);
  }
}

void PluginVarSerializationRules::ReleaseObjectRef(const PP_Var& var) {
  var_tracker_->Release(var);
}

}  // namespace proxy
}  // namespace pp
