// Copyright 2010 Google Inc. All Rights Reserved.
// Author: ajwong@google.com (Albert Wong)

#include "remoting/client/plugin/chromoting_scriptable_object.h"

#include "remoting/client/client_config.h"
#include "remoting/client/plugin/chromoting_plugin.h"

#include "third_party/ppapi/cpp/var.h"

using pp::Var;

namespace remoting {
ChromotingScriptableObject::ChromotingScriptableObject(
    ChromotingPlugin* instance)
    : instance_(instance) {
}

ChromotingScriptableObject::~ChromotingScriptableObject() {
}

void ChromotingScriptableObject::Init() {
  // Property addition order should match the interface description at the
  // top of chromoting_scriptable_object.h.
  AddAttribute("onreadystatechange", Var());

  AddAttribute("NOT_CONNECTED", Var(0));
  AddAttribute("CONNECTED", Var(1));

  AddMethod("connect", &ChromotingScriptableObject::DoConnect);

  // TODO(ajwong): Figure out how to implement the status variable.
  AddAttribute("status", Var("not_implemented_yet"));
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
  // Somehow the ARM compiler is thinking that the above is using
  // NULL as an arithmetic expression.
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

  // Only allow writing to onreadystatechange.  See top of
  // chromoting_scriptable_object.h for the object interface definition.
  std::string property_name = name.AsString();
  if (property_name != "onstatechange") {
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
    return pp::ScriptableObject::Call(method_name, args, exception);
  }

  return (this->*(properties_[iter->second].method))(args, exception);
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

pp::Var ChromotingScriptableObject::DoConnect(const std::vector<Var>& args,
                                           Var* exception) {
  if (args.size() != 3) {
    *exception = Var("Usage: connect(username, host_jid, auth_token");
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

  instance_->Connect(config);

  return Var();
}

}  // namespace remoting
