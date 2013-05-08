// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_EXT_CRX_FILE_SYSTEM_API_H_
#define PPAPI_THUNK_PPB_EXT_CRX_FILE_SYSTEM_API_H_

#include "base/memory/ref_counted.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Ext_CrxFileSystem_Private_API {
 public:
  virtual ~PPB_Ext_CrxFileSystem_Private_API() {}

  virtual int32_t Open(PP_Instance instance,
                       PP_Resource* file_system,
                       scoped_refptr<TrackedCallback> callback) = 0;

  static const SingletonResourceID kSingletonResourceID =
      CRX_FILESYSTEM_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_EXT_CRX_FILE_SYSTEM_API_H_
