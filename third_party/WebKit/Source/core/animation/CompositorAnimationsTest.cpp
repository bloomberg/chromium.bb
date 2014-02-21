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
#include "platform/animation/TimingFunctionTestHelper.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/filters/FilterOperations.h"
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

class AnimationCompositorAnimationsTest : public AnimationCompositorAnimationsTestBase {

protected:
    RefPtr<TimingFunction> m_linearTimingFunction;
    RefPtr<TimingFunction> m_cubicEaseTimingFunction;
    RefPtr<TimingFunction> m_cubicCustomTimingFunction;
    RefPtr<TimingFunction> m_stepTimingFunction;

    Timing m_timing;
    CompositorAnimationsImpl::CompositorTiming m_compositorTiming;
    KeyframeEffectModel::KeyframeVector m_keyframeVector2;
    RefPtr<KeyframeEffectModel> m_keyframeAnimationEffect2;
    KeyframeEffectModel::KeyframeVector m_keyframeVector5;
    RefPtr<KeyframeEffectModel> m_keyframeAnimationEffect5;

    virtual void SetUp()
    {
        AnimationCompositorAnimationsTestBase::SetUp();

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
        m_keyframeAnimationEffect2 = KeyframeEffectModel::create(m_keyframeVector2);

        m_keyframeVector5 = createCompositableFloatKeyframeVector(5);
        m_keyframeAnimationEffect5 = KeyframeEffectModel::create(m_keyframeVector5);
    }

public:

    bool convertTimingForCompositor(const Timing& t, CompositorAnimationsImpl::CompositorTiming& out)
    {
        return CompositorAnimationsImpl::convertTimingForCompositor(t, out);
    }
    bool isCandidateForAnimationOnCompositor(const Timing& timing, const AnimationEffect& effect)
    {
        return CompositorAnimations::instance()->isCandidateForAnimationOnCompositor(timing, effect);
    }
    void getAnimationOnCompositor(Timing& timing, KeyframeEffectModel& effect, Vector<OwnPtr<blink::WebAnimation> >& animations)
    {
        return CompositorAnimationsImpl::getAnimationOnCompositor(timing, effect, animations);
    }

    bool isCandidateHelperForSingleKeyframe(Keyframe* frame)
    {
        EXPECT_EQ(frame->offset(), 0);
        KeyframeEffectModel::KeyframeVector frames;
        frames.append(frame);
        EXPECT_EQ(m_keyframeVector2[1]->offset(), 1.0);
        frames.append(m_keyframeVector2[1]);
        return isCandidateForAnimationOnCompositor(m_timing, *KeyframeEffectModel::create(frames).get());
    }

    // -------------------------------------------------------------------

    Timing createCompositableTiming()
    {
        Timing timing;
        timing.startDelay = 0;
        timing.fillMode = Timing::FillModeNone;
        timing.iterationStart = 0;
        timing.iterationCount = 1;
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
        RefPtr<AnimatableValue> value;
        if (id == CSSPropertyWebkitTransform)
            value = AnimatableTransform::create(TransformOperations());
        else
            value = AnimatableDouble::create(10.0);

        RefPtr<Keyframe> keyframe = createReplaceOpKeyframe(id, value.get(), offset);
        keyframe->setComposite(op);
        return keyframe;
    }

    KeyframeEffectModel::KeyframeVector createCompositableFloatKeyframeVector(size_t n)
    {
        Vector<double> values;
        for (size_t i = 0; i < n; i++) {
            values.append(static_cast<double>(i));
        }
        return createCompositableFloatKeyframeVector(values);
    }

