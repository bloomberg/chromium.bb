// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Additional set of macros for the JS RPC.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_
#define WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_

// Do not remove this one although it is not used.
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "webkit/glue/devtools/bound_object.h"
#include "webkit/glue/devtools/devtools_rpc.h"

///////////////////////////////////////////////////////
// JS RPC binds and stubs

#define TOOLS_RPC_JS_BIND_METHOD0(Method) \
  bound_obj.AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD1(Method, T1) \
  bound_obj.AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD2(Method, T1, T2) \
  bound_obj.AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD3(Method, T1, T2, T3) \
  bound_obj.AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD4(Method, T1, T2, T3, T4) \
  bound_obj.AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD5(Method, T1, T2, T3, T4, T5) \
  bound_obj.AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_STUB_METHOD0(Method) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessageFromJs(#Method, args, 0); \
    return v8::Undefined(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD1(Method, T1) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessageFromJs(#Method, args, 1); \
    return v8::Undefined(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD2(Method, T1, T2) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessageFromJs(#Method, args, 2); \
    return v8::Undefined(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD3(Method, T1, T2, T3) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessageFromJs(#Method, args, 3); \
    return v8::Undefined(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD4(Method, T1, T2, T3, T4) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessageFromJs(#Method, args, 4); \
    return v8::Undefined(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD5(Method, T1, T2, T3, T4, T5) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessageFromJs(#Method, args, 5); \
    return v8::Undefined(); \
  }

///////////////////////////////////////////////////////
// JS RPC main obj macro

#define DEFINE_RPC_JS_BOUND_OBJ(Class, STRUCT, DClass, DELEGATE_STRUCT) \
class Js##Class##BoundObj : public Class##Stub { \
 public: \
  Js##Class##BoundObj(Delegate* rpc_delegate, \
                      v8::Handle<v8::Context> context, \
                      const char* classname) \
      : Class##Stub(rpc_delegate) { \
    BoundObject bound_obj(context, this, classname); \
    STRUCT( \
        TOOLS_RPC_JS_BIND_METHOD0, \
        TOOLS_RPC_JS_BIND_METHOD1, \
        TOOLS_RPC_JS_BIND_METHOD2, \
        TOOLS_RPC_JS_BIND_METHOD3, \
        TOOLS_RPC_JS_BIND_METHOD4, \
        TOOLS_RPC_JS_BIND_METHOD5) \
    bound_obj.Build(); \
  } \
  virtual ~Js##Class##BoundObj() {} \
  typedef Js##Class##BoundObj OCLASS; \
  STRUCT( \
      TOOLS_RPC_JS_STUB_METHOD0, \
      TOOLS_RPC_JS_STUB_METHOD1, \
      TOOLS_RPC_JS_STUB_METHOD2, \
      TOOLS_RPC_JS_STUB_METHOD3, \
      TOOLS_RPC_JS_STUB_METHOD4, \
      TOOLS_RPC_JS_STUB_METHOD5) \
 private: \
  static void SendRpcMessageFromJs(const char* method, \
                                   const v8::Arguments& js_arguments, \
                                   size_t args_count) { \
    Vector<String> args(args_count); \
    for (size_t i = 0; i < args_count; i++) { \
      args[i] = WebCore::toWebCoreStringWithNullCheck(js_arguments[i]); \
    } \
    void* self_ptr = v8::External::Cast(*js_arguments.Data())->Value(); \
    Js##Class##BoundObj* self = static_cast<Js##Class##BoundObj*>(self_ptr); \
    self->SendRpcMessage(#Class, method, args); \
  } \
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_
