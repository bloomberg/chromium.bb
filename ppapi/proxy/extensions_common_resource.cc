// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/extensions_common_resource.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var_value_conversions.h"

namespace ppapi {
namespace proxy {

ExtensionsCommonResource::ExtensionsCommonResource(Connection connection,
                                                   PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(RENDERER, PpapiHostMsg_ExtensionsCommon_Create());
}

ExtensionsCommonResource::~ExtensionsCommonResource() {
}

thunk::ExtensionsCommon_API*
ExtensionsCommonResource::AsExtensionsCommon_API() {
  return this;
}

int32_t ExtensionsCommonResource::Call(
    const std::string& request_name,
    const std::vector<PP_Var>& input_args,
    const std::vector<PP_Var*>& output_args,
    scoped_refptr<TrackedCallback> callback) {
  // TODO(yzshen): CreateValueFromVar() doesn't generate null fields for
  // dictionary values. That is the expected behavior for most APIs. If later we
  // want to support APIs that require to preserve null fields in dictionaries,
  // we should change the behavior to always preserve null fields at the plugin
  // side, and figure out whether they should be stripped at the renderer side.
  scoped_ptr<base::ListValue> input_args_value(
      CreateListValueFromVarVector(input_args));
  if (!input_args_value.get()) {
    LOG(WARNING) << "Failed to convert PP_Var input arguments.";
    return PP_ERROR_BADARGUMENT;
  }

  PluginResource::Call<PpapiPluginMsg_ExtensionsCommon_CallReply>(
      RENDERER,
      PpapiHostMsg_ExtensionsCommon_Call(request_name, *input_args_value),
      base::Bind(&ExtensionsCommonResource::OnPluginMsgCallReply,
                 base::Unretained(this), output_args, callback));
  return PP_OK_COMPLETIONPENDING;
}

void ExtensionsCommonResource::Post(const std::string& request_name,
                                    const std::vector<PP_Var>& args) {
  scoped_ptr<base::ListValue> args_value(CreateListValueFromVarVector(args));
  if (!args_value.get()) {
    LOG(WARNING) << "Failed to convert PP_Var input arguments.";
    return;
  }

  PluginResource::Post(
      RENDERER, PpapiHostMsg_ExtensionsCommon_Post(request_name, *args_value));
}

void ExtensionsCommonResource::OnPluginMsgCallReply(
    const std::vector<PP_Var*>& output_args,
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    const base::ListValue& output) {
  // |output_args| may be invalid and shouldn't be accessed if the callback has
  // been called.
  if (!TrackedCallback::IsPending(callback))
    return;

  int32_t result = params.result();
  if (result == PP_OK) {
    // If the size doesn't match, something must be really wrong.
    CHECK_EQ(output_args.size(), output.GetSize());

    std::vector<PP_Var> output_vars;
    if (CreateVarVectorFromListValue(output, &output_vars)) {
      DCHECK_EQ(output_args.size(), output_vars.size());
      std::vector<PP_Var>::const_iterator src_iter = output_vars.begin();
      std::vector<PP_Var*>::const_iterator dest_iter = output_args.begin();
      for (; src_iter != output_vars.end() && dest_iter != output_args.end();
           ++src_iter, ++dest_iter) {
        **dest_iter = *src_iter;
      }
    } else {
      result = PP_ERROR_FAILED;
    }
  }

  callback->Run(result);
}

}  // namespace proxy
}  // namespace ppapi
