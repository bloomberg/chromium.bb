// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_meta_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/media_query_list.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class HTMLMetaElementTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();

    RuntimeEnabledFeatures::SetDisplayCutoutAPIEnabled(true);
    RuntimeEnabledFeatures::SetMetaSupportedColorSchemesEnabled(true);
    RuntimeEnabledFeatures::SetMediaQueryPrefersColorSchemeEnabled(true);
    GetDocument().GetSettings()->SetViewportMetaEnabled(true);
  }

  mojom::ViewportFit LoadTestPageAndReturnViewportFit(const String& value) {
    LoadTestPageWithViewportFitValue(value);
    return GetDocument()
        .GetViewportData()
        .GetViewportDescription()
        .GetViewportFit();
  }

 protected:
  HTMLMetaElement* CreateSupportedColorSchemesMeta(
      const AtomicString& content) {
    auto* meta = MakeGarbageCollected<HTMLMetaElement>(GetDocument());
    meta->setAttribute(html_names::kNameAttr, "supported-color-schemes");
    meta->setAttribute(html_names::kContentAttr, content);
    return meta;
  }

  void SetSupportedColorSchemes(const AtomicString& content) {
    GetDocument().head()->AppendChild(CreateSupportedColorSchemesMeta(content));
  }

  bool SupportsColorScheme(ColorScheme scheme) const {
    return GetDocument().GetStyleEngine().GetSupportedColorSchemes().Contains(
        scheme);
  }

 private:
  void LoadTestPageWithViewportFitValue(const String& value) {
    GetDocument().documentElement()->SetInnerHTMLFromString(
        "<head>"
        "<meta name='viewport' content='viewport-fit=" +
        value +
        "'>"
        "</head>");
  }
};

TEST_F(HTMLMetaElementTest, ViewportFit_Auto) {
  EXPECT_EQ(mojom::ViewportFit::kAuto,
            LoadTestPageAndReturnViewportFit("auto"));
}

TEST_F(HTMLMetaElementTest, ViewportFit_Contain) {
  EXPECT_EQ(mojom::ViewportFit::kContain,
            LoadTestPageAndReturnViewportFit("contain"));
}

TEST_F(HTMLMetaElementTest, ViewportFit_Cover) {
  EXPECT_EQ(mojom::ViewportFit::kCover,
            LoadTestPageAndReturnViewportFit("cover"));
}

TEST_F(HTMLMetaElementTest, ViewportFit_Invalid) {
  EXPECT_EQ(mojom::ViewportFit::kAuto,
            LoadTestPageAndReturnViewportFit("invalid"));
}

TEST_F(HTMLMetaElementTest, SupportedColorSchemesProcessing_LastWins) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="supported-color-schemes" content="dark">
    <meta name="supported-color-schemes" content="light">
  )HTML");

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, SupportedColorSchemesProcessing_Remove) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="supported-color-schemes" content="dark">
    <meta id="last-meta" name="supported-color-schemes" content="light">
  )HTML");

  GetDocument().getElementById("last-meta")->remove();

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, SupportedColorSchemesProcessing_InsertBefore) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="supported-color-schemes" content="dark">
  )HTML");

  Element* head = GetDocument().head();
  head->insertBefore(CreateSupportedColorSchemesMeta("light"),
                     head->firstChild());

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, SupportedColorSchemesProcessing_AppendChild) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="supported-color-schemes" content="dark">
  )HTML");

  GetDocument().head()->AppendChild(CreateSupportedColorSchemesMeta("light"));

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, SupportedColorSchemesProcessing_SetAttribute) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta id="meta" name="supported-color-schemes" content="dark">
  )HTML");

  GetDocument().getElementById("meta")->setAttribute(html_names::kContentAttr,
                                                     "light");

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, SupportedColorSchemesProcessing_RemoveAttribute) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta id="meta" name="supported-color-schemes" content="dark">
  )HTML");

  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kDark));

  GetDocument().getElementById("meta")->removeAttribute(
      html_names::kContentAttr);

  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, SupportedColorSchemesParsing) {
  SetSupportedColorSchemes("");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));

  SetSupportedColorSchemes("light");
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));

  SetSupportedColorSchemes("dark");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kDark));

  SetSupportedColorSchemes("light dark");
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kDark));

  SetSupportedColorSchemes("light,dark");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));

  SetSupportedColorSchemes("light,");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));

  SetSupportedColorSchemes(",light");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));

  SetSupportedColorSchemes(", light");
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));

  SetSupportedColorSchemes("light, dark");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, SupportedColorSchemesForcedDarkeningAndMQ) {
  GetDocument().GetSettings()->SetPreferredColorScheme(
      PreferredColorScheme::kDark);

  auto* media_query = GetDocument().GetMediaQueryMatcher().MatchMedia(
      "(prefers-color-scheme: dark)");
  EXPECT_TRUE(media_query->matches());
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  EXPECT_FALSE(media_query->matches());

  SetSupportedColorSchemes("light");
  EXPECT_FALSE(media_query->matches());

  SetSupportedColorSchemes("dark");
  EXPECT_TRUE(media_query->matches());

  SetSupportedColorSchemes("light dark");
  EXPECT_TRUE(media_query->matches());
}

}  // namespace blink
