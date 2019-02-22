// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media {

TEST(GetMediaURLScheme, MissingUnknown) {
  EXPECT_EQ(mojom::MediaURLScheme::kMissing, GetMediaURLScheme(GURL()));
  EXPECT_EQ(mojom::MediaURLScheme::kUnknown,
            GetMediaURLScheme(GURL("abcd://ab")));
}

TEST(GetMediaURLScheme, WebCommon) {
  EXPECT_EQ(mojom::MediaURLScheme::kFtp,
            GetMediaURLScheme(GURL("ftp://abc.123")));
  EXPECT_EQ(mojom::MediaURLScheme::kHttp,
            GetMediaURLScheme(GURL("http://abc.123")));
  EXPECT_EQ(mojom::MediaURLScheme::kHttps,
            GetMediaURLScheme(GURL("https://abc.123")));
  EXPECT_EQ(mojom::MediaURLScheme::kData,
            GetMediaURLScheme(GURL("data://abc.123")));
  EXPECT_EQ(mojom::MediaURLScheme::kBlob,
            GetMediaURLScheme(GURL("blob://abc.123")));
  EXPECT_EQ(mojom::MediaURLScheme::kJavascript,
            GetMediaURLScheme(GURL("javascript://abc.123")));
}

TEST(GetMediaURLScheme, Files) {
  EXPECT_EQ(mojom::MediaURLScheme::kFile,
            GetMediaURLScheme(GURL("file://abc.123")));
  EXPECT_EQ(mojom::MediaURLScheme::kFileSystem,
            GetMediaURLScheme(GURL("filesystem://abc.123")));
}

TEST(GetMediaURLScheme, Android) {
  EXPECT_EQ(mojom::MediaURLScheme::kContent,
            GetMediaURLScheme(GURL("content://abc.123")));
  EXPECT_EQ(mojom::MediaURLScheme::kContentId,
            GetMediaURLScheme(GURL("cid://abc.123")));
}

TEST(GetMediaURLScheme, Chrome) {
  EXPECT_EQ(mojom::MediaURLScheme::kChrome,
            GetMediaURLScheme(GURL("chrome://abc.123")));
  EXPECT_EQ(mojom::MediaURLScheme::kChromeExtension,
            GetMediaURLScheme(GURL("chrome-extension://abc.123")));
}

}  // namespace media
