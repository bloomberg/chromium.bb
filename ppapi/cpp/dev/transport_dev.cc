// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/transport_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Transport_Dev>() {
  return PPB_TRANSPORT_DEV_INTERFACE;
}

}  // namespace

Transport_Dev::Transport_Dev(Instance* instance,
                             const char* name,
                             const char* proto) {
  if (has_interface<PPB_Transport_Dev>())
    PassRefFromConstructor(get_interface<PPB_Transport_Dev>()->CreateTransport(
        instance->pp_instance(), name, proto));
}

bool Transport_Dev::IsWritable() {
  if (!has_interface<PPB_Transport_Dev>())
    return false;
  return PPBoolToBool(
      get_interface<PPB_Transport_Dev>()->IsWritable(pp_resource()));
}

int32_t Transport_Dev::Connect(const CompletionCallback& cc) {
  if (!has_interface<PPB_Transport_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_Transport_Dev>()->Connect(
      pp_resource(), cc.pp_completion_callback());
}

int32_t Transport_Dev::GetNextAddress(Var* address,
                                      const CompletionCallback& cc) {
  if (!has_interface<PPB_Transport_Dev>())
    return PP_ERROR_NOINTERFACE;
  PP_Var temp_address = PP_MakeUndefined();
  int32_t ret_val = get_interface<PPB_Transport_Dev>()->GetNextAddress(
      pp_resource(), &temp_address, cc.pp_completion_callback());
  *address = Var(Var::PassRef(), temp_address);
  return ret_val;
}

int32_t Transport_Dev::ReceiveRemoteAddress(const pp::Var& address) {
  if (!has_interface<PPB_Transport_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_Transport_Dev>()->ReceiveRemoteAddress(
      pp_resource(), address.pp_var());
}

int32_t Transport_Dev::Recv(void* data, uint32_t len,
                            const CompletionCallback& cc) {
  if (!has_interface<PPB_Transport_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_Transport_Dev>()->Recv(
      pp_resource(), data, len, cc.pp_completion_callback());
}

int32_t Transport_Dev::Send(const void* data, uint32_t len,
                            const CompletionCallback& cc) {
  if (!has_interface<PPB_Transport_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_Transport_Dev>()->Send(
      pp_resource(), data, len, cc.pp_completion_callback());
}

int32_t Transport_Dev::Close() {
  if (!has_interface<PPB_Transport_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_Transport_Dev>()->Close(pp_resource());
}

}  // namespace pp
