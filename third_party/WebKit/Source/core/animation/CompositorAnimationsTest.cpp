/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/animation/CompositorAnimations.h"

#include "core/animation/AnimatableDouble.h"
#include "core/animation/AnimatableFilterOperations.h"
#include "core/animation/AnimatableTransform.h"
#include "core/animation/AnimatableValueTestHelper.h"
#include "core/animation/CompositorAnimationsImpl.h"
#include "core/animation/CompositorAnimationsTestHelper.h"
#include "core/platform/animation/TimingFunctionTestHelper.h"
#include "core/platform/graphics/filters/FilterOperations.h"
#include "platform/geometry/IntSize.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "public/platform/WebAnimation.h"
#include "wtf/HashFunctions.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace WebCore {

using ::testing::CloneToPassOwnPtr;
using ::testing::ExpectationSet;
using ::testing::Ref;
using ::testing::Return;
using ::testing::_;

class CoreAnimationCompositorAnimationsTest : public CoreAnimationCompositorAnimationsTestBase {

protected:
    RefPtr<TimingFunction> m_linearTimingFunction;
    RefPtr<TimingFunction> m_cubicEaseTimingFunction;
    RefPtr<TimingFunction> m_cubicCustomTimingFunction;
    RefPtr<TimingFunction> m_stepTimingFunction;

    Timing m_timing;
    CompositorAnimationsImpl::CompositorTiming m_compositorTiming;
    KeyframeAnimationEffect::KeyframeVector m_keyframeVector2;
    KeyframeAnimationEffect::KeyframeVector m_keyframeVector5;

    virtual void SetUp()
    {
        CoreAnimationCompositorAnimationsTestBase::SetUp();

        m_linearTimingFunction = LinearTimingFunction::create();
        m_cubicEaseTimingFunction = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease);
        m_cubicCustomTimingFunction = CubicBezierTimingFunction::create(1, 2, 3, 4);
        m_stepTimingFunction = StepsTimingFunction::create(1, false);

        m_timing = createCompositableTiming();
        m_compositorTiming = CompositorAnimationsImpl::CompositorTiming();
        // Make sure the CompositableTiming is really compositable, otherwise
        // most other tests will fail.
        ASSERT(convertTimingForCompositor(m_timing, m_compositorTiming));

        m_keyframeVector2 = createCompositableFloatKeyframeVector(2);
        m_keyframeVector5 = createCompositableFloatKeyframeVector(5);
    }

public:

    bool convertTimingForCompositor(const Timing& t, CompositorAnimationsImpl::CompositorTiming& out)
    {
        return CompositorAnimationsImpl::convertTimingForCompositor(t, out);
    }
    bool isCandidateForCompositor(const Timing& t, const KeyframeAnimationEffect::KeyframeVector& frames)
    {
        return CompositorAnimationsImpl::isCandidateForCompositor(t, frames);
    }
    bool isCandidateForCompositor(TimingFunction& t, const KeyframeAnimationEffect::KeyframeVector* frames)
    {
        return CompositorAnimationsImpl::isCandidateForCompositor(t, frames);
    }
    bool isCandidateForCompositor(const Keyframe& k)
    {
        return CompositorAnimationsImpl::isCandidateForCompositor(k);
    }
    bool isCandidateForCompositor(const KeyframeAnimationEffect& k)
    {
        return CompositorAnimationsImpl::isCandidateForCompositor(k);
    }
    void getCompositorAnimations(Timing& timing, KeyframeAnimationEffect& effect, Vector<OwnPtr<blink::WebAnimation> >& animations, const IntSize& size = empty)
    {
        return CompositorAnimationsImpl::getCompositorAnimations(timing, effect, animations, size);
    }

    static IntSize empty;

    // -------------------------------------------------------------------

    Timing createCompositableTiming()
    {
        Timing timing;
        timing.startDelay = 0;
        timing.fillMode = Timing::FillModeNone;
        timing.iterationStart = 0;
        timing.iterationCount = 1;
        timing.hasIterationDuration = true;
        timing.iterationDuration = 1.0;
        timing.playbackRate = 1.0;
        timing.direction = Timing::PlaybackDirectionNormal;
        ASSERT(m_linearTimingFunction);
        timing.timingFunction = m_linearTimingFunction;
        return timing;
    }

    PassRefPtr<Keyframe> createReplaceOpKeyframe(CSSPropertyID id, AnimatableValue* value, double offset = 0)
    {
        RefPtr<Keyframe> keyframe = Keyframe::create();
        keyframe->setPropertyValue(id, value);
        keyframe->setComposite(AnimationEffect::CompositeReplace);
        keyframe->setOffset(offset);
        return keyframe;
    }

    PassRefPtr<Keyframe> createDefaultKeyframe(CSSPropertyID id, AnimationEffect::CompositeOperation op, double offset = 0)
    {
        RefPtr<AnimatableDouble> value = AnimatableDouble::create(10.0);

        RefPtr<Keyframe> keyframe = createReplaceOpKeyframe(id, value.get(), offset);
        keyframe->setComposite(op);
        return keyframe;
    }

    KeyframeAnimationEffect::KeyframeVector createCompositableFloatKeyframeVector(size_t n)
    {
        Vector<double> values;
        for (size_t i = 0; i < n; i++) {
            values.append(static_cast<double>(i));
        }
        return createCompositableFloatKeyframeVector(values);
    }

    KeyframeAnimationEffect::KeyframeVector createCompositableFloatKeyframeVector(Vector<double>& values)
    {
        KeyframeAnimationEffect::KeyframeVector frames;
        for (size_t i = 0; i < values.size(); i++) {
            double offset = 1.0 / (values.size() - 1) * i;
            RefPtr<AnimatableDouble> value = AnimatableDouble::create(values[i]);
            frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, value.get(), offset).get());
        }
        return frames;
    }

    PassRefPtr<KeyframeAnimationEffect> createKeyframeAnimationEffect(PassRefPtr<Keyframe> prpFrom, PassRefPtr<Keyframe> prpTo, PassRefPtr<Keyframe> prpC = 0, PassRefPtr<Keyframe> prpD = 0)
    {
        RefPtr<Keyframe> from = prpFrom;
        RefPtr<Keyframe> to = prpTo;
        RefPtr<Keyframe> c = prpC;
        RefPtr<Keyframe> d = prpD;

        EXPECT_EQ(from->offset(), 0);
        KeyframeAnimationEffect::KeyframeVector frames;
        frames.append(from);
        EXPECT_LE(from->offset(), to->offset());
        frames.append(to);
        if (c) {
            EXPECT_LE(to->offset(), c->offset());
            frames.append(c);
        }
        if (d) {
            frames.append(d);
            EXPECT_LE(c->offset(), d->offset());
            EXPECT_EQ(d->offset(), 1.0);
        } else {
            EXPECT_EQ(to->offset(), 1.0);
        }
        if (!HasFatalFailure()) {
            return KeyframeAnimationEffect::create(frames);
        }
        return PassRefPtr<KeyframeAnimationEffect>();
    }

};

