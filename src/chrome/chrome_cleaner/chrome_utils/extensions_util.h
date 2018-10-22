// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_
#define CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/chrome_cleaner/json_parser/json_parser_api.h"
#include "chrome/chrome_cleaner/os/registry_util.h"

namespace chrome_cleaner {

// A registry key that holds some form of policy for |extension_id|.
struct ExtensionPolicyRegistryEntry {
  base::string16 extension_id;
  HKEY hkey;
  base::string16 path;
  base::string16 name;

  ExtensionPolicyRegistryEntry(const base::string16& extension_id,
                               HKEY hkey,
                               const base::string16& path,
                               const base::string16& name);
  ExtensionPolicyRegistryEntry(ExtensionPolicyRegistryEntry&&);
  ExtensionPolicyRegistryEntry& operator=(ExtensionPolicyRegistryEntry&&);

  DISALLOW_COPY_AND_ASSIGN(ExtensionPolicyRegistryEntry);
};

// A file that holds some form of policy for |extension_id|.
struct ExtensionPolicyFile {
  base::string16 extension_id;
  base::FilePath path;

  ExtensionPolicyFile(const base::string16& extension_id,
                      const base::FilePath& path);
  ExtensionPolicyFile(ExtensionPolicyFile&&);
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

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_
