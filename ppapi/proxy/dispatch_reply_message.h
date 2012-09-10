// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides infrastructure for dispatching host resource reply
// messages. Normal IPC Reply handlers can't take extra parameters.
// We want to take a ResourceMessageReplyParams as a parameter.

#ifndef PPAPI_PROXY_DISPATCH_REPLY_MESSAGE_H_
#define PPAPI_PROXY_DISPATCH_REPLY_MESSAGE_H_

#include "base/profiler/scoped_profile.h"  // For TRACK_RUN_IN_IPC_HANDLER.
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace proxy {

struct Context;
class ResourceMessageReplyParams;

template <class ObjT, class Method>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const Tuple0& arg) {
  (obj->*method)(params);
}

template <class ObjT, class Method, class A>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const Tuple1<A>& arg) {
  return (obj->*method)(params, arg.a);
}

template<class ObjT, class Method, class A, class B>
inline int32_t DispatchResourceReply(ObjT* obj, Method method,
                                     const ResourceMessageReplyParams& params,
                                     const Tuple2<A, B>& arg) {
  return (obj->*method)(params, arg.a, arg.b);
}

template<class ObjT, class Method, class A, class B, class C>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const Tuple3<A, B, C>& arg) {
  return (obj->*method)(params, arg.a, arg.b, arg.c);
}

template<class ObjT, class Method, class A, class B, class C, class D>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const Tuple4<A, B, C, D>& arg) {
  return (obj->*method)(params, arg.a, arg.b, arg.c, arg.d);
}

template<class ObjT, class Method, class A, class B, class C, class D, class E>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const Tuple5<A, B, C, D, E>& arg) {
  return (obj->*method)(params, arg.a, arg.b, arg.c, arg.d, arg.e);
}

#define PPAPI_DISPATCH_RESOURCE_REPLY(msg_class, member_func) \
    case msg_class::ID: { \
      TRACK_RUN_IN_IPC_HANDLER(member_func); \
      msg_class::Schema::Param p; \
      if (msg_class::Read(&ipc_message__, &p)) { \
        ppapi::proxy::DispatchResourceReply( \
            this, \
            &_IpcMessageHandlerClass::member_func, \
            params, p); \
      }  \
      break; \
    }

#define PPAPI_DISPATCH_RESOURCE_REPLY_0(msg_class, member_func) \
    case msg_class::ID: { \
      TRACK_RUN_IN_IPC_HANDLER(member_func); \
      member_func(params); \
      break; \
    }

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_DISPATCH_REPLY_MESSAGE_H_
