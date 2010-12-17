// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_NACL_PRIVATE_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_NACL_PRIVATE_IMPL_H_

struct PPB_NaCl_Private;

namespace webkit {
namespace ppapi {

class PPB_NaCl_Private_Impl {
 public:
  static const PPB_NaCl_Private* GetInterface();
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_NACL_PRIVATE_IMPL_H_
