// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class ManifestParserTest : public testing::Test {
 protected:
  ManifestParserTest() {}
  ~ManifestParserTest() override {}

  Manifest ParseManifestWithURLs(const base::StringPiece& data,
                                 const GURL& manifest_url,
                                 const GURL& document_url) {
    ManifestParser parser(data, KURL(manifest_url), KURL(document_url));
    parser.Parse();
    Vector<mojom::blink::ManifestErrorPtr> errors;
    parser.TakeErrors(&errors);

    errors_.clear();
    for (auto& error : errors)
      errors_.push_back(std::move(error->message));
    return parser.manifest();
  }

  Manifest ParseManifest(const base::StringPiece& data) {
    return ParseManifestWithURLs(data, default_manifest_url,
                                 default_document_url);
  }

  const Vector<String>& errors() const { return errors_; }

  unsigned int GetErrorCount() const { return errors_.size(); }

  static const GURL default_document_url;
  static const GURL default_manifest_url;

 private:
  Vector<String> errors_;

  DISALLOW_COPY_AND_ASSIGN(ManifestParserTest);
};

const GURL ManifestParserTest::default_document_url(
    "http://foo.com/index.html");
const GURL ManifestParserTest::default_manifest_url(
    "http://foo.com/manifest.json");

TEST_F(ManifestParserTest, CrashTest) {
  // Passing temporary variables should not crash.
  const base::StringPiece json = "{\"start_url\": \"/\"}";
  KURL url("http://example.com");
  ManifestParser parser(json, url, url);

  parser.Parse();
  Vector<mojom::blink::ManifestErrorPtr> errors;
  parser.TakeErrors(&errors);

  // .Parse() should have been call without crashing and succeeded.
  EXPECT_EQ(0u, errors.size());
  EXPECT_FALSE(parser.manifest().IsEmpty());
}

TEST_F(ManifestParserTest, EmptyStringNull) {
  Manifest manifest = ParseManifest("");

  // This Manifest is not a valid JSON object, it's a parsing error.
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ("Line: 1, column: 1, Unexpected token.", errors()[0]);

  // A parsing error is equivalent to an empty manifest.
  ASSERT_TRUE(manifest.IsEmpty());
  ASSERT_TRUE(manifest.name.is_null());
  ASSERT_TRUE(manifest.short_name.is_null());
  ASSERT_TRUE(manifest.start_url.is_empty());
  ASSERT_EQ(manifest.display, kWebDisplayModeUndefined);
  ASSERT_EQ(manifest.orientation, kWebScreenOrientationLockDefault);
  ASSERT_FALSE(manifest.theme_color.has_value());
  ASSERT_FALSE(manifest.background_color.has_value());
  ASSERT_TRUE(manifest.splash_screen_url.is_empty());
  ASSERT_TRUE(manifest.gcm_sender_id.is_null());
  ASSERT_TRUE(manifest.scope.is_empty());
}

TEST_F(ManifestParserTest, ValidNoContentParses) {
  Manifest manifest = ParseManifest("{}");

  // Empty Manifest is not a parsing error.
  EXPECT_EQ(0u, GetErrorCount());

  // Check that the fields are null or set to their default values.
  ASSERT_FALSE(manifest.IsEmpty());
  ASSERT_TRUE(manifest.name.is_null());
  ASSERT_TRUE(manifest.short_name.is_null());
  ASSERT_TRUE(manifest.start_url.is_empty());
  ASSERT_EQ(manifest.display, kWebDisplayModeUndefined);
  ASSERT_EQ(manifest.orientation, kWebScreenOrientationLockDefault);
  ASSERT_FALSE(manifest.theme_color.has_value());
  ASSERT_FALSE(manifest.background_color.has_value());
  ASSERT_TRUE(manifest.splash_screen_url.is_empty());
  ASSERT_TRUE(manifest.gcm_sender_id.is_null());
  ASSERT_EQ(default_document_url.GetWithoutFilename(), manifest.scope);
}

TEST_F(ManifestParserTest, MultipleErrorsReporting) {
  Manifest manifest = ParseManifest(
      "{ \"name\": 42, \"short_name\": 4,"
      "\"orientation\": {}, \"display\": \"foo\","
      "\"start_url\": null, \"icons\": {}, \"theme_color\": 42,"
      "\"background_color\": 42 }");

  EXPECT_EQ(8u, GetErrorCount());

  EXPECT_EQ("property 'name' ignored, type string expected.", errors()[0]);
  EXPECT_EQ("property 'short_name' ignored, type string expected.",
            errors()[1]);
  EXPECT_EQ("property 'start_url' ignored, type string expected.", errors()[2]);
  EXPECT_EQ("unknown 'display' value ignored.", errors()[3]);
  EXPECT_EQ("property 'orientation' ignored, type string expected.",
            errors()[4]);
  EXPECT_EQ("property 'icons' ignored, type array expected.", errors()[5]);
  EXPECT_EQ("property 'theme_color' ignored, type string expected.",
            errors()[6]);
  EXPECT_EQ("property 'background_color' ignored, type string expected.",
            errors()[7]);
}

TEST_F(ManifestParserTest, NameParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"name\": \"foo\" }");
    ASSERT_TRUE(base::EqualsASCII(manifest.name.string(), "foo"));
    ASSERT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"name\": \"  foo  \" }");
    ASSERT_TRUE(base::EqualsASCII(manifest.name.string(), "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"name\": {} }");
    ASSERT_TRUE(manifest.name.is_null());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"name\": 42 }");
    ASSERT_TRUE(manifest.name.is_null());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' ignored, type string expected.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShortNameParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"short_name\": \"foo\" }");
    ASSERT_TRUE(base::EqualsASCII(manifest.short_name.string(), "foo"));
    ASSERT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"short_name\": \"  foo  \" }");
    ASSERT_TRUE(base::EqualsASCII(manifest.short_name.string(), "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"short_name\": {} }");
    ASSERT_TRUE(manifest.short_name.is_null());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'short_name' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"short_name\": 42 }");
    ASSERT_TRUE(manifest.short_name.is_null());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'short_name' ignored, type string expected.",
              errors()[0]);
  }
}

