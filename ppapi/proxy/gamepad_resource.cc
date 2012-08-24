// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/gamepad_resource.h"

#include <string.h>

#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

GamepadResource::GamepadResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance),
      buffer_(NULL) {
  SendCreateToBrowser(PpapiHostMsg_Gamepad_Create());
  CallBrowser(PpapiHostMsg_Gamepad_RequestMemory());
}

GamepadResource::~GamepadResource() {
}

void GamepadResource::Sample(PP_GamepadsSampleData* data) {
  if (!buffer_) {
    // Browser hasn't sent back our shared memory, give the plugin gamepad
    // data corresponding to "not connected".
    memset(data, 0, sizeof(PP_GamepadsSampleData));
  } else {
    memcpy(data, buffer_, sizeof(PP_GamepadsSampleData));
  }
}

void GamepadResource::OnReplyReceived(const ResourceMessageReplyParams& params,
                                      const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GamepadResource, msg)
    PPAPI_DISPATCH_RESOURCE_REPLY(PpapiPluginMsg_Gamepad_SendMemory,
                                  OnPluginMsgSendMemory)
  IPC_END_MESSAGE_MAP()
}

void GamepadResource::OnPluginMsgSendMemory(
    const ResourceMessageReplyParams& params,
    base::SharedMemoryHandle shared_memory_handle) {
  /* TODO(brettw) implement this when we have shared gamepad code. It would be
     something like this:
  shared_memory_.reset(
      new base::SharedMemory(shared_memory_handle, true));
  CHECK(shared_memory_->Map(sizeof(GamepadHardwareBuffer)));
  void *memory = shared_memory_->memory();
  // Use the memory...
  */
}

}  // namespace proxy
}  // namespace ppapi
