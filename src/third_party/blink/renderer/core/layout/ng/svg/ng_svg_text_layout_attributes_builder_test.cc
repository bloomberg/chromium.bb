// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_layout_attributes_builder.h"

#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class NGSvgTextLayoutAttributesBuilderTest : public NGLayoutTest {};

TEST_F(NGSvgTextLayoutAttributesBuilderTest, TextPathCrash) {
  SetBodyInnerHTML(R"HTML(
<svg>
<text>
<textPath>
<a>
<textPath />)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

}  // namespace blink
