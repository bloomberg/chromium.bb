// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

extern const char kSamplePakContents[];
extern const size_t kSamplePakSize;

TEST(ResourceBundle, LoadDataResourceBytes) {
  // Verify that we don't crash when trying to load a resource that is not
  // found.  In some cases, we fail to mmap resources.pak, but try to keep
  // going anyway.
  ResourceBundle resource_bundle;

  // On Windows, the default data is compiled into the binary so this does
  // nothing.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath data_path = dir.path().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak file.
  ASSERT_EQ(file_util::WriteFile(data_path, kSamplePakContents, kSamplePakSize),
            static_cast<int>(kSamplePakSize));

  // Create a resource bundle from the file.
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
