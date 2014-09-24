// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/message_handler.h"

#include "ipc/ipc_message.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_message_loop_proxy.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace proxy {
namespace {

typedef void (*HandleMessageFunc)(PP_Instance, void*, const PP_Var*);
typedef void (*HandleBlockingMessageFunc)(
    PP_Instance, void*, const PP_Var*, PP_Var*);
typedef void (*HandleMessageFunc_0_1)(PP_Instance, void*, PP_Var);
typedef PP_Var (*HandleBlockingMessageFunc_0_1)(PP_Instance, void*, PP_Var);

void HandleMessageWrapper(HandleMessageFunc function,
                          PP_Instance instance,
                          void* user_data,
                          ScopedPPVar message_data) {
  CallWhileUnlocked(function, instance, user_data,
                    &message_data.get());
}

void HandleBlockingMessageWrapper(HandleBlockingMessageFunc function,
                                  PP_Instance instance,
                                  void* user_data,
                                  ScopedPPVar message_data,
                                  scoped_ptr<IPC::Message> reply_msg) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  PP_Var result = PP_MakeUndefined();
  MessageLoopResource::GetCurrent()->
      set_currently_handling_blocking_message(true);
  CallWhileUnlocked(
      function, instance, user_data, &message_data.get(), &result);
  MessageLoopResource::GetCurrent()->
      set_currently_handling_blocking_message(false);
  PpapiMsg_PPPMessageHandler_HandleBlockingMessage::WriteReplyParams(
      reply_msg.get(),
      SerializedVarReturnValue::Convert(dispatcher, result),
      true /* was_handled */);
  dispatcher->Send(reply_msg.release());
}

// TODO(dmichael): Remove the 0_1 verions; crbug.com/414398
void HandleMessageWrapper_0_1(HandleMessageFunc_0_1 function,
                              PP_Instance instance,
                              void* user_data,
                              ScopedPPVar message_data) {
  CallWhileUnlocked(function, instance, user_data, message_data.get());
}

void HandleBlockingMessageWrapper_0_1(HandleBlockingMessageFunc_0_1 function,
                                      PP_Instance instance,
                                      void* user_data,
                                      ScopedPPVar message_data,
                                      scoped_ptr<IPC::Message> reply_msg) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  MessageLoopResource::GetCurrent()->
      set_currently_handling_blocking_message(true);
  PP_Var return_value = CallWhileUnlocked(function,
                                          instance,
                                          user_data,
                                          message_data.get());
  MessageLoopResource::GetCurrent()->
      set_currently_handling_blocking_message(false);
  PpapiMsg_PPPMessageHandler_HandleBlockingMessage::WriteReplyParams(
      reply_msg.get(),
      SerializedVarReturnValue::Convert(dispatcher, return_value),
      true /* was_handled */);
  dispatcher->Send(reply_msg.release());
}

}  // namespace

// static
scoped_ptr<MessageHandler> MessageHandler::Create(
      PP_Instance instance,
      const PPP_MessageHandler_0_2* handler_if,
      void* user_data,
      PP_Resource message_loop,
      int32_t* error) {
  scoped_ptr<MessageHandler> result;
  // The interface and all function pointers must be valid.
  if (!handler_if ||
      !handler_if->HandleMessage ||
      !handler_if->HandleBlockingMessage ||
      !handler_if->Destroy) {
    *error = PP_ERROR_BADARGUMENT;
    return result.Pass();
  }
  thunk::EnterResourceNoLock<thunk::PPB_MessageLoop_API>
      enter_loop(message_loop, true);
  if (enter_loop.failed()) {
    *error = PP_ERROR_BADRESOURCE;
    return result.Pass();
  }
  scoped_refptr<MessageLoopResource> message_loop_resource(
      static_cast<MessageLoopResource*>(enter_loop.object()));
  if (message_loop_resource->is_main_thread_loop()) {
    *error = PP_ERROR_WRONG_THREAD;
    return result.Pass();
  }

  result.reset(new MessageHandler(
      instance, handler_if, user_data, message_loop_resource));
  *error = PP_OK;
  return result.Pass();
}

