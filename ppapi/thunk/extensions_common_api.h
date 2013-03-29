// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_EXTENSIONS_COMMON_API_H_
#define PPAPI_THUNK_EXTENSIONS_COMMON_API_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT ExtensionsCommon_API {
 public:
  virtual ~ExtensionsCommon_API() {}

  virtual int32_t Call(const std::string& request_name,
                       const std::vector<PP_Var>& input_args,
                       const std::vector<PP_Var*>& output_args,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual void Post(const std::string& request_name,
                    const std::vector<PP_Var>& args) = 0;

  static const SingletonResourceID kSingletonResourceID =
      EXTENSIONS_COMMON_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif // PPAPI_THUNK_EXTENSIONS_COMMON_API_H_
