// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/flash_resource.h"

#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

FlashResource::FlashResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(RENDERER, PpapiHostMsg_Flash_Create());
}

FlashResource::~FlashResource() {
}

thunk::PPB_Flash_Functions_API* FlashResource::AsPPB_Flash_Functions_API() {
  return this;
}

}  // namespace proxy
}  // namespace ppapi
