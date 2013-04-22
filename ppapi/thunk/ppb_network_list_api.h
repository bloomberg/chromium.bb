// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_NETWORK_LIST_API_H_
#define PPAPI_THUNK_PPB_NETWORK_LIST_API_H_

#include <vector>

#include "ppapi/c/private/ppb_network_list_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct NetworkInfo;
typedef std::vector<NetworkInfo> NetworkList;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_NetworkList_API {
 public:
  virtual ~PPB_NetworkList_API() {}

  // This function is not exposed through the C API, but returns the
  // internal data for easy proxying.
  virtual const NetworkList& GetNetworkListData() const = 0;

  // Private API
  virtual uint32_t GetCount() = 0;
  virtual PP_Var GetName(uint32_t index) = 0;
  virtual PP_NetworkListType_Private GetType(uint32_t index) = 0;
  virtual PP_NetworkListState_Private GetState(uint32_t index) = 0;
  virtual int32_t GetIpAddresses(uint32_t index,
                                 PP_NetAddress_Private addresses[],
                                 uint32_t count) = 0;
  virtual PP_Var GetDisplayName(uint32_t index) = 0;
  virtual uint32_t GetMTU(uint32_t index) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_NETWORK_LIST_API_H_
