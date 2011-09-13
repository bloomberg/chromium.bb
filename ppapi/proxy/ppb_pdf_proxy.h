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
  PPB_PDF_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_PDF_Proxy();

  static const Info* GetInfo();

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

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_PDF* ppb_pdf_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_PDF_PROXY_H_
