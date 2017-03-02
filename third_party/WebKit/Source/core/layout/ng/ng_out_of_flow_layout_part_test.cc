// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_out_of_flow_layout_part.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/layout_ng_block_flow.h"

namespace blink {
namespace {

class NGOutOfFlowLayoutPartTest : public RenderingTest {
 public:
  NGOutOfFlowLayoutPartTest() {
    RuntimeEnabledFeatures::setLayoutNGEnabled(true);
  };
  ~NGOutOfFlowLayoutPartTest() {
    RuntimeEnabledFeatures::setLayoutNGEnabled(false);
  };
};

// Fixed blocks inside absolute blocks trigger otherwise unused while loop
// inside NGOutOfFlowLayoutPart::Run.
// This test exercises this loop by placing two fixed elements inside abs.
TEST_F(NGOutOfFlowLayoutPartTest, FixedInsideAbs) {
  setBodyInnerHTML(
      R"HTML(
      <style>
        body{ padding:0px; margin:0px}
        #rel { position:relative }
        #abs {
          position: absolute;
          top:49px;
          left:0px;
        }
        #pad {
          width:100px;
          height:50px;
        }
        #fixed1 {
          position:fixed;
          width:50px;
        }
        #fixed2 {
          position:fixed;
          top:9px;
          left:7px;
        }
      </style>
      <div id='rel'>
        <div id='abs'>
          <div id='pad'></div>
          <div id='fixed1'>
            <p>fixed static</p>
          </div>
          <div id='fixed2'>
            <p>fixed plain</p>
          </div>
        </div>
      </div>
      )HTML");

  // Test whether the oof fragments have been collected at NG->Legacy boundary.
  Element* rel = document().getElementById("rel");
  LayoutNGBlockFlow* block_flow = toLayoutNGBlockFlow(rel->layoutObject());
  RefPtr<NGConstraintSpace> space =
      NGConstraintSpace::CreateFromLayoutObject(*block_flow);
  NGBlockNode* node = new NGBlockNode(block_flow);
  RefPtr<NGLayoutResult> result = node->Layout(space.get());
  EXPECT_EQ(result->OutOfFlowDescendants().size(), (size_t)2);

  // Test the final result.
  Element* fixed_1 = document().getElementById("fixed1");
  Element* fixed_2 = document().getElementById("fixed2");
  // fixed1 top is static: #abs.top + #pad.height
  EXPECT_EQ(fixed_1->offsetTop(), LayoutUnit(99));
  // fixed2 top is positioned: #fixed2.top
  EXPECT_EQ(fixed_2->offsetTop(), LayoutUnit(9));
};
}
}
