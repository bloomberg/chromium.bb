// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_var_serialization_rules.h"

#include "ppapi/proxy/plugin_var_tracker.h"

namespace pp {
namespace proxy {

PluginVarSerializationRules::PluginVarSerializationRules(
    PluginVarTracker* var_tracker)
    : var_tracker_(var_tracker) {
}

PluginVarSerializationRules::~PluginVarSerializationRules() {
}

void PluginVarSerializationRules::SendCallerOwned(const PP_Var& var,
                                                  std::string* str_val) {
  // Nothing to do since we manage the refcount, other than retrieve the string
  // to use for IPC.
  if (var.type == PP_VARTYPE_STRING)
    *str_val = var_tracker_->GetString(var);
}

PP_Var PluginVarSerializationRules::BeginReceiveCallerOwned(
    const PP_Var& var,
    const std::string* str_val) {
  if (var.type == PP_VARTYPE_STRING) {
    // Convert the string to the context of the current process.
    PP_Var ret;
    ret.type = PP_VARTYPE_STRING;
    ret.value.as_id = var_tracker_->MakeString(*str_val);
    return ret;
  }

  return var;
}

void PluginVarSerializationRules::EndReceiveCallerOwned(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    // Destroy the string BeginReceiveCallerOwned created above.
    var_tracker_->Release(var);
  }
}

PP_Var PluginVarSerializationRules::ReceivePassRef(const PP_Var& var,
                                                   const std::string& str_val) {
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
  if (var.type == PP_VARTYPE_OBJECT)
    var_tracker_->ReceiveObjectPassRef(var);
  return var;
}

void PluginVarSerializationRules::BeginSendPassRef(const PP_Var& var,
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

  if (var.type == PP_VARTYPE_STRING)
    *str_val = var_tracker_->GetString(var);
}

void PluginVarSerializationRules::EndSendPassRef(const PP_Var& var) {
  // See BeginSendPassRef for an example of why we release our ref here.
  if (var.type == PP_VARTYPE_OBJECT)
    var_tracker_->Release(var);
}

void PluginVarSerializationRules::ReleaseObjectRef(const PP_Var& var) {
  var_tracker_->Release(var);
}

}  // namespace proxy
}  // namespace pp
