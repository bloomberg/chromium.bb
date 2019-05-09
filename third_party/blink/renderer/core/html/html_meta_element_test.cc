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
    RuntimeEnabledFeatures::SetMetaColorSchemeEnabled(true);
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
  HTMLMetaElement* CreateColorSchemeMeta(const AtomicString& content) {
    auto* meta = MakeGarbageCollected<HTMLMetaElement>(GetDocument());
    meta->setAttribute(html_names::kNameAttr, "color-scheme");
    meta->setAttribute(html_names::kContentAttr, content);
    return meta;
  }

  void SetColorScheme(const AtomicString& content) {
    GetDocument().head()->AppendChild(CreateColorSchemeMeta(content));
  }

  bool SupportsColorScheme(ColorScheme scheme) const {
    return GetDocument().GetStyleEngine().GetMetaColorScheme().Contains(scheme);
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

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_LastWins) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
    <meta name="color-scheme" content="light">
  )HTML");

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_Remove) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
    <meta id="last-meta" name="color-scheme" content="light">
  )HTML");

  GetDocument().getElementById("last-meta")->remove();

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_InsertBefore) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");

  Element* head = GetDocument().head();
  head->insertBefore(CreateColorSchemeMeta("light"), head->firstChild());

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_AppendChild) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");

  GetDocument().head()->AppendChild(CreateColorSchemeMeta("light"));

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_SetAttribute) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  GetDocument().getElementById("meta")->setAttribute(html_names::kContentAttr,
                                                     "light");

  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_RemoveAttribute) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kDark));

  GetDocument().getElementById("meta")->removeAttribute(
      html_names::kContentAttr);

  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeParsing) {
  SetColorScheme("");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));

  SetColorScheme("light");
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));

  SetColorScheme("dark");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kDark));

  SetColorScheme("light dark");
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kDark));

  SetColorScheme("light,dark");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kDark));

  SetColorScheme("light,");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));

  SetColorScheme(",light");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));

  SetColorScheme(", light");
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kLight));

  SetColorScheme("light, dark");
  EXPECT_FALSE(SupportsColorScheme(ColorScheme::kLight));
  EXPECT_TRUE(SupportsColorScheme(ColorScheme::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeForcedDarkeningAndMQ) {
  GetDocument().GetSettings()->SetPreferredColorScheme(
      PreferredColorScheme::kDark);

  auto* media_query = GetDocument().GetMediaQueryMatcher().MatchMedia(
      "(prefers-color-scheme: dark)");
  EXPECT_TRUE(media_query->matches());
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  EXPECT_FALSE(media_query->matches());

  SetColorScheme("light");
  EXPECT_FALSE(media_query->matches());

  SetColorScheme("dark");
  EXPECT_TRUE(media_query->matches());

  SetColorScheme("light dark");
  EXPECT_TRUE(media_query->matches());
}

}  // namespace blink
