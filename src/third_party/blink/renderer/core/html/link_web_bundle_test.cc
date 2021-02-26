// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_web_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"

namespace blink {

namespace {

void TestParseResourceUrl(const AtomicString& url, bool is_valid) {
  ASSERT_EQ(LinkWebBundle::ParseResourceUrl(url).IsValid(), is_valid);
}

}  // namespace

TEST(LinkWebBundleTest, ParseResourceUrl) {
  TestParseResourceUrl("https://test.example.com/", true);
  TestParseResourceUrl("http://test.example.com/", true);
  TestParseResourceUrl("https://user@test.example.com/", false);
  TestParseResourceUrl("https://user:password@test.example.com/", false);
  TestParseResourceUrl("https://test.example.com/#fragment", false);
  TestParseResourceUrl("ftp://test.example.com/", false);
  TestParseResourceUrl("file:///test.html", false);
}

TEST(LinkWebBundleTest, ResourcesAttribute) {
  auto* document = Document::CreateForTest();
  auto* link =
      MakeGarbageCollected<HTMLLinkElement>(*document, CreateElementFlags());
  DOMTokenList* resources = link->resources();
  EXPECT_EQ(g_null_atom, resources->value());

  link->setAttribute(html_names::kRelAttr, "webbundle");

  // Valid url
  link->setAttribute(html_names::kResourcesAttr, "https://test.example.com");
  EXPECT_EQ("https://test.example.com", resources->value());
  EXPECT_EQ(1u, link->ValidResourceUrls().size());
  EXPECT_TRUE(
      link->ValidResourceUrls().Contains(KURL("https://test.example.com")));

  // Invalid urls
  link->setAttribute(html_names::kResourcesAttr,
                     "https://user:test.example.com");
  EXPECT_EQ("https://user:test.example.com", resources->value());
  EXPECT_TRUE(link->ValidResourceUrls().IsEmpty());

  link->setAttribute(html_names::kResourcesAttr,
                     "https://:pass@test.example.com");
  EXPECT_TRUE(link->ValidResourceUrls().IsEmpty());

  link->setAttribute(html_names::kResourcesAttr,
                     "https://test.example.com/#fragment");
  EXPECT_TRUE(link->ValidResourceUrls().IsEmpty());

  // Spece-separated valid urls
  link->setAttribute(html_names::kResourcesAttr,
                     "https://test1.example.com https://test2.example.com");
  EXPECT_EQ(2u, link->ValidResourceUrls().size());
  EXPECT_TRUE(
      link->ValidResourceUrls().Contains(KURL("https://test1.example.com")));
  EXPECT_TRUE(
      link->ValidResourceUrls().Contains(KURL("https://test2.example.com")));

  // Space-separated valid and invalid urls
  link->setAttribute(html_names::kResourcesAttr,
                     "https://test1.example.com https://user:test.example.com "
                     "https://test2.example.com");
  EXPECT_EQ(
      "https://test1.example.com https://user:test.example.com "
      "https://test2.example.com",
      resources->value());
  EXPECT_EQ(2u, link->ValidResourceUrls().size());
  EXPECT_TRUE(
      link->ValidResourceUrls().Contains(KURL("https://test1.example.com")));
  EXPECT_TRUE(
      link->ValidResourceUrls().Contains(KURL("https://test2.example.com")));
}

}  // namespace blink
