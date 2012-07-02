// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_service_data.h"

using webkit_glue::WebIntentServiceData;

namespace {

void Expect(
    const std::string& action,
    const std::string& type,
    const std::string& scheme,
    const std::string& url,
    const std::string& title,
    const WebIntentServiceData::Disposition disposition,
    const WebIntentServiceData* intent) {

  EXPECT_EQ(GURL(url), intent->service_url);
  EXPECT_EQ(ASCIIToUTF16(action), intent->action);
  EXPECT_EQ(ASCIIToUTF16(type), intent->type);
  EXPECT_EQ(ASCIIToUTF16(scheme), intent->scheme);
  EXPECT_EQ(ASCIIToUTF16(title), intent->title);
  EXPECT_EQ(disposition, intent->disposition);
}

TEST(WebIntentServiceDataTest, Defaults) {
  WebIntentServiceData intent;
  EXPECT_EQ(string16(), intent.action);
  EXPECT_EQ(string16(), intent.type);
  EXPECT_EQ(string16(), intent.scheme);
  EXPECT_EQ(string16(), intent.title);
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_WINDOW, intent.disposition);
}

TEST(WebIntentServiceDataTest, ActionServicesEqual) {

  // with default disposition...
  WebIntentServiceData intent(
      ASCIIToUTF16("http://webintents.org/share"),
      ASCIIToUTF16("image/png"),
      string16(),
      GURL("http://abc.com/xyx.html"),
      ASCIIToUTF16("Image Sharing Service"));

  Expect(
      "http://webintents.org/share",
      "image/png",
      std::string(),
      "http://abc.com/xyx.html",
      "Image Sharing Service",
      WebIntentServiceData::DISPOSITION_WINDOW,
      &intent);
}

TEST(WebIntentServiceDataTest, SchemeServicesEqual) {

  // with default disposition...
  WebIntentServiceData intent(
      string16(),
      string16(),
      ASCIIToUTF16("mailto"),
      GURL("http://abc.com/xyx.html"),
      ASCIIToUTF16("Image Sharing Service"));

  Expect(
      "",
      "",
      "mailto",
      "http://abc.com/xyx.html",
      "Image Sharing Service",
      WebIntentServiceData::DISPOSITION_WINDOW,
      &intent);
}

} // namespace
