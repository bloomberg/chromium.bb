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
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/third_party_mod/jsoncpp/include/json/reader.h"
#include "native_client/src/third_party/ppapi/cpp/dev/url_util_dev.h"
#include "native_client/src/third_party/ppapi/cpp/var.h"

namespace plugin {

namespace {
// Top-level section name keys
const char* const kProgramKey =     "program";
const char* const kInterpreterKey = "interpreter";
const char* const kFilesKey =       "files";

// ISA Dictionary keys
const char* const kX8632Key =       "x86-32";
const char* const kX8664Key =       "x86-64";
const char* const kArmKey =         "arm";
const char* const kPortableKey =    "portable";
const char* const kUrlKey =         "url";

// Sample manifest file:
// {
//   "program": {
//     "x86-32": {"url": "myprogram_x86-32.nexe"},
//     "x86-64": {"url": "myprogram_x86-64.nexe"},
//     "arm": {"url": "myprogram_arm.nexe"},
//     "portable": {"url": "myprogram.pexe"}
//   },
//   "interpreter": {
//     "x86-32": {"url": "interpreter_x86-32.nexe"},
//     "x86-64": {"url": "interpreter_x86-64.nexe"},
//     "arm": {"url": "interpreter_arm.nexe"}
//   },
//   "files": {
//     "foo.txt": {
//       "portable": {"url": "foo.txt"}
//     },
//     "bar.txt": {
//       "x86-32": {"url": "x86-32/bar.txt"},
//       "portable": {"url": "bar.txt"}
//     }
//   }
// }

// TODO(jvoung): Remove these when we find a better way to store/install them.
const char* const kPnaclLlcKey = "pnacl-llc";
const char* const kPnaclLdKey = "pnacl-ld";

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

// Validates that |dictionary| is a valid ISA dictionary.  An ISA dictionary
// is validated to have keys from within the set of recognized ISAs.  Unknown
// ISAs are allowed, but ignored and warnings are produced. It is also validated
// that it must have an entry to match the ISA specified in |sandbox_isa| or
// have a fallback 'portable' entry if there is no match. Returns true if
// |dictionary| is an ISA to URL map.  Sets |error_string| to something
// descriptive if it fails.
bool IsValidISADictionary(const Json::Value& dictionary,
                          const nacl::string& sandbox_isa,
                          nacl::string* error_string) {
  if (error_string == NULL)
    return false;

  // An ISA to URL dictionary has to be an object.
  if (!dictionary.isObject()) {
    *error_string = " property is not an ISA to URL dictionary";
    return false;
  }
  // The keys to the dictionary have to be valid ISA names.
  Json::Value::Members members = dictionary.getMemberNames();
  for (size_t i = 0; i < members.size(); ++i) {
    // The known ISA values for ISA dictionaries in the manifest.
    static const char* kManifestISAProperties[] = {
      kX8632Key,
      kX8664Key,
      kArmKey,
      kPortableKey
    };
    nacl::string property_name = members[i];
    if (!FindMatchingProperty(property_name,
                              kManifestISAProperties,
                              NACL_ARRAY_SIZE(kManifestISAProperties))) {
      PLUGIN_PRINTF(("IsValidISADictionary: unrecognized ISA '%s'.\n",
                     property_name.c_str()));
    }
    Json::Value url_spec = dictionary[property_name];
    if (!url_spec.isObject()) {
      *error_string = " ISA property '" + property_name +
          "' has non-dictionary value'" + url_spec.toStyledString() + "'.";
      return false;
    }
    // Check properties of the arch-specific 'URL spec' dictionary
    static const char* kManifestUrlSpecProperties[] = {
      kUrlKey
    };
    Json::Value::Members ISA_members = url_spec.getMemberNames();
    for (size_t j = 0; j < ISA_members.size(); ++j) {
      nacl::string ISA_property_name = ISA_members[j];
      if (!FindMatchingProperty(ISA_property_name,
                                kManifestUrlSpecProperties,
                                NACL_ARRAY_SIZE(kManifestUrlSpecProperties))) {
        PLUGIN_PRINTF(("IsValidISADictionary: unrecognized property for '%s'"
                       ": '%s'.",
                       property_name.c_str(), ISA_property_name.c_str()));
      }
    }
    // A "url" key is required for each URL spec.
    if (!url_spec.isMember(kUrlKey)) {
      *error_string = " ISA property '" + property_name +
          "' has no key '" + kUrlKey + "'.";
      return false;
    }
    Json::Value url = url_spec[kUrlKey];
    if (!url.isString()) {
      *error_string = " ISA property '" + property_name +
          "' has non-string value '" + url.toStyledString() +
          "' for key '" + kUrlKey + "'.";
      return false;
    }
  }

  // TODO(elijahtaylor) add ISA resolver here if we expand ISAs to include
  // micro-architectures that can resolve to multiple valid sandboxes.
  bool has_isa = dictionary.isMember(sandbox_isa);
  bool has_portable = dictionary.isMember(kPortableKey);

  if (!has_isa && !has_portable) {
    *error_string =
        " no version given for current arch and no portable version found.";
    return false;
  }

  return true;
}

bool GetURLFromISADictionary(const Json::Value& dictionary,
                             const nacl::string& sandbox_isa,
                             nacl::string* url,
                             nacl::string* error_string,
                             bool* is_portable) {
  if (url == NULL || error_string == NULL)
    return false;

  if (!IsValidISADictionary(dictionary, sandbox_isa, error_string))
    return false;

  bool has_isa = dictionary.isMember(sandbox_isa);
  const char *isa_string = has_isa ? sandbox_isa.c_str() : kPortableKey;
  *is_portable = !has_isa;

  const Json::Value& json_url = dictionary[isa_string][kUrlKey];
  *url = json_url.asString();

  return true;
}


}  // namespace

bool Manifest::Init(const nacl::string& manifest_json, ErrorInfo* error_info) {
  if (error_info == NULL) {
    return false;
  }
  Json::Reader reader;
  if (!reader.parse(manifest_json, dictionary_)) {
    std::string json_error = reader.getFormatedErrorMessages();
    error_info->SetReport(ERROR_MANIFEST_PARSING,
                          "manifest JSON parsing failed: " + json_error);
    return false;
  }
  // Parse has ensured the string was valid JSON.  Check that it matches the
  // manifest schema.
  return MatchesSchema(error_info);
}

bool Manifest::MatchesSchema(ErrorInfo* error_info) {
  pp::Var exception;
  if (error_info == NULL) {
    return false;
  }
  if (!dictionary_.isObject()) {
    error_info->SetReport(
        ERROR_MANIFEST_SCHEMA_VALIDATE,
        "manifest: is not a json dictionary.");
    return false;
  }
  Json::Value::Members members = dictionary_.getMemberNames();
  for (size_t i = 0; i < members.size(); ++i) {
    // The top level dictionary entries valid in the manifest file.
    static const char* kManifestTopLevelProperties[] = { kProgramKey,
                                                         kInterpreterKey,
                                                         kFilesKey };
    nacl::string property_name = members[i];
    if (!FindMatchingProperty(property_name,
                              kManifestTopLevelProperties,
                              NACL_ARRAY_SIZE(kManifestTopLevelProperties))) {
      PLUGIN_PRINTF(("Manifest::MatchesSchema: WARNING: unknown top-level "
                     "section '%s' in manifest.\n", property_name.c_str()));
    }
  }

  nacl::string error_string;

  // A manifest file must have a program section.
  if (!dictionary_.isMember(kProgramKey)) {
    error_info->SetReport(
        ERROR_MANIFEST_SCHEMA_VALIDATE,
        nacl::string("manifest: missing '") + kProgramKey + "' section.");
    return false;
  }

  // Validate the program section.
  if (!IsValidISADictionary(dictionary_[kProgramKey],
                            sandbox_isa_,
                            &error_string)) {
    error_info->SetReport(
        ERROR_MANIFEST_SCHEMA_VALIDATE,
        nacl::string("manifest: ") + kProgramKey + error_string);
    return false;
  }

  // Validate the interpreter section (if given).
  if (dictionary_.isMember(kInterpreterKey)) {
    if (!IsValidISADictionary(dictionary_[kProgramKey],
                              sandbox_isa_,
                              &error_string)) {
      error_info->SetReport(
          ERROR_MANIFEST_SCHEMA_VALIDATE,
          nacl::string("manifest: ") + kInterpreterKey + error_string);
      return false;
    }
  }

  // Validate the file dictionary (if given).
  if (dictionary_.isMember(kFilesKey)) {
    const Json::Value& files = dictionary_[kFilesKey];
    if (!files.isObject()) {
      error_info->SetReport(
          ERROR_MANIFEST_SCHEMA_VALIDATE,
          nacl::string("manifest: '") + kFilesKey + "' is not a dictionary.");
    }
    Json::Value::Members members = files.getMemberNames();
    for (size_t i = 0; i < members.size(); ++i) {
      nacl::string property_name = members[i];
      if (!IsValidISADictionary(files[property_name],
                                sandbox_isa_,
                                &error_string)) {
        error_info->SetReport(
            ERROR_MANIFEST_SCHEMA_VALIDATE,
            nacl::string("manifest: file '") + property_name + "'" +
            error_string);
        return false;
      }
    }
  }

  return true;
}

bool Manifest::ResolveURL(const nacl::string& relative_url,
                          nacl::string* full_url,
                          ErrorInfo* error_info) {
  // The contents of the manifest are resolved relative to the manifest URL.
  CHECK(url_util_ != NULL);
  pp::Var resolved_url =
      url_util_->ResolveRelativeToURL(pp::Var(manifest_base_url_),
                                      relative_url);
  if (!resolved_url.is_string()) {
    error_info->SetReport(
        ERROR_MANIFEST_RESOLVE_URL,
        "could not resolve url '" + relative_url +
        "' relative to manifest base url '" + manifest_base_url_.c_str() +
        "'.");
    return false;
  }
  *full_url = resolved_url.AsString();
  return true;
}

bool Manifest::GetProgramURL(nacl::string* full_url,
                             ErrorInfo* error_info,
                             bool* is_portable) {
  if (full_url == NULL || error_info == NULL)
    return false;

  Json::Value program = dictionary_[kProgramKey];

  nacl::string nexe_url;
  nacl::string error_string;

  if (!GetURLFromISADictionary(program,
                               sandbox_isa_,
                               &nexe_url,
                               &error_string,
                               is_portable)) {
    error_info->SetReport(ERROR_MANIFEST_GET_NEXE_URL,
                          nacl::string("program:") + sandbox_isa_ +
                          error_string);
    return false;
  }

  return ResolveURL(nexe_url, full_url, error_info);
}

// TODO(jvoung): We won't need these if we figure out how to install llc and ld.
bool Manifest::GetLLCURL(nacl::string* full_url, ErrorInfo* error_info) {
  if (full_url == NULL || error_info == NULL)
    return false;

  Json::Value pnacl_llc = dictionary_[kPnaclLlcKey];

  nacl::string nexe_url;
  nacl::string error_string;
  bool is_portable;
  if (!GetURLFromISADictionary(pnacl_llc,
                               sandbox_isa_,
                               &nexe_url,
                               &error_string,
                               &is_portable)) {
    error_info->SetReport(ERROR_MANIFEST_GET_NEXE_URL,
                          nacl::string(kPnaclLlcKey) + ":" + sandbox_isa_ +
                          error_string);
    return false;
  }

  if (is_portable) {
    // Bootstrap problem -- we need this to translate portable programs!
    error_info->SetReport(ERROR_MANIFEST_GET_NEXE_URL,
                          nacl::string(kPnaclLlcKey) +
                          " must be pre-translated for " + sandbox_isa_ + "!");
    return false;
  }

  return ResolveURL(nexe_url, full_url, error_info);
}

bool Manifest::GetLDURL(nacl::string* full_url, ErrorInfo* error_info) {
  if (full_url == NULL || error_info == NULL)
    return false;

  Json::Value pnacl_ld = dictionary_[kPnaclLdKey];

  nacl::string nexe_url;
  nacl::string error_string;
  bool is_portable;
  if (!GetURLFromISADictionary(pnacl_ld,
                               sandbox_isa_,
                               &nexe_url,
                               &error_string,
                               &is_portable)) {
    error_info->SetReport(ERROR_MANIFEST_GET_NEXE_URL,
                          nacl::string(kPnaclLdKey) + ":" + sandbox_isa_ +
                          error_string);
    return false;
  }

  if (is_portable) {
    // Bootstrap problem -- we need this to translate portable programs!
    error_info->SetReport(ERROR_MANIFEST_GET_NEXE_URL,
                          nacl::string(kPnaclLdKey) +
                          " must be pre-translated for " + sandbox_isa_ + "!");
    return false;
  }

  return ResolveURL(nexe_url, full_url, error_info);
}

}  // namespace plugin
