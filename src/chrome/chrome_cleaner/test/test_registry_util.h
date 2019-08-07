// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_REGISTRY_UTIL_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_REGISTRY_UTIL_H_

#include <windows.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

class ScopedRegistryValue {
 public:
  ScopedRegistryValue(HKEY rootkey,
                      const base::char16* subkey,
                      REGSAM access,
                      const base::char16* value_name,
                      const base::char16* content,
                      uint32_t value_type);

  ~ScopedRegistryValue();

 private:
  const base::char16* value_name_;
  base::win::RegKey key_;
  DWORD old_value_type_ = REG_NONE;
  std::vector<base::char16> old_value_;
  DWORD old_value_size_ = 0;
  bool has_value_ = false;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedRegistryValue);
};

class ScopedTempRegistryKey {
 public:
  ScopedTempRegistryKey(HKEY key, const base::char16* key_path, REGSAM access);
  ~ScopedTempRegistryKey();

  bool Valid() const { return key_.Valid(); }
  base::win::RegKey* Get() { return &key_; }

 private:
  base::win::RegKey key_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedTempRegistryKey);
};

// Expect the registry footprint to be found in |pup|.
void ExpectRegistryFootprint(const PUPData::PUP& pup,
                             const RegKeyPath& key_path,
                             const base::char16* value_name,
                             const base::char16* value_substring,
                             RegistryMatchRule rule);

// Expect the registry footprint not to be found in |pup|.
void ExpectRegistryFootprintAbsent(const PUPData::PUP& pup,
                                   const RegKeyPath& key_path,
                                   const base::char16* value_name,
                                   const base::char16* value_substring,
                                   RegistryMatchRule rule);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_REGISTRY_UTIL_H_
