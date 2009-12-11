// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DevTools RPC subsystem is a simple string serialization-based rpc
// implementation. The client is responsible for defining the Rpc-enabled
// interface in terms of its macros:
//
// #define MYAPI_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3)
//   METHOD0(Method1)
//   METHOD1(Method3, int)
// (snippet above should be multiline macro, add trailing backslashes)
//
// DEFINE_RPC_CLASS(MyApi, MYAPI_STRUCT)
//
// The snippet above will generate three classes: MyApi, MyApiStub and
// MyApiDispatch.
//
// 1. For each method defined in the marco MyApi will have a
// pure virtual function generated, so that MyApi would look like:
//
// class MyApi {
//  private:
//   MyApi() {}
//   ~MyApi() {}
//   virtual void Method1() = 0;
//   virtual void Method2(
//       int param1,
//       const String& param2,
//       const Value& param3) = 0;
//   virtual void Method3(int param1) = 0;
// };
//
// 2. MyApiStub will implement MyApi interface and would serialize all calls
// into the string-based calls of the underlying transport:
//
// DevToolsRpc::Delegate* transport;
// my_api = new MyApiStub(transport);
// my_api->Method1();
// my_api->Method3(2);
//
// 3. MyApiDelegate is capable of dispatching the calls and convert them to the
// calls to the underlying MyApi methods:
//
// MyApi* real_object;
// MyApiDispatch::Dispatch(real_object, raw_string_call_generated_by_stub);
//
// will make corresponding calls to the real object.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_H_
#define WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_H_

#include "PlatformString.h"
#include "Vector.h"

#include <wtf/Noncopyable.h>

#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsMessageData.h"
#include "webkit/glue/glue_util.h"

namespace WebCore {
class String;
}

using WebCore::String;
using WTF::Vector;

///////////////////////////////////////////////////////
// RPC dispatch macro

template<typename T>
struct RpcTypeTrait {
  typedef T ApiType;
};

template<>
struct RpcTypeTrait<bool> {
  typedef bool ApiType;
  static bool Parse(const WebCore::String& t) {
    bool success;
    int i = t.toIntStrict(&success);
    ASSERT(success);
    return i;
  }
  static WebCore::String ToString(bool b) {
    return WebCore::String::number(b ? 1 : 0);
  }
};

template<>
struct RpcTypeTrait<int> {
  typedef int ApiType;
  static int Parse(const WebCore::String& t) {
    bool success;
    int i = t.toIntStrict(&success);
    ASSERT(success);
    return i;
  }
  static WebCore::String ToString(int i) {
    return WebCore::String::number(i);
  }
};

template<>
struct RpcTypeTrait<String> {
  typedef const String& ApiType;
  static String Parse(const WebCore::String& t) {
    return t;
  }
  static WebCore::String ToString(const String& t) {
    return t;
  }
};

///////////////////////////////////////////////////////
// RPC Api method declarations

#define TOOLS_RPC_API_METHOD0(Method) \
  virtual void Method() = 0;

#define TOOLS_RPC_API_METHOD1(Method, T1) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1) = 0;

#define TOOLS_RPC_API_METHOD2(Method, T1, T2) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2) = 0;

#define TOOLS_RPC_API_METHOD3(Method, T1, T2, T3) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3) = 0;

#define TOOLS_RPC_API_METHOD4(Method, T1, T2, T3, T4) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3, \
                      RpcTypeTrait<T4>::ApiType t4) = 0;

#define TOOLS_RPC_API_METHOD5(Method, T1, T2, T3, T4, T5) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3, \
                      RpcTypeTrait<T4>::ApiType t4, \
                      RpcTypeTrait<T5>::ApiType t5) = 0;

///////////////////////////////////////////////////////
// RPC stub method implementations

