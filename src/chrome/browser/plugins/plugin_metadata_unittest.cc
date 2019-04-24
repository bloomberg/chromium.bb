// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_metadata.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/common/webplugininfo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

PluginMetadata::SecurityStatus GetSecurityStatus(
    PluginMetadata* plugin_metadata,
    const char* version) {
  content::WebPluginInfo plugin(
      base::ASCIIToUTF16("Foo plugin"),
      base::FilePath(FILE_PATH_LITERAL("/tmp/plugin.so")),
      base::ASCIIToUTF16(version),
      base::ASCIIToUTF16("Foo plugin."));
  return plugin_metadata->GetSecurityStatus(plugin);
}

}  // namespace

TEST(PluginMetadataTest, SecurityStatus) {
  const PluginMetadata::SecurityStatus kUpToDate =
      PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
  const PluginMetadata::SecurityStatus kOutOfDate =
      PluginMetadata::SECURITY_STATUS_OUT_OF_DATE;
  const PluginMetadata::SecurityStatus kRequiresAuthorization =
      PluginMetadata::SECURITY_STATUS_REQUIRES_AUTHORIZATION;

  PluginMetadata plugin_metadata("claybrick-writer",
                                 base::ASCIIToUTF16("ClayBrick Writer"),
                                 true,
                                 GURL(),
                                 GURL(),
                                 base::ASCIIToUTF16("ClayBrick"),
                                 std::string());
  EXPECT_EQ(kRequiresAuthorization,
            GetSecurityStatus(&plugin_metadata, "1.2.3"));

  plugin_metadata.AddVersion(base::Version("9.4.1"), kRequiresAuthorization);
  plugin_metadata.AddVersion(base::Version("10"), kOutOfDate);
  plugin_metadata.AddVersion(base::Version("10.2.1"), kUpToDate);

  // Invalid version.
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&plugin_metadata, "foo"));

  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&plugin_metadata, "0"));
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&plugin_metadata, "1.2.3"));
  EXPECT_EQ(kRequiresAuthorization,
            GetSecurityStatus(&plugin_metadata, "9.4.1"));
  EXPECT_EQ(kRequiresAuthorization,
            GetSecurityStatus(&plugin_metadata, "9.4.2"));
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&plugin_metadata, "10.2.0"));
  EXPECT_EQ(kUpToDate, GetSecurityStatus(&plugin_metadata, "10.2.1"));
  EXPECT_EQ(kUpToDate, GetSecurityStatus(&plugin_metadata, "11"));
}
