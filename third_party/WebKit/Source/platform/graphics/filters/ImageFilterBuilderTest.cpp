/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SkImageFilter.h"
#include "platform/graphics/filters/FEBlend.h"
#include "platform/graphics/filters/FEGaussianBlur.h"
#include "platform/graphics/filters/FEMerge.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Test;

namespace blink {

class ImageFilterBuilderTest : public Test {
 protected:
  void InterpolationSpaceTest() {
    // Build filter tree
    Filter* reference_filter = Filter::Create(1.0f);

    // Add a dummy source graphic input
    FilterEffect* source_effect = reference_filter->GetSourceGraphic();
    source_effect->SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);

    // Add a blur effect (with input : source)
    FilterEffect* blur_effect =
        FEGaussianBlur::Create(reference_filter, 3.0f, 3.0f);
    blur_effect->SetOperatingInterpolationSpace(kInterpolationSpaceLinear);
    blur_effect->InputEffects().push_back(source_effect);

    // Add a blend effect (with inputs : blur, source)
    FilterEffect* blend_effect =
        FEBlend::Create(reference_filter, WebBlendMode::kNormal);
    blend_effect->SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
    FilterEffectVector& blend_inputs = blend_effect->InputEffects();
    blend_inputs.ReserveCapacity(2);
    blend_inputs.push_back(source_effect);
    blend_inputs.push_back(blur_effect);

    // Add a merge effect (with inputs : blur, blend)
    FilterEffect* merge_effect = FEMerge::Create(reference_filter);
    merge_effect->SetOperatingInterpolationSpace(kInterpolationSpaceLinear);
    FilterEffectVector& merge_inputs = merge_effect->InputEffects();
    merge_inputs.ReserveCapacity(2);
    merge_inputs.push_back(blur_effect);
    merge_inputs.push_back(blend_effect);
    reference_filter->SetLastEffect(merge_effect);

    // Get SkImageFilter resulting tree
    sk_sp<SkImageFilter> filter = SkiaImageFilterBuilder::Build(
        reference_filter->LastEffect(), kInterpolationSpaceSRGB);

    // Let's check that the resulting tree looks like this :
    //      InterpolationSpace (Linear->Device) : CS (L->D)
    //                |
    //             Merge (L)
    //              |     |
    //              |    CS (D->L)
    //              |          |
    //              |      Blend (D)
    //              |       /    |
    //              |  CS (L->D) |
    //              |  /         |
    //             Blur (L)      |
    //                 \         |
    //               CS (D->L)   |
    //                   \       |
    //                 Source Graphic (D)

    EXPECT_EQ(filter->countInputs(), 1);         // Should be CS (L->D)
    SkImageFilter* child = filter->getInput(0);  // Should be Merge
    EXPECT_EQ(child->asColorFilter(0), false);
    EXPECT_EQ(child->countInputs(), 2);
    child = child->getInput(1);  // Should be CS (D->L)
    EXPECT_EQ(child->asColorFilter(0), true);
    EXPECT_EQ(child->countInputs(), 1);
    child = child->getInput(0);  // Should be Blend
    EXPECT_EQ(child->asColorFilter(0), false);
    EXPECT_EQ(child->countInputs(), 2);
    child = child->getInput(0);  // Should be CS (L->D)
    EXPECT_EQ(child->asColorFilter(0), true);
    EXPECT_EQ(child->countInputs(), 1);
    child = child->getInput(0);  // Should be Blur
    EXPECT_EQ(child->asColorFilter(0), false);
    EXPECT_EQ(child->countInputs(), 1);
    child = child->getInput(0);  // Should be CS (D->L)
    EXPECT_EQ(child->asColorFilter(0), true);
    EXPECT_EQ(child->countInputs(), 1);
  }
};

TEST_F(ImageFilterBuilderTest, testInterpolationSpace) {
  InterpolationSpaceTest();
}

}  // namespace blink
