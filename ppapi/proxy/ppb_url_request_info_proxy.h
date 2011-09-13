// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_
#define PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {

struct PPB_URLRequestInfo_Data;

namespace proxy {

class PPB_URLRequestInfo_Proxy : public InterfaceProxy {
 public:
  PPB_URLRequestInfo_Proxy(Dispatcher* dispatcher,
                           const void* target_interface);
  virtual ~PPB_URLRequestInfo_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_URLRequestInfo_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_
