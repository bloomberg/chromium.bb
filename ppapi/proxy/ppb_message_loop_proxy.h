// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_
#define PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_MessageLoop_Dev_0_1;

namespace ppapi {
namespace proxy {

class PPB_MessageLoop_Proxy : public InterfaceProxy {
 public:
  PPB_MessageLoop_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_MessageLoop_Proxy();

  static const PPB_MessageLoop_Dev_0_1* GetInterface();

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_MessageLoop_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_
