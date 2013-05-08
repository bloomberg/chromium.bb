// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CRX filesystem is a filesystem that allows an extension to read its own
// package directory tree.  See ppapi/examples/crxfs for example.
//
// IMPLEMENTATION
//
// The implementation involves both browser and renderer.  In order to provide
// readonly access to CRX filesystem (i.e. extension directory), we create an
// "isolated filesystem" pointing to current extension directory in browser.
// Then browser grants read permission to renderer, and tells plugin the
// filesystem id, or fsid.
//
// Once the plugin receives the fsid, it creates a PPB_FileSystem and forwards
// the fsid to PepperFileSystemHost in order to construct root url.

#ifndef PPAPI_PROXY_EXT_CRX_FILE_SYSTEM_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_EXT_CRX_FILE_SYSTEM_PRIVATE_RESOURCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_ext_crx_file_system_private_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class ResourceMessageReplyParams;

class PPAPI_PROXY_EXPORT ExtCrxFileSystemPrivateResource
    : public PluginResource,
      public thunk::PPB_Ext_CrxFileSystem_Private_API {
 public:
  ExtCrxFileSystemPrivateResource(Connection connection, PP_Instance instance);
  virtual ~ExtCrxFileSystemPrivateResource();

  // Resource overrides.
  virtual thunk::PPB_Ext_CrxFileSystem_Private_API*
      AsPPB_Ext_CrxFileSystem_Private_API() OVERRIDE;

  // PPB_Ext_CrxFileSystem_Private_API implementation.
  virtual int32_t Open(PP_Instance instance,
                       PP_Resource* file_system_resource,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  void OnBrowserOpenComplete(PP_Resource* file_system_resource,
                             scoped_refptr<TrackedCallback> callback,
                             const ResourceMessageReplyParams& params,
                             const std::string& fsid);

  bool called_open_;

  DISALLOW_COPY_AND_ASSIGN(ExtCrxFileSystemPrivateResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_EXT_CRX_FILE_SYSTEM_PRIVATE_RESOURCE_H_
