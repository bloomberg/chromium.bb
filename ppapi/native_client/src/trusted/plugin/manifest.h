/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Manifest interface class.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_MANIFEST_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_MANIFEST_H_

#include <map>
#include <set>
#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace pp {
class URLUtil_Dev;
}  // namespace pp

namespace plugin {

class ErrorInfo;

class Manifest {
 public:
  Manifest(const pp::URLUtil_Dev* url_util,
           const nacl::string& manifest_base_url,
           const nacl::string& sandbox_isa,
           bool prefer_portable)
      : url_util_(url_util),
        manifest_base_url_(manifest_base_url),
        sandbox_isa_(sandbox_isa),
        prefer_portable_(prefer_portable) { }
  virtual ~Manifest() { }

  // Gets the full program URL for the current sandbox ISA from the
  // manifest file. Sets |is_portable| to |true| if the program is
  // portable bitcode.
  virtual bool GetProgramURL(nacl::string* full_url,
                             ErrorInfo* error_info,
                             bool* is_portable) = 0;

  // Resolves a URL relative to the manifest base URL
  virtual bool ResolveURL(const nacl::string& relative_url,
                          nacl::string* full_url,
                          ErrorInfo* error_info) const = 0;

  // Gets the file names from the "files" section of the manifest.  No
  // checking that the keys' values are proper ISA dictionaries -- it
  // is assumed that other consistency checks take care of that, and
  // that the keys are appropriate for use with ResolveKey.
  virtual bool GetFileKeys(std::set<nacl::string>* keys) const = 0;

  // Resolves a key from the "files" section to a fully resolved URL,
  // i.e., relative URL values are fully expanded relative to the
  // manifest's URL (via ResolveURL).  If there was an error, details
  // are reported via error_info, and is_portable, if non-NULL, tells
  // the caller whether the resolution used the portable
  // representation or an ISA-specific version of the file.
  virtual bool ResolveKey(const nacl::string& key,
                          nacl::string* full_url,
                          ErrorInfo* error_info,
                          bool* is_portable) const = 0;

  // Some manifests (e.g., the pnacl coordinator manifest) need to access
  // resources from an extension origin distinct from the plugin's origin
  // (e.g., the pnacl coordinator needs to load llc, ld, and some libraries).
  // This method returns true if such access should be allowed.
  virtual bool PermitsExtensionUrls() const = 0;

 protected:
  NACL_DISALLOW_COPY_AND_ASSIGN(Manifest);

  const pp::URLUtil_Dev* url_util_;
  nacl::string manifest_base_url_;
  nacl::string sandbox_isa_;
  // Determines whether portable programs are chosen in manifest files over
  // native programs.
  bool prefer_portable_;
};


}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_MANIFEST_H_
