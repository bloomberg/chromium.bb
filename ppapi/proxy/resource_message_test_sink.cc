// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/resource_message_test_sink.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace ppapi {
namespace proxy {

namespace {

// Backend for GetFirstResource[Call|Reply]Matching.
template<class WrapperMessage, class Params>
bool GetFirstResourceMessageMatching(const ResourceMessageTestSink& sink,
                                     uint32 id,
                                     Params* params,
                                     IPC::Message* nested_msg) {
  for (size_t i = 0; i < sink.message_count(); i++) {
    const IPC::Message* msg = sink.GetMessageAt(i);
    if (msg->type() == WrapperMessage::ID) {
      Params cur_params;
      IPC::Message cur_msg;
      WrapperMessage::Read(msg, &cur_params, &cur_msg);
      if (cur_msg.type() == id) {
        *params = cur_params;
        *nested_msg = cur_msg;
        return true;
      }
    }
  }
  return false;
}

}  // namespace

ResourceMessageTestSink::ResourceMessageTestSink() {
}

ResourceMessageTestSink::~ResourceMessageTestSink() {
}

bool ResourceMessageTestSink::GetFirstResourceCallMatching(
    uint32 id,
    ResourceMessageCallParams* params,
    IPC::Message* nested_msg) const {
  return GetFirstResourceMessageMatching<PpapiHostMsg_ResourceCall,
                                         ResourceMessageCallParams>(
      *this, id, params, nested_msg);
}

bool ResourceMessageTestSink::GetFirstResourceReplyMatching(
    uint32 id,
    ResourceMessageReplyParams* params,
    IPC::Message* nested_msg) {
  return GetFirstResourceMessageMatching<PpapiPluginMsg_ResourceReply,
                                         ResourceMessageReplyParams>(
      *this, id, params, nested_msg);
}

}  // namespace proxy
}  // namespace ppapi
