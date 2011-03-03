// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_scriptable_object.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "ppapi/cpp/var.h"
#include "remoting/client/client_config.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_xmpp_proxy.h"

using pp::Var;

namespace remoting {

const char kConnectionInfoUpdate[] = "connectionInfoUpdate";
const char kDebugInfo[] = "debugInfo";
const char kDesktopHeight[] = "desktopHeight";
const char kDesktopWidth[] = "desktopWidth";
const char kDesktopSizeUpdate[] = "desktopSizeUpdate";
const char kLoginChallenge[] = "loginChallenge";
const char kQualityAttribute[] = "quality";
const char kStatusAttribute[] = "status";

ChromotingScriptableObject::ChromotingScriptableObject(
    ChromotingInstance* instance)
    : instance_(instance) {
}

ChromotingScriptableObject::~ChromotingScriptableObject() {
}

void ChromotingScriptableObject::Init() {
  // Property addition order should match the interface description at the
  // top of chromoting_scriptable_object.h.

  // Connection status.
  AddAttribute(kStatusAttribute, Var(STATUS_UNKNOWN));

  // Connection status values.
  AddAttribute("STATUS_UNKNOWN", Var(STATUS_UNKNOWN));
  AddAttribute("STATUS_CONNECTING", Var(STATUS_CONNECTING));
  AddAttribute("STATUS_INITIALIZING", Var(STATUS_INITIALIZING));
  AddAttribute("STATUS_CONNECTED", Var(STATUS_CONNECTED));
  AddAttribute("STATUS_CLOSED", Var(STATUS_CLOSED));
  AddAttribute("STATUS_FAILED", Var(STATUS_FAILED));

  // Connection quality.
  AddAttribute(kQualityAttribute, Var(QUALITY_UNKNOWN));

  // Connection quality values.
  AddAttribute("QUALITY_UNKNOWN", Var(QUALITY_UNKNOWN));
  AddAttribute("QUALITY_GOOD", Var(QUALITY_GOOD));
  AddAttribute("QUALITY_BAD", Var(QUALITY_BAD));

  // Debug info to display.
  AddAttribute(kConnectionInfoUpdate, Var());
  AddAttribute(kDebugInfo, Var());
  AddAttribute(kDesktopSizeUpdate, Var());
  AddAttribute(kLoginChallenge, Var());
  AddAttribute(kDesktopWidth, Var(0));
  AddAttribute(kDesktopHeight, Var(0));

  AddMethod("connect", &ChromotingScriptableObject::DoConnect);
  AddMethod("disconnect", &ChromotingScriptableObject::DoDisconnect);
  AddMethod("submitLoginInfo", &ChromotingScriptableObject::DoSubmitLogin);
}

bool ChromotingScriptableObject::HasProperty(const Var& name, Var* exception) {
  // TODO(ajwong): Check if all these name.is_string() sentinels are required.
  if (!name.is_string()) {
    *exception = Var("HasProperty expects a string for the name.");
    return false;
  }

  PropertyNameMap::const_iterator iter = property_names_.find(name.AsString());
  if (iter == property_names_.end()) {
    return false;
  }

  // TODO(ajwong): Investigate why ARM build breaks if you do:
  //    properties_[iter->second].method == NULL;
  // Somehow the ARM compiler is thinking that the above is using NULL as an
  // arithmetic expression.
  return properties_[iter->second].method == 0;
}

bool ChromotingScriptableObject::HasMethod(const Var& name, Var* exception) {
  // TODO(ajwong): Check if all these name.is_string() sentinels are required.
  if (!name.is_string()) {
    *exception = Var("HasMethod expects a string for the name.");
    return false;
  }

  PropertyNameMap::const_iterator iter = property_names_.find(name.AsString());
  if (iter == property_names_.end()) {
    return false;
  }

  // See comment from HasProperty about why to use 0 instead of NULL here.
  return properties_[iter->second].method != 0;
}

Var ChromotingScriptableObject::GetProperty(const Var& name, Var* exception) {
  // TODO(ajwong): Check if all these name.is_string() sentinels are required.
  if (!name.is_string()) {
    *exception = Var("GetProperty expects a string for the name.");
    return Var();
  }

  PropertyNameMap::const_iterator iter = property_names_.find(name.AsString());

  // No property found.
  if (iter == property_names_.end()) {
    return ScriptableObject::GetProperty(name, exception);
  }

  // TODO(ajwong): This incorrectly return a null object if a function
  // property is requested.
  return properties_[iter->second].attribute;
}

void ChromotingScriptableObject::GetAllPropertyNames(
    std::vector<Var>* properties,
    Var* exception) {
  for (size_t i = 0; i < properties_.size(); i++) {
    properties->push_back(Var(properties_[i].name));
  }
}

void ChromotingScriptableObject::SetProperty(const Var& name,
                                             const Var& value,
                                             Var* exception) {
  // TODO(ajwong): Check if all these name.is_string() sentinels are required.
  if (!name.is_string()) {
    *exception = Var("SetProperty expects a string for the name.");
    return;
  }

  // Allow writing to connectionInfoUpdate or loginChallenge.  See top of
  // chromoting_scriptable_object.h for the object interface definition.
  std::string property_name = name.AsString();
  if (property_name != kConnectionInfoUpdate &&
      property_name != kDebugInfo &&
      property_name != kDesktopSizeUpdate &&
      property_name != kLoginChallenge &&
      property_name != kDesktopWidth &&
      property_name != kDesktopHeight) {
    *exception =
        Var("Cannot set property " + property_name + " on this object.");
    return;
  }

  // Since we're whitelisting the propertie that are settable above, we can
  // assume that the property exists in the map.
  properties_[property_names_[property_name]].attribute = value;
}

Var ChromotingScriptableObject::Call(const Var& method_name,
                                     const std::vector<Var>& args,
                                     Var* exception) {
  PropertyNameMap::const_iterator iter =
      property_names_.find(method_name.AsString());
  if (iter == property_names_.end()) {
    return pp::deprecated::ScriptableObject::Call(method_name, args, exception);
  }

  return (this->*(properties_[iter->second].method))(args, exception);
}

void ChromotingScriptableObject::SetConnectionInfo(ConnectionStatus status,
                                                   ConnectionQuality quality) {
  int status_index = property_names_[kStatusAttribute];
  int quality_index = property_names_[kQualityAttribute];

  LogDebugInfo(
      base::StringPrintf("Connection status is updated: %d.", status));

  if (properties_[status_index].attribute.AsInt() != status ||
      properties_[quality_index].attribute.AsInt() != quality) {
    // Update the connection state properties..
    properties_[status_index].attribute = Var(status);
    properties_[quality_index].attribute = Var(quality);

    // Signal the Chromoting Tab UI to get the update connection state values.
    SignalConnectionInfoChange();
  }
}

void ChromotingScriptableObject::LogDebugInfo(const std::string& info) {
  Var exception;
  Var cb = GetProperty(Var(kDebugInfo), &exception);

  // Var() means call the object directly as a function rather than calling
  // a method in the object.
  cb.Call(Var(), Var(info), &exception);

  if (!exception.is_undefined()) {
    LOG(WARNING) << "Exception when invoking debugInfo JS callback: "
                 << exception.DebugString();
  }
}

void ChromotingScriptableObject::SetDesktopSize(int width, int height) {
  int width_index = property_names_[kDesktopWidth];
  int height_index = property_names_[kDesktopHeight];

  if (properties_[width_index].attribute.AsInt() != width ||
      properties_[height_index].attribute.AsInt() != height) {
    properties_[width_index].attribute = Var(width);
    properties_[height_index].attribute = Var(height);
    SignalDesktopSizeChange();
  }

  LogDebugInfo(base::StringPrintf("Update desktop size to: %d x %d.",
                                  width, height));
}

void ChromotingScriptableObject::AddAttribute(const std::string& name,
                                              Var attribute) {
  property_names_[name] = properties_.size();
  properties_.push_back(PropertyDescriptor(name, attribute));
}

void ChromotingScriptableObject::AddMethod(const std::string& name,
                                           MethodHandler handler) {
  property_names_[name] = properties_.size();
  properties_.push_back(PropertyDescriptor(name, handler));
}

void ChromotingScriptableObject::SignalConnectionInfoChange() {
  Var exception;
  Var cb = GetProperty(Var(kConnectionInfoUpdate), &exception);

  // Var() means call the object directly as a function rather than calling
  // a method in the object.
  cb.Call(Var(), 0, NULL, &exception);

  if (!exception.is_undefined())
    LogDebugInfo(
        "Exception when invoking connectionInfoUpdate JS callback.");
}

void ChromotingScriptableObject::SignalDesktopSizeChange() {
  Var exception;

  // The JavaScript callback function is the 'callback' property on the
  // 'desktopSizeUpdate' object.
  Var cb = GetProperty(Var(kDesktopSizeUpdate), &exception);

  // Var() means call the object directly as a function rather than calling
  // a method in the object.
  cb.Call(Var(), 0, NULL, &exception);

  if (!exception.is_undefined()) {
    LOG(WARNING) << "Exception when invoking JS callback"
                 << exception.AsString();
  }
}

void ChromotingScriptableObject::SignalLoginChallenge() {
  Var exception;
  Var cb = GetProperty(Var(kLoginChallenge), &exception);

  // Var() means call the object directly as a function rather than calling
  // a method in the object.
  cb.Call(Var(), 0, NULL, &exception);

  if (!exception.is_undefined())
    LogDebugInfo("Exception when invoking loginChallenge JS callback.");
}

void ChromotingScriptableObject::AttachXmppProxy(XmppProxy* xmpp_proxy) {
  xmpp_proxy_ = xmpp_proxy;
}

void ChromotingScriptableObject::SendIq(const std::string& iq_request_xml) {
  // TODO(ajwong): Do something smart here.
  NOTIMPLEMENTED();
}

void ChromotingScriptableObject::ReceiveIq(const std::string& iq_response_xml) {
  xmpp_proxy_->ReceiveIq(iq_response_xml);
}

Var ChromotingScriptableObject::DoConnect(const std::vector<Var>& args,
                                          Var* exception) {
  if (args.size() != 3) {
    *exception = Var("Usage: connect(username, host_jid, auth_token)");
    return Var();
  }

  ClientConfig config;

  if (!args[0].is_string()) {
    *exception = Var("The username must be a string.");
    return Var();
  }
  config.username = args[0].AsString();

  if (!args[1].is_string()) {
    *exception = Var("The host_jid must be a string.");
    return Var();
  }
  config.host_jid = args[1].AsString();

  if (!args[2].is_string()) {
    *exception = Var("The auth_token must be a string.");
    return Var();
  }
  config.auth_token = args[2].AsString();

  LogDebugInfo("Connecting to host.");
  instance_->Connect(config);

  return Var();
}

Var ChromotingScriptableObject::DoDisconnect(const std::vector<Var>& args,
                                             Var* exception) {
  LogDebugInfo("Disconnecting from host.");

  instance_->Disconnect();
  return Var();
}

Var ChromotingScriptableObject::DoSubmitLogin(const std::vector<Var>& args,
                                              Var* exception) {
  if (args.size() != 2) {
    *exception = Var("Usage: login(username, password)");
    return Var();
  }

  if (!args[0].is_string()) {
    *exception = Var("Username must be a string.");
    return Var();
  }
  std::string username = args[0].AsString();

  if (!args[1].is_string()) {
    *exception = Var("Password must be a string.");
    return Var();
  }
  std::string password = args[1].AsString();

  LogDebugInfo("Submitting login info to host.");
  instance_->SubmitLoginInfo(username, password);
  return Var();
}

}  // namespace remoting
