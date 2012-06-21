// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/resource_message_params.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

ResourceMessageParams::ResourceMessageParams()
    : pp_resource_(0),
      sequence_(0) {
}

ResourceMessageParams::ResourceMessageParams(PP_Resource resource,
                                             int32_t sequence)
    : pp_resource_(resource),
      sequence_(sequence) {
}

ResourceMessageParams::~ResourceMessageParams() {
}

void ResourceMessageParams::Serialize(IPC::Message* msg) const {
  IPC::ParamTraits<PP_Resource>::Write(msg, pp_resource_);
  IPC::ParamTraits<int32_t>::Write(msg, sequence_);
}

bool ResourceMessageParams::Deserialize(const IPC::Message* msg,
                                        PickleIterator* iter) {
  return IPC::ParamTraits<PP_Resource>::Read(msg, iter, &pp_resource_) &&
         IPC::ParamTraits<int32_t>::Read(msg, iter, &sequence_);
}

ResourceMessageCallParams::ResourceMessageCallParams()
    : ResourceMessageParams(),
      has_callback_(0) {
}

ResourceMessageCallParams::ResourceMessageCallParams(PP_Resource resource,
                                                     int32_t sequence)
    : ResourceMessageParams(resource, sequence),
      has_callback_(0) {
}

ResourceMessageCallParams::~ResourceMessageCallParams() {
}

void ResourceMessageCallParams::Serialize(IPC::Message* msg) const {
  ResourceMessageParams::Serialize(msg);
  IPC::ParamTraits<bool>::Write(msg, has_callback_);
}

bool ResourceMessageCallParams::Deserialize(const IPC::Message* msg,
                                            PickleIterator* iter) {
  if (!ResourceMessageParams::Deserialize(msg, iter))
    return false;
  return IPC::ParamTraits<bool>::Read(msg, iter, &has_callback_);
}

ResourceMessageReplyParams::ResourceMessageReplyParams()
    : ResourceMessageParams(),
      result_(PP_OK) {
}

ResourceMessageReplyParams::ResourceMessageReplyParams(PP_Resource resource,
                                                       int32_t sequence)
    : ResourceMessageParams(resource, sequence),
      result_(PP_OK) {
}

ResourceMessageReplyParams::~ResourceMessageReplyParams() {
}

void ResourceMessageReplyParams::Serialize(IPC::Message* msg) const {
  ResourceMessageParams::Serialize(msg);
  IPC::ParamTraits<int32_t>::Write(msg, result_);
}

bool ResourceMessageReplyParams::Deserialize(const IPC::Message* msg,
                                             PickleIterator* iter) {
  if (!ResourceMessageParams::Deserialize(msg, iter))
    return false;
  return IPC::ParamTraits<int32_t>::Read(msg, iter, &result_);
}

}  // namespace proxy
}  // namespace ppapi
