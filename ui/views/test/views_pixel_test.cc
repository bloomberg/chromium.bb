// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_pixel_test.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace views {
namespace test {
namespace {

// Platform names. Note these use names consistent with layout tests.
const char* GetVersionedPlatform() {
#if defined(OS_MACOSX)
  if (base::mac::IsOS10_9())
    return "mac-mac10.9";

  // When Chrome stops supporting 10.12, the next line won't compile. That means
  // 10.12 bots will disappear, so the following needs to be replaced with the
  // next OS version chosen for the swarming fleet.
  if (base::mac::IsOS10_12(/* See note above */))
    return "mac-mac10.12";

  return "mac";
#elif defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN10)
    return "win10";
  if (base::win::GetVersion() == base::win::VERSION_WIN7)
    return "win7";
  return "win";
#else
  return "linux";
#endif
}

base::FilePath GetDataPathRoot() {
  base::FilePath root_path;
  DCHECK(PathService::Get(base::DIR_SOURCE_ROOT, &root_path));
  return root_path.AppendASCII("ui")
      .AppendASCII("views")
      .AppendASCII("test")
      .AppendASCII("data");
}

}  // namespace

bool PixelTest(const SkBitmap& bitmap, const char* name, bool* used_fallback) {
  // Priority order of paths to try. This must include every possible return
  // from GetVersionedPlatform().
  const std::vector<std::string> paths = {
      "linux", "win7", "win10", "win", "mac-mac10.9", "mac-mac10.12", "mac"};
  const base::FilePath data_path = GetDataPathRoot();

  auto index_of = [&paths](const char* platform) {
    return std::find(paths.begin(), paths.end(), platform) - paths.begin();
  };

  // First check for a platform-specific reference image.
  const size_t start_index = index_of(GetVersionedPlatform());
  size_t i = start_index;
  base::FilePath path = data_path.AppendASCII(paths[i]).AppendASCII(name);

  // If there's no platform-specific image. Try the common one next. Note this
  // does not update |i| for the |used_fallback| check below. The presence of a
  // "common" image says there should never be platform-specific differences,
  // unless explicitly special-cased and caught by the first check.
  if (!base::PathExists(path))
    path = data_path.AppendASCII("common").AppendASCII(name);

  // Fallback approach: If there is no common file, and no platform file, start
  // at the current platform index and try lower indexes until a file is found.
  // Then try higher indexes.
  // TODO(tapted): Explore other fallback mechanisms if needed. E.g. layout
  // tests use a graph of fallbacks. Or, this could test a set of reference
  // images and permit a match against any.
  if (!base::PathExists(path)) {
    for (i = start_index; i != 0 && !base::PathExists(path); --i)
      path = data_path.AppendASCII(paths[i - 1]).AppendASCII(name);
  }

  // Search the other way.
  if (!base::PathExists(path)) {
    for (i = start_index + 1; i < paths.size() && !base::PathExists(path); ++i)
      path = data_path.AppendASCII(paths[i]).AppendASCII(name);
  }

  if (!base::PathExists(path)) {
    ADD_FAILURE() << "Couldn't find any reference image. Name: " << name;
    return false;
  }

  *used_fallback = i != start_index;
  if (*used_fallback)
    VLOG(0) << "Note: Using fallback reference image from " << path.value();

  return cc::MatchesPNGFile(bitmap, path,
                            cc::ExactPixelComparator(true /* discard alpha */));
}

void UpdateReferenceImage(const SkBitmap& bitmap, const char* name) {
  const base::FilePath path =
      GetDataPathRoot().AppendASCII(GetVersionedPlatform()).AppendASCII(name);
  if (cc::WritePNGFile(bitmap, path, true /* discard transparency */))
    VLOG(0) << "Wrote to " << path.value();
  else
    PLOG(ERROR) << "FAILED to write to " << path.value();
}

}  // namespace test
}  // namespace views
