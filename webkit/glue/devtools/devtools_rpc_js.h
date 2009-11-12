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
#include "webkit/glue/glue_util.h"

///////////////////////////////////////////////////////
// JS RPC binds and stubs

#define TOOLS_RPC_JS_BIND_METHOD0(Method) \
  bound_obj_->AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD1(Method, T1) \
  bound_obj_->AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD2(Method, T1, T2) \
  bound_obj_->AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD3(Method, T1, T2, T3) \
  bound_obj_->AddProtoFunction(#Method, OCLASS::Js##Method);

#define TOOLS_RPC_JS_STUB_METHOD0(Method) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessage(v8::External::Cast(*args.Data())->Value(), \
                   #Method, "", "", ""); \
    return v8::Undefined(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD1(Method, T1) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessage(v8::External::Cast(*args.Data())->Value(), \
                   #Method, \
                   WebCore::toWebCoreStringWithNullCheck(args[0]), "", ""); \
    return v8::Undefined(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD2(Method, T1, T2) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessage(v8::External::Cast(*args.Data())->Value(), \
                   #Method, \
                   WebCore::toWebCoreStringWithNullCheck(args[0]), \
                   WebCore::toWebCoreStringWithNullCheck(args[1]), \
                   ""); \
    return v8::Undefined(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD3(Method, T1, T2, T3) \
  static v8::Handle<v8::Value> Js##Method(const v8::Arguments& args) { \
    SendRpcMessage(v8::External::Cast(*args.Data())->Value(), \
                   #Method, \
                   WebCore::toWebCoreStringWithNullCheck(args[0]), \
                   WebCore::toWebCoreStringWithNullCheck(args[1]), \
                   WebCore::toWebCoreStringWithNullCheck(args[2])); \
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
    bound_obj_.set(new BoundObject(context, this, classname)); \
    STRUCT( \
        TOOLS_RPC_JS_BIND_METHOD0, \
        TOOLS_RPC_JS_BIND_METHOD1, \
        TOOLS_RPC_JS_BIND_METHOD2, \
        TOOLS_RPC_JS_BIND_METHOD3) \
    bound_obj_->Build(); \
  } \
  virtual ~Js##Class##BoundObj() {} \
  typedef Js##Class##BoundObj OCLASS; \
  STRUCT( \
      TOOLS_RPC_JS_STUB_METHOD0, \
      TOOLS_RPC_JS_STUB_METHOD1, \
      TOOLS_RPC_JS_STUB_METHOD2, \
      TOOLS_RPC_JS_STUB_METHOD3) \
 private: \
  static void SendRpcMessage(void* self_ptr, \
                             const char* method, \
                             const String& param1, \
                             const String& param2, \
                             const String& param3) { \
    Js##Class##BoundObj* self = static_cast<Js##Class##BoundObj*>(self_ptr); \
    self->delegate_->SendRpcMessage( \
        #Class, \
        method, \
        param1, \
        param2, \
        param3); \
  } \
  OwnPtr<BoundObject> bound_obj_; \
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_
