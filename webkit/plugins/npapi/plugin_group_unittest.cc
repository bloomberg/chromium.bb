// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_group.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webkit {
namespace npapi {

TEST(PluginGroupTest, VersionExtraction) {
  // Some real-world plugin versions (spaces, commata, parentheses, 'r', oh my)
  const char* versions[][2] = {
    { "7.6.6 (1671)", "7.6.6.1671" },  // Quicktime
    { "2, 0, 0, 254", "2.0.0.254" },   // DivX
    { "3, 0, 0, 0", "3.0.0.0" },       // Picasa
    { "1, 0, 0, 1", "1.0.0.1" },       // Earth
    { "10,0,45,2", "10.0.45.2" },      // Flash
    { "10.1 r102", "10.1.102"},        // Flash
    { "10.3 d180", "10.3.180" },       // Flash (Debug)
    { "11.5.7r609", "11.5.7.609"},     // Shockwave
    { "1.6.0_22", "1.6.0.22"},         // Java
  };

  for (size_t i = 0; i < arraysize(versions); i++) {
    scoped_ptr<Version> version(PluginGroup::CreateVersionFromString(
        ASCIIToUTF16(versions[i][0])));
    EXPECT_EQ(versions[i][1], version->GetString());
  }
}

}  // namespace npapi
}  // namespace webkit
