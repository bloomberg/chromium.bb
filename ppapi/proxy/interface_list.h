// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_INTERFACE_LIST_H_
#define PPAPI_PROXY_INTERFACE_LIST_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {
namespace proxy {

class InterfaceList {
 public:
  InterfaceList();
  ~InterfaceList();

  static InterfaceList* GetInstance();

  // Looks up the ID for the given interface name. Returns API_ID_NONE if
  // the interface string is not found.
  ApiID GetIDForPPBInterface(const std::string& name) const;
  ApiID GetIDForPPPInterface(const std::string& name) const;

  // Looks up the factory function for the given ID. Returns NULL if not
  // supported.
  InterfaceProxy::Factory GetFactoryForID(ApiID id) const;

  // Returns the interface pointer for the given browser or plugin interface,
  // or NULL if it's not supported.
  const void* GetInterfaceForPPB(const std::string& name) const;
  const void* GetInterfaceForPPP(const std::string& name) const;

 private:
  struct InterfaceInfo {
    InterfaceInfo()
        : id(API_ID_NONE),
          iface(NULL) {
    }
    InterfaceInfo(ApiID in_id, const void* in_interface)
        : id(in_id),
          iface(in_interface) {
    }

    ApiID id;
    const void* iface;
  };

  typedef std::map<std::string, InterfaceInfo> NameToInterfaceInfoMap;

  void AddProxy(ApiID id, InterfaceProxy::Factory factory);

  void AddPPB(const char* name, ApiID id, const void* iface);
  void AddPPP(const char* name, ApiID id, const void* iface);

  // Old-style add functions. These should be removed when the rest of the
  // proxies are converted over to using the new system.
  void AddPPB(const InterfaceProxy::Info* info);
  void AddPPP(const InterfaceProxy::Info* info);

  NameToInterfaceInfoMap name_to_browser_info_;
  NameToInterfaceInfoMap name_to_plugin_info_;

  InterfaceProxy::Factory id_to_factory_[API_ID_COUNT];

  DISALLOW_COPY_AND_ASSIGN(InterfaceList);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_INTERFACE_LIST_H_

