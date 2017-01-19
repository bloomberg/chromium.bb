// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Modulator.h"

#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ModulatorTest, resolveModuleSpecifier) {
  // Taken from examples listed in
  // https://html.spec.whatwg.org/#resolve-a-module-specifier

  // "The following are valid module specifiers according to the above
  // algorithm:"
  EXPECT_TRUE(
      Modulator::resolveModuleSpecifier("https://example.com/apples.js", KURL())
          .isValid());

  KURL resolved =
      Modulator::resolveModuleSpecifier("http:example.com\\pears.mjs", KURL());
  EXPECT_TRUE(resolved.isValid());
  EXPECT_EQ("http://example.com/pears.mjs", resolved.getString());

  KURL baseURL(KURL(), "https://example.com");
  EXPECT_TRUE(
      Modulator::resolveModuleSpecifier("//example.com/", baseURL).isValid());
  EXPECT_TRUE(
      Modulator::resolveModuleSpecifier("./strawberries.js.cgi", baseURL)
          .isValid());
  EXPECT_TRUE(
      Modulator::resolveModuleSpecifier("../lychees", baseURL).isValid());
  EXPECT_TRUE(
      Modulator::resolveModuleSpecifier("/limes.jsx", baseURL).isValid());
  EXPECT_TRUE(Modulator::resolveModuleSpecifier(
                  "data:text/javascript,export default 'grapes';", KURL())
                  .isValid());
  EXPECT_TRUE(
      Modulator::resolveModuleSpecifier(
          "blob:https://whatwg.org/d0360e2f-caee-469f-9a2f-87d5b0456f6f",
          KURL())
          .isValid());

  // "The following are valid module specifiers according to the above
  // algorithm, but will invariably cause failures when they are fetched:"
  EXPECT_TRUE(Modulator::resolveModuleSpecifier(
                  "javascript:export default 'artichokes';", KURL())
                  .isValid());
  EXPECT_TRUE(Modulator::resolveModuleSpecifier(
                  "data:text/plain,export default 'kale';", KURL())
                  .isValid());
  EXPECT_TRUE(
      Modulator::resolveModuleSpecifier("about:legumes", KURL()).isValid());
  EXPECT_TRUE(
      Modulator::resolveModuleSpecifier("wss://example.com/celery", KURL())
          .isValid());

  // "The following are not valid module specifiers according to the above
  // algorithm:"
  EXPECT_FALSE(
      Modulator::resolveModuleSpecifier("https://f:b/c", KURL()).isValid());
  EXPECT_FALSE(
      Modulator::resolveModuleSpecifier("pumpkins.js", KURL()).isValid());

  // Unprefixed module specifiers should still fail w/ a valid baseURL.
  EXPECT_FALSE(
      Modulator::resolveModuleSpecifier("avocados.js", baseURL).isValid());
}

}  // namespace blink