#define TOOLS_RPC_STUB_METHOD0(Method) \
  virtual void Method() { \
    Vector<String> args; \
    this->SendRpcMessage(class_name, #Method, args); \
  }

#define TOOLS_RPC_STUB_METHOD1(Method, T1) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1) { \
    Vector<String> args(1); \
    args[0] = RpcTypeTrait<T1>::ToString(t1); \
    this->SendRpcMessage(class_name, #Method, args); \
  }

#define TOOLS_RPC_STUB_METHOD2(Method, T1, T2) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2) { \
    Vector<String> args(2); \
    args[0] = RpcTypeTrait<T1>::ToString(t1); \
    args[1] = RpcTypeTrait<T2>::ToString(t2); \
    this->SendRpcMessage(class_name, #Method, args); \
  }

#define TOOLS_RPC_STUB_METHOD3(Method, T1, T2, T3) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3) { \
    Vector<String> args(3); \
    args[0] = RpcTypeTrait<T1>::ToString(t1); \
    args[1] = RpcTypeTrait<T2>::ToString(t2); \
    args[2] = RpcTypeTrait<T3>::ToString(t3); \
    this->SendRpcMessage(class_name, #Method, args); \
  }

#define TOOLS_RPC_STUB_METHOD4(Method, T1, T2, T3, T4) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3, \
                      RpcTypeTrait<T4>::ApiType t4) { \
    Vector<String> args(4); \
    args[0] = RpcTypeTrait<T1>::ToString(t1); \
    args[1] = RpcTypeTrait<T2>::ToString(t2); \
    args[2] = RpcTypeTrait<T3>::ToString(t3); \
    args[3] = RpcTypeTrait<T4>::ToString(t4); \
    this->SendRpcMessage(class_name, #Method, args); \
  }

#define TOOLS_RPC_STUB_METHOD5(Method, T1, T2, T3, T4, T5) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3, \
                      RpcTypeTrait<T4>::ApiType t4, \
                      RpcTypeTrait<T5>::ApiType t5) { \
    Vector<String> args(5); \
    args[0] = RpcTypeTrait<T1>::ToString(t1); \
    args[1] = RpcTypeTrait<T2>::ToString(t2); \
    args[2] = RpcTypeTrait<T3>::ToString(t3); \
    args[3] = RpcTypeTrait<T4>::ToString(t4); \
    args[4] = RpcTypeTrait<T5>::ToString(t5); \
    this->SendRpcMessage(class_name, #Method, args); \
  }

///////////////////////////////////////////////////////
// RPC dispatch method implementations

#define TOOLS_RPC_DISPATCH0(Method) \
if (method_name == #Method) { \
  delegate->Method(); \
  return true; \
}

#define TOOLS_RPC_DISPATCH1(Method, T1) \
if (method_name == #Method) { \
  delegate->Method(RpcTypeTrait<T1>::Parse(args[0])); \
  return true; \
}

#define TOOLS_RPC_DISPATCH2(Method, T1, T2) \
if (method_name == #Method) { \
  delegate->Method( \
      RpcTypeTrait<T1>::Parse(args[0]), \
      RpcTypeTrait<T2>::Parse(args[1]) \
  ); \
  return true; \
}

#define TOOLS_RPC_DISPATCH3(Method, T1, T2, T3) \
if (method_name == #Method) { \
  delegate->Method( \
      RpcTypeTrait<T1>::Parse(args[0]), \
      RpcTypeTrait<T2>::Parse(args[1]), \
      RpcTypeTrait<T3>::Parse(args[2]) \
  ); \
  return true; \
}

#define TOOLS_RPC_DISPATCH4(Method, T1, T2, T3, T4) \
if (method_name == #Method) { \
  delegate->Method( \
      RpcTypeTrait<T1>::Parse(args[0]), \
      RpcTypeTrait<T2>::Parse(args[1]), \
      RpcTypeTrait<T3>::Parse(args[2]), \
      RpcTypeTrait<T4>::Parse(args[3]) \
  ); \
  return true; \
}

#define TOOLS_RPC_DISPATCH5(Method, T1, T2, T3, T4, T5) \
if (method_name == #Method) { \
  delegate->Method( \
      RpcTypeTrait<T1>::Parse(args[0]), \
      RpcTypeTrait<T2>::Parse(args[1]), \
      RpcTypeTrait<T3>::Parse(args[2]), \
      RpcTypeTrait<T4>::Parse(args[3]), \
      RpcTypeTrait<T5>::Parse(args[4]) \
  ); \
  return true; \
}

#define TOOLS_END_RPC_DISPATCH() \
}

