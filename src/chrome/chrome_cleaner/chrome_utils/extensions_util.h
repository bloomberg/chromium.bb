// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_
#define CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"

namespace chrome_cleaner {

typedef base::RefCountedData<base::Value> RefValue;
typedef uint32_t ContentType;

constexpr int64_t kParseAttemptTimeoutMilliseconds = 10000;

// A registry key that holds some form of policy for |extension_id|.
struct ExtensionPolicyRegistryEntry {
  base::string16 extension_id;
  HKEY hkey;
  base::string16 path;
  base::string16 name;
  ContentType content_type;
  scoped_refptr<RefValue> json;

  ExtensionPolicyRegistryEntry(const base::string16& extension_id,
                               HKEY hkey,
                               const base::string16& path,
                               const base::string16& name,
                               ContentType content_type,
                               scoped_refptr<RefValue>);
  ExtensionPolicyRegistryEntry(ExtensionPolicyRegistryEntry&&);
  ~ExtensionPolicyRegistryEntry();
  ExtensionPolicyRegistryEntry& operator=(ExtensionPolicyRegistryEntry&&);

  DISALLOW_COPY_AND_ASSIGN(ExtensionPolicyRegistryEntry);
};

// A file that holds some form of policy for |extension_id|.
struct ExtensionPolicyFile {
  base::string16 extension_id;
  base::FilePath path;
  scoped_refptr<RefValue> json;

  ExtensionPolicyFile(const base::string16& extension_id,
                      const base::FilePath& path,
                      scoped_refptr<RefValue> json);
  ExtensionPolicyFile(ExtensionPolicyFile&&);
  ~ExtensionPolicyFile();
  ExtensionPolicyFile& operator=(ExtensionPolicyFile&&);

  DISALLOW_COPY_AND_ASSIGN(ExtensionPolicyFile);
};

// Find all extension forcelist registry policies and append to |policies|.
void GetExtensionForcelistRegistryPolicies(
    std::vector<ExtensionPolicyRegistryEntry>* policies);

// Find non-whitelisted extension IDs in external_extensions.json files, which
// are extensions that will be installed by default on new user profiles. Using
// the input |json_parser| to parse JSON, append found extensions to |policies|.
// Signals |done| when all parse tasks have finished.
void GetNonWhitelistedDefaultExtensions(
    JsonParserAPI* json_parser,
    std::vector<ExtensionPolicyFile>* policies,
    base::WaitableEvent* done);

// Find all extensions whose enterprise policy settings contain
// "installation_mode":"force_installed" and append them to |policies|. Uses the
// input |json_parser| to parse JSON, and signals |done| when all parse tasks
// have finished.
void GetExtensionSettingsForceInstalledExtensions(
    JsonParserAPI* json_parser,
    std::vector<ExtensionPolicyRegistryEntry>* policies,
    base::WaitableEvent* done);

// Find master preferences extensions, which are installed to new user profiles,
// and append them to |policies|. Uses the input |json_parser| to parse JSON,
// and signals |done| when all parse tasks have finished.
void GetMasterPreferencesExtensions(JsonParserAPI* json_parser,
                                    std::vector<ExtensionPolicyFile>* policies,
                                    base::WaitableEvent* done);

// Attempts to remove an |extension| installed through the whitelist.
// The extension id will be removed from the |json_result| passed in, so that
// the caller can build up the new JSON value before writing it to the disk.
// On failure returns false and doesn't modify the |json_result|.
bool RemoveDefaultExtension(const ForceInstalledExtension& extension,
                            base::Value* json_result);

// Attempts to remove an extension installed through the forcelist.
// Return True on success.
bool RemoveForcelistPolicyExtension(const ForceInstalledExtension& extension);

// Attempts to remove an extension installed from the policy settings
// The extension id will be removed from the |json_result| passed in so that
// the caller can build up a new JSON value before writing it to the registry.
// On failure returns false and does not modify the |json_result|.
bool RemoveExtensionSettingsPoliciesExtension(
    const ForceInstalledExtension& extension,
    base::Value* json_result);

// Attempts to remove an extension installed through the master preferences.
// The extension id will be removed from the |json_result| passed in so that the
// caller can build up the a new JSON value before writing it to the disk.
// On failure returns false and does not modify the |json_result|.
bool RemoveMasterPreferencesExtension(const ForceInstalledExtension& extension,
                                      base::Value* json_result);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_
