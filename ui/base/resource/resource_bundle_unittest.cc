// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(ResourceBundle, LoadDataResourceBytes) {
  // Verify that we don't crash when trying to load a resource that is not
  // found.  In some cases, we fail to mmap resources.pak, but try to keep
  // going anyway.
  ResourceBundle resource_bundle;

  // On Windows, the default data is compiled into the binary so this does
  // nothing.
  FilePath data_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &data_path);
  data_path = data_path.Append(
      FILE_PATH_LITERAL("ui/base/test/data/data_pack_unittest/sample.pak"));

  resource_bundle.LoadTestResources(data_path);

  const int kUnfoundResourceId = 10000;
  EXPECT_EQ(NULL, resource_bundle.LoadDataResourceBytes(kUnfoundResourceId));

  // Give a .pak file that doesn't exist so we will fail to load it.
  resource_bundle.AddDataPackToSharedInstance(FilePath(
      FILE_PATH_LITERAL("non-existant-file.pak")));
  EXPECT_EQ(NULL, resource_bundle.LoadDataResourceBytes(kUnfoundResourceId));
}

TEST(ResourceBundle, LocaleDataPakExists) {
  // Check that ResourceBundle::LocaleDataPakExists returns the correct results.
  EXPECT_TRUE(ResourceBundle::LocaleDataPakExists("en-US"));
  EXPECT_FALSE(ResourceBundle::LocaleDataPakExists("not_a_real_locale"));
}

}  // namespace ui
