// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_options.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class DisplayLockUtilitiesTest : public RenderingTest,
                                 private ScopedDisplayLockingForTest {
 public:
  DisplayLockUtilitiesTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedDisplayLockingForTest(true) {}
};

TEST_F(DisplayLockUtilitiesTest, ActivatableLockedInclusiveAncestors) {
  // TODO(vmpstr): Implement for layout ng.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id='outer'>
      <div id='innerA'>
        <div id='innermost'>text_node</div>
      </div>
      <div id='innerB'></div>
    </div>
  )HTML");

  Element& outer = *GetDocument().getElementById("outer");
  Element& inner_a = *GetDocument().getElementById("innerA");
  Element& inner_b = *GetDocument().getElementById("innerB");
  Element& innermost = *GetDocument().getElementById("innermost");
  ShadowRoot& shadow_root =
      inner_b.AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.SetInnerHTMLFromString("<div id='shadowDiv'>shadow!</div>");
  Element& shadow_div = *shadow_root.getElementById("shadowDiv");

  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());

  DisplayLockOptions options;
  options.setActivatable(true);
  // Lock outer with activatable flag.
  {
    ScriptState::Scope scope(script_state);
    outer.getDisplayLockForBindings()->acquire(script_state, &options);
  }

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  // Querying from every element gives |outer|.
  HeapVector<Member<Element>> result_for_outer =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(outer);
  EXPECT_EQ(result_for_outer.size(), 1u);
  EXPECT_EQ(result_for_outer.at(0), outer);

  HeapVector<Member<Element>> result_for_inner_a =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(inner_a);
  EXPECT_EQ(result_for_inner_a.size(), 1u);
  EXPECT_EQ(result_for_inner_a.at(0), outer);

  HeapVector<Member<Element>> result_for_innermost =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(innermost);
  EXPECT_EQ(result_for_innermost.size(), 1u);
  EXPECT_EQ(result_for_innermost.at(0), outer);

  HeapVector<Member<Element>> result_for_inner_b =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(inner_b);
  EXPECT_EQ(result_for_inner_b.size(), 1u);
  EXPECT_EQ(result_for_inner_b.at(0), outer);

  HeapVector<Member<Element>> result_for_shadow_div =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(shadow_div);
  EXPECT_EQ(result_for_shadow_div.size(), 1u);
  EXPECT_EQ(result_for_shadow_div.at(0), outer);

  // Lock innermost with activatable flag.
  {
    ScriptState::Scope scope(script_state);
    innermost.getDisplayLockForBindings()->acquire(script_state, &options);
  }

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  result_for_outer =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(outer);
  EXPECT_EQ(result_for_outer.size(), 1u);
  EXPECT_EQ(result_for_outer.at(0), outer);

  result_for_inner_a =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(inner_a);
  EXPECT_EQ(result_for_inner_a.size(), 1u);
  EXPECT_EQ(result_for_inner_a.at(0), outer);

  result_for_innermost =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(innermost);
  EXPECT_EQ(result_for_innermost.size(), 2u);
  EXPECT_EQ(result_for_innermost.at(0), innermost);
  EXPECT_EQ(result_for_innermost.at(1), outer);

  result_for_inner_b =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(inner_b);
  EXPECT_EQ(result_for_inner_b.size(), 1u);
  EXPECT_EQ(result_for_inner_b.at(0), outer);

  result_for_shadow_div =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(shadow_div);
  EXPECT_EQ(result_for_shadow_div.size(), 1u);
  EXPECT_EQ(result_for_shadow_div.at(0), outer);

  // Unlock everything.
  {
    ScriptState::Scope scope(script_state);
    innermost.getDisplayLockForBindings()->commit(script_state);
    outer.getDisplayLockForBindings()->commit(script_state);
  }

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  EXPECT_EQ(
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(outer).size(),
      0u);
  EXPECT_EQ(
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(inner_a).size(),
      0u);
  EXPECT_EQ(DisplayLockUtilities::ActivatableLockedInclusiveAncestors(innermost)
                .size(),
            0u);
  EXPECT_EQ(
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(inner_b).size(),
      0u);
  EXPECT_EQ(
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(shadow_div)
          .size(),
      0u);
}

TEST_F(DisplayLockUtilitiesTest, LockedSubtreeCrossingFrames) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="grandparent">
      <iframe id="frame" src="http://test.com"></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="parent">
      <div id="child"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* grandparent = GetDocument().getElementById("grandparent");
  Element* parent = ChildDocument().getElementById("parent");
  Element* child = ChildDocument().getElementById("child");

  ASSERT_TRUE(grandparent);
  ASSERT_TRUE(parent);
  ASSERT_TRUE(child);

  auto* child_frame_state =
      ToScriptStateForMainWorld(ChildDocument().GetFrame());
  auto* parent_frame_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());

  // Lock parent.
  {
    ScriptState::Scope scope(child_frame_state);
    parent->getDisplayLockForBindings()->acquire(child_frame_state, nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(ChildDocument().LockedDisplayLockCount(), 1);

  EXPECT_FALSE(
      DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*grandparent));
  EXPECT_FALSE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*parent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*child));

  // Lock grandparent.
  {
    ScriptState::Scope scope(parent_frame_state);
    grandparent->getDisplayLockForBindings()->acquire(parent_frame_state,
                                                      nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(
      DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*grandparent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*parent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*child));

  // Unlock parent.
  {
    ScriptState::Scope scope(child_frame_state);
    parent->getDisplayLockForBindings()->commit(child_frame_state);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(
      DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*grandparent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*parent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*child));

  // Unlock grandparent.
  {
    ScriptState::Scope scope(parent_frame_state);
    grandparent->getDisplayLockForBindings()->commit(parent_frame_state);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(
      DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*grandparent));
  EXPECT_FALSE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*parent));
  EXPECT_FALSE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*child));
}

}  // namespace blink
