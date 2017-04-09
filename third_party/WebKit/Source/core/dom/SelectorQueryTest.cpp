// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/SelectorQuery.h"

#include <memory>
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/dom/Document.h"
#include "core/dom/StaticNodeList.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLHtmlElement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
struct QueryTest {
  const char* selector;
  bool query_all;
  unsigned matches;
  // {totalCount, fastId, fastClass, fastTagName, fastScan, slowScan,
  //  slowTraversingShadowTreeScan}
  SelectorQuery::QueryStats stats;
};

template <unsigned length>
void RunTests(Document& document, const QueryTest (&test_cases)[length]) {
  for (const auto& test_case : test_cases) {
    const char* selector = test_case.selector;
    SCOPED_TRACE(testing::Message()
                 << (test_case.query_all ? "querySelectorAll('"
                                         : "querySelector('")
                 << selector << "')");
    if (test_case.query_all) {
      StaticElementList* match_all = document.QuerySelectorAll(selector);
      EXPECT_EQ(test_case.matches, match_all->length());
    } else {
      Element* match = document.QuerySelector(selector);
      EXPECT_EQ(test_case.matches, match ? 1u : 0u);
    }
#if DCHECK_IS_ON()
    SelectorQuery::QueryStats stats = SelectorQuery::LastQueryStats();
    EXPECT_EQ(test_case.stats.total_count, stats.total_count);
    EXPECT_EQ(test_case.stats.fast_id, stats.fast_id);
    EXPECT_EQ(test_case.stats.fast_class, stats.fast_class);
    EXPECT_EQ(test_case.stats.fast_tag_name, stats.fast_tag_name);
    EXPECT_EQ(test_case.stats.fast_scan, stats.fast_scan);
    EXPECT_EQ(test_case.stats.slow_scan, stats.slow_scan);
    EXPECT_EQ(test_case.stats.slow_traversing_shadow_tree_scan,
              stats.slow_traversing_shadow_tree_scan);
#endif
  }
}
};

TEST(SelectorQueryTest, NotMatchingPseudoElement) {
  Document* document = Document::Create();
  HTMLHtmlElement* html = HTMLHtmlElement::Create(*document);
  document->AppendChild(html);
  document->documentElement()->setInnerHTML(
      "<body><style>span::before { content: 'X' }</style><span></span></body>");

  CSSSelectorList selector_list = CSSParser::ParseSelector(
      CSSParserContext::Create(*document, KURL(), kReferrerPolicyDefault,
                               g_empty_string,
                               CSSParserContext::kStaticProfile),
      nullptr, "span::before");
  std::unique_ptr<SelectorQuery> query =
      SelectorQuery::Adopt(std::move(selector_list));
  Element* elm = query->QueryFirst(*document);
  EXPECT_EQ(nullptr, elm);

  selector_list = CSSParser::ParseSelector(
      CSSParserContext::Create(*document, KURL(), kReferrerPolicyDefault,
                               g_empty_string,
                               CSSParserContext::kStaticProfile),
      nullptr, "span");
  query = SelectorQuery::Adopt(std::move(selector_list));
  elm = query->QueryFirst(*document);
  EXPECT_NE(nullptr, elm);
}

TEST(SelectorQueryTest, LastOfTypeNotFinishedParsing) {
  Document* document = HTMLDocument::Create();
  HTMLHtmlElement* html = HTMLHtmlElement::Create(*document);
  document->AppendChild(html);
  document->documentElement()->setInnerHTML(
      "<body><p></p><p id=last></p></body>", ASSERT_NO_EXCEPTION);

  document->body()->BeginParsingChildren();

  CSSSelectorList selector_list = CSSParser::ParseSelector(
      CSSParserContext::Create(*document, KURL(), kReferrerPolicyDefault,
                               g_empty_string,
                               CSSParserContext::kStaticProfile),
      nullptr, "p:last-of-type");
  std::unique_ptr<SelectorQuery> query =
      SelectorQuery::Adopt(std::move(selector_list));
  Element* elm = query->QueryFirst(*document);
  ASSERT_TRUE(elm);
  EXPECT_EQ("last", elm->IdForStyleResolution());
}

