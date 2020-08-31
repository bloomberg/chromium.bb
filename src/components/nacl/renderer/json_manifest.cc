// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/json_manifest.h"

#include <stddef.h>
#include <stdint.h>

#include <set>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "url/gurl.h"

namespace nacl {

namespace {
// Top-level section name keys
const char kProgramKey[] = "program";
const char kInterpreterKey[] = "interpreter";
const char kFilesKey[] = "files";

// ISA Dictionary keys
const char kX8632Key[] = "x86-32";
const char kX8632NonSFIKey[] = "x86-32-nonsfi";
const char kX8664Key[] = "x86-64";
const char kX8664NonSFIKey[] = "x86-64-nonsfi";
const char kArmKey[] = "arm";
const char kArmNonSFIKey[] = "arm-nonsfi";
const char kPortableKey[] = "portable";

// Url Resolution keys
const char kPnaclDebugKey[] = "pnacl-debug";
const char kPnaclTranslateKey[] = "pnacl-translate";
const char kUrlKey[] = "url";

// PNaCl keys
const char kOptLevelKey[] = "optlevel";

// Sample NaCl manifest file:
// {
//   "program": {
//     "x86-32": {"url": "myprogram_x86-32.nexe"},
//     "x86-64": {"url": "myprogram_x86-64.nexe"},
//     "arm": {"url": "myprogram_arm.nexe"}
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
//     },
//     "libfoo.so": {
//       "x86-64" : { "url": "..." }
//     }
//   }
// }

// Sample PNaCl manifest file:
// {
//   "program": {
//     "portable": {
//       "pnacl-translate": {
//         "url": "myprogram.pexe"
//       },
//       "pnacl-debug": {
//         "url": "myprogram.debug.pexe",
//         "opt_level": 0
//       }
//     }
//   },
//   "files": {
//     "foo.txt": {
//       "portable": {"url": "foo.txt"}
//     },
//     "bar.txt": {
//       "portable": {"url": "bar.txt"}
//     }
//   }
// }

// Returns the key for the architecture in non-SFI mode.
std::string GetNonSFIKey(const std::string& sandbox_isa) {
  return sandbox_isa + "-nonsfi";
}

// Looks up |property_name| in the vector |valid_names| with length
// |valid_name_count|.  Returns true if |property_name| is found.
bool FindMatchingProperty(const std::string& property_name,
                          const char* const* valid_names,
                          size_t valid_name_count) {
  for (size_t i = 0; i < valid_name_count; ++i) {
    if (property_name == valid_names[i]) {
      return true;
    }
  }
  return false;
}

// Return true if this is a valid dictionary.  Having only keys present in
// |valid_keys| and having at least the keys in |required_keys|.
// Error messages will be placed in |error_string|, given that the dictionary
// was the property value of |container_key|.
// E.g., "container_key" : dictionary
bool IsValidDictionary(const base::DictionaryValue& dictionary,
                       const std::string& container_key,
                       const std::string& parent_key,
                       const char* const* valid_keys,
                       size_t valid_key_count,
                       const char* const* required_keys,
                       size_t required_key_count,
                       std::string* error_string) {
  // Check for unknown dictionary members.
  for (base::DictionaryValue::Iterator it(dictionary); !it.IsAtEnd();
       it.Advance()) {
    const std::string& property_name = it.key();
    if (!FindMatchingProperty(property_name,
                              valid_keys,
                              valid_key_count)) {
      // For forward compatibility, we do not prohibit other keys being in
      // the dictionary.
      VLOG(1) << "WARNING: '" << parent_key << "' property '"
              << container_key << "' has unknown key '"
              << property_name << "'.";
    }
  }
  // Check for required members.
  for (size_t i = 0; i < required_key_count; ++i) {
    if (!dictionary.HasKey(required_keys[i])) {
      std::stringstream error_stream;
      error_stream << parent_key << " property '" << container_key
                   << "' does not have required key: '"
                   << required_keys[i] << "'.";
      *error_string = error_stream.str();
      return false;
    }
  }
  return true;
}

// Validate a "url" dictionary assuming it was resolved from container_key.
// E.g., "container_key" : { "url": "foo.txt" }
bool IsValidUrlSpec(const base::Value& url_spec,
                    const std::string& container_key,
                    const std::string& parent_key,
                    const std::string& sandbox_isa,
                    std::string* error_string) {
  const base::DictionaryValue* url_dict = nullptr;
  if (!url_spec.GetAsDictionary(&url_dict)) {
    std::stringstream error_stream;
    error_stream << parent_key << " property '" << container_key
                 << "' is non-dictionary value '" << url_spec << "'.";
    *error_string = error_stream.str();
    return false;
  }
  static constexpr const char* kManifestUrlSpecRequired[] = {kUrlKey};
  const char* const* url_spec_plus_optional;
  size_t url_spec_plus_optional_length;
  if (sandbox_isa == kPortableKey) {
    static constexpr const char* kPnaclUrlSpecPlusOptional[] = {
        kUrlKey, kOptLevelKey,
    };
    url_spec_plus_optional = kPnaclUrlSpecPlusOptional;
    url_spec_plus_optional_length = base::size(kPnaclUrlSpecPlusOptional);
  } else {
    // URL specifications must not contain "pnacl-translate" keys.
    // This prohibits NaCl clients from invoking PNaCl.
    if (url_dict->HasKey(kPnaclTranslateKey)) {
      std::stringstream error_stream;
      error_stream << "PNaCl-like NMF with application/x-nacl mimetype instead "
                   << "of x-pnacl mimetype (has " << kPnaclTranslateKey << ").";
      *error_string = error_stream.str();
      return false;
    }
    url_spec_plus_optional = kManifestUrlSpecRequired;
    url_spec_plus_optional_length = base::size(kManifestUrlSpecRequired);
  }
  if (!IsValidDictionary(*url_dict, container_key, parent_key,
                         url_spec_plus_optional, url_spec_plus_optional_length,
                         kManifestUrlSpecRequired,
                         base::size(kManifestUrlSpecRequired), error_string)) {
    return false;
  }
  // Verify the correct types of the fields if they exist.
  const base::Value* url = nullptr;
  // URL was already verified above by IsValidDictionary to be required.
  url_dict->GetWithoutPathExpansion(kUrlKey, &url);
  DCHECK(url);
  if (!url->is_string()) {
    std::stringstream error_stream;
    error_stream << parent_key << " property '" << container_key
                 << "' has non-string value '" << *url << "' for key '"
                 << kUrlKey << "'.";
    *error_string = error_stream.str();
    return false;
  }
  if (url_dict->HasKey(kOptLevelKey)) {
    const base::Value* opt_level = nullptr;
    url_dict->GetWithoutPathExpansion(kOptLevelKey, &opt_level);
    DCHECK(opt_level);
    if (!opt_level->is_int()) {
      std::stringstream error_stream;
      error_stream << parent_key << " property '" << container_key
                   << "' has non-numeric value '" << *opt_level << "' for key '"
                   << kOptLevelKey << "'.";
      *error_string = error_stream.str();
      return false;
    }
  }
  return true;
}

// Validate a "pnacl-translate" or "pnacl-debug" dictionary, assuming
// it was resolved from container_key.
// E.g., "container_key" : { "pnacl-translate" : URLSpec }
bool IsValidPnaclTranslateSpec(const base::Value& pnacl_spec,
                               const std::string& container_key,
                               const std::string& parent_key,
                               const std::string& sandbox_isa,
                               std::string* error_string) {
  static const char* kManifestPnaclSpecValid[] = {
    kPnaclDebugKey,
    kPnaclTranslateKey
  };
  static const char* kManifestPnaclSpecRequired[] = { kPnaclTranslateKey };
  const base::DictionaryValue* pnacl_dict = nullptr;
  if (!pnacl_spec.GetAsDictionary(&pnacl_dict)) {
    std::stringstream error_stream;
    error_stream << parent_key << " property '" << container_key
                 << "' is non-dictionary value '" << pnacl_spec << "'.";
    *error_string = error_stream.str();
    return false;
  }

  if (!IsValidDictionary(
          *pnacl_dict, container_key, parent_key, kManifestPnaclSpecValid,
          base::size(kManifestPnaclSpecValid), kManifestPnaclSpecRequired,
          base::size(kManifestPnaclSpecRequired), error_string)) {
    return false;
  }
  // kPnaclTranslateKey checked to be required above.
  const base::Value* url_spec = nullptr;
  pnacl_dict->GetWithoutPathExpansion(kPnaclTranslateKey, &url_spec);
  DCHECK(url_spec);
  return IsValidUrlSpec(*url_spec, kPnaclTranslateKey, container_key,
                        sandbox_isa, error_string);
}

// Validates that parent_dictionary[parent_key] is a valid ISA dictionary.
// An ISA dictionary is validated to have keys from within the set of
// recognized ISAs.  Unknown ISAs are allowed, but ignored and warnings
// are produced. It is also validated that it must have an entry to match the
// ISA specified in |sandbox_isa| or have a fallback 'portable' entry if
// there is no match. Returns true if parent_dictionary[parent_key] is an
// ISA to URL map.  Sets |error_info| to something descriptive if it fails.
bool IsValidISADictionary(const base::DictionaryValue& parent_dictionary,
                          const std::string& parent_key,
                          const std::string& sandbox_isa,
                          bool must_find_matching_entry,
                          bool nonsfi_enabled,
                          JsonManifest::ErrorInfo* error_info) {
  const base::DictionaryValue* dictionary = nullptr;
  if (!parent_dictionary.GetDictionaryWithoutPathExpansion(parent_key,
                                                           &dictionary)) {
    error_info->error = PP_NACL_ERROR_MANIFEST_SCHEMA_VALIDATE;
    error_info->string = std::string("manifest: ") + parent_key +
                         " property is not an ISA to URL dictionary";
    return false;
  }
  // Build the set of reserved ISA dictionary keys.
  const char** isaProperties;
  size_t isaPropertiesLength;
  if (sandbox_isa == kPortableKey) {
    // The known values for PNaCl ISA dictionaries in the manifest.
    static const char* kPnaclManifestISAProperties[] = {
      kPortableKey
    };
    isaProperties = kPnaclManifestISAProperties;
    isaPropertiesLength = base::size(kPnaclManifestISAProperties);
  } else {
    // The known values for NaCl ISA dictionaries in the manifest.
    static const char* kNaClManifestISAProperties[] = {
      kX8632Key,
      kX8632NonSFIKey,
      kX8664Key,
      kX8664NonSFIKey,
      kArmKey,
      kArmNonSFIKey,
      // "portable" is here to allow checking that, if present, it can
      // only refer to an URL, such as for a data file, and not to
      // "pnacl-translate", which would cause the creation of a nexe.
      kPortableKey
    };
    isaProperties = kNaClManifestISAProperties;
    isaPropertiesLength = base::size(kNaClManifestISAProperties);
  }
  // Check that entries in the dictionary are structurally correct.
  for (base::DictionaryValue::Iterator it(*dictionary); !it.IsAtEnd();
       it.Advance()) {
    const std::string& property_name = it.key();
    const base::Value& property_value = it.value();
    std::string error_string;
    if (FindMatchingProperty(property_name,
                             isaProperties,
                             isaPropertiesLength)) {
      // For NaCl, arch entries can only be
      //     "arch/portable" : URLSpec
      // For PNaCl arch in "program" dictionary entries can be
      //     "portable" : { "pnacl-translate": URLSpec }
      //  or "portable" : { "pnacl-debug": URLSpec }
      // For PNaCl arch elsewhere, dictionary entries can only be
      //     "portable" : URLSpec
      if ((sandbox_isa != kPortableKey &&
           !IsValidUrlSpec(property_value, property_name, parent_key,
                           sandbox_isa, &error_string)) ||
          (sandbox_isa == kPortableKey &&
           parent_key == kProgramKey &&
           !IsValidPnaclTranslateSpec(property_value, property_name, parent_key,
                                      sandbox_isa, &error_string)) ||
          (sandbox_isa == kPortableKey &&
           parent_key != kProgramKey &&
           !IsValidUrlSpec(property_value, property_name, parent_key,
                           sandbox_isa, &error_string))) {
        error_info->error = PP_NACL_ERROR_MANIFEST_SCHEMA_VALIDATE;
        error_info->string = "manifest: " + error_string;
        return false;
      }
    } else {
      // For forward compatibility, we do not prohibit other keys being in
      // the dictionary, as they may be architectures supported in later
      // versions.  However, the value of these entries must be an URLSpec.
      VLOG(1) << "IsValidISADictionary: unrecognized key '"
              << property_name << "'.";
      if (!IsValidUrlSpec(property_value, property_name, parent_key,
                          sandbox_isa, &error_string)) {
        error_info->error = PP_NACL_ERROR_MANIFEST_SCHEMA_VALIDATE;
        error_info->string = "manifest: " + error_string;
        return false;
      }
    }
  }

  if (sandbox_isa == kPortableKey) {
    if (!dictionary->HasKey(kPortableKey)) {
      error_info->error = PP_NACL_ERROR_MANIFEST_PROGRAM_MISSING_ARCH;
      error_info->string = "manifest: no version of " + parent_key +
                           " given for portable.";
      return false;
    }
  } else if (must_find_matching_entry) {
    // TODO(elijahtaylor) add ISA resolver here if we expand ISAs to include
    // micro-architectures that can resolve to multiple valid sandboxes.
    bool has_isa = dictionary->HasKey(sandbox_isa);
    bool has_nonsfi_isa =
        nonsfi_enabled && dictionary->HasKey(GetNonSFIKey(sandbox_isa));
    bool has_portable = dictionary->HasKey(kPortableKey);

    if (!has_isa && !has_nonsfi_isa && !has_portable) {
      error_info->error = PP_NACL_ERROR_MANIFEST_PROGRAM_MISSING_ARCH;
      error_info->string = "manifest: no version of " + parent_key +
          " given for current arch and no portable version found.";
      return false;
    }
  }
  return true;
}

void GrabUrlAndPnaclOptions(const base::DictionaryValue& url_spec,
                            std::string* url,
                            PP_PNaClOptions* pnacl_options) {
  // url_spec should have been validated as a first pass.
  bool get_url_success = url_spec.GetStringWithoutPathExpansion(kUrlKey, url);
  DCHECK(get_url_success);
  pnacl_options->translate = PP_TRUE;
  if (url_spec.HasKey(kOptLevelKey)) {
    int32_t opt_raw = 0;
    bool get_opt_success =
        url_spec.GetIntegerWithoutPathExpansion(kOptLevelKey, &opt_raw);
    DCHECK(get_opt_success);
    // Currently only allow 0 or 2, since that is what we test.
    if (opt_raw <= 0)
      pnacl_options->opt_level = 0;
    else
      pnacl_options->opt_level = 2;
  }
}

}  // namespace

JsonManifest::JsonManifest(const std::string& manifest_base_url,
                           const std::string& sandbox_isa,
                           bool nonsfi_enabled,
                           bool pnacl_debug)
    : manifest_base_url_(manifest_base_url),
      sandbox_isa_(sandbox_isa),
      nonsfi_enabled_(nonsfi_enabled),
      pnacl_debug_(pnacl_debug) { }

JsonManifest::~JsonManifest() {}

bool JsonManifest::Init(const std::string& manifest_json_data,
                        ErrorInfo* error_info) {
  CHECK(error_info);

  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(manifest_json_data);
  if (!parsed_json.value) {
    error_info->error = PP_NACL_ERROR_MANIFEST_PARSING;
    error_info->string = std::string("manifest JSON parsing failed: ") +
                         parsed_json.error_message;
    return false;
  }
  std::unique_ptr<base::Value> json_data =
      base::Value::ToUniquePtrValue(std::move(*parsed_json.value));
  // Ensure it's actually a dictionary before capturing as dictionary_.
  base::DictionaryValue* json_dict = nullptr;
  if (!json_data->GetAsDictionary(&json_dict)) {
    error_info->error = PP_NACL_ERROR_MANIFEST_SCHEMA_VALIDATE;
    error_info->string = "manifest: is not a json dictionary.";
    return false;
  }
  // Can't quite use json_data.swap(dictionary_), since the types are different
  // so do this kludgy manual swap.
  DCHECK(json_dict == json_data.get());
  dictionary_.reset(static_cast<base::DictionaryValue*>(json_data.release()));
  // Parse has ensured the string was valid JSON.  Check that it matches the
  // manifest schema.
  return MatchesSchema(error_info);
}

bool JsonManifest::GetProgramURL(std::string* full_url,
                                 PP_PNaClOptions* pnacl_options,
                                 bool* uses_nonsfi_mode,
                                 ErrorInfo* error_info) const {
  if (!full_url)
    return false;
  CHECK(pnacl_options);
  CHECK(uses_nonsfi_mode);
  CHECK(error_info);

  std::string nexe_url;
  if (!GetURLFromISADictionary(*dictionary_, kProgramKey, &nexe_url,
                               pnacl_options, uses_nonsfi_mode, error_info)) {
    return false;
  }

  // The contents of the manifest are resolved relative to the manifest URL.
  GURL base_gurl(manifest_base_url_);
  if (!base_gurl.is_valid())
    return false;

  GURL resolved_gurl = base_gurl.Resolve(nexe_url);
  if (!resolved_gurl.is_valid()) {
    error_info->error = PP_NACL_ERROR_MANIFEST_RESOLVE_URL;
    error_info->string =
        "could not resolve url '" + nexe_url +
        "' relative to manifest base url '" + manifest_base_url_.c_str() +
        "'.";
    return false;
  }
  *full_url = resolved_gurl.possibly_invalid_spec();
  return true;
}

void JsonManifest::GetPrefetchableFiles(
    std::vector<NaClResourcePrefetchRequest>* out_files) const {
  const base::DictionaryValue* files_dict;
  if (!dictionary_->GetDictionaryWithoutPathExpansion(kFilesKey, &files_dict))
    return;

  for (base::DictionaryValue::Iterator it(*files_dict); !it.IsAtEnd();
       it.Advance()) {
    std::string full_url;
    PP_PNaClOptions unused_pnacl_options;  // pnacl does not support "files".
    const std::string& file_key = it.key();
    // We skip invalid entries in "files".
    if (GetKeyUrl(*files_dict, file_key, &full_url, &unused_pnacl_options)) {
      if (GURL(full_url).SchemeIs("chrome-extension"))
        out_files->push_back(NaClResourcePrefetchRequest(file_key, full_url));
    }
  }
}

bool JsonManifest::ResolveKey(const std::string& key,
                              std::string* full_url,
                              PP_PNaClOptions* pnacl_options) const {
  if (full_url == NULL || pnacl_options == NULL)
    return false;

  const base::DictionaryValue* files_dict;
  if (!dictionary_->GetDictionaryWithoutPathExpansion(kFilesKey, &files_dict)) {
    VLOG(1) << "ResolveKey failed: no \"files\" dictionary";
    return false;
  }

  if (!files_dict->HasKey(key)) {
    VLOG(1) << "ResolveKey failed: no such \"files\" entry: " << key;
    return false;
  }
  return GetKeyUrl(*files_dict, key, full_url, pnacl_options);
}

bool JsonManifest::MatchesSchema(ErrorInfo* error_info) {
  // The top level dictionary entries valid in the manifest file.
  static const char* kManifestTopLevelProperties[] = {
      kProgramKey, kInterpreterKey, kFilesKey};
  for (base::DictionaryValue::Iterator it(*dictionary_); !it.IsAtEnd();
       it.Advance()) {
    const std::string& property_name = it.key();
    if (!FindMatchingProperty(property_name, kManifestTopLevelProperties,
                              base::size(kManifestTopLevelProperties))) {
      VLOG(1) << "JsonManifest::MatchesSchema: WARNING: unknown top-level "
              << "section '" << property_name << "' in manifest.";
    }
  }

  // A manifest file must have a program section.
  if (!dictionary_->HasKey(kProgramKey)) {
    error_info->error = PP_NACL_ERROR_MANIFEST_SCHEMA_VALIDATE;
    error_info->string = std::string("manifest: missing '") + kProgramKey +
                         "' section.";
    return false;
  }

  // Validate the program section.
  // There must be a matching (portable or sandbox_isa_) entry for program for
  // NaCl.
  if (!IsValidISADictionary(*dictionary_, kProgramKey, sandbox_isa_, true,
                            nonsfi_enabled_, error_info)) {
    return false;
  }

  // Validate the interpreter section (if given).
  // There must be a matching (portable or sandbox_isa_) entry for interpreter
  // for NaCl.
  if (dictionary_->HasKey(kInterpreterKey)) {
    if (!IsValidISADictionary(*dictionary_, kInterpreterKey, sandbox_isa_, true,
                              nonsfi_enabled_, error_info)) {
      return false;
    }
  }

  // Validate the file dictionary (if given).
  // The "files" key does not require a matching (portable or sandbox_isa_)
  // entry at schema validation time for NaCl.  This allows manifests to
  // specify resources that are only loaded for a particular sandbox_isa.
  if (dictionary_->HasKey(kFilesKey)) {
    const base::DictionaryValue* files_dictionary = nullptr;
    if (!dictionary_->GetDictionaryWithoutPathExpansion(kFilesKey,
                                                        &files_dictionary)) {
      error_info->error = PP_NACL_ERROR_MANIFEST_SCHEMA_VALIDATE;
      error_info->string = std::string("manifest: '") + kFilesKey +
                           "' is not a dictionary.";
    }
    for (base::DictionaryValue::Iterator it(*files_dictionary); !it.IsAtEnd();
         it.Advance()) {
      const std::string& file_name = it.key();
      if (!IsValidISADictionary(*files_dictionary, file_name, sandbox_isa_,
                                false, nonsfi_enabled_, error_info)) {
        return false;
      }
    }
  }
  return true;
}

bool JsonManifest::GetKeyUrl(const base::DictionaryValue& dictionary,
                             const std::string& key,
                             std::string* full_url,
                             PP_PNaClOptions* pnacl_options) const {
  DCHECK(full_url && pnacl_options);
  if (!dictionary.HasKey(key)) {
    VLOG(1) << "GetKeyUrl failed: file " << key << " not found in manifest.";
    return false;
  }
  std::string relative_url;
  bool uses_nonsfi_mode;
  ErrorInfo ignored_error_info;
  if (!GetURLFromISADictionary(dictionary, key, &relative_url, pnacl_options,
                               &uses_nonsfi_mode, &ignored_error_info))
    return false;

  // The contents of the manifest are resolved relative to the manifest URL.
  GURL base_gurl(manifest_base_url_);
  if (!base_gurl.is_valid())
    return false;
  GURL resolved_gurl = base_gurl.Resolve(relative_url);
  if (!resolved_gurl.is_valid())
    return false;
  *full_url = resolved_gurl.possibly_invalid_spec();
  return true;
}

bool JsonManifest::GetURLFromISADictionary(
    const base::DictionaryValue& parent_dictionary,
    const std::string& parent_key,
    std::string* url,
    PP_PNaClOptions* pnacl_options,
    bool* uses_nonsfi_mode,
    ErrorInfo* error_info) const {
  DCHECK(url && pnacl_options && error_info);

  const base::DictionaryValue* dictionary = nullptr;
  if (!parent_dictionary.GetDictionaryWithoutPathExpansion(parent_key,
                                                           &dictionary)) {
    error_info->error = PP_NACL_ERROR_MANIFEST_RESOLVE_URL;
    error_info->string = std::string("GetURLFromISADictionary failed: ") +
                         parent_key + "'s value is not a json dictionary.";
    return false;
  }

  // When the application actually requests a resolved URL, we must have
  // a matching entry (sandbox_isa_ or portable) for NaCl.
  ErrorInfo ignored_error_info;
  if (!IsValidISADictionary(parent_dictionary, parent_key, sandbox_isa_, true,
                            nonsfi_enabled_, &ignored_error_info)) {
    error_info->error = PP_NACL_ERROR_MANIFEST_RESOLVE_URL;
    error_info->string = "architecture " + sandbox_isa_ +
                         " is not found for file " + parent_key;
    return false;
  }

  // The call to IsValidISADictionary() above guarantees that either
  // sandbox_isa_, its nonsfi mode, or kPortableKey is present in the
  // dictionary.
  *uses_nonsfi_mode = false;
  std::string chosen_isa;
  if (sandbox_isa_ == kPortableKey) {
    chosen_isa = kPortableKey;
  } else {
    std::string nonsfi_isa = GetNonSFIKey(sandbox_isa_);
    if (nonsfi_enabled_ && dictionary->HasKey(nonsfi_isa)) {
      chosen_isa = nonsfi_isa;
      *uses_nonsfi_mode = true;
    } else if (dictionary->HasKey(sandbox_isa_)) {
      chosen_isa = sandbox_isa_;
    } else if (dictionary->HasKey(kPortableKey)) {
      chosen_isa = kPortableKey;
    } else {
      // Should not reach here, because the earlier IsValidISADictionary()
      // call checked that the manifest covers the current architecture.
      NOTREACHED();
      return false;
    }
  }

  const base::DictionaryValue* isa_spec = nullptr;
  if (!dictionary->GetDictionaryWithoutPathExpansion(chosen_isa, &isa_spec)) {
    error_info->error = PP_NACL_ERROR_MANIFEST_RESOLVE_URL;
    error_info->string = std::string("GetURLFromISADictionary failed: ") +
                         chosen_isa + "'s value is not a json dictionary.";
    return false;
  }
  // If the PNaCl debug flag is turned on, look for pnacl-debug entries first.
  // If found, mark that it is a debug URL. Otherwise, fall back to
  // checking for pnacl-translate URLs, etc. and don't mark it as a debug URL.
  if (pnacl_debug_ && isa_spec->HasKey(kPnaclDebugKey)) {
    const base::DictionaryValue* pnacl_dict = nullptr;
    if (!isa_spec->GetDictionaryWithoutPathExpansion(kPnaclDebugKey,
                                                     &pnacl_dict)) {
      error_info->error = PP_NACL_ERROR_MANIFEST_RESOLVE_URL;
      error_info->string = std::string("GetURLFromISADictionary failed: ") +
                           kPnaclDebugKey +
                           "'s value is not a json dictionary.";
      return false;
    }
    GrabUrlAndPnaclOptions(*pnacl_dict, url, pnacl_options);
    pnacl_options->is_debug = PP_TRUE;
  } else if (isa_spec->HasKey(kPnaclTranslateKey)) {
    const base::DictionaryValue* pnacl_dict = nullptr;
    if (!isa_spec->GetDictionaryWithoutPathExpansion(kPnaclTranslateKey,
                                                     &pnacl_dict)) {
      error_info->error = PP_NACL_ERROR_MANIFEST_RESOLVE_URL;
      error_info->string = std::string("GetURLFromISADictionary failed: ") +
                           kPnaclTranslateKey +
                           "'s value is not a json dictionary.";
      return false;
    }
    GrabUrlAndPnaclOptions(*pnacl_dict, url, pnacl_options);
  } else {
    // The native NaCl case.
    if (!isa_spec->GetStringWithoutPathExpansion(kUrlKey, url)) {
      error_info->error = PP_NACL_ERROR_MANIFEST_RESOLVE_URL;
      error_info->string = std::string("GetURLFromISADictionary failed: ") +
                           kUrlKey + "'s value is not a string.";
      return false;
    }
    pnacl_options->translate = PP_FALSE;
  }

  return true;
}

}  // namespace nacl
