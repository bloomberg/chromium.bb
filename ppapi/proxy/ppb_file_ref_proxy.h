// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FILE_REF_PROXY_H_
#define PPAPI_PROXY_PPB_FILE_REF_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_FileRef_Dev;

namespace ppapi {

class HostResource;
struct PPB_FileRef_CreateInfo;

namespace proxy {

class PPB_FileRef_Proxy : public InterfaceProxy {
 public:
  PPB_FileRef_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_FileRef_Proxy();

  static const Info* GetInfo();

  static PP_Resource CreateProxyResource(PP_Resource file_system,
                                         const char* path);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Takes a resource in the host and converts it into a serialized file ref
  // "create info" for reconstitution in the plugin. This struct contains all
  // the necessary information about the file ref.
  //
  // This function is not static because it needs access to the particular
  // dispatcher and host interface.
  //
  // Various PPAPI functions return file refs from various interfaces, so this
  // function is public so anybody can send a file ref.
  void SerializeFileRef(PP_Resource file_ref,
                        PPB_FileRef_CreateInfo* result);

  // Creates a plugin resource from the given CreateInfo sent from the host.
  // The value will be the result of calling SerializeFileRef on the host.
  // This represents passing the resource ownership to the plugin. This
  // function also checks the validity of the result and returns 0 on failure.
  //
  // Various PPAPI functions return file refs from various interfaces, so this
  // function is public so anybody can receive a file ref.
  static PP_Resource DeserializeFileRef(
      const PPB_FileRef_CreateInfo& serialized);

 private:
  // Message handlers.
  void OnMsgCreate(const ppapi::HostResource& file_system,
                   const std::string& path,
                   PPB_FileRef_CreateInfo* result);
  void OnMsgGetParent(const ppapi::HostResource& host_resource,
                      PPB_FileRef_CreateInfo* result);
  void OnMsgMakeDirectory(const ppapi::HostResource& host_resource,
                          PP_Bool make_ancestors,
                          uint32_t serialized_callback);
  void OnMsgTouch(const ppapi::HostResource& host_resource,
                  PP_Time last_access,
                  PP_Time last_modified,
                  uint32_t serialized_callback);
  void OnMsgDelete(const ppapi::HostResource& host_resource,
                   uint32_t serialized_callback);
  void OnMsgRename(const ppapi::HostResource& file_ref,
                   const ppapi::HostResource& new_file_ref,
                   uint32_t serialized_callback);

  DISALLOW_COPY_AND_ASSIGN(PPB_FileRef_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FILE_REF_PROXY_H_
