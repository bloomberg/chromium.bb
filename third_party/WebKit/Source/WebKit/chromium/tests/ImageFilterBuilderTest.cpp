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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SkImageFilter.h"
#include "core/platform/graphics/filters/FEBlend.h"
#include "core/platform/graphics/filters/FEGaussianBlur.h"
#include "core/platform/graphics/filters/FEMerge.h"
#include "core/platform/graphics/filters/FilterOperations.h"
#include "core/platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "core/platform/graphics/filters/SourceGraphic.h"
#include "core/rendering/FilterEffectRenderer.h"
#include <gtest/gtest.h>

using testing::Test;
using namespace WebCore;

class ImageFilterBuilderTest : public Test {
protected:
    void colorSpaceTest()
    {
        FilterOperations filterOps;

        // Build filter tree
        Vector<RefPtr<FilterOperation> >& ops = filterOps.operations();

        const String dummyUrl, dummyFragment;

        RefPtr<FilterEffectRenderer> dummyFilterEffectRenderer = FilterEffectRenderer::create();

        // Add a dummy source graphic input
        RefPtr<FilterEffect> sourceEffect =
            SourceGraphic::create(dummyFilterEffectRenderer.get());
        sourceEffect->setOperatingColorSpace(ColorSpaceDeviceRGB);

        // Add a blur effect (with input : source)
        RefPtr<ReferenceFilterOperation> blurOperation =
            ReferenceFilterOperation::create(dummyUrl, dummyFragment, FilterOperation::REFERENCE);
        RefPtr<FilterEffect> blurEffect =
            FEGaussianBlur::create(dummyFilterEffectRenderer.get(), 3.0f, 3.0f);
        blurEffect->setOperatingColorSpace(ColorSpaceLinearRGB);
        blurOperation->setFilterEffect(blurEffect, dummyFilterEffectRenderer);
        blurEffect->inputEffects().append(sourceEffect);
        ops.append(blurOperation);

        // Add a blend effect (with inputs : blur, source)
        RefPtr<ReferenceFilterOperation> blendOperation =
            ReferenceFilterOperation::create(dummyUrl, dummyFragment, FilterOperation::REFERENCE);
        RefPtr<FilterEffect> blendEffect =
            FEBlend::create(dummyFilterEffectRenderer.get(), FEBLEND_MODE_NORMAL);
        blendEffect->setOperatingColorSpace(ColorSpaceDeviceRGB);
        FilterEffectVector& blendInputs = blendEffect->inputEffects();
        blendInputs.reserveCapacity(2);
        blendInputs.append(sourceEffect);
        blendInputs.append(blurEffect);
        blendOperation->setFilterEffect(blendEffect, dummyFilterEffectRenderer);
        ops.append(blendOperation);

        // Add a merge effect (with inputs : blur, blend)
        RefPtr<ReferenceFilterOperation> mergeOperation =
            ReferenceFilterOperation::create(dummyUrl, dummyFragment, FilterOperation::REFERENCE);
        RefPtr<FilterEffect> mergeEffect =
            FEMerge::create(dummyFilterEffectRenderer.get());
        mergeEffect->setOperatingColorSpace(ColorSpaceLinearRGB);
        FilterEffectVector& mergeInputs = mergeEffect->inputEffects();
        mergeInputs.reserveCapacity(2);
        mergeInputs.append(blurEffect);
        mergeInputs.append(blendEffect);
        mergeOperation->setFilterEffect(mergeEffect, dummyFilterEffectRenderer);
        ops.append(mergeOperation);

        // Get SkImageFilter resulting tree
        SkiaImageFilterBuilder builder;
        SkAutoTUnref<SkImageFilter> filter(builder.build(filterOps));

        // Let's check that the resulting tree looks like this :
        //      ColorSpace (Linear->Device) : CS (L->D)
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

        EXPECT_EQ(filter->countInputs(), 1); // Should be CS (L->D)
        SkImageFilter* child = filter->getInput(0); // Should be Merge
        EXPECT_EQ(child->asColorFilter(0), false);
        EXPECT_EQ(child->countInputs(), 2);
        child = child->getInput(1); // Should be CS (D->L)
        EXPECT_EQ(child->asColorFilter(0), true);
        EXPECT_EQ(child->countInputs(), 1);
        child = child->getInput(0); // Should be Blend
        EXPECT_EQ(child->asColorFilter(0), false);
        EXPECT_EQ(child->countInputs(), 2);
        child = child->getInput(0); // Should be CS (L->D)
        EXPECT_EQ(child->asColorFilter(0), true);
        EXPECT_EQ(child->countInputs(), 1);
        child = child->getInput(0); // Should be Blur
        EXPECT_EQ(child->asColorFilter(0), false);
        EXPECT_EQ(child->countInputs(), 1);
        child = child->getInput(0); // Should be CS (D->L)
        EXPECT_EQ(child->asColorFilter(0), true);
        EXPECT_EQ(child->countInputs(), 1);
    }
};

namespace {

TEST_F(ImageFilterBuilderTest, testColorSpace)
{
    colorSpaceTest();
}

} // namespace
