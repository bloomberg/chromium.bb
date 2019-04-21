// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_MOCK_FORCE_INSTALLED_EXTENSION_SCANNER_H_
#define CHROME_CHROME_CLEANER_SCANNER_MOCK_FORCE_INSTALLED_EXTENSION_SCANNER_H_

#include <memory>
#include <vector>

#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"
#include "chrome/chrome_cleaner/proto/uwe_matcher.pb.h"
#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_cleaner {

class MockForceInstalledExtensionScanner
    : public ForceInstalledExtensionScanner {
 public:
  MockForceInstalledExtensionScanner();
  ~MockForceInstalledExtensionScanner() override;

  MOCK_METHOD1(CreateUwEMatchersFromResource,
               std::unique_ptr<UwEMatchers>(int resource_ids));
  MOCK_METHOD1(
      GetForceInstalledExtensions,
      std::vector<ForceInstalledExtension>(JsonParserAPI* json_parser));
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_MOCK_FORCE_INSTALLED_EXTENSION_SCANNER_H_
