// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_var_tracker.h"

#include "base/ref_counted.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

class RefCountedString : public base::RefCounted<RefCountedString> {
 public:
  RefCountedString() {
  }
  RefCountedString(const std::string& str) : value_(str) {
  }
  RefCountedString(const char* data, size_t len)
      : value_(data, len) {
  }

  const std::string& value() const { return value_; }

 private:
  std::string value_;
};

// When running in the plugin, this will convert the string ID to the object
// using casting. No validity checking is done.
RefCountedString* PluginStringFromID(int64 id) {
  return reinterpret_cast<RefCountedString*>(static_cast<intptr_t>(id));
}

}  // namespace

PluginVarTracker::PluginVarTracker(PluginDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      browser_var_interface_(NULL) {
}

void PluginVarTracker::Init() {
  browser_var_interface_ = reinterpret_cast<const PPB_Var*>(
      dispatcher_->GetLocalInterface(PPB_VAR_INTERFACE));
}

int64 PluginVarTracker::MakeString(const std::string& str) {
  RefCountedString* out = new RefCountedString(str);
  out->AddRef();
  return static_cast<int64>(reinterpret_cast<intptr_t>(out));
}

std::string PluginVarTracker::GetString(const PP_Var& var) const {
  return PluginStringFromID(var.value.as_id)->value();
}

const std::string* PluginVarTracker::GetExistingString(
    const PP_Var& var) const {
  if (var.type != PP_VARTYPE_STRING)
    return NULL;
  RefCountedString* str = PluginStringFromID(var.value.as_id);
  return &str->value();
}

void PluginVarTracker::AddRef(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    PluginStringFromID(var.value.as_id)->AddRef();
  } else if (var.type == PP_VARTYPE_OBJECT && var.value.as_id != 0) {
    int& ref_count = object_ref_count_[var.value.as_id];
    ref_count++;
    if (ref_count == 1) {
      // We must handle the case where we got requested to AddRef an object
      // that we've never seen before. This should tell the browser we've
      // taken a ref. This comes up when the browser passes an object as an
      // input param and holds a ref for us. We may not have seen that object
      // and the plugin handler may want to AddRef and release it internally.
      SendAddRefObjectMsg(var.value.as_id);
    }
  }
}

void PluginVarTracker::Release(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    PluginStringFromID(var.value.as_id)->Release();
  } else if (var.type == PP_VARTYPE_OBJECT) {
    ObjectRefCount::iterator found = object_ref_count_.find(var.value.as_id);
    if (found == object_ref_count_.end())
      return;  // Invalid object.
    found->second--;

    if (found->second == 0) {
      // Plugin has released all of its refs, tell the browser.
      object_ref_count_.erase(found);
      SendReleaseObjectMsg(var.value.as_id);
    }
  }
}

void PluginVarTracker::ReceiveObjectPassRef(const PP_Var& var) {
  // We're the plugin and the renderer just sent us a ref. The renderer has
  // addrefed the var in its tracker for us since it's returning it.
  //
  // - If We don't have a reference to this already, then we just add it to
  //   our tracker and use that one ref.
  //
  // - If we do already have a reference to it, that means the renderer now
  //   has two references on our behalf. We want to transfer that extra
  //   reference to our list. This means we addref in the plugin, and release
  //   the extra one in the renderer.
  ObjectRefCount::iterator found = object_ref_count_.find(var.value.as_id);
  if (found == object_ref_count_.end()) {
    object_ref_count_[var.value.as_id] = 1;
  } else {
    SendReleaseObjectMsg(var.value.as_id);
    found->second++;
  }
}

void PluginVarTracker::SendAddRefObjectMsg(int64_t id) {
  dispatcher_->Send(new PpapiHostMsg_PPBVar_AddRefObject(
        INTERFACE_ID_PPB_VAR_DEPRECATED, id));
}

void PluginVarTracker::SendReleaseObjectMsg(int64_t id) {
  dispatcher_->Send(new PpapiHostMsg_PPBVar_ReleaseObject(
        INTERFACE_ID_PPB_VAR_DEPRECATED, id));
}

}  // namesace proxy
}  // namespace pp
