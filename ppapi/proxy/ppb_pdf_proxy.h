// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_PDF_PROXY_H_
#define PPAPI_PPB_PDF_PROXY_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PPB_PDF;

namespace ppapi {
namespace proxy {

struct SerializedFontDescription;

class PPB_PDF_Proxy : public InterfaceProxy {
 public:
  PPB_PDF_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_PDF_Proxy();

  static const Info* GetInfo();

  const PPB_PDF* ppb_pdf_target() const {
    return static_cast<const PPB_PDF*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgGetFontFileWithFallback(PP_Module module,
                                    const SerializedFontDescription& desc,
                                    int32_t charset,
                                    ppapi::HostResource* result);
  void OnMsgGetFontTableForPrivateFontFile(const ppapi::HostResource& font_file,
                                           uint32_t table,
                                           std::string* result);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_PDF_PROXY_H_
