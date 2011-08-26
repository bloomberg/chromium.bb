// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_url_request_info_proxy.h"

#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/shared_impl/url_request_info_impl.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreateURLRequestInfoProxy(Dispatcher* dispatcher,
                                          const void* target_interface) {
  return new PPB_URLRequestInfo_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_URLRequestInfo_Proxy::PPB_URLRequestInfo_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_URLRequestInfo_Proxy::~PPB_URLRequestInfo_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_URLRequestInfo_Proxy::GetInfo() {
  static const Info info = {
    thunk::GetPPB_URLRequestInfo_Thunk(),
    PPB_URLREQUESTINFO_INTERFACE,
    INTERFACE_ID_PPB_URL_REQUEST_INFO,
    false,
    &CreateURLRequestInfoProxy,
  };
  return &info;
}

bool PPB_URLRequestInfo_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // No messages to handle.
  return false;
}

}  // namespace proxy
}  // namespace ppapi