IntSize CoreAnimationCompositorAnimationsTest::empty;

class CustomFilterOperationMock : public FilterOperation {
public:
    virtual bool operator==(const FilterOperation&) const OVERRIDE FINAL {
        ASSERT_NOT_REACHED();
        return false;
    }

    MOCK_CONST_METHOD2(blend, PassRefPtr<FilterOperation>(const FilterOperation*, double));

    static PassRefPtr<CustomFilterOperationMock> create()
    {
        return adoptRef(new CustomFilterOperationMock());
    }

    CustomFilterOperationMock()
        : FilterOperation(FilterOperation::CUSTOM)
    {
    }
};

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorKeyframeCSSPropertySupported)
{
    EXPECT_TRUE(
        isCandidateForCompositor(
            *createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace).get()));

    EXPECT_TRUE(
        isCandidateForCompositor(
            *createDefaultKeyframe(CSSPropertyWebkitTransform, AnimationEffect::CompositeReplace).get()));

    EXPECT_FALSE(
        isCandidateForCompositor(
            *createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeAdd).get()));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorKeyframeCSSPropertyNotSupported)
{
    EXPECT_FALSE(
        isCandidateForCompositor(
            *createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace).get()));

    EXPECT_FALSE(
        isCandidateForCompositor(
            *createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeAdd).get()));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorKeyframeMultipleCSSProperties)
{
    // In this test, we cheat by using an AnimatableDouble even with Transform
    // as the actual value isn't considered.
    RefPtr<Keyframe> keyframeGoodMultiple = createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace);
    keyframeGoodMultiple->setPropertyValue(CSSPropertyWebkitTransform, AnimatableDouble::create(10.0).get());
    EXPECT_TRUE(isCandidateForCompositor(*keyframeGoodMultiple.get()));

    RefPtr<Keyframe> keyframeBadMultipleOp = createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeAdd);
    keyframeBadMultipleOp->setPropertyValue(CSSPropertyWebkitTransform, AnimatableDouble::create(10.0).get());
    EXPECT_FALSE(isCandidateForCompositor(*keyframeBadMultipleOp.get()));

    // Check both an unsupported property which hashes before and after the
    // supported property.
    typedef DefaultHash<CSSPropertyID>::Hash HashFunctions;

    RefPtr<Keyframe> keyframeBadMultiple1ID = createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace);
    keyframeBadMultiple1ID->setPropertyValue(CSSPropertyOpacity, AnimatableDouble::create(10.0).get());
    EXPECT_FALSE(isCandidateForCompositor(*keyframeBadMultiple1ID.get()));
    EXPECT_LT(HashFunctions::hash(CSSPropertyColor), HashFunctions::hash(CSSPropertyOpacity));

    RefPtr<Keyframe> keyframeBadMultiple2ID = createDefaultKeyframe(CSSPropertyWebkitTransform, AnimationEffect::CompositeReplace);
    keyframeBadMultiple2ID->setPropertyValue(CSSPropertyWidth, AnimatableDouble::create(10.0).get());
    EXPECT_FALSE(isCandidateForCompositor(*keyframeBadMultiple2ID.get()));
    EXPECT_GT(HashFunctions::hash(CSSPropertyWebkitTransform), HashFunctions::hash(CSSPropertyWidth));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isNotCandidateForCompositorCustomFilter)
{
    FilterOperations ops;
    ops.operations().append(BasicColorMatrixFilterOperation::create(0.5, FilterOperation::SATURATE));
    RefPtr<Keyframe> goodKeyframe = createReplaceOpKeyframe(CSSPropertyWebkitFilter, AnimatableFilterOperations::create(ops).get());
    EXPECT_TRUE(isCandidateForCompositor(*goodKeyframe.get()));

    ops.operations().append(CustomFilterOperationMock::create());
    RefPtr<Keyframe> badKeyframe = createReplaceOpKeyframe(CSSPropertyFilter, AnimatableFilterOperations::create(ops).get());
    EXPECT_FALSE(isCandidateForCompositor(*badKeyframe.get()));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorKeyframeEffectGoodSingleFrame)
{
    KeyframeAnimationEffect::KeyframeVector frames;
    frames.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace).get());
    EXPECT_TRUE(isCandidateForCompositor(*KeyframeAnimationEffect::create(frames).get()));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorKeyframeEffectBadSingleFrame)
{
    KeyframeAnimationEffect::KeyframeVector framesBadSingle;
    framesBadSingle.append(createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace).get());
    EXPECT_FALSE(isCandidateForCompositor(*KeyframeAnimationEffect::create(framesBadSingle).get()));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorKeyframeEffectMultipleFramesOkay)
{
    KeyframeAnimationEffect::KeyframeVector framesSame;
    framesSame.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    framesSame.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 1.0).get());
    EXPECT_TRUE(isCandidateForCompositor(*KeyframeAnimationEffect::create(framesSame).get()));

    KeyframeAnimationEffect::KeyframeVector framesMixed;
    framesMixed.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    framesMixed.append(createDefaultKeyframe(CSSPropertyWebkitTransform, AnimationEffect::CompositeReplace, 1.0).get());
    EXPECT_TRUE(isCandidateForCompositor(*KeyframeAnimationEffect::create(framesMixed).get()));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorKeyframeEffectMultipleFramesNotOkay)
{
    KeyframeAnimationEffect::KeyframeVector framesSame;
    framesSame.append(createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace, 0.0).get());
    framesSame.append(createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace, 1.0).get());
    EXPECT_FALSE(isCandidateForCompositor(*KeyframeAnimationEffect::create(framesSame).get()));

    KeyframeAnimationEffect::KeyframeVector framesMixedProperties;
    framesMixedProperties.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    framesMixedProperties.append(createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace, 1.0).get());
    EXPECT_FALSE(isCandidateForCompositor(*KeyframeAnimationEffect::create(framesMixedProperties).get()));

    KeyframeAnimationEffect::KeyframeVector framesMixedOps;
    framesMixedOps.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    framesMixedOps.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeAdd, 1.0).get());
    EXPECT_FALSE(isCandidateForCompositor(*KeyframeAnimationEffect::create(framesMixedOps).get()));
}

