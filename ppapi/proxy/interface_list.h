// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_INTERFACE_LIST_H_
#define PPAPI_PROXY_INTERFACE_LIST_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {
namespace proxy {

class InterfaceList {
 public:
  InterfaceList();
  ~InterfaceList();

  static InterfaceList* GetInstance();

  // Sets the permissions that the interface list will use to compute
  // whether an interface is available to the current process. By default,
  // this will be "no permissions", which will give only access to public
  // stable interfaces via GetInterface.
  //
  // IMPORTANT: This is not a security boundary. Malicious plugins can bypass
  // this check since they run in the same address space as this code in the
  // plugin process. A real security check is required for all IPC messages.
  // This check just allows us to return NULL for interfaces you "shouldn't" be
  // using to keep honest plugins honest.
  static PPAPI_PROXY_EXPORT void SetProcessGlobalPermissions(
      const PpapiPermissions& permissions);

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
          iface(NULL),
          required_permission(PERMISSION_NONE) {
    }
    InterfaceInfo(ApiID in_id, const void* in_interface, Permission in_perm)
        : id(in_id),
          iface(in_interface),
          required_permission(in_perm) {
    }

    ApiID id;
    const void* iface;

    // Permission required to return non-null for this interface. This will
    // be checked with the value set via SetProcessGlobalPermissionBits when
    // an interface is requested.
    Permission required_permission;
  };

  typedef std::map<std::string, InterfaceInfo> NameToInterfaceInfoMap;

  void AddProxy(ApiID id, InterfaceProxy::Factory factory);

  // Permissions is the type of permission required to access the corresponding
  // interface. Currently this must be just one unique permission (rather than
  // a bitfield).
  void AddPPB(const char* name, ApiID id, const void* iface,
              Permission permission);
  void AddPPP(const char* name, ApiID id, const void* iface);

  // Old-style add functions. These should be removed when the rest of the
  // proxies are converted over to using the new system.
  void AddPPB(const InterfaceProxy::Info* info, Permission perm);
  void AddPPP(const InterfaceProxy::Info* info);

  PpapiPermissions permissions_;

  NameToInterfaceInfoMap name_to_browser_info_;
  NameToInterfaceInfoMap name_to_plugin_info_;

  InterfaceProxy::Factory id_to_factory_[API_ID_COUNT];

  DISALLOW_COPY_AND_ASSIGN(InterfaceList);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_INTERFACE_LIST_H_