TEST_F(ManifestParserTest, StartURLParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"start_url\": \"land.html\" }");
    ASSERT_EQ(manifest.start_url.spec(),
              default_document_url.Resolve("land.html").spec());
    ASSERT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"start_url\": \"  land.html  \" }");
    ASSERT_EQ(manifest.start_url.spec(),
              default_document_url.Resolve("land.html").spec());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"start_url\": {} }");
    ASSERT_TRUE(manifest.start_url.is_empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'start_url' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"start_url\": 42 }");
    ASSERT_TRUE(manifest.start_url.is_empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'start_url' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if property isn't a valid URL.
  {
    Manifest manifest =
        ParseManifest("{ \"start_url\": \"http://www.google.ca:a\" }");
    ASSERT_TRUE(manifest.start_url.is_empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'start_url' ignored, URL is invalid.", errors()[0]);
  }

  // Absolute start_url, same origin with document.
  {
    Manifest manifest =
        ParseManifestWithURLs("{ \"start_url\": \"http://foo.com/land.html\" }",
                              GURL("http://foo.com/manifest.json"),
                              GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.start_url.spec(), "http://foo.com/land.html");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Absolute start_url, cross origin with document.
  {
    Manifest manifest =
        ParseManifestWithURLs("{ \"start_url\": \"http://bar.com/land.html\" }",
                              GURL("http://foo.com/manifest.json"),
                              GURL("http://foo.com/index.html"));
    ASSERT_TRUE(manifest.start_url.is_empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'start_url' ignored, should "
        "be same origin as document.",
        errors()[0]);
  }

  // Resolving has to happen based on the manifest_url.
  {
    Manifest manifest =
        ParseManifestWithURLs("{ \"start_url\": \"land.html\" }",
                              GURL("http://foo.com/landing/manifest.json"),
                              GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.start_url.spec(), "http://foo.com/landing/land.html");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ScopeParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest(
        "{ \"scope\": \"land\", \"start_url\": \"land/landing.html\" }");
    ASSERT_EQ(manifest.scope.spec(),
              default_document_url.Resolve("land").spec());
    ASSERT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Whitespaces.
  {
    Manifest manifest = ParseManifest(
        "{ \"scope\": \"  land  \", \"start_url\": \"land/landing.html\" }");
    ASSERT_EQ(manifest.scope.spec(),
              default_document_url.Resolve("land").spec());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Return the default value if the property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"scope\": {} }");
    ASSERT_EQ(manifest.scope.spec(),
              default_document_url.GetWithoutFilename().spec());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'scope' ignored, type string expected.", errors()[0]);
  }

  // Return the default value if property isn't a string.
  {
    Manifest manifest = ParseManifest(
        "{ \"scope\": 42, "
        "\"start_url\": \"http://foo.com/land/landing.html\" }");
    ASSERT_EQ(manifest.scope.spec(),
              default_document_url.Resolve("land/").spec());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'scope' ignored, type string expected.", errors()[0]);
  }

  // Absolute scope, start URL is in scope.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"scope\": \"http://foo.com/land\", "
        "\"start_url\": \"http://foo.com/land/landing.html\" }",
        GURL("http://foo.com/manifest.json"),
        GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.scope.spec(), "http://foo.com/land");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Absolute scope, start URL is not in scope.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"scope\": \"http://foo.com/land\", "
        "\"start_url\": \"http://foo.com/index.html\" }",
        GURL("http://foo.com/manifest.json"),
        GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.scope.spec(),
              default_document_url.GetWithoutFilename().spec());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.",
        errors()[0]);
  }

  // Absolute scope, start URL has different origin than scope URL.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"scope\": \"http://foo.com/land\", "
        "\"start_url\": \"http://bar.com/land/landing.html\" }",
        GURL("http://foo.com/manifest.json"),
        GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.scope.spec(),
              default_document_url.GetWithoutFilename().spec());
    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'start_url' ignored, should be same origin as document.",
        errors()[0]);
    EXPECT_EQ(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.",
        errors()[1]);
  }

  // scope and start URL have diferent origin than document URL.
  {
    GURL document_url("http://bar.com/index.html");
    Manifest manifest = ParseManifestWithURLs(
        "{ \"scope\": \"http://foo.com/land\", "
        "\"start_url\": \"http://foo.com/land/landing.html\" }",
        GURL("http://foo.com/manifest.json"), document_url);
    ASSERT_EQ(manifest.scope.spec(), document_url.GetWithoutFilename().spec());
    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'start_url' ignored, should be same origin as document.",
        errors()[0]);
    EXPECT_EQ("property 'scope' ignored, should be same origin as document.",
              errors()[1]);
  }

  // No start URL. Document URL is in scope.
  {
    Manifest manifest =
        ParseManifestWithURLs("{ \"scope\": \"http://foo.com/land\" }",
                              GURL("http://foo.com/manifest.json"),
                              GURL("http://foo.com/land/index.html"));
    ASSERT_EQ(manifest.scope.spec(), "http://foo.com/land");
    ASSERT_EQ(0u, GetErrorCount());
  }

  // No start URL. Document is out of scope.
  {
    GURL document_url("http://foo.com/index.html");
    Manifest manifest =
        ParseManifestWithURLs("{ \"scope\": \"http://foo.com/land\" }",
                              GURL("http://foo.com/manifest.json"),
                              GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.scope.spec(), document_url.GetWithoutFilename().spec());
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.",
        errors()[0]);
  }

  // Resolving has to happen based on the manifest_url.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"scope\": \"treasure\" }", GURL("http://foo.com/map/manifest.json"),
        GURL("http://foo.com/map/treasure/island/index.html"));
    ASSERT_EQ(manifest.scope.spec(), "http://foo.com/map/treasure");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope is parent directory.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"scope\": \"..\" }", GURL("http://foo.com/map/manifest.json"),
        GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.scope.spec(), "http://foo.com/");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope tries to go up past domain.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"scope\": \"../..\" }", GURL("http://foo.com/map/manifest.json"),
        GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.scope.spec(), "http://foo.com/");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope defaults to start_url with the filename, query, and fragment removed.
  {
    blink::Manifest manifest =
        ParseManifest("{ \"start_url\": \"land/landing.html\" }");
    ASSERT_EQ(manifest.scope.spec(),
              default_document_url.Resolve("land/").spec());
    EXPECT_EQ(0u, GetErrorCount());
  }

  {
    blink::Manifest manifest =
        ParseManifest("{ \"start_url\": \"land/land/landing.html\" }");
    ASSERT_EQ(manifest.scope.spec(),
              default_document_url.Resolve("land/land/").spec());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope defaults to document_url if start_url is not present.
  {
    blink::Manifest manifest = ParseManifest("{}");
    ASSERT_EQ(manifest.scope.spec(), default_document_url.Resolve(".").spec());
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, DisplayParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"browser\" }");
    EXPECT_EQ(manifest.display, kWebDisplayModeBrowser);
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"  browser  \" }");
    EXPECT_EQ(manifest.display, kWebDisplayModeBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"display\": {} }");
    EXPECT_EQ(manifest.display, kWebDisplayModeUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'display' ignored,"
        " type string expected.",
        errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"display\": 42 }");
    EXPECT_EQ(manifest.display, kWebDisplayModeUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'display' ignored,"
        " type string expected.",
        errors()[0]);
  }

  // Parse fails if string isn't known.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"browser_something\" }");
    EXPECT_EQ(manifest.display, kWebDisplayModeUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("unknown 'display' value ignored.", errors()[0]);
  }

  // Accept 'fullscreen'.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"fullscreen\" }");
    EXPECT_EQ(manifest.display, kWebDisplayModeFullscreen);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'fullscreen'.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"standalone\" }");
    EXPECT_EQ(manifest.display, kWebDisplayModeStandalone);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'minimal-ui'.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"minimal-ui\" }");
    EXPECT_EQ(manifest.display, kWebDisplayModeMinimalUi);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'browser'.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"browser\" }");
    EXPECT_EQ(manifest.display, kWebDisplayModeBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Case insensitive.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"BROWSER\" }");
    EXPECT_EQ(manifest.display, kWebDisplayModeBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, OrientationParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockNatural);
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockNatural);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": {} }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockDefault);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'orientation' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": 42 }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockDefault);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'orientation' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string isn't known.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"naturalish\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockDefault);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("unknown 'orientation' value ignored.", errors()[0]);
  }

  // Accept 'any'.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"any\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockAny);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'natural'.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockNatural);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape'.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"landscape\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockLandscape);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape-primary'.
  {
    Manifest manifest =
        ParseManifest("{ \"orientation\": \"landscape-primary\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockLandscapePrimary);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape-secondary'.
  {
    Manifest manifest =
        ParseManifest("{ \"orientation\": \"landscape-secondary\" }");
    EXPECT_EQ(manifest.orientation,
              kWebScreenOrientationLockLandscapeSecondary);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait'.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"portrait\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockPortrait);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait-primary'.
  {
    Manifest manifest =
        ParseManifest("{ \"orientation\": \"portrait-primary\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockPortraitPrimary);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait-secondary'.
  {
    Manifest manifest =
        ParseManifest("{ \"orientation\": \"portrait-secondary\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockPortraitSecondary);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Case insensitive.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"LANDSCAPE\" }");
    EXPECT_EQ(manifest.orientation, kWebScreenOrientationLockLandscape);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconsParseRules) {
  // Smoke test: if no icon, empty list.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [] }");
    EXPECT_EQ(manifest.icons.size(), 0u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty icon, empty list.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {} ] }");
    EXPECT_EQ(manifest.icons.size(), 0u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: icon with invalid src, empty list.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ { \"icons\": [] } ] }");
    EXPECT_EQ(manifest.icons.size(), 0u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if icon with empty src, it will be present in the list.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ { \"src\": \"\" } ] }");
    EXPECT_EQ(manifest.icons.size(), 1u);
    EXPECT_EQ(manifest.icons[0].src.spec(), "http://foo.com/manifest.json");
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if one icons with valid src, it will be present in the list.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [{ \"src\": \"foo.jpg\" }] }");
    EXPECT_EQ(manifest.icons.size(), 1u);
    EXPECT_EQ(manifest.icons[0].src.spec(), "http://foo.com/foo.jpg");
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconSrcParseRules) {
  // Smoke test.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"foo.png\" } ] }");
    EXPECT_EQ(manifest.icons[0].src.spec(),
              default_document_url.Resolve("foo.png").spec());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Whitespaces.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"   foo.png   \" } ] }");
    EXPECT_EQ(manifest.icons[0].src.spec(),
              default_document_url.Resolve("foo.png").spec());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": {} } ] }");
    EXPECT_TRUE(manifest.icons.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'src' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": 42 } ] }");
    EXPECT_TRUE(manifest.icons.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'src' ignored, type string expected.", errors()[0]);
  }

  // Resolving has to happen based on the document_url.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"icons\": [ {\"src\": \"icons/foo.png\" } ] }",
        GURL("http://foo.com/landing/index.html"), default_manifest_url);
    EXPECT_EQ(manifest.icons[0].src.spec(),
              "http://foo.com/landing/icons/foo.png");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconTypeParseRules) {
  // Smoke test.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": \"foo\" } ] }");
    EXPECT_TRUE(base::EqualsASCII(manifest.icons[0].type, "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        " \"type\": \"  foo  \" } ] }");
    EXPECT_TRUE(base::EqualsASCII(manifest.icons[0].type, "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": {} } ] }");
    EXPECT_TRUE(manifest.icons[0].type.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'type' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": 42 } ] }");
    EXPECT_TRUE(manifest.icons[0].type.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'type' ignored, type string expected.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, IconSizesParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42x42\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"  42x42  \" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Ignore sizes if property isn't a string.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": {} } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'sizes' ignored, type string expected.", errors()[0]);
  }

  // Ignore sizes if property isn't a string.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": 42 } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'sizes' ignored, type string expected.", errors()[0]);
  }

  // Smoke test: value correctly parsed.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42x42  48x48\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(manifest.icons[0].sizes[1], gfx::Size(48, 48));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // <WIDTH>'x'<HEIGHT> and <WIDTH>'X'<HEIGHT> are equivalent.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42X42  48X48\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(manifest.icons[0].sizes[1], gfx::Size(48, 48));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Twice the same value is parsed twice.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42X42  42x42\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(manifest.icons[0].sizes[1], gfx::Size(42, 42));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Width or height can't start with 0.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"004X007  042x00\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("found icon with no valid size.", errors()[0]);
  }

  // Width and height MUST contain digits.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"e4X1.0  55ax1e10\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("found icon with no valid size.", errors()[0]);
  }

  // 'any' is correctly parsed and transformed to gfx::Size(0,0).
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"any AnY ANY aNy\" } ] }");
    gfx::Size any = gfx::Size(0, 0);
    EXPECT_EQ(manifest.icons[0].sizes.size(), 4u);
    EXPECT_EQ(manifest.icons[0].sizes[0], any);
    EXPECT_EQ(manifest.icons[0].sizes[1], any);
    EXPECT_EQ(manifest.icons[0].sizes[2], any);
    EXPECT_EQ(manifest.icons[0].sizes[3], any);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Some invalid width/height combinations.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"x 40xx 1x2x3 x42 42xx42\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("found icon with no valid size.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, IconPurposeParseRules) {
  const String kPurposeParseStringError =
      "property 'purpose' ignored, type string expected.";
  const String kPurposeInvalidValueError =
      "found icon with no valid purpose; ignoring it.";
  const String kSomeInvalidPurposeError =
      "found icon with one or more invalid purposes; those purposes are "
      "ignored.";

  // Smoke test.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"any\" } ] }");
    EXPECT_EQ(manifest.icons[0].purpose.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim leading and trailing whitespaces.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"  any  \" } ] }");
    EXPECT_EQ(manifest.icons[0].purpose.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // 'any' is added when property isn't present.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\" } ] }");
    EXPECT_EQ(manifest.icons[0].purpose.size(), 1u);
    EXPECT_EQ(manifest.icons[0].purpose[0],
              Manifest::ImageResource::Purpose::ANY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // 'any' is added with error message when property isn't a string (is a
  // number).
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": 42 } ] }");
    EXPECT_EQ(manifest.icons[0].purpose.size(), 1u);
    EXPECT_EQ(manifest.icons[0].purpose[0],
              Manifest::ImageResource::Purpose::ANY);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeParseStringError, errors()[0]);
  }

  // 'any' is added with error message when property isn't a string (is a
  // dictionary).
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": {} } ] }");
    EXPECT_EQ(manifest.icons[0].purpose.size(), 1u);
    EXPECT_EQ(manifest.icons[0].purpose[0],
              Manifest::ImageResource::Purpose::ANY);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeParseStringError, errors()[0]);
  }

  // Smoke test: values correctly parsed.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"Any Badge Maskable\" } ] }");
    ASSERT_EQ(manifest.icons[0].purpose.size(), 3u);
    EXPECT_EQ(manifest.icons[0].purpose[0],
              Manifest::ImageResource::Purpose::ANY);
    EXPECT_EQ(manifest.icons[0].purpose[1],
              Manifest::ImageResource::Purpose::BADGE);
    EXPECT_EQ(manifest.icons[0].purpose[2],
              Manifest::ImageResource::Purpose::MASKABLE);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces between values.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"  Any   Badge  \" } ] }");
    ASSERT_EQ(manifest.icons[0].purpose.size(), 2u);
    EXPECT_EQ(manifest.icons[0].purpose[0],
              Manifest::ImageResource::Purpose::ANY);
    EXPECT_EQ(manifest.icons[0].purpose[1],
              Manifest::ImageResource::Purpose::BADGE);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Twice the same value is parsed twice.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"badge badge\" } ] }");
    ASSERT_EQ(manifest.icons[0].purpose.size(), 2u);
    EXPECT_EQ(manifest.icons[0].purpose[0],
              Manifest::ImageResource::Purpose::BADGE);
    EXPECT_EQ(manifest.icons[0].purpose[1],
              Manifest::ImageResource::Purpose::BADGE);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Invalid icon purpose is ignored.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"badge fizzbuzz\" } ] }");
    ASSERT_EQ(manifest.icons[0].purpose.size(), 1u);
    EXPECT_EQ(manifest.icons[0].purpose[0],
              Manifest::ImageResource::Purpose::BADGE);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kSomeInvalidPurposeError, errors()[0]);
  }

  // If developer-supplied purpose is invalid, entire icon is removed.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"fizzbuzz\" } ] }");
    ASSERT_EQ(0u, manifest.icons.size());
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeInvalidValueError, errors()[0]);
  }

  // Two icons, one with an invalid purpose and the other normal.
  {
    Manifest manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\", \"purpose\": \"fizzbuzz\" }, "
        "               {\"src\": \"\" }] }");
    ASSERT_EQ(1u, manifest.icons.size());
    ASSERT_EQ(manifest.icons[0].purpose.size(), 1u);
    EXPECT_EQ(manifest.icons[0].purpose[0],
              Manifest::ImageResource::Purpose::ANY);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeInvalidValueError, errors()[0]);
  }
}

