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

TEST(WebIntentServiceDataTest, SetDisposition) {

  // Test using the default disposition (window).
  WebIntentServiceData intent;
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_WINDOW, intent.disposition);

  // Set the disposition to "inline", another supported disposition.
  intent.setDisposition(ASCIIToUTF16("inline"));
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_INLINE, intent.disposition);

  // "native" is a special internal disposition we use for
  // "built-in" services. We don't allow the "native" disposition to be
  // set via web-content (which is how this SetDisposition gets called).
  // So after trying to set the value to "native" the disposition should
  // remain unchanged.
  intent.setDisposition(ASCIIToUTF16("native"));
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_INLINE, intent.disposition);

  // Unrecognized values are ignored. When trying to set the value to "poodles"
  // the disposition should remain unchanged.
  intent.setDisposition(ASCIIToUTF16("poodles"));
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_INLINE, intent.disposition);

  // Set the value back to "window" to confirm we can set back to
  // the default disposition value, and to confirm setting still
  // works after our special casing of the "native" and unrecognized values.
  intent.setDisposition(ASCIIToUTF16("window"));
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_WINDOW, intent.disposition);
}

} // namespace
