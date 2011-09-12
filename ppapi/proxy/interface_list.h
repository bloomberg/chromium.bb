// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  // Looks up the ID for the given interface name. Returns INTERFACE_ID_NONE if
  // the interface string is not found.
  InterfaceID GetIDForPPBInterface(const std::string& name) const;
  InterfaceID GetIDForPPPInterface(const std::string& name) const;

  // Looks up the factory function for the given ID. Returns NULL if not
  // supported.
  InterfaceProxy::Factory GetFactoryForID(InterfaceID id) const;

  // Returns the interface pointer for the given browser or plugin interface,
  // or NULL if it's not supported.
  const void* GetInterfaceForPPB(const std::string& name) const;
  const void* GetInterfaceForPPP(const std::string& name) const;

 private:
  struct InterfaceInfo {
    InterfaceInfo()
        : id(INTERFACE_ID_NONE),
          interface(NULL) {
    }
    InterfaceInfo(InterfaceID in_id, const void* in_interface)
        : id(in_id),
          interface(in_interface) {
    }

    InterfaceID id;
    const void* interface;
  };

  typedef std::map<std::string, InterfaceInfo> NameToInterfaceInfoMap;

  void AddProxy(InterfaceID id, InterfaceProxy::Factory factory);

  void AddPPB(const char* name, InterfaceID id, const void* interface);
  void AddPPP(const char* name, InterfaceID id, const void* interface);

  // Old-style add functions. These should be removed when the rest of the
  // proxies are converted over to using the new system.
  void AddPPB(const InterfaceProxy::Info* info);
  void AddPPP(const InterfaceProxy::Info* info);

  NameToInterfaceInfoMap name_to_browser_info_;
  NameToInterfaceInfoMap name_to_plugin_info_;

  InterfaceProxy::Factory id_to_factory_[INTERFACE_ID_COUNT];

  DISALLOW_COPY_AND_ASSIGN(InterfaceList);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_INTERFACE_LIST_H_