TEST_F(CoreAnimationCompositorAnimationsTest, ConvertTimingForCompositorStartDelay)
{
    m_timing.iterationDuration = 20.0;

    m_timing.startDelay = 2.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

    m_timing.startDelay = -2.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-2.0, m_compositorTiming.scaledTimeOffset);
}

TEST_F(CoreAnimationCompositorAnimationsTest, ConvertTimingForCompositorIterationStart)
{
    m_timing.iterationStart = 2.2;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));
}

TEST_F(CoreAnimationCompositorAnimationsTest, DISABLED_ConvertTimingForCompositorIterationCount)
{
    m_timing.iterationCount = 5.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(5, m_compositorTiming.adjustedIterationCount);

    m_timing.iterationCount = 5.5;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

    m_timing.iterationCount = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

#ifndef NDEBUG
    m_timing.iterationCount = -1;
    EXPECT_DEATH(convertTimingForCompositor(m_timing, m_compositorTiming), "");
#endif
}

TEST_F(CoreAnimationCompositorAnimationsTest, ConvertTimingForCompositorIterationsAndStartDelay)
{
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;

    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-1.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_DOUBLE_EQ(3.0, m_compositorTiming.adjustedIterationCount);
    EXPECT_FALSE(m_compositorTiming.reverse);

    m_timing.iterationCount = 1.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

    m_timing.iterationCount = 5.0;
    m_timing.iterationDuration = 1.0;
    m_timing.startDelay = -6.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));
}

TEST_F(CoreAnimationCompositorAnimationsTest, ConvertTimingForCompositorPlaybackRate)
{
    m_timing.playbackRate = 2.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

    m_timing.playbackRate = 0.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

    m_timing.playbackRate = -2.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));
}

