// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"

namespace blink {
namespace {

// These tests exercise the caching logic of |NGLayoutResult|s. They are
// rendering tests which contain two children: "test" and "src".
//
// Both have layout initially performed on them, however the "src" will have a
// different |NGConstraintSpace| which is then used to test either a cache hit
// or miss.
class NGLayoutResultCachingTest : public NGLayoutTest {};

TEST_F(NGLayoutResultCachingTest, HitDifferentExclusionSpace) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Same BFC offset, different exclusion space.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="height: 20px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 30px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
  EXPECT_EQ(result->BfcBlockOffset().value(), LayoutUnit(50));
  EXPECT_EQ(result->BfcLineOffset(), LayoutUnit());
}

TEST_F(NGLayoutResultCachingTest, HitDifferentBFCOffset) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Different BFC offset, same exclusion space.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="height: 20px; padding-top: 5px;">
        <div class="float" style="height: 20px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 40px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="src" style="height: 20px; padding-top: 5px;">
        <div class="float" style="height: 20px;"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
  EXPECT_EQ(result->BfcBlockOffset().value(), LayoutUnit(40));
  EXPECT_EQ(result->BfcLineOffset(), LayoutUnit());

  // Also check that the exclusion(s) got moved correctly.
  LayoutOpportunityVector opportunities =
      result->ExclusionSpace().AllLayoutOpportunities(
          /* offset */ {LayoutUnit(), LayoutUnit()},
          /* available_inline_size */ LayoutUnit(100));

  EXPECT_EQ(opportunities.size(), 3u);
  EXPECT_EQ(opportunities[0].rect.start_offset,
            NGBfcOffset(LayoutUnit(50), LayoutUnit()));
  EXPECT_EQ(opportunities[0].rect.end_offset,
            NGBfcOffset(LayoutUnit(100), LayoutUnit::Max()));

  EXPECT_EQ(opportunities[1].rect.start_offset,
            NGBfcOffset(LayoutUnit(), LayoutUnit(20)));
  EXPECT_EQ(opportunities[1].rect.end_offset,
            NGBfcOffset(LayoutUnit(100), LayoutUnit(45)));

  EXPECT_EQ(opportunities[2].rect.start_offset,
            NGBfcOffset(LayoutUnit(), LayoutUnit(65)));
  EXPECT_EQ(opportunities[2].rect.end_offset,
            NGBfcOffset(LayoutUnit(100), LayoutUnit::Max()));
}

