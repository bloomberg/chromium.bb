// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_NETWORK_LIST_PRIVATE_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_NETWORK_LIST_PRIVATE_SHARED_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_network_list_private_api.h"

namespace ppapi {

struct PPAPI_SHARED_EXPORT NetworkInfo {
  NetworkInfo();
  ~NetworkInfo();

  std::string name;
  PP_NetworkListType_Private type;
  PP_NetworkListState_Private state;
  std::vector<PP_NetAddress_Private> addresses;
  std::string display_name;
  int mtu;
};
typedef std::vector<NetworkInfo> NetworkList;

// NetworkListStorage is refcounted container for NetworkList. It
// allows sharing of one NetworkList object between multiple
// NetworkList resources.
class PPAPI_SHARED_EXPORT NetworkListStorage
    : public base::RefCountedThreadSafe<NetworkListStorage> {
 public:
  explicit NetworkListStorage(const NetworkList& list);

  const NetworkList& list() { return list_; }

 private:
  friend class base::RefCountedThreadSafe<NetworkListStorage>;
  ~NetworkListStorage();

  NetworkList list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkListStorage);
};

class PPAPI_SHARED_EXPORT PPB_NetworkList_Private_Shared
    : public ::ppapi::Resource,
      public ::ppapi::thunk::PPB_NetworkList_Private_API {
 public:
  static PP_Resource Create(ResourceObjectType type,
                            PP_Instance instance,
                            const scoped_refptr<NetworkListStorage>& list);

  virtual ~PPB_NetworkList_Private_Shared();

  // Resource override.
  virtual ::ppapi::thunk::PPB_NetworkList_Private_API*
      AsPPB_NetworkList_Private_API() OVERRIDE;

  // PPB_NetworkList_Private_API implementation.
  virtual const NetworkList& GetNetworkListData() const OVERRIDE;
  virtual uint32_t GetCount() OVERRIDE;
  virtual PP_Var GetName(uint32_t index) OVERRIDE;
  virtual PP_NetworkListType_Private GetType(uint32_t index) OVERRIDE;
  virtual PP_NetworkListState_Private GetState(uint32_t index) OVERRIDE;
  virtual int32_t GetIpAddresses(uint32_t index,
                                 PP_NetAddress_Private addresses[],
                                 uint32_t count) OVERRIDE;
  virtual PP_Var GetDisplayName(uint32_t index) OVERRIDE;
  virtual uint32_t GetMTU(uint32_t index) OVERRIDE;

 private:
  PPB_NetworkList_Private_Shared(ResourceObjectType type,
                                 PP_Instance instance,
                                 const scoped_refptr<NetworkListStorage>& list);

  scoped_refptr<NetworkListStorage> list_;

  DISALLOW_COPY_AND_ASSIGN(PPB_NetworkList_Private_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_NETWORK_LIST_PRIVATE_SHARED_H_
