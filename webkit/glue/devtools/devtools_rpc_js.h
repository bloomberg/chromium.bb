// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Additional set of macros for the JS RPC.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_
#define WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_

#include <string>

// Do not remove this one although it is not used.
#include <wtf/OwnPtr.h>

#include "base/basictypes.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/glue_util.h"

///////////////////////////////////////////////////////
// JS RPC binds and stubs

template<typename T>
struct RpcJsTypeTrait {
};

template<>
struct RpcJsTypeTrait<bool> {
  static std::string ToString(const CppVariant& var) {
    return IntToString(var.ToBoolean() ? 1 : 0);
  }
};

template<>
struct RpcJsTypeTrait<int> {
  static std::string ToString(const CppVariant& var) {
    return IntToString(var.ToInt32());
  }
};

template<>
struct RpcJsTypeTrait<String> {
  static std::string ToString(const CppVariant& var) {
    return var.ToString();
  }
};

template<>
struct RpcJsTypeTrait<std::string> {
  static std::string ToString(const CppVariant& var) {
    return var.ToString();
  }
};

#define TOOLS_RPC_JS_BIND_METHOD0(Method) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD1(Method, T1) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD2(Method, T1, T2) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD3(Method, T1, T2, T3) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_STUB_METHOD0(Method) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    this->delegate_->SendRpcMessage(class_name, #Method); \
    result->SetNull(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD1(Method, T1) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    this->delegate_->SendRpcMessage(class_name, #Method, \
        RpcJsTypeTrait<T1>::ToString(args[0])); \
    result->SetNull(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD2(Method, T1, T2) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    this->delegate_->SendRpcMessage(class_name, #Method, \
        RpcJsTypeTrait<T1>::ToString(args[0]), \
        RpcJsTypeTrait<T2>::ToString(args[1])); \
    result->SetNull(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD3(Method, T1, T2, T3) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    this->delegate_->SendRpcMessage(class_name, #Method, \
        RpcJsTypeTrait<T1>::ToString(args[0]), \
        RpcJsTypeTrait<T2>::ToString(args[1]), \
        RpcJsTypeTrait<T3>::ToString(args[2])); \
    result->SetNull(); \
  }

///////////////////////////////////////////////////////
// JS RPC main obj macro

#define DEFINE_RPC_JS_BOUND_OBJ(Class, STRUCT, DClass, DELEGATE_STRUCT) \
class Js##Class##BoundObj : public Class##Stub, \
                            public CppBoundClass { \
 public: \
  Js##Class##BoundObj(Delegate* rpc_delegate, WebFrame* frame, \
      const std::wstring& classname) \
      : Class##Stub(rpc_delegate) { \
    BindToJavascript(frame, classname); \
    STRUCT( \
        TOOLS_RPC_JS_BIND_METHOD0, \
        TOOLS_RPC_JS_BIND_METHOD1, \
        TOOLS_RPC_JS_BIND_METHOD2, \
        TOOLS_RPC_JS_BIND_METHOD3) \
  } \
  virtual ~Js##Class##BoundObj() {} \
  typedef Js##Class##BoundObj OCLASS; \
  STRUCT( \
      TOOLS_RPC_JS_STUB_METHOD0, \
      TOOLS_RPC_JS_STUB_METHOD1, \
      TOOLS_RPC_JS_STUB_METHOD2, \
      TOOLS_RPC_JS_STUB_METHOD3) \
 private: \
  DISALLOW_COPY_AND_ASSIGN(Js##Class##BoundObj); \
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_
