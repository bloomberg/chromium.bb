// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple C++ Pepper plugin for exercising deprecated PPAPI interfaces in
// Blink layout tests.
//
// Most layout tests should prefer to use the normal Blink test plugin, with the
// MIME type application/x-blink-test-plugin. For layout tests that absolutely
// need to test deprecated synchronous scripting interfaces, this plugin can be
// instantiated using the application/x-blink-deprecated-test-plugin MIME type.

#include <stdint.h>

#include <map>
#include <sstream>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/instance_private.h"
#include "ppapi/cpp/private/var_private.h"
#include "ppapi/cpp/var.h"

namespace {

class InstanceSO : public pp::deprecated::ScriptableObject {
 public:
  explicit InstanceSO(pp::InstancePrivate* instance) : instance_(instance) {
    methods_.insert(std::make_pair(
        "testExecuteScript",
        base::Bind(&InstanceSO::TestExecuteScript, base::Unretained(this))));
    methods_.insert(std::make_pair(
        "testGetProperty",
        base::Bind(&InstanceSO::TestGetProperty, base::Unretained(this))));
  }

  // pp::deprecated::ScriptableObject overrides:
  bool HasMethod(const pp::Var& name, pp::Var* exception) {
    return FindMethod(name) != methods_.end();
  }

  pp::Var Call(const pp::Var& method_name,
               const std::vector<pp::Var>& args,
               pp::Var* exception) override {
    auto method = FindMethod(method_name);
    if (method != methods_.end()) {
      return method->second.Run(args, exception);
    }

    return ScriptableObject::Call(method_name, args, exception);
  }

 private:
  using MethodMap =
      std::map<std::string,
               base::Callback<pp::Var(const std::vector<pp::Var>&, pp::Var*)>>;

  MethodMap::iterator FindMethod(const pp::Var& name) {
    if (!name.is_string())
      return methods_.end();
    return methods_.find(name.AsString());
  }

  // Requires one argument. The argument is passed through as-is to
  // pp::InstancePrivate::ExecuteScript().
  pp::Var TestExecuteScript(const std::vector<pp::Var>& args,
                            pp::Var* exception) {
    if (args.size() != 1) {
      *exception = pp::Var("testExecuteScript requires one argument");
      return pp::Var();
    }
    return instance_->ExecuteScript(args[0], exception);
  }

  // Requires one or more arguments. Roughly analogous to NPN_GetProperty.
  // The arguments are the chain of properties to traverse, starting with the
  // global context.
  pp::Var TestGetProperty(const std::vector<pp::Var>& args,
                          pp::Var* exception) {
    if (args.size() < 1) {
      *exception = pp::Var("testGetProperty requires at least one argument");
      return pp::Var();
    }
    pp::VarPrivate object = instance_->GetWindowObject();
    for (const auto& arg : args) {
      if (!object.HasProperty(arg, exception))
        return pp::Var();
      object = object.GetProperty(arg, exception);
    }
    return object;
  }

  pp::InstancePrivate* const instance_;
  MethodMap methods_;
};

class BlinkDeprecatedTestInstance : public pp::InstancePrivate {
 public:
  explicit BlinkDeprecatedTestInstance(PP_Instance instance)
      : pp::InstancePrivate(instance) {}
  ~BlinkDeprecatedTestInstance() override {}

  bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  // pp::InstancePrivate overrides:
  pp::Var GetInstanceObject() override {
    if (instance_var_.is_undefined()) {
      instance_so_ = new InstanceSO(this);
      instance_var_ = pp::VarPrivate(this, instance_so_);
    }
    return instance_var_;
  }

 private:
  pp::VarPrivate instance_var_;
  // Owned by |instance_var_|.
  InstanceSO* instance_so_;
};

class BlinkDeprecatedTestModule : public pp::Module {
 public:
  BlinkDeprecatedTestModule() {}
  ~BlinkDeprecatedTestModule() override {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new BlinkDeprecatedTestInstance(instance);
  }
};

}  // namespace

namespace pp {

Module* CreateModule() {
  return new BlinkDeprecatedTestModule();
}

}  // namespace pp
