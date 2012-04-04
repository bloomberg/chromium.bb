// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_X509_CERTIFICATE_PRIVATE_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_X509_CERTIFICATE_PRIVATE_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {
class PPB_X509Certificate_Fields;
}

namespace webkit {
namespace ppapi {

class PPB_X509Certificate_Private_Impl :
    public ::ppapi::PPB_X509Certificate_Private_Shared {
 public:
  PPB_X509Certificate_Private_Impl(PP_Instance instance);
  static PP_Resource CreateResource(PP_Instance instance);
  virtual bool ParseDER(const std::vector<char>& der,
                        ::ppapi::PPB_X509Certificate_Fields* result) OVERRIDE;

 private:
  virtual ~PPB_X509Certificate_Private_Impl();

  DISALLOW_COPY_AND_ASSIGN(PPB_X509Certificate_Private_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_X509_CERTIFICATE_PRIVATE_IMPL_H_
