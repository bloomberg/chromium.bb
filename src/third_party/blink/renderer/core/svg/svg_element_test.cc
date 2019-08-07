// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using LifecycleUpdateReason = DocumentLifecycle::LifecycleUpdateReason;

class SVGElementTest : public PageTestBase {};

TEST_F(SVGElementTest, InstanceRemovedWhenNonAttachedTargetRemoved) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style></style>
    <svg>
        <unknown>
          <g id="parent">
            <a id="target">
          </g>
          <use id="use" href="#parent">
        </unknown>
    </svg>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // Remove #target.
  ASSERT_TRUE(GetDocument().getElementById("target"));
  GetDocument().getElementById("target")->remove();

  // This should cause a rebuild of the <use> shadow tree.
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // There should be no instance for #target anymore, since that element was
  // removed.
  SVGUseElement* use = ToSVGUseElement(GetDocument().getElementById("use"));
  ASSERT_TRUE(use);
  ASSERT_TRUE(use->GetShadowRoot());
  Element* instance = use->GetShadowRoot()->getElementById("target");
  // TODO(https://crbug.com/953263): Change to ASSERT_FALSE once the issue
  // is resolved. ASSERT_TRUE is used so we'll remember to update this test.
  ASSERT_TRUE(instance);

  // TODO(https://crbug.com/953263): Remove when issue is resolved.
  instance->EnsureComputedStyle();  // Don't crash.
}

}  // namespace blink
