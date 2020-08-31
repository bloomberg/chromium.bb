// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_FILE_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_FILE_PROVIDER_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "libassistant/shared/public/platform_file.h"

namespace chromeos {
namespace assistant {

class FileProviderImpl : public assistant_client::FileProvider {
 public:
  FileProviderImpl();
  ~FileProviderImpl() override;

  // assistant_client::FileProvider overrides:
  std::string ReadFile(const std::string& path) override;
  bool WriteFile(const std::string& path, const std::string& data) override;
  std::string ReadSecureFile(const std::string& path) override;
  bool WriteSecureFile(const std::string& path,
                       const std::string& data) override;
  void CleanAssistantData() override;
  bool GetResource(uint16_t resource_id, std::string* out) override;

 private:
  // Root path which other paths are relative to.
  const base::FilePath root_path_;

  DISALLOW_COPY_AND_ASSIGN(FileProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_FILE_PROVIDER_IMPL_H_
