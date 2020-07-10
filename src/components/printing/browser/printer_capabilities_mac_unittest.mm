// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/printer_capabilities_mac.h"

#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

base::FilePath WriteOutCustomPapersPlist(const base::FilePath& dir,
                                         const char* name,
                                         NSDictionary* dict) {
  base::FilePath path = dir.Append(name);
  NSString* plist_path = base::mac::FilePathToNSString(path);
  if (![dict writeToFile:plist_path atomically:YES])
    path.clear();
  return path;
}

}  // namespace

TEST(PrinterCapabilitiesMacTest, GetMacCustomPaperSizesFromFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"name" : @"foo",
            @"width" : @144,
            @"height" : @288,
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "good1.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(1u, papers.size());
    EXPECT_EQ("foo", papers[0].display_name);
    EXPECT_EQ("", papers[0].vendor_id);
    EXPECT_EQ(50800, papers[0].size_um.width());
    EXPECT_EQ(101600, papers[0].size_um.height());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"width" : @100,
            @"height" : @200,
            @"name" : @"bar",
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "good2.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(1u, papers.size());
    EXPECT_EQ("bar", papers[0].display_name);
    EXPECT_EQ("", papers[0].vendor_id);
    EXPECT_EQ(35278, papers[0].size_um.width());
    EXPECT_EQ(70556, papers[0].size_um.height());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{}]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "empty.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"height" : @200,
            @"name" : @"bar",
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "no_width.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"width" : @100,
            @"name" : @"bar",
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "no_height.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"width" : @100,
            @"height" : @200,
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "no_name.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"width" : @0,
            @"height" : @200,
            @"name" : @"bar",
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "zero_width.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"width" : @100,
            @"height" : @0,
            @"name" : @"bar",
          }
        }]);
    base::FilePath path = WriteOutCustomPapersPlist(temp_dir.GetPath(),
                                                    "zero_height.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"width" : @7199929,
            @"height" : @200,
            @"name" : @"bar",
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_width.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"width" : @100,
            @"height" : @7199929,
            @"name" : @"bar",
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_height.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[NSMutableDictionary alloc] initWithDictionary:@{
          @"foo" : @{
            @"width" : @100,
            @"height" : @200,
            @"name" : @"",
          }
        }]);
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "empty_name.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
}

}  // namespace printing