TEST_F(CoreAnimationCompositorAnimationsTest, ConvertTimingForCompositorDirection)
{
    m_timing.direction = Timing::PlaybackDirectionAlternate;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_FALSE(m_compositorTiming.reverse);

    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_TRUE(m_compositorTiming.reverse);

    m_timing.direction = Timing::PlaybackDirectionReverse;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_FALSE(m_compositorTiming.alternate);
    EXPECT_TRUE(m_compositorTiming.reverse);
}

TEST_F(CoreAnimationCompositorAnimationsTest, ConvertTimingForCompositorDirectionIterationsAndStartDelay)
{
    m_timing.direction = Timing::PlaybackDirectionAlternate;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-1.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(3, m_compositorTiming.adjustedIterationCount);
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_TRUE(m_compositorTiming.reverse);

    m_timing.direction = Timing::PlaybackDirectionAlternate;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -11.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-1.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(2, m_compositorTiming.adjustedIterationCount);
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_FALSE(m_compositorTiming.reverse);

    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-1.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(3, m_compositorTiming.adjustedIterationCount);
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_FALSE(m_compositorTiming.reverse);

    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -11.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-1.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(2, m_compositorTiming.adjustedIterationCount);
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_TRUE(m_compositorTiming.reverse);
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTiming)
{
    EXPECT_TRUE(isCandidateForCompositor(m_timing, m_keyframeVector2));

    m_timing.startDelay = 2.0;
    EXPECT_FALSE(isCandidateForCompositor(m_timing, m_keyframeVector2));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingTimingFunctionPassThru)
{
    m_timing.timingFunction = m_stepTimingFunction;
    EXPECT_FALSE(isCandidateForCompositor(m_timing, m_keyframeVector2));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionLinear)
{
    EXPECT_TRUE(isCandidateForCompositor(*m_linearTimingFunction.get(), &m_keyframeVector2));
    EXPECT_TRUE(isCandidateForCompositor(*m_linearTimingFunction.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionCubic)
{
    // Cubic bezier are okay if we only have two keyframes
    EXPECT_TRUE(isCandidateForCompositor(*m_cubicEaseTimingFunction.get(), &m_keyframeVector2));
    EXPECT_FALSE(isCandidateForCompositor(*m_cubicEaseTimingFunction.get(), &m_keyframeVector5));

    EXPECT_TRUE(isCandidateForCompositor(*m_cubicCustomTimingFunction.get(), &m_keyframeVector2));
    EXPECT_FALSE(isCandidateForCompositor(*m_cubicCustomTimingFunction.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionSteps)
{
    RefPtr<TimingFunction> stepTiming = StepsTimingFunction::create(1, false);
    EXPECT_FALSE(isCandidateForCompositor(*m_stepTimingFunction.get(), &m_keyframeVector2));
    EXPECT_FALSE(isCandidateForCompositor(*m_stepTimingFunction.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionChainedEmpty)
{
    RefPtr<ChainedTimingFunction> chainedEmpty = ChainedTimingFunction::create();
    EXPECT_FALSE(isCandidateForCompositor(*chainedEmpty.get(), &m_keyframeVector2));
    EXPECT_FALSE(isCandidateForCompositor(*chainedEmpty.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionChainedLinear)
{
    RefPtr<ChainedTimingFunction> chainedLinearSingle = ChainedTimingFunction::create();
    chainedLinearSingle->appendSegment(1.0, m_linearTimingFunction.get());
    EXPECT_TRUE(isCandidateForCompositor(*chainedLinearSingle.get(), &m_keyframeVector2));

    RefPtr<ChainedTimingFunction> chainedLinearMultiple = ChainedTimingFunction::create();
    chainedLinearMultiple->appendSegment(0.25, m_linearTimingFunction.get());
    chainedLinearMultiple->appendSegment(0.5, m_linearTimingFunction.get());
    chainedLinearMultiple->appendSegment(0.75, m_linearTimingFunction.get());
    chainedLinearMultiple->appendSegment(1.0, m_linearTimingFunction.get());
    EXPECT_TRUE(isCandidateForCompositor(*chainedLinearMultiple.get(), &m_keyframeVector5));

    // FIXME: Technically a chained timing function of linear functions don't
    // have to be aligned to keyframes. We don't support that currently as
    // nothing generates that yet.
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionChainedCubicMatchingOffsets)
{
    RefPtr<ChainedTimingFunction> chainedSingleAGood = ChainedTimingFunction::create();
    chainedSingleAGood->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    EXPECT_TRUE(isCandidateForCompositor(*chainedSingleAGood.get(), &m_keyframeVector2));

    RefPtr<ChainedTimingFunction> chainedSingleBGood = ChainedTimingFunction::create();
    chainedSingleBGood->appendSegment(1.0, m_cubicCustomTimingFunction.get());
    EXPECT_TRUE(isCandidateForCompositor(*chainedSingleBGood.get(), &m_keyframeVector2));

    RefPtr<ChainedTimingFunction> chainedMultipleGood = ChainedTimingFunction::create();
    chainedMultipleGood->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chainedMultipleGood->appendSegment(0.5, m_cubicCustomTimingFunction.get());
    chainedMultipleGood->appendSegment(0.75, m_cubicCustomTimingFunction.get());
    chainedMultipleGood->appendSegment(1.0, m_cubicCustomTimingFunction.get());
    EXPECT_TRUE(isCandidateForCompositor(*chainedMultipleGood.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionChainedCubicNonMatchingOffsets)
{
    RefPtr<ChainedTimingFunction> chained0 = ChainedTimingFunction::create();
    chained0->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained0.get(), &m_keyframeVector2));

    RefPtr<ChainedTimingFunction> chained1 = ChainedTimingFunction::create();
    chained1->appendSegment(0.24, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained1.get(), &m_keyframeVector5));

    RefPtr<ChainedTimingFunction> chained2 = ChainedTimingFunction::create();
    chained2->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.51, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained2.get(), &m_keyframeVector5));

    RefPtr<ChainedTimingFunction> chained3 = ChainedTimingFunction::create();
    chained3->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.8, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained3.get(), &m_keyframeVector5));

    RefPtr<ChainedTimingFunction> chained4 = ChainedTimingFunction::create();
    chained4->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained4->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained4->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained4->appendSegment(1.1, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained4.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionMissingFrames)
{
    // Missing first
    RefPtr<ChainedTimingFunction> chained1 = ChainedTimingFunction::create();
    chained1->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained1.get(), &m_keyframeVector5));

    // Missing middle
    RefPtr<ChainedTimingFunction> chained2 = ChainedTimingFunction::create();
    chained2->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained2.get(), &m_keyframeVector5));

    // Missing last
    RefPtr<ChainedTimingFunction> chained3 = ChainedTimingFunction::create();
    chained3->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained3.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionToManyFrames)
{
    RefPtr<ChainedTimingFunction> chained1 = ChainedTimingFunction::create();
    chained1->appendSegment(0.1, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained1.get(), &m_keyframeVector2));

    RefPtr<ChainedTimingFunction> chained2 = ChainedTimingFunction::create();
    chained2->appendSegment(0.1, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chained2.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionMixedGood)
{
    RefPtr<ChainedTimingFunction> chainedMixed = ChainedTimingFunction::create();
    chainedMixed->appendSegment(0.25, m_linearTimingFunction.get());
    chainedMixed->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chainedMixed->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chainedMixed->appendSegment(1.0, m_linearTimingFunction.get());
    EXPECT_TRUE(isCandidateForCompositor(*chainedMixed.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionWithStepNotOkay)
{
    RefPtr<ChainedTimingFunction> chainedStepSingle = ChainedTimingFunction::create();
    chainedStepSingle->appendSegment(1.0, m_stepTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chainedStepSingle.get(), &m_keyframeVector2));

    RefPtr<ChainedTimingFunction> chainedStepMixedA = ChainedTimingFunction::create();
    chainedStepMixedA->appendSegment(0.25, m_stepTimingFunction.get());
    chainedStepMixedA->appendSegment(0.5, m_linearTimingFunction.get());
    chainedStepMixedA->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chainedStepMixedA.get(), &m_keyframeVector5));

    RefPtr<ChainedTimingFunction> chainedStepMixedB = ChainedTimingFunction::create();
    chainedStepMixedB->appendSegment(0.25, m_linearTimingFunction.get());
    chainedStepMixedB->appendSegment(0.5, m_stepTimingFunction.get());
    chainedStepMixedB->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chainedStepMixedB.get(), &m_keyframeVector5));

    RefPtr<ChainedTimingFunction> chainedStepMixedC = ChainedTimingFunction::create();
    chainedStepMixedC->appendSegment(0.25, m_linearTimingFunction.get());
    chainedStepMixedC->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chainedStepMixedC->appendSegment(1.0, m_stepTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chainedStepMixedC.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositorTimingFunctionNestedNotOkay)
{
    RefPtr<ChainedTimingFunction> chainedChild = ChainedTimingFunction::create();
    chainedChild->appendSegment(1.0, m_linearTimingFunction.get());

    RefPtr<ChainedTimingFunction> chainedParent = ChainedTimingFunction::create();
    chainedParent->appendSegment(0.25, m_linearTimingFunction.get());
    chainedParent->appendSegment(0.5, chainedChild.get());
    chainedParent->appendSegment(0.75, m_linearTimingFunction.get());
    chainedParent->appendSegment(1.0, m_linearTimingFunction.get());
    EXPECT_FALSE(isCandidateForCompositor(*chainedParent.get(), &m_keyframeVector5));
}

