// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test.h"

#include <utility>
#include <vector>

#include "content/public/test/web_contents_tester.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

WebAppTest::WebAppTest() = default;

WebAppTest::~WebAppTest() = default;

void WebAppTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SetContents(CreateTestWebContents());
}

// static
SkBitmap WebAppTest::CreateSquareIcon(int size_px, SkColor solid_color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size_px, size_px);
  bitmap.eraseColor(solid_color);
  return bitmap;
}

// static
WebApplicationInfo::IconInfo WebAppTest::GenerateIconInfo(const GURL& url,
                                                          int size_px,
                                                          SkColor solid_color) {
  WebApplicationInfo::IconInfo icon_info;
  icon_info.url = url;
  icon_info.width = size_px;
  icon_info.height = size_px;
  icon_info.data = CreateSquareIcon(size_px, solid_color);

  return icon_info;
}

// static
void WebAppTest::AddIconToIconsMap(const GURL& icon_url,
                                   int size_px,
                                   SkColor solid_color,
                                   IconsMap* icons_map) {
  SkBitmap bitmap = CreateSquareIcon(size_px, solid_color);

  std::vector<SkBitmap> bitmaps;
  bitmaps.push_back(std::move(bitmap));

  icons_map->emplace(icon_url, std::move(bitmaps));
}

}  // namespace web_app
