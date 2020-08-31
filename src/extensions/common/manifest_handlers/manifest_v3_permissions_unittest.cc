// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using ManifestV3PermissionsTest = ManifestTest;
const char* kManifestV3NotSupported =
    "The maximum currently-supported manifest version is 2, but this is 3.  "
    "Certain features may not work as expected.";

TEST_F(ManifestV3PermissionsTest, WebRequestBlockingPermissionsTest) {
  const std::string kPermissionRequiresV2OrLower =
      "'webRequestBlocking' requires manifest version of 2 or lower.";
  {
    // Manifest V3 extension that is not policy installed. This should trigger a
    // warning that manifest V3 is not currently supported and that the
    // webRequestBlocking permission requires a lower manifest version.
    scoped_refptr<Extension> extension(LoadAndExpectWarnings(
        "web_request_blocking_v3.json",
        {kManifestV3NotSupported, kPermissionRequiresV2OrLower},
        extensions::Manifest::Location::UNPACKED));
    ASSERT_TRUE(extension);
  }
  {
    // Manifest V3 extension that is policy extension. This should only trigger
    // a warning that manifest V3 is not supported currently.
    scoped_refptr<Extension> extension(LoadAndExpectWarnings(
        "web_request_blocking_v3.json", {kManifestV3NotSupported},
        extensions::Manifest::Location::EXTERNAL_POLICY));
    ASSERT_TRUE(extension);
  }
  {
    // Manifest V2 extension that is not policy installed. This should not
    // trigger any warnings.
    scoped_refptr<Extension> extension(
        LoadAndExpectSuccess("web_request_blocking_v2.json",
                             extensions::Manifest::Location::UNPACKED));
    ASSERT_TRUE(extension);
  }
}

TEST_F(ManifestV3PermissionsTest, DisallowNaClTest) {
  const std::string kPermissionRequiresV2OrLower =
      "'nacl_modules' requires manifest version of 2 or lower.";
  {
    // Unpacked Manifest V3 extension should trigger a warning that
    // manifest V3 is not currently supported and that 'nacl_modules' requires a
    // lower manifest version.
    scoped_refptr<Extension> extension(LoadAndExpectWarnings(
        "nacl_module_v3.json",
        {kManifestV3NotSupported, kPermissionRequiresV2OrLower},
        extensions::Manifest::Location::UNPACKED));
    ASSERT_TRUE(extension);
  }
  {
    // Unpacked Manifest V2 extension should not trigger any warnings.
    scoped_refptr<Extension> extension(LoadAndExpectSuccess(
        "nacl_module_v2.json", extensions::Manifest::Location::UNPACKED));
    ASSERT_TRUE(extension);
  }
}

}  // namespace extensions
