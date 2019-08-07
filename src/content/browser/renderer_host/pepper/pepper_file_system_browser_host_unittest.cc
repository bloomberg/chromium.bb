// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_test.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PepperFileSystemBrowserHostTest : public testing::Test,
                                        public BrowserPpapiHostTest {
 public:
  PepperFileSystemBrowserHostTest() {}
  ~PepperFileSystemBrowserHostTest() override {}

  void SetUp() override {
    PP_Instance pp_instance = 12345;
    PP_Resource pp_resource = 67890;
    host_.reset(new PepperFileSystemBrowserHost(GetBrowserPpapiHost(),
                                                pp_instance,
                                                pp_resource,
                                                PP_FILESYSTEMTYPE_ISOLATED));
  }

  void TearDown() override { host_.reset(); }

 protected:
  std::string GeneratePluginId(const std::string& mime_type) {
    return host_->GeneratePluginId(mime_type);
  }

 private:
  std::unique_ptr<PepperFileSystemBrowserHost> host_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileSystemBrowserHostTest);
};

TEST_F(PepperFileSystemBrowserHostTest, GeneratePluginId) {
  // Should not contain wild card characters.
  EXPECT_TRUE(GeneratePluginId("*").empty());
  EXPECT_TRUE(GeneratePluginId("*/*").empty());

  // Should contain only one slash.
  EXPECT_TRUE(GeneratePluginId(".").empty());
  EXPECT_TRUE(GeneratePluginId("..").empty());
  EXPECT_TRUE(GeneratePluginId("application").empty());
  EXPECT_TRUE(GeneratePluginId("application/mime/type").empty());

  // Should start with "legal_top_level_types"
  // (ex. "application", "audio", "x-").  See "mime_util.cc" for more details.
  EXPECT_TRUE(GeneratePluginId("/mime").empty());
  EXPECT_TRUE(GeneratePluginId("./mime").empty());
  EXPECT_TRUE(GeneratePluginId("../mime").empty());
  EXPECT_TRUE(GeneratePluginId("app/mime").empty());

  // Should not contain characters other than alphanumeric or "._-".
  EXPECT_TRUE(GeneratePluginId("application/mime+type").empty());
  EXPECT_TRUE(GeneratePluginId("application/mime:type").empty());
  EXPECT_TRUE(GeneratePluginId("application/mime;type").empty());

  // Valid cases.
  EXPECT_EQ("application_mime", GeneratePluginId("application/mime"));
  EXPECT_EQ("x-app_mime", GeneratePluginId("x-app/mime"));
  EXPECT_EQ("application_mime.type", GeneratePluginId("application/mime.type"));
  EXPECT_EQ("application_mime_type", GeneratePluginId("application/mime_type"));
  EXPECT_EQ("application_mime-type", GeneratePluginId("application/mime-type"));
  EXPECT_EQ("application_..", GeneratePluginId("application/.."));
}

}  // namespace content