// This macro defines three classes: Class with the Api, ClassStub that is
// serializing method calls and ClassDispatch that is capable of dispatching
// the serialized message into its delegate.
#define DEFINE_RPC_CLASS(Class, STRUCT) \
class Class : public Noncopyable {\
 public: \
  Class() { \
    class_name = #Class; \
  } \
  ~Class() {} \
  \
  STRUCT( \
      TOOLS_RPC_API_METHOD0, \
      TOOLS_RPC_API_METHOD1, \
      TOOLS_RPC_API_METHOD2, \
      TOOLS_RPC_API_METHOD3, \
      TOOLS_RPC_API_METHOD4, \
      TOOLS_RPC_API_METHOD5) \
  WebCore::String class_name; \
}; \
\
class Class##Stub \
    : public Class, \
      public DevToolsRpc { \
 public: \
  explicit Class##Stub(Delegate* delegate) : DevToolsRpc(delegate) {} \
  virtual ~Class##Stub() {} \
  typedef Class CLASS; \
  STRUCT( \
      TOOLS_RPC_STUB_METHOD0, \
      TOOLS_RPC_STUB_METHOD1, \
      TOOLS_RPC_STUB_METHOD2, \
      TOOLS_RPC_STUB_METHOD3, \
      TOOLS_RPC_STUB_METHOD4, \
      TOOLS_RPC_STUB_METHOD5) \
}; \
\
class Class##Dispatch : public Noncopyable { \
 public: \
  Class##Dispatch() {} \
  virtual ~Class##Dispatch() {} \
  \
  static bool Dispatch(Class* delegate, \
                       const WebKit::WebDevToolsMessageData& data) { \
    String class_name = webkit_glue::WebStringToString(data.className); \
    if (class_name != #Class) { \
      return false; \
    } \
    String method_name = webkit_glue::WebStringToString(data.methodName); \
    Vector<String> args; \
    for (size_t i = 0; i < data.arguments.size(); i++) { \
      args.append(webkit_glue::WebStringToString(data.arguments[i])); \
    } \
    typedef Class CLASS; \
    STRUCT( \
        TOOLS_RPC_DISPATCH0, \
        TOOLS_RPC_DISPATCH1, \
        TOOLS_RPC_DISPATCH2, \
        TOOLS_RPC_DISPATCH3, \
        TOOLS_RPC_DISPATCH4, \
        TOOLS_RPC_DISPATCH5) \
    return false; \
  } \
};

///////////////////////////////////////////////////////
// RPC base class
class DevToolsRpc {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    virtual void SendRpcMessage(
        const WebKit::WebDevToolsMessageData& data) = 0;
  };

  explicit DevToolsRpc(Delegate* delegate)
      : delegate_(delegate) {}
  virtual ~DevToolsRpc() {};

 protected:
  void SendRpcMessage(const String& class_name,
                      const String& method_name,
                      const Vector<String>& args) {
    WebKit::WebVector<WebKit::WebString> web_args(args.size());
    for (size_t i = 0; i < args.size(); i++) {
      web_args[i] = webkit_glue::StringToWebString(args[i]);
    }
    WebKit::WebDevToolsMessageData data;
    data.className = webkit_glue::StringToWebString(class_name);
    data.methodName = webkit_glue::StringToWebString(method_name);
    data.arguments.swap(web_args);
    this->delegate_->SendRpcMessage(data);
  }

  Delegate* delegate_;
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_H_
