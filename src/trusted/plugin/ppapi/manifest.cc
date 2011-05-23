/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/manifest.h"

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/third_party_mod/jsoncpp/include/json/reader.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/var.h"

namespace plugin {

namespace {
// The key used to find the dictionary nexe URLs in the manifest file.
const char* const kNexesKey = "nexes";

// Looks up |property_name| in the vector |valid_names| with length
// |valid_name_count|.  Returns true if |property_name| is found.
bool FindMatchingProperty(nacl::string property_name,
                          const char** valid_names,
                          size_t valid_name_count) {
  for (size_t i = 0; i < valid_name_count; ++i) {
    if (property_name == valid_names[i]) {
      return true;
    }
  }
  return false;
}

// Validates that |dictionary| is a valid ISA to URL map.  An ISA to URL map
// is validated to have keys from within the set of recognized ISAs.  It is
// also validated that it must have an entry to match the ISA specified in
// |sandbox_isa|.  Returns true if |dictionary| is an ISA to URL map.  Sets
// |error_string| to something descriptive if it fails.
bool ValidateISAToURL(const Json::Value& dictionary,
                      const nacl::string& sandbox_isa,
                      nacl::string* error_string) {
  // An ISA to URL dictionary has to be an object.
  if (!dictionary.isObject()) {
    *error_string = "\" property is not an ISA to URL dictionary";
    return false;
  }
  // The keys to the dictionary have to be valid ISA names.
  Json::Value::Members members = dictionary.getMemberNames();
  for (size_t i = 0; i < members.size(); ++i) {
    // The entries valid under in name to ISA dictionaries in the manifest.
    static const char* kManifestNexesISAProperties[] = {
      "x86-32",
      "x86-64",
      "arm"
    };
    nacl::string property_name = members[i];
    if (!FindMatchingProperty(property_name,
                              kManifestNexesISAProperties,
                              NACL_ARRAY_SIZE(kManifestNexesISAProperties))) {
      *error_string =
          " has an unrecognized ISA property \"" + property_name + "\".";
      return false;
    }
    Json::Value url = dictionary[members[i]];
    if (!url.isString()) {
      *error_string = " ISA property \"" + property_name +
          "\" has non-string value \"" + url.toStyledString() + "\".";
      return false;
    }
  }
  // And the dictionary has to have an entry for the current sandbox ISA.
  // TODO(sehr): change this when PNaCl arrives.
  if (!dictionary[sandbox_isa].isString()) {
    *error_string =
        " property for \"" + sandbox_isa + "\" is invalid or missing.";
    return false;
  }
  return true;
}

}  // namespace

bool Manifest::Init(const nacl::string& manifest_json,
                    nacl::string* error_string) {
  if (error_string == NULL) {
    return false;
  }
  Json::Reader reader;
  if (!reader.parse(manifest_json, dictionary_)) {
    *error_string = "manifest JSON parsing failed.";
    return false;
  }
  // Parse has ensured the string was valid JSON.  Check that it matches the
  // manifest schema.
  return MatchesSchema(error_string);
}

bool Manifest::MatchesSchema(nacl::string* error_string) {
  pp::Var exception;
  if (error_string == NULL) {
    return false;
  }
  // Check that there are no unrecognized top-level elements of the manifest.
  // TODO(sehr,polina): replace this when the manifest spec is extended.
  if (!dictionary_.isObject()) {
    *error_string = "manifest: is not a json dictionary.";
    return false;
  }
  Json::Value::Members members = dictionary_.getMemberNames();
  for (size_t i = 0; i < members.size(); ++i) {
    // The top level dictionary entries valid in the manifest file.
    static const char* kManifestTopLevelProperties[] = { kNexesKey };
    nacl::string property_name = members[i];
    if (!FindMatchingProperty(property_name,
                              kManifestTopLevelProperties,
                              NACL_ARRAY_SIZE(kManifestTopLevelProperties))) {
      *error_string =
          nacl::string("manifest: has unrecognized top-level property \"") +
          property_name + "\".";
      return false;
    }
  }
  // Check that the manifest file has the "nexes" property, and that property
  // is a dictionary from sandbox ISA to string URL.
  if (!ValidateISAToURL(dictionary_[kNexesKey], sandbox_isa_, error_string)) {
    *error_string = nacl::string("manifest: ") + kNexesKey + *error_string;
    return false;
  }
  // TODO(sehr,polina): Add other manifest schema checks here.
  return true;
}

bool Manifest::GetNexeURL(nacl::string* full_url, nacl::string* error_string) {
  if (full_url == NULL || error_string == NULL)
    return false;
  if (!dictionary_.isObject()) {
    *error_string = "manifest is not an object.";
    return false;
  }
  nacl::string nexe_url = dictionary_[kNexesKey][sandbox_isa_].asString();
  // The contents of the manifest are resolved relative to the manifest URL.
  CHECK(url_util_ != NULL);
  pp::Var resolved_url =
      url_util_->ResolveRelativeToURL(pp::Var(manifest_base_url_), nexe_url);
  if (!resolved_url.is_string()) {
    *error_string =
        "could not resolve nexe url \"" + nexe_url +
        "\" relative to manifest base url \"" + manifest_base_url_.c_str() +
        "\".";
    return false;
  }
  *full_url = resolved_url.AsString();

  return true;
}

}  // namespace plugin
