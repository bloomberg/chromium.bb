// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements the JavaScript class entrypoint for the plugin.
// The Javascript API is defined as follows.
//
// interface ChromotingScriptableObject {
//   // Called when the Chromoting plugin has had a state change such as
//   // connection completed.
//   attribute Function onreadystatechange;
//
//   // Constants for states, etc.
//   const unsigned short NOT_CONNECTED = 0;
//   const unsigned short CONNECTED = 1;
//
//   // Methods on the object.
//   void connect(string username, string host_jid, string auth_token);
//
//   // Attributes.
//   readonly attribute unsigned short status;
// }
//
//  onreadystatechange
//
// Methods:
//   Connect(username, auth_token, host_jid, onstatechange);

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_SCRIPTABLE_OBJECT_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_SCRIPTABLE_OBJECT_H_

#include <map>
#include <string>
#include <vector>

#include "third_party/ppapi/cpp/scriptable_object.h"
#include "third_party/ppapi/cpp/var.h"

namespace remoting {

class ChromotingPlugin;

class ChromotingScriptableObject : public pp::ScriptableObject {
 public:
  explicit ChromotingScriptableObject(ChromotingPlugin* instance);
  virtual ~ChromotingScriptableObject();

  virtual void Init();

  // Override the ScriptableObject functions.
  virtual bool HasProperty(const pp::Var& name, pp::Var* exception);
  virtual bool HasMethod(const pp::Var& name, pp::Var* exception);
  virtual pp::Var GetProperty(const pp::Var& name, pp::Var* exception);
  virtual void GetAllPropertyNames(std::vector<pp::Var>* properties,
                                   pp::Var* exception);
  virtual void SetProperty(const pp::Var& name,
                           const pp::Var& value,
                           pp::Var* exception);
  virtual pp::Var Call(const pp::Var& method_name,
                       const std::vector<pp::Var>& args,
                       pp::Var* exception);

 private:
  typedef std::map<std::string, int> PropertyNameMap;
  typedef pp::Var (ChromotingScriptableObject::*MethodHandler)(
      const std::vector<pp::Var>& args, pp::Var* exception);
  struct PropertyDescriptor {
    explicit PropertyDescriptor(const std::string& n, pp::Var a)
        : name(n), attribute(a), method(NULL) {
    }

    explicit PropertyDescriptor(const std::string& n, MethodHandler m)
        : name(n), method(m) {
    }

    enum Type {
      ATTRIBUTE,
      METHOD,
    } type;

    std::string name;
    pp::Var attribute;
    MethodHandler method;
  };


  void AddAttribute(const std::string& name, pp::Var attribute);
  void AddMethod(const std::string& name, MethodHandler handler);

  pp::Var DoConnect(const std::vector<pp::Var>& args, pp::Var* exception);

  PropertyNameMap property_names_;
  std::vector<PropertyDescriptor> properties_;

  ChromotingPlugin* instance_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_SCRIPTABLE_OBJECT_H_
