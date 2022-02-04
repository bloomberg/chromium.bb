// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class SVGElementTest : public PageTestBase {};

TEST_F(SVGElementTest, BaseComputedStyleForSMILWithContainerQueries) {
  ScopedCSSContainerQueriesForTest scoped_cq(true);
  ScopedCSSContainerSkipStyleRecalcForTest scoped_skip(true);
  ScopedLayoutNGForTest scoped_ng(true);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #rect2 { display: none }
      @container size(max-width: 200px) {
        rect, g { color: green; }
      }
      @container size(min-width: 300px) {
        rect, g { background-color: red; }
      }
    </style>
    <div style="container-type: inline-size; width: 200px">
      <svg>
        <rect id="rect1" />
        <rect id="rect2" />
        <g id="g"></g>
      </svg>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* rect1 = To<SVGElement>(GetDocument().getElementById("rect1"));
  auto* rect2 = To<SVGElement>(GetDocument().getElementById("rect2"));
  auto* g = To<SVGElement>(GetDocument().getElementById("g"));

  auto force_needs_override_style = [](SVGElement& svg_element) {
    svg_element.EnsureSVGRareData()->SetNeedsOverrideComputedStyleUpdate();
  };

  force_needs_override_style(*rect1);
  force_needs_override_style(*rect2);
  force_needs_override_style(*g);

  const auto* rect1_style = rect1->BaseComputedStyleForSMIL();
  const auto* rect2_style = rect2->BaseComputedStyleForSMIL();
  const auto* g_style = g->BaseComputedStyleForSMIL();

  const Color green(0, 128, 0);

  EXPECT_EQ(rect1_style->VisitedDependentColor(GetCSSPropertyColor()), green);
  EXPECT_EQ(rect2_style->VisitedDependentColor(GetCSSPropertyColor()), green);
  EXPECT_EQ(g_style->VisitedDependentColor(GetCSSPropertyColor()), green);

  EXPECT_EQ(rect1_style->VisitedDependentColor(GetCSSPropertyBackgroundColor()),
            Color::kTransparent);
  EXPECT_EQ(rect2_style->VisitedDependentColor(GetCSSPropertyBackgroundColor()),
            Color::kTransparent);
  EXPECT_EQ(g_style->VisitedDependentColor(GetCSSPropertyBackgroundColor()),
            Color::kTransparent);
}

}  // namespace blink