// CreateDeprecated is a near-exact copy of Create, but we'll just delete it
// when 0.1 is deprecated, so need to get fancy to avoid code duplication.
// TODO(dmichael) crbug.com/414398
// static
scoped_ptr<MessageHandler> MessageHandler::CreateDeprecated(
      PP_Instance instance,
      const PPP_MessageHandler_0_1_Deprecated* handler_if,
      void* user_data,
      PP_Resource message_loop,
      int32_t* error) {
  scoped_ptr<MessageHandler> result;
  // The interface and all function pointers must be valid.
  if (!handler_if ||
      !handler_if->HandleMessage ||
      !handler_if->HandleBlockingMessage ||
      !handler_if->Destroy) {
    *error = PP_ERROR_BADARGUMENT;
    return result.Pass();
  }
  thunk::EnterResourceNoLock<thunk::PPB_MessageLoop_API>
      enter_loop(message_loop, true);
  if (enter_loop.failed()) {
    *error = PP_ERROR_BADRESOURCE;
    return result.Pass();
  }
  scoped_refptr<MessageLoopResource> message_loop_resource(
      static_cast<MessageLoopResource*>(enter_loop.object()));
  if (message_loop_resource->is_main_thread_loop()) {
    *error = PP_ERROR_WRONG_THREAD;
    return result.Pass();
  }

  result.reset(new MessageHandler(
      instance, handler_if, user_data, message_loop_resource));
  *error = PP_OK;
  return result.Pass();
}

MessageHandler::~MessageHandler() {
  // It's possible the message_loop_proxy is NULL if that loop has been quit.
  // In that case, we unfortunately just can't call Destroy.
  if (message_loop_->message_loop_proxy().get()) {
    // The posted task won't have the proxy lock, but that's OK, it doesn't
    // touch any internal state; it's a direct call on the plugin's function.
    if (handler_if_0_1_) {
      message_loop_->message_loop_proxy()->PostTask(FROM_HERE,
          base::Bind(handler_if_0_1_->Destroy,
                     instance_,
                     user_data_));
      return;
    }
    message_loop_->message_loop_proxy()->PostTask(FROM_HERE,
        base::Bind(handler_if_->Destroy,
                   instance_,
                   user_data_));
  }
}

bool MessageHandler::LoopIsValid() const {
  return !!message_loop_->message_loop_proxy().get();
}

void MessageHandler::HandleMessage(ScopedPPVar var) {
  if (handler_if_0_1_) {
    // TODO(dmichael): Remove this code path. crbug.com/414398
    message_loop_->message_loop_proxy()->PostTask(FROM_HERE,
        RunWhileLocked(base::Bind(&HandleMessageWrapper_0_1,
                                  handler_if_0_1_->HandleMessage,
                                  instance_,
                                  user_data_,
                                  var)));
    return;
  }
  message_loop_->message_loop_proxy()->PostTask(FROM_HERE,
      RunWhileLocked(base::Bind(&HandleMessageWrapper,
                                handler_if_->HandleMessage,
                                instance_,
                                user_data_,
                                var)));
}

void MessageHandler::HandleBlockingMessage(ScopedPPVar var,
                                           scoped_ptr<IPC::Message> reply_msg) {
  if (handler_if_0_1_) {
    // TODO(dmichael): Remove this code path. crbug.com/414398
    message_loop_->message_loop_proxy()->PostTask(FROM_HERE,
        RunWhileLocked(base::Bind(&HandleBlockingMessageWrapper_0_1,
                                  handler_if_0_1_->HandleBlockingMessage,
                                  instance_,
                                  user_data_,
                                  var,
                                  base::Passed(reply_msg.Pass()))));
    return;
  }
  message_loop_->message_loop_proxy()->PostTask(FROM_HERE,
      RunWhileLocked(base::Bind(&HandleBlockingMessageWrapper,
                                handler_if_->HandleBlockingMessage,
                                instance_,
                                user_data_,
                                var,
                                base::Passed(reply_msg.Pass()))));
}

MessageHandler::MessageHandler(
    PP_Instance instance,
    const PPP_MessageHandler_0_2* handler_if,
    void* user_data,
    scoped_refptr<MessageLoopResource> message_loop)
    : instance_(instance),
      handler_if_(handler_if),
      handler_if_0_1_(NULL),
      user_data_(user_data),
      message_loop_(message_loop) {
}

MessageHandler::MessageHandler(
    PP_Instance instance,
    const PPP_MessageHandler_0_1_Deprecated* handler_if,
    void* user_data,
    scoped_refptr<MessageLoopResource> message_loop)
    : instance_(instance),
      handler_if_(NULL),
      handler_if_0_1_(handler_if),
      user_data_(user_data),
      message_loop_(message_loop) {
}

}  // namespace proxy
}  // namespace ppapi
