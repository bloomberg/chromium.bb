// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ContainerQueryEvaluatorTest : public PageTestBase,
                                    private ScopedCSSContainerQueriesForTest,
                                    private ScopedLayoutNGForTest {
 public:
  ContainerQueryEvaluatorTest()
      : ScopedCSSContainerQueriesForTest(true), ScopedLayoutNGForTest(true) {}

  ContainerQuery* ParseContainer(String query) {
    String rule = "@container " + query + " {}";
    auto* style_rule = DynamicTo<StyleRuleContainer>(
        css_test_helpers::ParseRule(GetDocument(), rule));
    if (!style_rule)
      return nullptr;
    return &style_rule->GetContainerQuery();
  }

  bool Eval(String query,
            double width,
            double height,
            unsigned container_type,
            PhysicalAxes contained_axes) {
    auto style = ComputedStyle::Clone(GetDocument().ComputedStyleRef());
    style->SetContainerType(container_type);

    ContainerQuery* container_query = ParseContainer(query);
    DCHECK(container_query);
    auto* evaluator = MakeGarbageCollected<ContainerQueryEvaluator>();
    evaluator->ContainerChanged(
        GetDocument(), *style,
        PhysicalSize(LayoutUnit(width), LayoutUnit(height)), contained_axes);
    return evaluator->Eval(*container_query);
  }

  bool ContainerChanged(ContainerQueryEvaluator* evaluator,
                        PhysicalSize size,
                        unsigned container_type,
                        PhysicalAxes axes) {
    auto style = ComputedStyle::Clone(GetDocument().ComputedStyleRef());
    style->SetContainerType(container_type);

    return evaluator->ContainerChanged(GetDocument(), *style, size, axes) !=
           ContainerQueryEvaluator::Change::kNone;
  }

  const PhysicalAxes none{kPhysicalAxisNone};
  const PhysicalAxes both{kPhysicalAxisBoth};
  const PhysicalAxes horizontal{kPhysicalAxisHorizontal};
  const PhysicalAxes vertical{kPhysicalAxisVertical};

  const unsigned type_none = kContainerTypeNone;
  const unsigned type_size = kContainerTypeSize;
  const unsigned type_inline_size = kContainerTypeInlineSize;
};

TEST_F(ContainerQueryEvaluatorTest, ContainmentMatch) {
  {
    String query = "size(min-width: 100px)";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, horizontal));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, both));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_inline_size, horizontal));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_inline_size, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, vertical));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, none));
    EXPECT_FALSE(Eval(query, 99.0, 100.0, type_size, horizontal));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_none, both));
  }

  {
    String query = "size(min-height: 100px)";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, vertical));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, horizontal));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, none));
    EXPECT_FALSE(Eval(query, 100.0, 99.0, type_size, vertical));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_none, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_inline_size, both));
  }

  {
    String query = "size((min-width: 100px) and (min-height: 100px))";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, vertical));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, horizontal));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, none));
    EXPECT_FALSE(Eval(query, 100.0, 99.0, type_size, both));
    EXPECT_FALSE(Eval(query, 99.0, 100.0, type_size, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_none, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_inline_size, both));
  }
}

TEST_F(ContainerQueryEvaluatorTest, ContainerChanged) {
  PhysicalSize size_100(LayoutUnit(100), LayoutUnit(100));
  PhysicalSize size_200(LayoutUnit(200), LayoutUnit(200));

  ContainerQuery* container_query_100 =
      ParseContainer("size(min-width: 100px)");
  ContainerQuery* container_query_200 =
      ParseContainer("size(min-width: 200px)");
  ASSERT_TRUE(container_query_100);
  ASSERT_TRUE(container_query_200);

  auto* evaluator = MakeGarbageCollected<ContainerQueryEvaluator>();
  ContainerChanged(evaluator, size_100, type_size, horizontal);

  MatchResult dummy_result;

  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_100, dummy_result));
  EXPECT_FALSE(evaluator->EvalAndAdd(*container_query_200, dummy_result));

  EXPECT_FALSE(ContainerChanged(evaluator, size_100, type_size, horizontal));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_100, dummy_result));
  EXPECT_FALSE(evaluator->EvalAndAdd(*container_query_200, dummy_result));

  EXPECT_TRUE(ContainerChanged(evaluator, size_200, type_size, horizontal));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_100, dummy_result));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_200, dummy_result));

  EXPECT_FALSE(ContainerChanged(evaluator, size_200, type_size, horizontal));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_100, dummy_result));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_200, dummy_result));

  EXPECT_TRUE(ContainerChanged(evaluator, size_200, type_size, vertical));
  EXPECT_FALSE(evaluator->EvalAndAdd(*container_query_100, dummy_result));
  EXPECT_FALSE(evaluator->EvalAndAdd(*container_query_200, dummy_result));
}