TEST_F(NGLayoutResultCachingTest, MissDescendantAboveBlockStart1) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Same BFC offset, different exclusion space, descendant above
  // block start.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="height: 20px; padding-top: 5px;">
        <div style="height: 10px; margin-top: -10px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 30px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, MissDescendantAboveBlockStart2) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Different BFC offset, same exclusion space, descendant above
  // block start.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="height: 20px; padding-top: 5px;">
        <div style="height: 10px; margin-top: -10px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 40px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitOOFDescendantAboveBlockStart) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Different BFC offset, same exclusion space, OOF-descendant above
  // block start.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="position: relative; height: 20px; padding-top: 5px;">
        <div style="position: absolute; height: 10px; top: -10px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 40px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, MissFloatInitiallyIntruding1) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Same BFC offset, different exclusion space, float initially
  // intruding.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 20px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 30px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, MissFloatInitiallyIntruding2) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Different BFC offset, same exclusion space, float initially
  // intruding.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 60px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 70px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, MissFloatWillIntrude1) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Same BFC offset, different exclusion space, float will intrude.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 40px;"></div>
      </div>
      <div id="test" style="height: 20px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, MissFloatWillIntrude2) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Different BFC offset, same exclusion space, float will intrude.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 40px;"></div>
      </div>
      <div id="test" style="height: 60px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 30px;">
        <div class="float" style="height: 40px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitPushedByFloats1) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Same BFC offset, different exclusion space, pushed by floats.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 20px; clear: left;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 70px;"></div>
      </div>
      <div id="src" style="height: 20px; clear: left;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitPushedByFloats2) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Different BFC offset, same exclusion space, pushed by floats.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 20px; clear: left;"></div>
    </div>
    <div class="bfc">
      <div style="height: 30px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px; clear: left;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, MissPushedByFloats1) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Same BFC offset, different exclusion space, pushed by floats.
  // Miss due to shrinking offset.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 70px;"></div>
      </div>
      <div id="test" style="height: 20px; clear: left;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px; clear: left;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, MissPushedByFloats2) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Different BFC offset, same exclusion space, pushed by floats.
  // Miss due to shrinking offset.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 30px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 20px; clear: left;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px; clear: left;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitDifferentRareData) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Same absolute fixed constraints.
  SetBodyInnerHTML(R"HTML(
    <style>
      .container { position: relative; width: 100px; height: 100px; }
      .abs { position: absolute; width: 100px; height: 100px; top: 0; left: 0; }
    </style>
    <div class="container">
      <div id="test" class="abs"></div>
    </div>
    <div class="container" style="width: 200px; height: 200px;">
      <div id="src" class="abs"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitPercentageMinWidth) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // min-width calculates to different values, but doesn't change size.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .inflow { width: 100px; min-width: 25%; }
    </style>
    <div class="bfc">
      <div id="test" class="inflow"></div>
    </div>
    <div class="bfc" style="width: 200px; height: 200px;">
      <div id="src" class="inflow"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitFixedMinWidth) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // min-width is always larger than the available size.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .inflow { min-width: 300px; }
    </style>
    <div class="bfc">
      <div id="test" class="inflow"></div>
    </div>
    <div class="bfc" style="width: 200px; height: 200px;">
      <div id="src" class="inflow"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitShrinkToFitSameIntrinsicSizes) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // We have a shrink-to-fit node, with the min, and max intrinsic sizes being
  // equal (the available size doesn't affect the final size).
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .shrink { width: fit-content; }
      .child { width: 250px; }
    </style>
    <div class="bfc">
      <div id="test" class="shrink">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="width: 200px; height: 200px;">
      <div id="src" class="shrink">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitShrinkToFitDifferentParent) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // The parent "bfc" node changes from shrink-to-fit, to a fixed width. But
  // these calculate as the same available space to the "test" element.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; }
      .child { width: 250px; }
    </style>
    <div class="bfc" style="width: fit-content; height: 100px;">
      <div id="test">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="width: 250px; height: 100px;">
      <div id="src">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, MissQuirksModePercentageBasedChild) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Quirks-mode %-block-size child.
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .child { height: 50%; }
    </style>
    <div class="bfc">
      <div id="test">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="height: 200px;">
      <div id="src">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitQuirksModePercentageBasedParentAndChild) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Quirks-mode %-block-size parent *and* child. Here we mark the parent as
  // depending on %-block-size changes, however itself doesn't change in
  // height.
  // We are able to hit the cache as we detect that the height for the child
  // *isn't* indefinite, and results in the same height as before.
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .parent { height: 50%; min-height: 200px; }
      .child { height: 50%; }
    </style>
    <div class="bfc">
      <div id="test" class="parent">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="height: 200px;">
      <div id="src" class="parent">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, HitStandardsModePercentageBasedChild) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  // Standards-mode %-block-size child.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .child { height: 50%; }
    </style>
    <div class="bfc">
      <div id="test">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="height: 200px;">
      <div id="src">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  const NGConstraintSpace& space =
      src->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);

  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(NGLayoutResultCachingTest, ChangeTableCellBlockSizeConstrainedness) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      .table { display: table; width: 300px; }
      .cell { display: table-cell; }
      .child1 { height: 100px; }
      .child2, .child3 { overflow:auto; height:10%; }
    </style>
    <div class="table">
      <div class="cell">
        <div class="child1" id="test1"></div>
        <div class="child2" id="test2">
          <div style="height:30px;"></div>
        </div>
        <div class="child3" id="test3"></div>
      </div>
    </div>
    <div class="table" style="height:300px;">
      <div class="cell">
        <div class="child1" id="src1"></div>
        <div class="child2" id="src2">
          <div style="height:30px;"></div>
        </div>
        <div class="child3" id="src3"></div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* test3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test3"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));
  auto* src3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src3"));

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  NGConstraintSpace space =
      src1->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  scoped_refptr<const NGLayoutResult> result = test1->CachedLayoutResult(
      space, nullptr, &fragment_geometry, &cache_status);
  // The first child has a fixed height, and shouldn't be affected by the cell
  // height.
  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);

  fragment_geometry.reset();
  space = src2->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = test2->CachedLayoutResult(space, nullptr, &fragment_geometry,
                                     &cache_status);
  // The second child has overflow:auto and a percentage height, but its
  // intrinsic height is identical to its extrinsic height (when the cell has a
  // height). So it won't need layout, either.
  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kHit);
  EXPECT_NE(result.get(), nullptr);

  fragment_geometry.reset();
  space = src3->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = test3->CachedLayoutResult(space, nullptr, &fragment_geometry,
                                     &cache_status);
  // The third child has overflow:auto and a percentage height, and its
  // intrinsic height is 0 (no children), so it matters whether the cell has a
  // height or not. We're only going to need simplified layout, though, since no
  // children will be affected by its height change.
  EXPECT_EQ(cache_status, NGLayoutCacheStatus::kNeedsSimplifiedLayout);
  EXPECT_EQ(result.get(), nullptr);
}

}  // namespace
}  // namespace blink