TEST_F(CoreAnimationCompositorAnimationsTest, isCandidateForCompositor)
{
    Timing linearTiming(createCompositableTiming());

    RefPtr<TimingFunction> cubicTimingFunc = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    Timing cubicTiming(createCompositableTiming());
    cubicTiming.timingFunction = cubicTimingFunc;

    RefPtr<ChainedTimingFunction> chainedTimingFunc = ChainedTimingFunction::create();
    chainedTimingFunc->appendSegment(0.5, m_linearTimingFunction.get());
    chainedTimingFunc->appendSegment(1.0, cubicTimingFunc.get());
    Timing chainedTiming(createCompositableTiming());
    chainedTiming.timingFunction = chainedTimingFunc;

    KeyframeAnimationEffect::KeyframeVector basicFramesVector;
    basicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    basicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 1.0).get());
    RefPtr<KeyframeAnimationEffect> basicFrames = KeyframeAnimationEffect::create(basicFramesVector).get();

    EXPECT_TRUE(CompositorAnimations::instance()->isCandidateForCompositorAnimation(linearTiming, *basicFrames.get()));
    EXPECT_TRUE(CompositorAnimations::instance()->isCandidateForCompositorAnimation(cubicTiming, *basicFrames.get()));
    // number of timing function and keyframes don't match
    EXPECT_FALSE(CompositorAnimations::instance()->isCandidateForCompositorAnimation(chainedTiming, *basicFrames.get()));

    KeyframeAnimationEffect::KeyframeVector nonBasicFramesVector;
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.5).get());
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 1.0).get());
    RefPtr<KeyframeAnimationEffect> nonBasicFrames = KeyframeAnimationEffect::create(nonBasicFramesVector).get();

    EXPECT_TRUE(CompositorAnimations::instance()->isCandidateForCompositorAnimation(linearTiming, *nonBasicFrames.get()));
    EXPECT_FALSE(CompositorAnimations::instance()->isCandidateForCompositorAnimation(cubicTiming, *nonBasicFrames.get()));
    EXPECT_TRUE(CompositorAnimations::instance()->isCandidateForCompositorAnimation(chainedTiming, *nonBasicFrames.get()));
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_F(CoreAnimationCompositorAnimationsTest, createSimpleOpacityAnimation)
{
    // Animation to convert
    RefPtr<KeyframeAnimationEffect> effect = createKeyframeAnimationEffect(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));
    // --

    WebCompositorSupportMock mockCompositor;

    // Curve is created
    blink::WebFloatAnimationCurveMock* mockCurvePtr = new blink::WebFloatAnimationCurveMock;
    ExpectationSet usesMockCurve;
    EXPECT_CALL(mockCompositor, createFloatAnimationCurve())
        .WillOnce(Return(mockCurvePtr));

    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.0, 2.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(1.0, 5.0)));

    // Create animation
    blink::WebAnimationMock* mockAnimationPtr = new blink::WebAnimationMock(blink::WebAnimation::TargetPropertyOpacity);
    ExpectationSet usesMockAnimation;

    usesMockCurve += EXPECT_CALL(mockCompositor, createAnimation(Ref(*mockCurvePtr), blink::WebAnimation::TargetPropertyOpacity, _))
        .WillOnce(Return(mockAnimationPtr));

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(1));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(0.0));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setAlternatesDirection(false));

    EXPECT_CALL(*mockAnimationPtr, delete_())
        .Times(1)
        .After(usesMockAnimation);
    EXPECT_CALL(*mockCurvePtr, delete_())
        .Times(1)
        .After(usesMockCurve);

    // Go!
    setCompositorForTesting(mockCompositor);
    Vector<OwnPtr<blink::WebAnimation> > result;
    getCompositorAnimations(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(CoreAnimationCompositorAnimationsTest, createSimpleOpacityAnimationDuration)
{
    // Animation to convert
    RefPtr<KeyframeAnimationEffect> effect = createKeyframeAnimationEffect(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.iterationDuration = 10.0;
    // --

    WebCompositorSupportMock mockCompositor;

    // Curve is created
    blink::WebFloatAnimationCurveMock* mockCurvePtr = new blink::WebFloatAnimationCurveMock;
    ExpectationSet usesMockCurve;
    EXPECT_CALL(mockCompositor, createFloatAnimationCurve())
        .WillOnce(Return(mockCurvePtr));

    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.0, 2.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(10.0, 5.0)));

    // Create animation
    blink::WebAnimationMock* mockAnimationPtr = new blink::WebAnimationMock(blink::WebAnimation::TargetPropertyOpacity);
    ExpectationSet usesMockAnimation;

    usesMockCurve += EXPECT_CALL(mockCompositor, createAnimation(Ref(*mockCurvePtr), blink::WebAnimation::TargetPropertyOpacity, _))
        .WillOnce(Return(mockAnimationPtr));

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(1));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(0.0));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setAlternatesDirection(false));

    EXPECT_CALL(*mockAnimationPtr, delete_())
        .Times(1)
        .After(usesMockAnimation);
    EXPECT_CALL(*mockCurvePtr, delete_())
        .Times(1)
        .After(usesMockCurve);

    // Go!
    setCompositorForTesting(mockCompositor);
    Vector<OwnPtr<blink::WebAnimation> > result;
    getCompositorAnimations(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(CoreAnimationCompositorAnimationsTest, createMultipleKeyframeOpacityAnimationLinear)
{
    // Animation to convert
    RefPtr<KeyframeAnimationEffect> effect = createKeyframeAnimationEffect(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(-1.0).get(), 0.25),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(20.0).get(), 0.5),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.iterationCount = 5;
    m_timing.direction = Timing::PlaybackDirectionAlternate;
    // --

    WebCompositorSupportMock mockCompositor;

    // Curve is created
    blink::WebFloatAnimationCurveMock* mockCurvePtr = new blink::WebFloatAnimationCurveMock();
    ExpectationSet usesMockCurve;

    EXPECT_CALL(mockCompositor, createFloatAnimationCurve())
        .WillOnce(Return(mockCurvePtr));

    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.0, 2.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.25, -1.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.5, 20.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(1.0, 5.0)));

    // Animation is created
    blink::WebAnimationMock* mockAnimationPtr = new blink::WebAnimationMock(blink::WebAnimation::TargetPropertyOpacity);
    ExpectationSet usesMockAnimation;

    usesMockCurve += EXPECT_CALL(mockCompositor, createAnimation(Ref(*mockCurvePtr), blink::WebAnimation::TargetPropertyOpacity, _))
        .WillOnce(Return(mockAnimationPtr));

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(5));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(0.0));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setAlternatesDirection(true));

    EXPECT_CALL(*mockAnimationPtr, delete_())
        .Times(1)
        .After(usesMockAnimation);
    EXPECT_CALL(*mockCurvePtr, delete_())
        .Times(1)
        .After(usesMockCurve);

    // Go!
    setCompositorForTesting(mockCompositor);
    Vector<OwnPtr<blink::WebAnimation> > result;
    getCompositorAnimations(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(CoreAnimationCompositorAnimationsTest, createSimpleOpacityAnimationNegativeStartDelay)
{
    // Animation to convert
    RefPtr<KeyframeAnimationEffect> effect = createKeyframeAnimationEffect(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.iterationCount = 5.0;
    m_timing.iterationDuration = 1.75;
    m_timing.startDelay = -3.25;
    // --

    WebCompositorSupportMock mockCompositor;

    // Curve is created
    blink::WebFloatAnimationCurveMock* mockCurvePtr = new blink::WebFloatAnimationCurveMock;
    ExpectationSet usesMockCurve;
    EXPECT_CALL(mockCompositor, createFloatAnimationCurve())
        .WillOnce(Return(mockCurvePtr));

    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.0, 2.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(1.75, 5.0)));

    // Create animation
    blink::WebAnimationMock* mockAnimationPtr = new blink::WebAnimationMock(blink::WebAnimation::TargetPropertyOpacity);
    ExpectationSet usesMockAnimation;

    usesMockCurve += EXPECT_CALL(mockCompositor, createAnimation(Ref(*mockCurvePtr), blink::WebAnimation::TargetPropertyOpacity, _))
        .WillOnce(Return(mockAnimationPtr));

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(4));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(-1.5));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setAlternatesDirection(false));

    EXPECT_CALL(*mockAnimationPtr, delete_())
        .Times(1)
        .After(usesMockAnimation);
    EXPECT_CALL(*mockCurvePtr, delete_())
        .Times(1)
        .After(usesMockCurve);

    // Go!
    setCompositorForTesting(mockCompositor);
    Vector<OwnPtr<blink::WebAnimation> > result;
    getCompositorAnimations(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(CoreAnimationCompositorAnimationsTest, createMultipleKeyframeOpacityAnimationChained)
{
    // Animation to convert
    RefPtr<KeyframeAnimationEffect> effect = createKeyframeAnimationEffect(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(-1.0).get(), 0.25),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(20.0).get(), 0.5),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    RefPtr<ChainedTimingFunction> chainedTimingFunction = ChainedTimingFunction::create();
    chainedTimingFunction->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chainedTimingFunction->appendSegment(0.5, m_linearTimingFunction.get());
    chainedTimingFunction->appendSegment(1.0, m_cubicCustomTimingFunction.get());

    m_timing.timingFunction = chainedTimingFunction;
    m_timing.iterationCount = 10;
    m_timing.direction = Timing::PlaybackDirectionAlternate;
    // --

    WebCompositorSupportMock mockCompositor;

    // Curve is created
    blink::WebFloatAnimationCurveMock* mockCurvePtr = new blink::WebFloatAnimationCurveMock();
    ExpectationSet usesMockCurve;

    EXPECT_CALL(mockCompositor, createFloatAnimationCurve())
        .WillOnce(Return(mockCurvePtr));

    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.0, 2.0), blink::WebAnimationCurve::TimingFunctionTypeEase));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.25, -1.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.5, 20.0), 1.0, 2.0, 3.0, 4.0));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(1.0, 5.0)));

    // Animation is created
    blink::WebAnimationMock* mockAnimationPtr = new blink::WebAnimationMock(blink::WebAnimation::TargetPropertyOpacity);
    ExpectationSet usesMockAnimation;

    usesMockCurve += EXPECT_CALL(mockCompositor, createAnimation(Ref(*mockCurvePtr), blink::WebAnimation::TargetPropertyOpacity, _))
        .WillOnce(Return(mockAnimationPtr));

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(10));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(0.0));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setAlternatesDirection(true));

    EXPECT_CALL(*mockAnimationPtr, delete_())
        .Times(1)
        .After(usesMockAnimation);
    EXPECT_CALL(*mockCurvePtr, delete_())
        .Times(1)
        .After(usesMockCurve);

    // Go!
    setCompositorForTesting(mockCompositor);
    Vector<OwnPtr<blink::WebAnimation> > result;
    getCompositorAnimations(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(CoreAnimationCompositorAnimationsTest, createReversedOpacityAnimation)
{
    // Animation to convert
    RefPtr<KeyframeAnimationEffect> effect = createKeyframeAnimationEffect(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(-1.0).get(), 0.25),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(20.0).get(), 0.5),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    RefPtr<TimingFunction> cubicEasyFlipTimingFunction = CubicBezierTimingFunction::create(0.0, 0.0, 0.0, 1.0);
    RefPtr<ChainedTimingFunction> chainedTimingFunction = ChainedTimingFunction::create();
    chainedTimingFunction->appendSegment(0.25, CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn));
    chainedTimingFunction->appendSegment(0.5, m_linearTimingFunction.get());
    chainedTimingFunction->appendSegment(1.0, cubicEasyFlipTimingFunction.get());

    m_timing.timingFunction = chainedTimingFunction;
    m_timing.iterationCount = 10;
    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    // --

    WebCompositorSupportMock mockCompositor;

    // Curve is created
    blink::WebFloatAnimationCurveMock* mockCurvePtr = new blink::WebFloatAnimationCurveMock();
    ExpectationSet usesMockCurve;

    EXPECT_CALL(mockCompositor, createFloatAnimationCurve())
        .WillOnce(Return(mockCurvePtr));

    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.0, 5.0), 1.0, 0.0, 1.0, 1.0));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.5, 20.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.75, -1.0), blink::WebAnimationCurve::TimingFunctionTypeEaseOut));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(1.0, 2.0)));

    // Create the animation
    blink::WebAnimationMock* mockAnimationPtr = new blink::WebAnimationMock(blink::WebAnimation::TargetPropertyOpacity);
    ExpectationSet usesMockAnimation;

    usesMockCurve += EXPECT_CALL(mockCompositor, createAnimation(Ref(*mockCurvePtr), blink::WebAnimation::TargetPropertyOpacity, _))
        .WillOnce(Return(mockAnimationPtr));

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(10));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(0.0));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setAlternatesDirection(true));

    EXPECT_CALL(*mockAnimationPtr, delete_())
        .Times(1)
        .After(usesMockAnimation);
    EXPECT_CALL(*mockCurvePtr, delete_())
        .Times(1)
        .After(usesMockCurve);

    // Go!
    setCompositorForTesting(mockCompositor);
    Vector<OwnPtr<blink::WebAnimation> > result;
    getCompositorAnimations(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(CoreAnimationCompositorAnimationsTest, createReversedOpacityAnimationNegativeStartDelay)
{
    // Animation to convert
    RefPtr<KeyframeAnimationEffect> effect = createKeyframeAnimationEffect(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.iterationCount = 5.0;
    m_timing.iterationDuration = 2.0;
    m_timing.startDelay = -3;
    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    // --

    WebCompositorSupportMock mockCompositor;

    // Curve is created
    blink::WebFloatAnimationCurveMock* mockCurvePtr = new blink::WebFloatAnimationCurveMock;
    ExpectationSet usesMockCurve;
    EXPECT_CALL(mockCompositor, createFloatAnimationCurve())
        .WillOnce(Return(mockCurvePtr));

    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.0, 2.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(2.0, 5.0)));

    // Create animation
    blink::WebAnimationMock* mockAnimationPtr = new blink::WebAnimationMock(blink::WebAnimation::TargetPropertyOpacity);
    ExpectationSet usesMockAnimation;

    usesMockCurve += EXPECT_CALL(mockCompositor, createAnimation(Ref(*mockCurvePtr), blink::WebAnimation::TargetPropertyOpacity, _))
        .WillOnce(Return(mockAnimationPtr));

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(4));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(-1.0));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setAlternatesDirection(true));

    EXPECT_CALL(*mockAnimationPtr, delete_())
        .Times(1)
        .After(usesMockAnimation);
    EXPECT_CALL(*mockCurvePtr, delete_())
        .Times(1)
        .After(usesMockCurve);

    // Go!
    setCompositorForTesting(mockCompositor);
    Vector<OwnPtr<blink::WebAnimation> > result;
    getCompositorAnimations(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}


} // namespace WebCore
