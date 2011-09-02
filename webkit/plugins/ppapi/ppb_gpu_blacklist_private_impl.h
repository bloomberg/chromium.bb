// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_GPU_BLACKLIST_PRIVATE_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_GPU_BLACKLIST_PRIVATE_IMPL_H_

struct PPB_GpuBlacklist_Private;

namespace webkit {
namespace ppapi {

class PPB_GpuBlacklist_Private_Impl {
 public:
  static const PPB_GpuBlacklist_Private* GetInterface();
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_GPU_BLACKLIST_PRIVATE_IMPL_H_

