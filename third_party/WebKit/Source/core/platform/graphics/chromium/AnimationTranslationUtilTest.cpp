/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"

#include "core/platform/graphics/chromium/AnimationTranslationUtil.h"

#include "core/platform/animation/CSSAnimationData.h"
#include "core/platform/animation/KeyframeValueList.h"
#include "core/platform/graphics/IntSize.h"
#include "core/platform/graphics/transforms/Matrix3DTransformOperation.h"
#include "core/platform/graphics/transforms/RotateTransformOperation.h"
#include "core/platform/graphics/transforms/ScaleTransformOperation.h"
#include "core/platform/graphics/transforms/TransformOperations.h"
#include "core/platform/graphics/transforms/TranslateTransformOperation.h"
#include "wtf/RefPtr.h"
#include <gtest/gtest.h>
#include "public/platform/WebAnimation.h"

using namespace WebCore;
using namespace WebKit;

namespace {

bool animationCanBeTranslated(const KeyframeValueList& values, CSSAnimationData* animation)
{
    IntSize boxSize;
    return createWebAnimation(values, animation, 0, 0, boxSize);
}

TEST(AnimationTranslationUtilTest, createOpacityAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyOpacity);
    values.insert(adoptPtr(new FloatAnimationValue(0, 0)));
    values.insert(adoptPtr(new FloatAnimationValue(duration, 1)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithBigRotation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(0, TransformOperation::Rotate));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(270, TransformOperation::Rotate));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithBigRotationAndEmptyTransformOperationList)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(270, TransformOperation::Rotate));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithRotationInvolvingNegativeAngles)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(-330, TransformOperation::Rotate));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(-320, TransformOperation::Rotate));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithSmallRotationInvolvingLargeAngles)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(270, TransformOperation::Rotate));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(360, TransformOperation::Rotate));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithNonDecomposableMatrix)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformationMatrix matrix1;
    TransformOperations operations1;
    operations1.operations().append(Matrix3DTransformOperation::create(matrix1));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformationMatrix matrix2;
    matrix2.setM11(0);
    TransformOperations operations2;
    operations2.operations().append(Matrix3DTransformOperation::create(matrix2));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);

    EXPECT_FALSE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithNonInvertibleTransform)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(ScaleTransformOperation::create(1, 1, 1, TransformOperation::Scale3D));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(ScaleTransformOperation::create(1, 0, 1, TransformOperation::Scale3D));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createReversedAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);
    animation->setDirection(CSSAnimationData::AnimationDirectionReverse);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createAlternatingAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);
    animation->setDirection(CSSAnimationData::AnimationDirectionAlternate);
    animation->setIterationCount(2);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createReversedAlternatingAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    values.insert(adoptPtr(new TransformAnimationValue(0, &operations1)));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    values.insert(adoptPtr(new TransformAnimationValue(duration, &operations2)));

    RefPtr<CSSAnimationData> animation = CSSAnimationData::create();
    animation->setDuration(duration);
    animation->setDirection(CSSAnimationData::AnimationDirectionAlternateReverse);
    animation->setIterationCount(2);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

}

