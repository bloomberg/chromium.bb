/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_RESOURCE_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_RESOURCE_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/dev/ppb_url_response_info_dev.h"

namespace fake_browser_ppapi {

class URLLoader;
class URLRequestInfo;
class URLResponseInfo;
class FileIO;
class FileRef;

// Represents a generic resource tracked by the Host.
// The C API functions usually start by mapping a PP_Resource id to a Resource
// and then access and modify its state.
// This is a simplified version of Chrome's pepper::Resource.
class Resource {
 public:
  Resource() : resource_id_(0) {}
  virtual ~Resource() {}

  void set_resource_id(PP_Resource resource_id) { resource_id_ = resource_id; }
  PP_Resource resource_id() const { return resource_id_; }

  // This will allow us to safely cast Resource to derived types.
  // The derived classes must therefore override their respective methods to
  // return |this|.
  virtual URLLoader* AsURLLoader() { return NULL; }
  virtual URLRequestInfo* AsURLRequestInfo() { return NULL; }
  virtual URLResponseInfo* AsURLResponseInfo() { return NULL; }
  virtual FileIO* AsFileIO() { return NULL; }
  virtual FileRef* AsFileRef() { return NULL; }

  static Resource* Invalid() { return &kInvalidResource; }

 private:
  PP_Resource resource_id_;

  static Resource kInvalidResource;
  NACL_DISALLOW_COPY_AND_ASSIGN(Resource);
};

// These are made global so that C API functions can access them from any file.
// To be implemented by main.cc.
PP_Resource TrackResource(Resource* resource);
// Returns Resource::Invalid() on error.
Resource* GetResource(PP_Resource resource_id);

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_RESOURCE_H_
