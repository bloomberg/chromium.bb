// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/message_loop_dev.h"

#include "ppapi/c/dev/ppb_message_loop_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_MessageLoop_Dev>() {
  return PPB_MESSAGELOOP_DEV_INTERFACE_0_1;
}

}  // namespace

MessageLoop_Dev::MessageLoop_Dev() : Resource() {
}

MessageLoop_Dev::MessageLoop_Dev(const InstanceHandle& instance) : Resource() {
  if (has_interface<PPB_MessageLoop_Dev>()) {
    PassRefFromConstructor(get_interface<PPB_MessageLoop_Dev>()->Create(
        instance.pp_instance()));
  }
}

MessageLoop_Dev::MessageLoop_Dev(const MessageLoop_Dev& other)
    : Resource(other) {
}

MessageLoop_Dev::MessageLoop_Dev(PP_Resource pp_message_loop)
    : Resource(pp_message_loop) {
}

// static
MessageLoop_Dev MessageLoop_Dev::GetForMainThread() {
  if (!has_interface<PPB_MessageLoop_Dev>())
    return MessageLoop_Dev();
  return MessageLoop_Dev(
      get_interface<PPB_MessageLoop_Dev>()->GetForMainThread());
}

// static
MessageLoop_Dev MessageLoop_Dev::GetCurrent() {
  if (!has_interface<PPB_MessageLoop_Dev>())
    return MessageLoop_Dev();
  return MessageLoop_Dev(
      get_interface<PPB_MessageLoop_Dev>()->GetCurrent());
}

int32_t MessageLoop_Dev::AttachToCurrentThread() {
  if (!has_interface<PPB_MessageLoop_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_MessageLoop_Dev>()->AttachToCurrentThread(
      pp_resource());
}

int32_t MessageLoop_Dev::Run() {
  if (!has_interface<PPB_MessageLoop_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_MessageLoop_Dev>()->Run(pp_resource());
}

int32_t MessageLoop_Dev::PostWork(const CompletionCallback& callback,
                                  int64_t delay_ms) {
  if (!has_interface<PPB_MessageLoop_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_MessageLoop_Dev>()->PostWork(
      pp_resource(),
      callback.pp_completion_callback(),
      delay_ms);
}

int32_t MessageLoop_Dev::PostQuit(bool should_destroy) {
  if (!has_interface<PPB_MessageLoop_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_MessageLoop_Dev>()->PostQuit(
      pp_resource(), PP_FromBool(should_destroy));
}

}  // namespace pp
