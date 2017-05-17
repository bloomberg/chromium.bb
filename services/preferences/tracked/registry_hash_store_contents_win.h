// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_REGISTRY_HASH_STORE_CONTENTS_WIN_H_
#define SERVICES_PREFERENCES_TRACKED_REGISTRY_HASH_STORE_CONTENTS_WIN_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "services/preferences/tracked/hash_store_contents.h"

// Implements HashStoreContents by storing MACs in the Windows registry.
class RegistryHashStoreContentsWin : public HashStoreContents {
 public:
  // Constructs a RegistryHashStoreContents which acts on a registry entry
  // defined by |registry_path| and |store_key|. If |store_key| begins with
  // base::ScopedTempDir::GetTempDirPrefix(), this RegistryHashStoreContentsWin
  // will self Reset() on destruction to avoid proliferating keys in tests that
  // create a profile in a ScopedTempDir (https://crbug.com/721245).
  explicit RegistryHashStoreContentsWin(const base::string16& registry_path,
                                        const base::string16& store_key);

  ~RegistryHashStoreContentsWin() override;

  // HashStoreContents overrides:
  bool IsCopyable() const override;
  std::unique_ptr<HashStoreContents> MakeCopy() const override;
  base::StringPiece GetUMASuffix() const override;
  void Reset() override;
  bool GetMac(const std::string& path, std::string* out_value) override;
  bool GetSplitMacs(const std::string& path,
                    std::map<std::string, std::string>* split_macs) override;
  void SetMac(const std::string& path, const std::string& value) override;
  void SetSplitMac(const std::string& path,
                   const std::string& split_path,
                   const std::string& value) override;
  bool RemoveEntry(const std::string& path) override;

  // Unsupported HashStoreContents overrides:
  void ImportEntry(const std::string& path,
                   const base::Value* in_value) override;
  const base::DictionaryValue* GetContents() const override;
  std::string GetSuperMac() const override;
  void SetSuperMac(const std::string& super_mac) override;

 private:
  // Helper constructor for |MakeCopy|.
  explicit RegistryHashStoreContentsWin(
      const RegistryHashStoreContentsWin& other);

  const base::string16 preference_key_name_;
  const bool reset_on_delete_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_REGISTRY_HASH_STORE_CONTENTS_WIN_H_
