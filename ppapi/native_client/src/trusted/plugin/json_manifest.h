/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Manifest processing for JSON manifests.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_JSON_MANIFEST_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_JSON_MANIFEST_H_

#include <map>
#include <set>
#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "ppapi/native_client/src/trusted/plugin/manifest.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace pp {
class URLUtil_Dev;
}  // namespace pp

namespace plugin {

class ErrorInfo;
class PnaclOptions;

class JsonManifest : public Manifest {
 public:
  JsonManifest(const pp::URLUtil_Dev* url_util,
               const nacl::string& manifest_base_url,
               const nacl::string& sandbox_isa,
               bool nonsfi_enabled,
               bool pnacl_debug)
      : url_util_(url_util),
        manifest_base_url_(manifest_base_url),
        sandbox_isa_(sandbox_isa),
        nonsfi_enabled_(nonsfi_enabled),
        pnacl_debug_(pnacl_debug),
        dictionary_(Json::nullValue) { }
  virtual ~JsonManifest() { }

  // Initialize the manifest object for use by later lookups.  The return
  // value is true if the manifest parses correctly and matches the schema.
  bool Init(const nacl::string& json, ErrorInfo* error_info);

  // Gets the full program URL for the current sandbox ISA from the
  // manifest file.
  virtual bool GetProgramURL(nacl::string* full_url,
                             PnaclOptions* pnacl_options,
                             bool* uses_nonsfi_mode,
                             ErrorInfo* error_info) const;

  // Gets the file names from the "files" section of the manifest.  No
  // checking that the keys' values are proper ISA dictionaries -- it
  // is assumed that other consistency checks take care of that, and
  // that the keys are appropriate for use with ResolveKey.
  virtual bool GetFileKeys(std::set<nacl::string>* keys) const;

  // Resolves a key from the "files" section to a fully resolved URL,
  // i.e., relative URL values are fully expanded relative to the
  // manifest's URL (via ResolveURL).
  // If there was an error, details are reported via error_info.
  virtual bool ResolveKey(const nacl::string& key,
                          nacl::string* full_url,
                          PnaclOptions* pnacl_options,
                          ErrorInfo* error_info) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(JsonManifest);

  // Resolves a URL relative to the manifest base URL
  bool ResolveURL(const nacl::string& relative_url,
                  nacl::string* full_url,
                  ErrorInfo* error_info) const;

  // Checks that |dictionary_| is a valid manifest, according to the schema.
  // Returns true on success, and sets |error_info| to a detailed message
  // if not.
  bool MatchesSchema(ErrorInfo* error_info);

  bool GetKeyUrl(const Json::Value& dictionary,
                 const nacl::string& key,
                 nacl::string* full_url,
                 PnaclOptions* pnacl_options,
                 ErrorInfo* error_info) const;

  bool GetURLFromISADictionary(const Json::Value& dictionary,
                               const nacl::string& parent_key,
                               nacl::string* url,
                               PnaclOptions* pnacl_options,
                               bool* uses_nonsfi_mode,
                               ErrorInfo* error_info) const;

  const pp::URLUtil_Dev* url_util_;
  nacl::string manifest_base_url_;
  nacl::string sandbox_isa_;
  bool nonsfi_enabled_;
  bool pnacl_debug_;

  Json::Value dictionary_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_JSON_MANIFEST_H_