TEST_F(ContainerQueryEvaluatorTest, SizeInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container-type: size;
        width: 500px;
        height: 500px;
      }
      @container size(min-width: 500px) {
        div { z-index:1; }
      }
    </style>
    <div id=container>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  ASSERT_TRUE(container->GetContainerQueryEvaluator());

  {
    // Causes re-layout, but the size does not change
    container->SetInlineStyleProperty(CSSPropertyID::kFloat, "left");

    unsigned before_count = GetStyleEngine().StyleForElementCount();

    UpdateAllLifecyclePhasesForTest();

    unsigned after_count = GetStyleEngine().StyleForElementCount();

    // Only #container should be affected. In particular, we should not
    // recalc any style for <div> children of #container.
    EXPECT_EQ(1u, after_count - before_count);
  }

  {
    // The size of the container changes, but it does not matter for
    // the result of the query (min-width: 500px).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "600px");

    unsigned before_count = GetStyleEngine().StyleForElementCount();

    UpdateAllLifecyclePhasesForTest();

    unsigned after_count = GetStyleEngine().StyleForElementCount();

    // Only #container should be affected. In particular, we should not
    // recalc any style for <div> children of #container.
    EXPECT_EQ(1u, after_count - before_count);
  }
}

TEST_F(ContainerQueryEvaluatorTest, DependentQueries) {
  PhysicalSize size_100(LayoutUnit(100), LayoutUnit(100));
  PhysicalSize size_150(LayoutUnit(150), LayoutUnit(150));
  PhysicalSize size_200(LayoutUnit(200), LayoutUnit(200));
  PhysicalSize size_300(LayoutUnit(300), LayoutUnit(300));
  PhysicalSize size_400(LayoutUnit(400), LayoutUnit(400));

  ContainerQuery* query_min_200px = ParseContainer("size(min-width: 200px)");
  ContainerQuery* query_max_300px = ParseContainer("size(max-width: 300px)");
  ASSERT_TRUE(query_min_200px);

  auto* evaluator = MakeGarbageCollected<ContainerQueryEvaluator>();
  ContainerChanged(evaluator, size_100, type_size, horizontal);

  MatchResult dummy_result;

  evaluator->EvalAndAdd(*query_min_200px, dummy_result);
  evaluator->EvalAndAdd(*query_max_300px, dummy_result);
  // Updating with the same size as we initially had should not invalidate
  // any query results.
  EXPECT_FALSE(ContainerChanged(evaluator, size_100, type_size, horizontal));

  // Makes no difference for either of (min-width: 200px), (max-width: 300px):
  EXPECT_FALSE(ContainerChanged(evaluator, size_150, type_size, horizontal));

  // (min-width: 200px) becomes true:
  EXPECT_TRUE(ContainerChanged(evaluator, size_200, type_size, horizontal));

  evaluator->EvalAndAdd(*query_min_200px, dummy_result);
  evaluator->EvalAndAdd(*query_max_300px, dummy_result);
  EXPECT_FALSE(ContainerChanged(evaluator, size_200, type_size, horizontal));

  // Makes no difference for either of (min-width: 200px), (max-width: 300px):
  EXPECT_FALSE(ContainerChanged(evaluator, size_300, type_size, horizontal));

  // (max-width: 300px) becomes false:
  EXPECT_TRUE(ContainerChanged(evaluator, size_400, type_size, horizontal));
}

TEST_F(ContainerQueryEvaluatorTest, EvaluatorDisplayNone) {
  SetBodyInnerHTML(R"HTML(
    <style>
      main {
        display: block;
        container-type: size;
        width: 500px;
        height: 500px;
      }
      main.none {
        display: none;
      }
      @container size(min-width: 500px) {
        div { --x:test; }
      }
    </style>
    <main id=outer>
      <div>
        <main id=inner>
          <div></div>
        </main>
      </div>
    </main>
  )HTML");

  // Inner container
  Element* inner = GetDocument().getElementById("inner");
  ASSERT_TRUE(inner);
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());

  inner->classList().Add("none");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(inner->GetContainerQueryEvaluator());

  inner->classList().Remove("none");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(inner->GetContainerQueryEvaluator());

  // Outer container
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_TRUE(outer);
  EXPECT_TRUE(outer->GetContainerQueryEvaluator());
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());

  outer->classList().Add("none");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(outer->GetContainerQueryEvaluator());
  EXPECT_FALSE(inner->GetContainerQueryEvaluator());

  outer->classList().Remove("none");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(outer->GetContainerQueryEvaluator());
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());
}

}  // namespace blink