TEST_F(ManifestParserTest, FileHandlerParseRules) {
  // Does not contain file_handler field.
  {
    Manifest manifest = ParseManifest("{ }");
    EXPECT_FALSE(manifest.file_handler.has_value());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Contains empty file_handler field.
  {
    Manifest manifest = ParseManifest("{ \"file_handler\": { } }");
    EXPECT_FALSE(manifest.file_handler.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'file_handler' ignored. Property 'action' is invalid.",
              errors()[0]);
  }

  // Contains file_handler field but no file handlers.
  {
    Manifest manifest =
        ParseManifest("{ \"file_handler\": { \"action\": \"/files\" } }");
    EXPECT_FALSE(manifest.file_handler.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("no file handlers were specified.", errors()[0]);
  }

  // Contains file_handler field but files list is empty.
  {
    Manifest manifest = ParseManifest(
        "{ \"file_handler\": { \"action\": \"/files\", \"files\": [] } }");
    EXPECT_FALSE(manifest.file_handler.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("no file handlers were specified.", errors()[0]);
  }

  // Invalid action causes parsing to fail.
  {
    Manifest manifest = ParseManifest(
        "{"
        "  \"file_handler\": {"
        "    \"files\": ["
        "      {"
        "        \"name\": \"name\", "
        "        \"accept\": \"image/png\""
        "      }"
        "    ]"
        "  }"
        "}");
    manifest.scope = GURL("http://frobnicate.notatld");
    EXPECT_FALSE(manifest.file_handler.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'file_handler' ignored. Property 'action' is invalid.",
              errors()[0]);
  }

  // Single accept value can be parsed from string.
  {
    Manifest manifest = ParseManifest(
        "{"
        "  \"file_handler\": {"
        "    \"files\": ["
        "      {"
        "        \"name\": \"name\", "
        "        \"accept\": \"image/png\""
        "      }"
        "    ], "
        "    \"action\": \"/files\""
        "  }"
        "}");
    EXPECT_TRUE(manifest.file_handler.has_value());

    auto file_handler = manifest.file_handler.value();
    EXPECT_EQ(file_handler.action, GURL("http://foo.com/files"));
    EXPECT_EQ(file_handler.files.size(), 1u);
    EXPECT_EQ(file_handler.files[0].name, base::ASCIIToUTF16("name"));
    EXPECT_EQ(file_handler.files[0].accept.size(), 1u);
    EXPECT_EQ(file_handler.files[0].accept[0], base::ASCIIToUTF16("image/png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Single accept value can be parsed from list.
  {
    Manifest manifest = ParseManifest(
        "{"
        "  \"file_handler\": {"
        "    \"action\": \"/files\", "
        "    \"files\": ["
        "      {"
        "        \"name\": \"name\", "
        "        \"accept\": ["
        "          \"image/png\""
        "        ]"
        "      }"
        "    ]"
        "  }"
        "}");
    EXPECT_TRUE(manifest.file_handler.has_value());

    auto file_handler = manifest.file_handler.value();
    EXPECT_EQ(file_handler.action, GURL("http://foo.com/files"));
    EXPECT_EQ(file_handler.files.size(), 1u);
    EXPECT_EQ(file_handler.files[0].name, base::ASCIIToUTF16("name"));
    EXPECT_EQ(file_handler.files[0].accept.size(), 1u);
    EXPECT_EQ(file_handler.files[0].accept[0], base::ASCIIToUTF16("image/png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Multiple accept values can be parsed.
  {
    Manifest manifest = ParseManifest(
        "{"
        "  \"file_handler\": {"
        "    \"action\": \"/files\", "
        "    \"files\": ["
        "      {"
        "        \"name\": \"name\", "
        "        \"accept\": ["
        "          \"image/png\", "
        "          \".png\""
        "        ]"
        "      }"
        "    ]"
        "  }"
        "}");
    EXPECT_TRUE(manifest.file_handler.has_value());

    auto file_handler = manifest.file_handler.value();
    EXPECT_EQ(file_handler.action, GURL("http://foo.com/files"));
    EXPECT_EQ(file_handler.files.size(), 1u);
    EXPECT_EQ(file_handler.files[0].name, base::ASCIIToUTF16("name"));
    EXPECT_EQ(file_handler.files[0].accept.size(), 2u);
    EXPECT_EQ(file_handler.files[0].accept[0], base::ASCIIToUTF16("image/png"));
    EXPECT_EQ(file_handler.files[0].accept[1], base::ASCIIToUTF16(".png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Multiple file handlers can be parsed.
  {
    Manifest manifest = ParseManifest(
        "{"
        "  \"file_handler\": {"
        "    \"action\": \"/files\", "
        "    \"files\": ["
        "      {"
        "        \"name\": \"name\", "
        "        \"accept\": ["
        "          \"image/png\", "
        "          \".png\""
        "        ]"
        "      }, "
        "      {"
        "        \"name\": \"svgish\", "
        "        \"accept\": ["
        "          \".svg\","
        "          \"xml/svg\""
        "        ]"
        "      }"
        "    ]"
        "  }"
        "}");
    EXPECT_TRUE(manifest.file_handler.has_value());

    auto file_handler = manifest.file_handler.value();
    EXPECT_EQ(file_handler.action, GURL("http://foo.com/files"));
    EXPECT_EQ(file_handler.files.size(), 2u);
    EXPECT_EQ(file_handler.files[0].name, base::ASCIIToUTF16("name"));
    EXPECT_EQ(file_handler.files[0].accept.size(), 2u);
    EXPECT_EQ(file_handler.files[0].accept[0], base::ASCIIToUTF16("image/png"));
    EXPECT_EQ(file_handler.files[0].accept[1], base::ASCIIToUTF16(".png"));
    EXPECT_EQ(file_handler.files[1].name, base::ASCIIToUTF16("svgish"));
    EXPECT_EQ(file_handler.files[1].accept.size(), 2u);
    EXPECT_EQ(file_handler.files[1].accept[0], base::ASCIIToUTF16(".svg"));
    EXPECT_EQ(file_handler.files[1].accept[1], base::ASCIIToUTF16("xml/svg"));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ShareTargetParseRules) {
  // Contains share_target field but no keys.
  {
    Manifest manifest = ParseManifest("{ \"share_target\": {} }");
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[0]);
  }

  // Contains share_target field but no params key.
  {
    Manifest manifest =
        ParseManifest("{ \"share_target\": { \"action\": \"\" } }");
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.",
        errors()[2]);
  }

  // Contains share_target field but no action key.
  {
    Manifest manifest =
        ParseManifest("{ \"share_target\": { \"params\": {} } }");
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[0]);
  }

  // Key in share_target that isn't valid.
  {
    Manifest manifest = ParseManifest(
        "{ \"share_target\": {\"incorrect_key\": \"some_value\" } }");
    ASSERT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShareTargetUrlTemplateParseRules) {
  GURL manifest_url = GURL("https://foo.com/manifest.json");
  GURL document_url = GURL("https://foo.com/index.html");

  // Contains share_target, but action is empty.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"\", \"params\": {} } }",
        manifest_url, document_url);
    ASSERT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->action.spec(), manifest_url.spec());
    EXPECT_TRUE(manifest.share_target->params.text.is_null());
    EXPECT_TRUE(manifest.share_target->params.title.is_null());
    EXPECT_TRUE(manifest.share_target->params.url.is_null());
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Parse but throw an error if url_template property isn't a string.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"\", \"params\": {} } }",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->action.spec(), manifest_url.spec());
    EXPECT_TRUE(manifest.share_target->params.text.is_null());
    EXPECT_TRUE(manifest.share_target->params.title.is_null());
    EXPECT_TRUE(manifest.share_target->params.url.is_null());
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Don't parse if action property isn't a string.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": {}, \"params\": {} } }",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[1]);
  }

  // Don't parse if action property isn't a string.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": 42, \"params\": {} } }",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[1]);
  }

  // Don't parse if params property isn't a dict.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"\", \"params\": \"\" } }",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.",
        errors()[2]);
  }

  // Don't parse if params property isn't a dict.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"\", \"params\": 42 } }",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.",
        errors()[2]);
  }

  // Ignore params keys with invalid types.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"\", \"params\": { \"text\": 42 }"
        " } }",
        manifest_url, document_url);
    ASSERT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->action.spec(), manifest_url.spec());
    EXPECT_TRUE(manifest.share_target->params.text.is_null());
    EXPECT_TRUE(manifest.share_target->params.title.is_null());
    EXPECT_TRUE(manifest.share_target->params.url.is_null());
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ("property 'text' ignored, type string expected.", errors()[2]);
  }

  // Ignore params keys with invalid types.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"\", "
        "\"params\": { \"title\": 42 } } }",
        manifest_url, document_url);
    ASSERT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->action.spec(), manifest_url.spec());
    EXPECT_TRUE(manifest.share_target->params.text.is_null());
    EXPECT_TRUE(manifest.share_target->params.title.is_null());
    EXPECT_TRUE(manifest.share_target->params.url.is_null());
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ("property 'title' ignored, type string expected.", errors()[2]);
  }

  // Don't parse if params property has keys with invalid types.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"\", \"params\": { \"url\": {}, "
        "\"text\": \"hi\" } } }",
        manifest_url, document_url);
    ASSERT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->action.spec(), manifest_url.spec());
    EXPECT_TRUE(
        base::EqualsASCII(manifest.share_target->params.text.string(), "hi"));
    EXPECT_TRUE(manifest.share_target->params.title.is_null());
    EXPECT_TRUE(manifest.share_target->params.url.is_null());
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ("property 'url' ignored, type string expected.", errors()[2]);
  }

  // Don't parse if action property isn't a valid URL.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com:a\", \"params\": "
        "{} } }",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, URL is invalid.", errors()[0]);
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[1]);
  }

  // Fail parsing if action is at a different origin than the Web
  // Manifest.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo2.com/\" }, "
        "\"params\": {} }",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, should be same origin as document.",
              errors()[0]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'action' is "
        "invalid.",
        errors()[1]);
  }

  // Smoke test: Contains share_target and action, and action is valid.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": {\"action\": \"share/\", \"params\": {} } }",
        manifest_url, document_url);
    ASSERT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->action.spec(), "https://foo.com/share/");
    EXPECT_TRUE(manifest.share_target->params.text.is_null());
    EXPECT_TRUE(manifest.share_target->params.title.is_null());
    EXPECT_TRUE(manifest.share_target->params.url.is_null());
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Smoke test: Contains share_target and action, and action is valid, params
  // is populated.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": {\"action\": \"share/\", \"params\": { \"text\": "
        "\"foo\", \"title\": \"bar\", \"url\": \"baz\" } } }",
        manifest_url, document_url);
    ASSERT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->action.spec(), "https://foo.com/share/");
    EXPECT_TRUE(
        base::EqualsASCII(manifest.share_target->params.text.string(), "foo"));
    EXPECT_TRUE(
        base::EqualsASCII(manifest.share_target->params.title.string(), "bar"));
    EXPECT_TRUE(
        base::EqualsASCII(manifest.share_target->params.url.string(), "baz"));
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Backwards compatibility test: Contains share_target, url_template and
  // action, and action is valid, params is populated.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"url_template\": "
        "\"foo.com/share?title={title}\", "
        "\"action\": \"share/\", \"params\": { \"text\": "
        "\"foo\", \"title\": \"bar\", \"url\": \"baz\" } } }",
        manifest_url, document_url);
    ASSERT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->action.spec(), "https://foo.com/share/");
    EXPECT_TRUE(
        base::EqualsASCII(manifest.share_target->params.text.string(), "foo"));
    EXPECT_TRUE(
        base::EqualsASCII(manifest.share_target->params.title.string(), "bar"));
    EXPECT_TRUE(
        base::EqualsASCII(manifest.share_target->params.url.string(), "baz"));
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Smoke test: Contains share_target, action and params. action is
  // valid and is absolute.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    ASSERT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target.value().action.spec(), "https://foo.com/#");
    EXPECT_TRUE(manifest.share_target->params.text.is_null());
    EXPECT_TRUE(base::EqualsASCII(manifest.share_target->params.title.string(),
                                  "mytitle"));
    EXPECT_TRUE(manifest.share_target->params.url.is_null());
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Return undefined if method or enctype is not string.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "10, \"enctype\": 10, \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid method. Allowed methods are:"
        "GET and POST.",
        errors()[0]);
  }

  // Valid method and enctype.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"GET\", \"enctype\": \"application/x-www-form-urlencoded\", "
        "\"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->method,
              Manifest::ShareTarget::Method::kGet);
    EXPECT_EQ(manifest.share_target->enctype,
              Manifest::ShareTarget::Enctype::kApplication);
  }

  // Auto-fill in "GET" for method and "application/x-www-form-urlencoded" for
  // enctype.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->method,
              Manifest::ShareTarget::Method::kGet);
    EXPECT_EQ(manifest.share_target->enctype,
              Manifest::ShareTarget::Enctype::kApplication);
  }

  // Invalid method values, return undefined.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"\", \"enctype\": \"application/x-www-form-urlencoded\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid method. Allowed methods are:"
        "GET and POST.",
        errors()[0]);
  }

  // When method is "GET", enctype cannot be anything other than
  // "application/x-www-form-urlencoded".
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"GET\", \"enctype\": \"RANDOM\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid enctype. Allowed enctypes are:"
        "application/x-www-form-urlencoded and multipart/form-data.",
        errors()[0]);
  }

  // When method is "POST", enctype cannot be anything other than
  // "application/x-www-form-urlencoded" or "multipart/form-data".
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"random\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid enctype. Allowed enctypes are:"
        "application/x-www-form-urlencoded and multipart/form-data.",
        errors()[0]);
  }

  // Valid enctype for when method is "POST".
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"application/x-www-form-urlencoded\", "
        "\"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->method,
              Manifest::ShareTarget::Method::kPost);
    EXPECT_EQ(manifest.share_target->enctype,
              Manifest::ShareTarget::Enctype::kApplication);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Valid enctype for when method is "POST".
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->method,
              Manifest::ShareTarget::Method::kPost);
    EXPECT_EQ(manifest.share_target->enctype,
              Manifest::ShareTarget::Enctype::kMultipart);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Ascii in-sensitive.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"PosT\", \"enctype\": \"mUltIparT/Form-dAta\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->method,
              Manifest::ShareTarget::Method::kPost);
    EXPECT_EQ(manifest.share_target->enctype,
              Manifest::ShareTarget::Enctype::kMultipart);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // No files is okay.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [] } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->method,
              Manifest::ShareTarget::Method::kPost);
    EXPECT_EQ(manifest.share_target->enctype,
              Manifest::ShareTarget::Enctype::kMultipart);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Nonempty file must have POST method and multipart/form-data enctype.
  // GET method, for example, will cause an error in this case.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"GET\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"text/plain\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid enctype for GET method. Only "
        "application/x-www-form-urlencoded is allowed.",
        errors()[0]);
  }

  // Nonempty file must have POST method and multipart/form-data enctype.
  // Enctype other than multipart/form-data will cause an error.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"application/x-www-form-urlencoded\", "
        "\"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"text/plain\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("files are only supported with multipart/form-data POST.",
              errors()[0]);
  }

  // Nonempty file must have POST method and multipart/form-data enctype.
  // This case is valid.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"text/plain\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(1u, manifest.share_target->params.files.size());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Invalid mimetype.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"helloworld\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"^$/@$\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"/\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\" \"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest.share_target.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Accept field is empty.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": []}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());
    EXPECT_EQ(manifest.share_target->params.files.size(), 0u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept sequence contains non-string elements.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": [{"
        "        \"name\": \"name\","
        "        \"accept\": [\"image/png\", 42]"
        "      }]"
        "    }"
        "  }"
        "}",
        manifest_url, document_url);
    const base::Optional<Manifest::ShareTarget> share_target =
        manifest.share_target;
    EXPECT_TRUE(share_target.has_value());

    const std::vector<Manifest::FileFilter>& files = share_target->params.files;
    EXPECT_EQ(1u, files.size());
    EXPECT_TRUE(base::EqualsASCII(files.at(0).name, "name"));

    const std::vector<base::string16>& accept = files.at(0).accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_TRUE(base::EqualsASCII(accept.at(0), "image/png"));

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("'accept' entry ignored, expected to be of type string.",
              errors()[0]);
  }

  // Accept is just a single string.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": [{"
        "        \"name\": \"name\","
        "        \"accept\": \"image/png\""
        "      }]"
        "    }"
        "  }"
        "}",
        manifest_url, document_url);
    const base::Optional<Manifest::ShareTarget> share_target =
        manifest.share_target;
    EXPECT_TRUE(share_target.has_value());

    const std::vector<Manifest::FileFilter>& files = share_target->params.files;
    EXPECT_EQ(1u, files.size());
    EXPECT_TRUE(base::EqualsASCII(files.at(0).name, "name"));

    const std::vector<base::string16>& accept = files.at(0).accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_TRUE(base::EqualsASCII(accept.at(0), "image/png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept is neither a string nor an array of strings.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": [{"
        "        \"name\": \"name\","
        "        \"accept\": true"
        "      }]"
        "    }"
        "  }"
        "}",
        manifest_url, document_url);
    const base::Optional<Manifest::ShareTarget> share_target =
        manifest.share_target;
    EXPECT_TRUE(share_target.has_value());

    const std::vector<Manifest::FileFilter>& files = share_target->params.files;
    EXPECT_EQ(0u, files.size());

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'accept' ignored, type array or string expected.",
              errors()[0]);
  }

  // Files is just a single FileFilter (not an array).
  {
    Manifest manifest = ParseManifestWithURLs(
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": {"
        "        \"name\": \"name\","
        "        \"accept\": \"image/png\""
        "      }"
        "    }"
        "  }"
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest.share_target.has_value());

    const Manifest::ShareTargetParams& params = manifest.share_target->params;
    EXPECT_EQ(1u, params.files.size());
    EXPECT_TRUE(base::EqualsASCII(params.files.at(0).name, "name"));

    const std::vector<base::string16>& accept = params.files.at(0).accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_TRUE(base::EqualsASCII(accept.at(0), "image/png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Files is neither array nor FileFilter.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": 3"
        "    }"
        "  }"
        "}",
        manifest_url, document_url);
    const base::Optional<Manifest::ShareTarget> share_target =
        manifest.share_target;
    EXPECT_TRUE(share_target.has_value());

    const std::vector<Manifest::FileFilter>& files = share_target->params.files;
    EXPECT_EQ(0u, files.size());

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'files' ignored, type array or FileFilter expected.",
              errors()[0]);
  }

  // Files contains a non-dictionary entry.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": ["
        "        {"
        "          \"name\": \"name\","
        "          \"accept\": \"image/png\""
        "        },"
        "        3"
        "      ]"
        "    }"
        "  }"
        "}",
        manifest_url, document_url);
    const base::Optional<Manifest::ShareTarget> share_target =
        manifest.share_target;
    EXPECT_TRUE(share_target.has_value());

    const std::vector<Manifest::FileFilter>& files = share_target->params.files;
    EXPECT_EQ(1u, files.size());
    EXPECT_TRUE(base::EqualsASCII(files.at(0).name, "name"));

    const std::vector<base::string16>& accept = files.at(0).accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_TRUE(base::EqualsASCII(accept.at(0), "image/png"));

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("files must be a sequence of non-empty file entries.",
              errors()[0]);
  }

  // Files contains empty file.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": ["
        "        {"
        "          \"name\": \"name\","
        "          \"accept\": \"image/png\""
        "        },"
        "        {}"
        "      ]"
        "    }"
        "  }"
        "}",
        manifest_url, document_url);
    const base::Optional<Manifest::ShareTarget> share_target =
        manifest.share_target;
    EXPECT_TRUE(share_target.has_value());

    const std::vector<Manifest::FileFilter>& files = share_target->params.files;
    EXPECT_EQ(1u, files.size());
    EXPECT_TRUE(base::EqualsASCII(files.at(0).name, "name"));

    const std::vector<base::string16>& accept = files.at(0).accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_TRUE(base::EqualsASCII(accept.at(0), "image/png"));

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' missing.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, RelatedApplicationsParseRules) {
  // If no application, empty list.
  {
    Manifest manifest = ParseManifest("{ \"related_applications\": []}");
    EXPECT_EQ(manifest.related_applications.size(), 0u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // If empty application, empty list.
  {
    Manifest manifest = ParseManifest("{ \"related_applications\": [{}]}");
    EXPECT_EQ(manifest.related_applications.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("'platform' is a required field, related application ignored.",
              errors()[0]);
  }

  // If invalid platform, application is ignored.
  {
    Manifest manifest =
        ParseManifest("{ \"related_applications\": [{\"platform\": 123}]}");
    EXPECT_EQ(manifest.related_applications.size(), 0u);
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'platform' ignored, type string expected.",
              errors()[0]);
    EXPECT_EQ(
        "'platform' is a required field, "
        "related application ignored.",
        errors()[1]);
  }

  // If missing platform, application is ignored.
  {
    Manifest manifest =
        ParseManifest("{ \"related_applications\": [{\"id\": \"foo\"}]}");
    EXPECT_EQ(manifest.related_applications.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("'platform' is a required field, related application ignored.",
              errors()[0]);
  }

  // If missing id and url, application is ignored.
  {
    Manifest manifest = ParseManifest(
        "{ \"related_applications\": [{\"platform\": \"play\"}]}");
    EXPECT_EQ(manifest.related_applications.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("one of 'url' or 'id' is required, related application ignored.",
              errors()[0]);
  }

  // Valid application, with url.
  {
    Manifest manifest = ParseManifest(
        "{ \"related_applications\": ["
        "{\"platform\": \"play\", \"url\": \"http://www.foo.com\"}]}");
    EXPECT_EQ(manifest.related_applications.size(), 1u);
    EXPECT_TRUE(base::EqualsASCII(
        manifest.related_applications[0].platform.string(), "play"));
    EXPECT_EQ(manifest.related_applications[0].url.spec(),
              "http://www.foo.com/");
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Application with an invalid url.
  {
    Manifest manifest = ParseManifest(
        "{ \"related_applications\": ["
        "{\"platform\": \"play\", \"url\": \"http://www.foo.com:co&uk\"}]}");
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, URL is invalid.", errors()[0]);
    EXPECT_EQ("one of 'url' or 'id' is required, related application ignored.",
              errors()[1]);
  }

  // Valid application, with id.
  {
    Manifest manifest = ParseManifest(
        "{ \"related_applications\": ["
        "{\"platform\": \"itunes\", \"id\": \"foo\"}]}");
    EXPECT_EQ(manifest.related_applications.size(), 1u);
    EXPECT_TRUE(base::EqualsASCII(
        manifest.related_applications[0].platform.string(), "itunes"));
    EXPECT_TRUE(
        base::EqualsASCII(manifest.related_applications[0].id.string(), "foo"));
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // All valid applications are in list.
  {
    Manifest manifest = ParseManifest(
        "{ \"related_applications\": ["
        "{\"platform\": \"play\", \"id\": \"foo\"},"
        "{\"platform\": \"itunes\", \"id\": \"bar\"}]}");
    EXPECT_EQ(manifest.related_applications.size(), 2u);
    EXPECT_TRUE(base::EqualsASCII(
        manifest.related_applications[0].platform.string(), "play"));
    EXPECT_TRUE(
        base::EqualsASCII(manifest.related_applications[0].id.string(), "foo"));
    EXPECT_TRUE(base::EqualsASCII(
        manifest.related_applications[1].platform.string(), "itunes"));
    EXPECT_TRUE(
        base::EqualsASCII(manifest.related_applications[1].id.string(), "bar"));
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Two invalid applications and one valid. Only the valid application should
  // be in the list.
  {
    Manifest manifest = ParseManifest(
        "{ \"related_applications\": ["
        "{\"platform\": \"itunes\"},"
        "{\"platform\": \"play\", \"id\": \"foo\"},"
        "{}]}");
    EXPECT_EQ(manifest.related_applications.size(), 1u);
    EXPECT_TRUE(base::EqualsASCII(
        manifest.related_applications[0].platform.string(), "play"));
    EXPECT_TRUE(
        base::EqualsASCII(manifest.related_applications[0].id.string(), "foo"));
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("one of 'url' or 'id' is required, related application ignored.",
              errors()[0]);
    EXPECT_EQ("'platform' is a required field, related application ignored.",
              errors()[1]);
  }
}

TEST_F(ManifestParserTest, ParsePreferRelatedApplicationsParseRules) {
  // Smoke test.
  {
    Manifest manifest =
        ParseManifest("{ \"prefer_related_applications\": true }");
    EXPECT_TRUE(manifest.prefer_related_applications);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if the property isn't a boolean.
  {
    Manifest manifest =
        ParseManifest("{ \"prefer_related_applications\": {} }");
    EXPECT_FALSE(manifest.prefer_related_applications);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'prefer_related_applications' "
        "ignored, type boolean expected.",
        errors()[0]);
  }
  {
    Manifest manifest =
        ParseManifest("{ \"prefer_related_applications\": \"true\" }");
    EXPECT_FALSE(manifest.prefer_related_applications);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'prefer_related_applications' "
        "ignored, type boolean expected.",
        errors()[0]);
  }
  {
    Manifest manifest = ParseManifest("{ \"prefer_related_applications\": 1 }");
    EXPECT_FALSE(manifest.prefer_related_applications);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'prefer_related_applications' "
        "ignored, type boolean expected.",
        errors()[0]);
  }

  // "False" should set the boolean false without throwing errors.
  {
    Manifest manifest =
        ParseManifest("{ \"prefer_related_applications\": false }");
    EXPECT_FALSE(manifest.prefer_related_applications);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ThemeColorParserRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"#FF0000\" }");
    EXPECT_EQ(*manifest.theme_color, 0xFFFF0000u);
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"  blue   \" }");
    EXPECT_EQ(*manifest.theme_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if theme_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": {} }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": false }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": null }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": [] }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": 42 }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"foo(bar)\" }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored,"
        " 'foo(bar)' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"bleu\" }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, 'bleu' is not a valid color.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"FF00FF\" }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, 'FF00FF'"
        " is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for theme_color are given.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"#ABC #DEF\" }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, "
        "'#ABC #DEF' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for theme_color are given.
  {
    Manifest manifest =
        ParseManifest("{ \"theme_color\": \"#AABBCC #DDEEFF\" }");
    EXPECT_FALSE(manifest.theme_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, "
        "'#AABBCC #DDEEFF' is not a valid color.",
        errors()[0]);
  }

  // Accept CSS color keyword format.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"blue\" }");
    EXPECT_EQ(*manifest.theme_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS color keyword format.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"chartreuse\" }");
    EXPECT_EQ(*manifest.theme_color, 0xFF7FFF00u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"#FFF\" }");
    EXPECT_EQ(*manifest.theme_color, 0xFFFFFFFFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"#ABC\" }");
    EXPECT_EQ(*manifest.theme_color, 0xFFAABBCCu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RRGGBB format.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"#FF0000\" }");
    EXPECT_EQ(*manifest.theme_color, 0xFFFF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept translucent colors.
  {
    Manifest manifest = ParseManifest(
        "{ \"theme_color\": \"rgba(255,0,0,"
        "0.4)\" }");
    EXPECT_EQ(*manifest.theme_color, 0x66FF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept transparent colors.
  {
    Manifest manifest = ParseManifest("{ \"theme_color\": \"rgba(0,0,0,0)\" }");
    EXPECT_EQ(*manifest.theme_color, 0x00000000u);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, BackgroundColorParserRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": \"#FF0000\" }");
    EXPECT_EQ(*manifest.background_color, 0xFFFF0000u);
    EXPECT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest =
        ParseManifest("{ \"background_color\": \"  blue   \" }");
    EXPECT_EQ(*manifest.background_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if background_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": {} }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": false }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": null }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": [] }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": 42 }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": \"foo(bar)\" }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'foo(bar)' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": \"bleu\" }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'bleu' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": \"FF00FF\" }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'FF00FF' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for background_color are given.
  {
    Manifest manifest =
        ParseManifest("{ \"background_color\": \"#ABC #DEF\" }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored, "
        "'#ABC #DEF' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for background_color are given.
  {
    Manifest manifest =
        ParseManifest("{ \"background_color\": \"#AABBCC #DDEEFF\" }");
    EXPECT_FALSE(manifest.background_color.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored, "
        "'#AABBCC #DDEEFF' is not a valid color.",
        errors()[0]);
  }

  // Accept CSS color keyword format.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": \"blue\" }");
    EXPECT_EQ(*manifest.background_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS color keyword format.
  {
    Manifest manifest =
        ParseManifest("{ \"background_color\": \"chartreuse\" }");
    EXPECT_EQ(*manifest.background_color, 0xFF7FFF00u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": \"#FFF\" }");
    EXPECT_EQ(*manifest.background_color, 0xFFFFFFFFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": \"#ABC\" }");
    EXPECT_EQ(*manifest.background_color, 0xFFAABBCCu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RRGGBB format.
  {
    Manifest manifest = ParseManifest("{ \"background_color\": \"#FF0000\" }");
    EXPECT_EQ(*manifest.background_color, 0xFFFF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept translucent colors.
  {
    Manifest manifest = ParseManifest(
        "{ \"background_color\": \"rgba(255,0,0,"
        "0.4)\" }");
    EXPECT_EQ(*manifest.background_color, 0x66FF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept transparent colors.
  {
    Manifest manifest = ParseManifest(
        "{ \"background_color\": \"rgba(0,0,0,"
        "0)\" }");
    EXPECT_EQ(*manifest.background_color, 0x00000000u);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, SplashScreenUrlParseRules) {
  // Smoke test.
  {
    Manifest manifest =
        ParseManifest("{ \"splash_screen_url\": \"splash.html\" }");
    ASSERT_EQ(manifest.splash_screen_url.spec(),
              default_document_url.Resolve("splash.html").spec());
    ASSERT_FALSE(manifest.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Whitespaces.
  {
    Manifest manifest =
        ParseManifest("{ \"splash_screen_url\": \"    splash.html\" }");
    ASSERT_EQ(manifest.splash_screen_url.spec(),
              default_document_url.Resolve("splash.html").spec());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"splash_screen_url\": {} }");
    ASSERT_TRUE(manifest.splash_screen_url.is_empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'splash_screen_url' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"splash_screen_url\": 42 }");
    ASSERT_TRUE(manifest.splash_screen_url.is_empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'splash_screen_url' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if property isn't a valid URL.
  {
    Manifest manifest =
        ParseManifest("{ \"splash_screen_url\": \"http://www.google.ca:a\" }");
    ASSERT_TRUE(manifest.splash_screen_url.is_empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'splash_screen_url' ignored, URL is invalid.",
              errors()[0]);
  }

  // Absolute splash_screen_url, same origin with document.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"splash_screen_url\": \"http://foo.com/splash.html\" }",
        GURL("http://foo.com/manifest.json"),
        GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.splash_screen_url.spec(), "http://foo.com/splash.html");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Absolute splash_screen_url, cross origin with document.
  {
    Manifest manifest = ParseManifestWithURLs(
        "{ \"splash_screen_url\": \"http://bar.com/splash.html\" }",
        GURL("http://foo.com/manifest.json"),
        GURL("http://foo.com/index.html"));
    ASSERT_TRUE(manifest.splash_screen_url.is_empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'splash_screen_url' ignored, should "
        "be same origin as document.",
        errors()[0]);
  }

  // Resolving has to happen based on the manifest_url.
  {
    Manifest manifest =
        ParseManifestWithURLs("{ \"splash_screen_url\": \"splash.html\" }",
                              GURL("http://foo.com/splashy/manifest.json"),
                              GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.splash_screen_url.spec(),
              "http://foo.com/splashy/splash.html");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, GCMSenderIDParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"gcm_sender_id\": \"foo\" }");
    EXPECT_TRUE(base::EqualsASCII(manifest.gcm_sender_id.string(), "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"gcm_sender_id\": \"  foo  \" }");
    EXPECT_TRUE(base::EqualsASCII(manifest.gcm_sender_id.string(), "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if the property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"gcm_sender_id\": {} }");
    EXPECT_TRUE(manifest.gcm_sender_id.is_null());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'gcm_sender_id' ignored, type string expected.",
              errors()[0]);
  }
  {
    Manifest manifest = ParseManifest("{ \"gcm_sender_id\": 42 }");
    EXPECT_TRUE(manifest.gcm_sender_id.is_null());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'gcm_sender_id' ignored, type string expected.",
              errors()[0]);
  }
}

}  // namespace blink