    KeyframeEffectModel::KeyframeVector createCompositableFloatKeyframeVector(Vector<double>& values)
    {
        KeyframeEffectModel::KeyframeVector frames;
        for (size_t i = 0; i < values.size(); i++) {
            double offset = 1.0 / (values.size() - 1) * i;
            RefPtr<AnimatableDouble> value = AnimatableDouble::create(values[i]);
            frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, value.get(), offset).get());
        }
        return frames;
    }

    PassRefPtr<KeyframeEffectModel> createKeyframeEffectModel(PassRefPtr<Keyframe> prpFrom, PassRefPtr<Keyframe> prpTo, PassRefPtr<Keyframe> prpC = nullptr, PassRefPtr<Keyframe> prpD = nullptr)
    {
        RefPtr<Keyframe> from = prpFrom;
        RefPtr<Keyframe> to = prpTo;
        RefPtr<Keyframe> c = prpC;
        RefPtr<Keyframe> d = prpD;

        EXPECT_EQ(from->offset(), 0);
        KeyframeEffectModel::KeyframeVector frames;
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
            return KeyframeEffectModel::create(frames);
        }
        return PassRefPtr<KeyframeEffectModel>();
    }

};

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorKeyframeMultipleCSSProperties)
{
    RefPtr<Keyframe> keyframeGoodMultiple = createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace);
    keyframeGoodMultiple->setPropertyValue(CSSPropertyWebkitTransform, AnimatableTransform::create(TransformOperations()).get());
    EXPECT_TRUE(isCandidateHelperForSingleKeyframe(keyframeGoodMultiple.get()));

    RefPtr<Keyframe> keyframeBadMultipleOp = createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeAdd);
    keyframeBadMultipleOp->setPropertyValue(CSSPropertyWebkitTransform, AnimatableDouble::create(10.0).get());
    EXPECT_FALSE(isCandidateHelperForSingleKeyframe(keyframeBadMultipleOp.get()));

    RefPtr<Keyframe> keyframeBadMultipleID = createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace);
    keyframeBadMultipleID->setPropertyValue(CSSPropertyOpacity, AnimatableDouble::create(10.0).get());
    EXPECT_FALSE(isCandidateHelperForSingleKeyframe(keyframeBadMultipleID.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isNotCandidateForCompositorAnimationTransformDependsOnBoxSize)
{
    TransformOperations ops;
    ops.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(2, WebCore::Fixed), TransformOperation::TranslateX));
    RefPtr<Keyframe> goodKeyframe = createReplaceOpKeyframe(CSSPropertyWebkitTransform, AnimatableTransform::create(ops).get());
    EXPECT_TRUE(isCandidateHelperForSingleKeyframe(goodKeyframe.get()));

    ops.operations().append(TranslateTransformOperation::create(Length(50, WebCore::Percent), Length(2, WebCore::Fixed), TransformOperation::TranslateX));
    RefPtr<Keyframe> badKeyframe = createReplaceOpKeyframe(CSSPropertyWebkitTransform, AnimatableTransform::create(ops).get());
    EXPECT_FALSE(isCandidateHelperForSingleKeyframe(badKeyframe.get()));

    TransformOperations ops2;
    Length calcLength = Length(100, WebCore::Percent).blend(Length(100, WebCore::Fixed), 0.5, WebCore::ValueRangeAll);
    ops2.operations().append(TranslateTransformOperation::create(calcLength, Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    RefPtr<Keyframe> badKeyframe2 = createReplaceOpKeyframe(CSSPropertyWebkitTransform, AnimatableTransform::create(ops2).get());
    EXPECT_FALSE(isCandidateHelperForSingleKeyframe(badKeyframe2.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorKeyframeEffectModelMultipleFramesOkay)
{
    KeyframeEffectModel::KeyframeVector framesSame;
    framesSame.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    framesSame.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 1.0).get());
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *KeyframeEffectModel::create(framesSame).get()));

    KeyframeEffectModel::KeyframeVector framesMixed;
    framesMixed.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    framesMixed.append(createDefaultKeyframe(CSSPropertyWebkitTransform, AnimationEffect::CompositeReplace, 1.0).get());
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *KeyframeEffectModel::create(framesMixed).get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorKeyframeEffectModel)
{
    KeyframeEffectModel::KeyframeVector framesSame;
    framesSame.append(createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace, 0.0).get());
    framesSame.append(createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace, 1.0).get());
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *KeyframeEffectModel::create(framesSame).get()));

    KeyframeEffectModel::KeyframeVector framesMixedProperties;
    framesMixedProperties.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    framesMixedProperties.append(createDefaultKeyframe(CSSPropertyColor, AnimationEffect::CompositeReplace, 1.0).get());
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *KeyframeEffectModel::create(framesMixedProperties).get()));

    KeyframeEffectModel::KeyframeVector framesMixedOps;
    framesMixedOps.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    framesMixedOps.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeAdd, 1.0).get());
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *KeyframeEffectModel::create(framesMixedOps).get()));
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorStartDelay)
{
    m_timing.iterationDuration = 20.0;

    m_timing.startDelay = 2.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-2.0, m_compositorTiming.scaledTimeOffset);

    m_timing.startDelay = -2.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(2.0, m_compositorTiming.scaledTimeOffset);
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorIterationStart)
{
    m_timing.iterationStart = 2.2;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorIterationCount)
{
    m_timing.iterationCount = 5.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(5, m_compositorTiming.adjustedIterationCount);

    m_timing.iterationCount = 5.5;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

    // Asserts will only trigger on DEBUG build.
    // EXPECT_DEATH tests are flaky on Android.
#if !defined(NDEBUG) && !OS(ANDROID)
    m_timing.iterationCount = -1;
    EXPECT_DEATH(convertTimingForCompositor(m_timing, m_compositorTiming), "");
#endif

    m_timing.iterationCount = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(-1, m_compositorTiming.adjustedIterationCount);

    m_timing.iterationCount = std::numeric_limits<double>::infinity();
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(-1, m_compositorTiming.adjustedIterationCount);
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorIterationsAndStartDelay)
{
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;

    m_timing.startDelay = 6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_DOUBLE_EQ(4.0, m_compositorTiming.adjustedIterationCount);

    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_DOUBLE_EQ(4.0, m_compositorTiming.adjustedIterationCount);

    m_timing.startDelay = 21.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorPlaybackRate)
{
    m_timing.playbackRate = 2.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

    m_timing.playbackRate = 0.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));

    m_timing.playbackRate = -2.0;
    EXPECT_FALSE(convertTimingForCompositor(m_timing, m_compositorTiming));
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorDirection)
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

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorDirectionIterationsAndStartDelay)
{
    m_timing.direction = Timing::PlaybackDirectionAlternate;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(4, m_compositorTiming.adjustedIterationCount);
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_FALSE(m_compositorTiming.reverse);

    m_timing.direction = Timing::PlaybackDirectionAlternate;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -11.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(11.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(4, m_compositorTiming.adjustedIterationCount);
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_FALSE(m_compositorTiming.reverse);

    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(4, m_compositorTiming.adjustedIterationCount);
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_TRUE(m_compositorTiming.reverse);

    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -11.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(11.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(4, m_compositorTiming.adjustedIterationCount);
    EXPECT_TRUE(m_compositorTiming.alternate);
    EXPECT_TRUE(m_compositorTiming.reverse);
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingTimingFunctionPassThru)
{
    m_timing.timingFunction = m_stepTimingFunction;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionLinear)
{
    m_timing.timingFunction = m_linearTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionCubic)
{
    // Cubic bezier are okay if we only have two keyframes
    m_timing.timingFunction = m_cubicEaseTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    m_timing.timingFunction = m_cubicCustomTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionSteps)
{
    m_timing.timingFunction = m_stepTimingFunction;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionChainedEmpty)
{
    RefPtr<ChainedTimingFunction> chainedEmpty = ChainedTimingFunction::create();
    m_timing.timingFunction = chainedEmpty;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionChainedLinear)
{
    RefPtr<ChainedTimingFunction> chainedLinearSingle = ChainedTimingFunction::create();
    chainedLinearSingle->appendSegment(1.0, m_linearTimingFunction.get());
    m_timing.timingFunction = chainedLinearSingle;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));

    RefPtr<ChainedTimingFunction> chainedLinearMultiple = ChainedTimingFunction::create();
    chainedLinearMultiple->appendSegment(0.25, m_linearTimingFunction.get());
    chainedLinearMultiple->appendSegment(0.5, m_linearTimingFunction.get());
    chainedLinearMultiple->appendSegment(0.75, m_linearTimingFunction.get());
    chainedLinearMultiple->appendSegment(1.0, m_linearTimingFunction.get());
    m_timing.timingFunction = chainedLinearMultiple;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    // FIXME: Technically a chained timing function of linear functions don't
    // have to be aligned to keyframes. We don't support that currently as
    // nothing generates that yet.
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionChainedCubicMatchingOffsets)
{
    RefPtr<ChainedTimingFunction> chainedSingleAGood = ChainedTimingFunction::create();
    chainedSingleAGood->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chainedSingleAGood;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));

    RefPtr<ChainedTimingFunction> chainedSingleBGood = ChainedTimingFunction::create();
    chainedSingleBGood->appendSegment(1.0, m_cubicCustomTimingFunction.get());
    m_timing.timingFunction = chainedSingleBGood;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));

    RefPtr<ChainedTimingFunction> chainedMultipleGood = ChainedTimingFunction::create();
    chainedMultipleGood->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chainedMultipleGood->appendSegment(0.5, m_cubicCustomTimingFunction.get());
    chainedMultipleGood->appendSegment(0.75, m_cubicCustomTimingFunction.get());
    chainedMultipleGood->appendSegment(1.0, m_cubicCustomTimingFunction.get());
    m_timing.timingFunction = chainedMultipleGood;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionChainedCubicNonMatchingOffsets)
{
    RefPtr<ChainedTimingFunction> chained0 = ChainedTimingFunction::create();
    chained0->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained0;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));

    RefPtr<ChainedTimingFunction> chained1 = ChainedTimingFunction::create();
    chained1->appendSegment(0.24, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained1;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    RefPtr<ChainedTimingFunction> chained2 = ChainedTimingFunction::create();
    chained2->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.51, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained2;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    RefPtr<ChainedTimingFunction> chained3 = ChainedTimingFunction::create();
    chained3->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.8, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained3;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    RefPtr<ChainedTimingFunction> chained4 = ChainedTimingFunction::create();
    chained4->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained4->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained4->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained4->appendSegment(1.1, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained4;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionMissingFrames)
{
    // Missing first
    RefPtr<ChainedTimingFunction> chained1 = ChainedTimingFunction::create();
    chained1->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained1;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    // Missing middle
    RefPtr<ChainedTimingFunction> chained2 = ChainedTimingFunction::create();
    chained2->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained2;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    // Missing last
    RefPtr<ChainedTimingFunction> chained3 = ChainedTimingFunction::create();
    chained3->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained3->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained3;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionToManyFrames)
{
    RefPtr<ChainedTimingFunction> chained1 = ChainedTimingFunction::create();
    chained1->appendSegment(0.1, m_cubicEaseTimingFunction.get());
    chained1->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained1;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));

    RefPtr<ChainedTimingFunction> chained2 = ChainedTimingFunction::create();
    chained2->appendSegment(0.1, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chained2->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chained2;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionMixedGood)
{
    RefPtr<ChainedTimingFunction> chainedMixed = ChainedTimingFunction::create();
    chainedMixed->appendSegment(0.25, m_linearTimingFunction.get());
    chainedMixed->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chainedMixed->appendSegment(0.75, m_cubicEaseTimingFunction.get());
    chainedMixed->appendSegment(1.0, m_linearTimingFunction.get());
    m_timing.timingFunction = chainedMixed;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionWithStepNotOkay)
{
    RefPtr<ChainedTimingFunction> chainedStepSingle = ChainedTimingFunction::create();
    chainedStepSingle->appendSegment(1.0, m_stepTimingFunction.get());
    m_timing.timingFunction = chainedStepSingle;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2.get()));

    RefPtr<ChainedTimingFunction> chainedStepMixedA = ChainedTimingFunction::create();
    chainedStepMixedA->appendSegment(0.25, m_stepTimingFunction.get());
    chainedStepMixedA->appendSegment(0.5, m_linearTimingFunction.get());
    chainedStepMixedA->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chainedStepMixedA;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    RefPtr<ChainedTimingFunction> chainedStepMixedB = ChainedTimingFunction::create();
    chainedStepMixedB->appendSegment(0.25, m_linearTimingFunction.get());
    chainedStepMixedB->appendSegment(0.5, m_stepTimingFunction.get());
    chainedStepMixedB->appendSegment(1.0, m_cubicEaseTimingFunction.get());
    m_timing.timingFunction = chainedStepMixedB;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));

    RefPtr<ChainedTimingFunction> chainedStepMixedC = ChainedTimingFunction::create();
    chainedStepMixedC->appendSegment(0.25, m_linearTimingFunction.get());
    chainedStepMixedC->appendSegment(0.5, m_cubicEaseTimingFunction.get());
    chainedStepMixedC->appendSegment(1.0, m_stepTimingFunction.get());
    m_timing.timingFunction = chainedStepMixedC;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionNestedNotOkay)
{
    RefPtr<ChainedTimingFunction> chainedChild = ChainedTimingFunction::create();
    chainedChild->appendSegment(1.0, m_linearTimingFunction.get());

    RefPtr<ChainedTimingFunction> chainedParent = ChainedTimingFunction::create();
    chainedParent->appendSegment(0.25, m_linearTimingFunction.get());
    chainedParent->appendSegment(0.5, chainedChild.get());
    chainedParent->appendSegment(0.75, m_linearTimingFunction.get());
    chainedParent->appendSegment(1.0, m_linearTimingFunction.get());
    m_timing.timingFunction = chainedParent;
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositor)
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

    KeyframeEffectModel::KeyframeVector basicFramesVector;
    basicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    basicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 1.0).get());
    RefPtr<KeyframeEffectModel> basicFrames = KeyframeEffectModel::create(basicFramesVector).get();

    EXPECT_TRUE(isCandidateForAnimationOnCompositor(linearTiming, *basicFrames.get()));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(cubicTiming, *basicFrames.get()));
    // number of timing function and keyframes don't match
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(chainedTiming, *basicFrames.get()));

    KeyframeEffectModel::KeyframeVector nonBasicFramesVector;
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.0).get());
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 0.5).get());
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, AnimationEffect::CompositeReplace, 1.0).get());
    RefPtr<KeyframeEffectModel> nonBasicFrames = KeyframeEffectModel::create(nonBasicFramesVector).get();

    EXPECT_TRUE(CompositorAnimations::instance()->isCandidateForAnimationOnCompositor(linearTiming, *nonBasicFrames.get()));
    EXPECT_FALSE(CompositorAnimations::instance()->isCandidateForAnimationOnCompositor(cubicTiming, *nonBasicFrames.get()));
    EXPECT_TRUE(CompositorAnimations::instance()->isCandidateForAnimationOnCompositor(chainedTiming, *nonBasicFrames.get()));
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimation)
{
    // Animation to convert
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(
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
    getAnimationOnCompositor(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimationDuration)
{
    // Animation to convert
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(
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
    getAnimationOnCompositor(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(AnimationCompositorAnimationsTest, createMultipleKeyframeOpacityAnimationLinear)
{
    // Animation to convert
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(
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
    getAnimationOnCompositor(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimationStartDelay)
{
    // Animation to convert
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.iterationCount = 5.0;
    m_timing.iterationDuration = 1.75;
    m_timing.startDelay = 3.25;
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

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(5));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(-3.25));
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
    getAnimationOnCompositor(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(AnimationCompositorAnimationsTest, createMultipleKeyframeOpacityAnimationChained)
{
    // Animation to convert
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(-1.0).get(), 0.25),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(20.0).get(), 0.5),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    RefPtr<ChainedTimingFunction> chainedTimingFunction = ChainedTimingFunction::create();
    chainedTimingFunction->appendSegment(0.25, m_cubicEaseTimingFunction.get());
    chainedTimingFunction->appendSegment(0.5, m_linearTimingFunction.get());
    chainedTimingFunction->appendSegment(1.0, m_cubicCustomTimingFunction.get());

    m_timing.timingFunction = chainedTimingFunction;
    m_timing.iterationDuration = 2.0;
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
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.5, -1.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(1.0, 20.0), 1.0, 2.0, 3.0, 4.0));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(2.0, 5.0)));

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
    getAnimationOnCompositor(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(AnimationCompositorAnimationsTest, createReversedOpacityAnimation)
{
    // Animation to convert
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(
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
    getAnimationOnCompositor(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}

TEST_F(AnimationCompositorAnimationsTest, createReversedOpacityAnimationNegativeStartDelay)
{
    // Animation to convert
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.iterationCount = 5.0;
    m_timing.iterationDuration = 1.5;
    m_timing.startDelay = -3;
    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    // --

    WebCompositorSupportMock mockCompositor;

    // Curve is created
    blink::WebFloatAnimationCurveMock* mockCurvePtr = new blink::WebFloatAnimationCurveMock;
    ExpectationSet usesMockCurve;
    EXPECT_CALL(mockCompositor, createFloatAnimationCurve())
        .WillOnce(Return(mockCurvePtr));

    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(0.0, 5.0), blink::WebAnimationCurve::TimingFunctionTypeLinear));
    usesMockCurve += EXPECT_CALL(*mockCurvePtr, add(blink::WebFloatKeyframe(1.5, 2.0)));

    // Create animation
    blink::WebAnimationMock* mockAnimationPtr = new blink::WebAnimationMock(blink::WebAnimation::TargetPropertyOpacity);
    ExpectationSet usesMockAnimation;

    usesMockCurve += EXPECT_CALL(mockCompositor, createAnimation(Ref(*mockCurvePtr), blink::WebAnimation::TargetPropertyOpacity, _))
        .WillOnce(Return(mockAnimationPtr));

    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setIterations(5));
    usesMockAnimation += EXPECT_CALL(*mockAnimationPtr, setTimeOffset(3.0));
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
    getAnimationOnCompositor(m_timing, *effect.get(), result);
    EXPECT_EQ(1U, result.size());
    result[0].clear();
}


} // namespace WebCore