TEST(SelectorQueryTest, StandardsModeFastPaths) {
  Document* document = HTMLDocument::Create();
  document->write(
      "<!DOCTYPE html>"
      "<html>"
      "  <head></head>"
      "  <body>"
      "    <span id=first class=A>"
      "      <span id=a class=one></span>"
      "      <span id=b class=two></span>"
      "      <span id=c class=one></span>"
      "      <div id=multiple class=two></div>"
      "    </span>"
      "    <div>"
      "      <span id=second class=B>"
      "        <span id=A class=one></span>"
      "        <span id=B class=two></span>"
      "        <span id=C class=one></span>"
      "        <span id=multiple class=two></span>"
      "      </span>"
      "    </div>"
      "  </body>"
      "</html>");
  static const struct QueryTest kTestCases[] = {
      // Id in right most selector fast path.
      {"#A", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple.two", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple", true, 2, {2, 2, 0, 0, 0, 0, 0}},
      {"span#multiple", true, 1, {2, 2, 0, 0, 0, 0, 0}},
      {"#multiple.two", true, 2, {2, 2, 0, 0, 0, 0, 0}},
      {"body #multiple", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"body span#multiple", false, 1, {2, 2, 0, 0, 0, 0, 0}},
      {"body #multiple", true, 2, {2, 2, 0, 0, 0, 0, 0}},

      // Single selector tag fast path.
      {"span", false, 1, {4, 0, 0, 4, 0, 0, 0}},
      {"span", true, 9, {14, 0, 0, 14, 0, 0, 0}},

      // Single selector class fast path.
      {".two", false, 1, {6, 0, 6, 0, 0, 0, 0}},
      {".two", true, 4, {14, 0, 14, 0, 0, 0, 0}},

      // Class in the right most selector fast path.
      {"body .two", false, 1, {6, 0, 0, 0, 6, 0, 0}},
      {"div .two", false, 1, {12, 0, 0, 0, 12, 0, 0}},

      // Classes in the right most selector for querySelectorAll use a fast
      // path.
      {"body .two", true, 4, {14, 0, 14, 0, 0, 0, 0}},
      {"div .two", true, 2, {14, 0, 14, 0, 0, 0, 0}},

      // TODO: querySelector disables the class fast path to favor the id, but
      // this means some selectors always end up doing fastScan.
      {"#second .two", false, 1, {2, 0, 0, 0, 2, 0, 0}},

      // TODO(esprehn): This should have used getElementById instead of doing
      // a fastClass scan. It could have looked at 4 elements instead.
      {"#second .two", true, 2, {14, 0, 14, 0, 0, 0, 0}},

      // Selectors with no classes or ids use the fast scan.
      {":scope", false, 1, {1, 0, 0, 0, 1, 0, 0}},
      {":scope", true, 1, {14, 0, 0, 0, 14, 0, 0}},
      {"foo bar", false, 0, {14, 0, 0, 0, 14, 0, 0}},

      // Multiple selectors always uses the slow path.
      // TODO(esprehn): We could make this fast if we sorted the output, not
      // sure it's worth it unless we're dealing with ids.
      {"#a, #b", false, 1, {5, 0, 0, 0, 0, 5, 0}},
      {"#a, #b", true, 2, {14, 0, 0, 0, 0, 14, 0}},

      // Anything that crosses shadow boundaries is slow path.
      {"#foo /deep/ .a", false, 0, {14, 0, 0, 0, 0, 0, 14}},
      {"#foo::shadow .a", false, 0, {14, 0, 0, 0, 0, 0, 14}},
      {"::content .a", false, 0, {14, 0, 0, 0, 0, 14, 0}},
      {"#foo /deep/ .a", true, 0, {14, 0, 0, 0, 0, 0, 14}},
      {"#foo::shadow .a", true, 0, {14, 0, 0, 0, 0, 0, 14}},
      {"::content .a", true, 0, {14, 0, 0, 0, 0, 14, 0}},
  };
  RunTests(*document, kTestCases);
}

TEST(SelectorQueryTest, QuirksModeSlowPath) {
  Document* document = HTMLDocument::Create();
  document->write(
      "<html>"
      "  <head></head>"
      "  <body>"
      "    <span id=first>"
      "      <span id=One class=Two></span>"
      "      <span id=one class=tWo></span>"
      "    </span>"
      "  </body>"
      "</html>");
  static const struct QueryTest kTestCases[] = {
      // Quirks mode always uses the slow path.
      {"#one", false, 1, {5, 0, 0, 0, 0, 5, 0}},
      {"#One", false, 1, {5, 0, 0, 0, 0, 5, 0}},
      {"#ONE", false, 1, {5, 0, 0, 0, 0, 5, 0}},
      {"#ONE", true, 2, {6, 0, 0, 0, 0, 6, 0}},
      {"span", false, 1, {4, 0, 0, 0, 0, 4, 0}},
      {"span", true, 3, {6, 0, 0, 0, 0, 6, 0}},
      {".two", false, 1, {5, 0, 0, 0, 0, 5, 0}},
      {".two", true, 2, {6, 0, 0, 0, 0, 6, 0}},
      {"body #first", false, 1, {4, 0, 0, 0, 0, 4, 0}},
      {"body #one", true, 2, {6, 0, 0, 0, 0, 6, 0}},
  };
  RunTests(*document, kTestCases);
}

}  // namespace blink
