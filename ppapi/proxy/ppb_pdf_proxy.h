// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_PDF_PROXY_H_
#define PPAPI_PPB_PDF_PROXY_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Private;

namespace pp {
namespace proxy {

struct SerializedFontDescription;

class PPB_Pdf_Proxy : public InterfaceProxy {
 public:
  PPB_Pdf_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Pdf_Proxy();

  const PPB_Private* ppb_pdf_target() const {
    return static_cast<const PPB_Private*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual void OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgGetFontFileWithFallback(PP_Module module,
                                    const SerializedFontDescription& desc,
                                    int32_t charset,
                                    PP_Resource* result);
  void OnMsgGetFontTableForPrivateFontFile(PP_Resource font_file,
                                           uint32_t table,
                                           std::string* result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_PDF_PROXY_H_
