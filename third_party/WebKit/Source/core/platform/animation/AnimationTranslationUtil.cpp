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

#include "core/platform/animation/AnimationTranslationUtil.h"

#include "core/platform/animation/CSSAnimationData.h"
#include "core/platform/animation/KeyframeValueList.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/transforms/InterpolatedTransformOperation.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/MatrixTransformOperation.h"
#include "platform/transforms/PerspectiveTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/SkewTransformOperation.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/transforms/TranslateTransformOperation.h"

#include "public/platform/Platform.h"
#include "public/platform/WebAnimation.h"
#include "public/platform/WebAnimationCurve.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFilterAnimationCurve.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebTransformAnimationCurve.h"

#include "wtf/OwnPtr.h"
#include "wtf/text/CString.h"

using namespace std;
using namespace blink;

namespace WebCore {

void toWebTransformOperations(const TransformOperations& transformOperations, const FloatSize& boxSize, WebTransformOperations* webTransformOperations)
{
    // We need to do a deep copy the transformOperations may contain ref pointers to TransformOperation objects.
    for (size_t j = 0; j < transformOperations.size(); ++j) {
        switch (transformOperations.operations()[j]->type()) {
        case TransformOperation::ScaleX:
        case TransformOperation::ScaleY:
        case TransformOperation::ScaleZ:
        case TransformOperation::Scale3D:
        case TransformOperation::Scale: {
            ScaleTransformOperation* transform = static_cast<ScaleTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendScale(transform->x(), transform->y(), transform->z());
            break;
        }
        case TransformOperation::TranslateX:
        case TransformOperation::TranslateY:
        case TransformOperation::TranslateZ:
        case TransformOperation::Translate3D:
        case TransformOperation::Translate: {
            TranslateTransformOperation* transform = static_cast<TranslateTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendTranslate(floatValueForLength(transform->x(), boxSize.width()), floatValueForLength(transform->y(), boxSize.height()), floatValueForLength(transform->z(), 1));
            break;
        }
        case TransformOperation::RotateX:
        case TransformOperation::RotateY:
        case TransformOperation::Rotate3D:
        case TransformOperation::Rotate: {
            RotateTransformOperation* transform = static_cast<RotateTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendRotate(transform->x(), transform->y(), transform->z(), transform->angle());
            break;
        }
        case TransformOperation::SkewX:
        case TransformOperation::SkewY:
        case TransformOperation::Skew: {
            SkewTransformOperation* transform = static_cast<SkewTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendSkew(transform->angleX(), transform->angleY());
            break;
        }
        case TransformOperation::Matrix: {
            MatrixTransformOperation* transform = static_cast<MatrixTransformOperation*>(transformOperations.operations()[j].get());
            TransformationMatrix m = transform->matrix();
            webTransformOperations->appendMatrix(TransformationMatrix::toSkMatrix44(m));
            break;
        }
        case TransformOperation::Matrix3D: {
            Matrix3DTransformOperation* transform = static_cast<Matrix3DTransformOperation*>(transformOperations.operations()[j].get());
            TransformationMatrix m = transform->matrix();
            webTransformOperations->appendMatrix(TransformationMatrix::toSkMatrix44(m));
            break;
        }
        case TransformOperation::Perspective: {
            PerspectiveTransformOperation* transform = static_cast<PerspectiveTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendPerspective(floatValueForLength(transform->perspective(), 0));
            break;
        }
        case TransformOperation::Interpolated: {
            TransformationMatrix m;
            transformOperations.operations()[j]->apply(m, boxSize);
            webTransformOperations->appendMatrix(TransformationMatrix::toSkMatrix44(m));
            break;
        }
        case TransformOperation::Identity:
            webTransformOperations->appendIdentity();
            break;
        case TransformOperation::None:
            // Do nothing.
            break;
        } // switch
    } // for each operation
}

template <class Value, class Keyframe, class Curve>
bool appendKeyframeWithStandardTimingFunction(Curve* curve, double keyTime, const Value* value, const Value* lastValue, blink::WebAnimationCurve::TimingFunctionType timingFunctionType, const FloatSize&)
{
    curve->add(Keyframe(keyTime, value->value()), timingFunctionType);
    return true;
}

template <class Value, class Keyframe, class Curve>
bool appendKeyframeWithCustomBezierTimingFunction(Curve* curve, double keyTime, const Value* value, const Value* lastValue, double x1, double y1, double x2, double y2, const FloatSize&)
{
    curve->add(Keyframe(keyTime, value->value()), x1, y1, x2, y2);
    return true;
}

template <>
bool appendKeyframeWithStandardTimingFunction<TransformAnimationValue, WebTransformKeyframe, WebTransformAnimationCurve>(WebTransformAnimationCurve* curve, double keyTime, const TransformAnimationValue* value, const TransformAnimationValue* lastValue, blink::WebAnimationCurve::TimingFunctionType timingFunctionType, const FloatSize& boxSize)
{
    bool canBlend = !lastValue;
    OwnPtr<WebTransformOperations> operations = adoptPtr(Platform::current()->compositorSupport()->createTransformOperations());
    if (!operations)
        return false;
    toWebTransformOperations(*value->value(), boxSize, operations.get());
    if (!canBlend) {
        OwnPtr<WebTransformOperations> lastOperations = adoptPtr(Platform::current()->compositorSupport()->createTransformOperations());
        if (!lastOperations)
            return false;
        toWebTransformOperations(*lastValue->value(), boxSize, lastOperations.get());
        canBlend = lastOperations->canBlendWith(*operations);
    }
    if (canBlend) {
        curve->add(WebTransformKeyframe(keyTime, operations.release()), timingFunctionType);
        return true;
    }
    return false;
}

template <>
bool appendKeyframeWithCustomBezierTimingFunction<TransformAnimationValue, WebTransformKeyframe, WebTransformAnimationCurve>(WebTransformAnimationCurve* curve, double keyTime, const TransformAnimationValue* value, const TransformAnimationValue* lastValue, double x1, double y1, double x2, double y2, const FloatSize& boxSize)
{
    bool canBlend = !lastValue;
    OwnPtr<WebTransformOperations> operations = adoptPtr(Platform::current()->compositorSupport()->createTransformOperations());
    if (!operations)
        return false;
    toWebTransformOperations(*value->value(), boxSize, operations.get());
    if (!canBlend) {
        OwnPtr<WebTransformOperations> lastOperations = adoptPtr(Platform::current()->compositorSupport()->createTransformOperations());
        if (!lastOperations)
            return false;
        toWebTransformOperations(*lastValue->value(), boxSize, lastOperations.get());
        canBlend = lastOperations->canBlendWith(*operations);
    }
    if (canBlend) {
        curve->add(WebTransformKeyframe(keyTime, operations.release()), x1, y1, x2, y2);
        return true;
    }
    return false;
}

bool toWebFilterOperations(const FilterOperations& inOperations, WebFilterOperations* outOperations)
{
    SkiaImageFilterBuilder builder;
    FilterOutsets outsets = inOperations.outsets();
    builder.setCropOffset(FloatSize(outsets.left(), outsets.top()));
    return builder.buildFilterOperations(inOperations, outOperations);
}

template <>
bool appendKeyframeWithStandardTimingFunction<FilterAnimationValue, WebFilterKeyframe, WebFilterAnimationCurve>(WebFilterAnimationCurve* curve, double keyTime, const FilterAnimationValue* value, const FilterAnimationValue* lastValue, blink::WebAnimationCurve::TimingFunctionType timingFunctionType, const FloatSize& boxSize)
{
    // FIXME(ajuma): In order to animate pixel-moving filters on the compositor thread, we need
    // to update overlap testing to take into account the bounds within which the animation
    // will be contained, and we need the compositor to update layer bounds as the animation
    // progresses.
    if (value->value()->hasFilterThatMovesPixels())
        return false;
    OwnPtr<WebFilterOperations> operations = adoptPtr(Platform::current()->compositorSupport()->createFilterOperations());
    if (!toWebFilterOperations(*(value->value()), operations.get()))
        return false;
    curve->add(WebFilterKeyframe(keyTime, operations.release()), timingFunctionType);
    return true;
}

template <>
bool appendKeyframeWithCustomBezierTimingFunction<FilterAnimationValue, WebFilterKeyframe, WebFilterAnimationCurve>(WebFilterAnimationCurve* curve, double keyTime, const FilterAnimationValue* value, const FilterAnimationValue* lastValue, double x1, double y1, double x2, double y2, const FloatSize& boxSize)
{
    // FIXME(ajuma): In order to animate pixel-moving filters on the compositor thread, we need
    // to update overlap testing to take into account the bounds within which the animation
    // will be contained, and we need the compositor to update layer bounds as the animation
    // progresses.
    if (value->value()->hasFilterThatMovesPixels())
        return false;

    OwnPtr<WebFilterOperations> operations = adoptPtr(Platform::current()->compositorSupport()->createFilterOperations());
    if (!toWebFilterOperations(*(value->value()), operations.get()))
        return false;
    curve->add(WebFilterKeyframe(keyTime, operations.release()), x1, y1, x2, y2);
    return true;
}

template <class Value, class Keyframe, class Curve>
PassOwnPtr<blink::WebAnimation> createWebAnimation(const KeyframeValueList& valueList, const CSSAnimationData* animation, int animationId, double timeOffset, Curve* curve, blink::WebAnimation::TargetProperty targetProperty, const FloatSize& boxSize)
{
    bool alternate = false;
    bool reverse = false;
    if (animation && animation->isDirectionSet()) {
        CSSAnimationData::AnimationDirection direction = animation->direction();
        if (direction == CSSAnimationData::AnimationDirectionAlternate || direction == CSSAnimationData::AnimationDirectionAlternateReverse)
            alternate = true;
        if (direction == CSSAnimationData::AnimationDirectionReverse || direction == CSSAnimationData::AnimationDirectionAlternateReverse)
            reverse = true;
    }

    for (size_t i = 0; i < valueList.size(); i++) {
        size_t index = reverse ? valueList.size() - i - 1 : i;
        const Value* originalValue = static_cast<const Value*>(valueList.at(index));
        const Value* lastOriginalValue = 0;
        if (valueList.size() > 1 && ((reverse && index + 1 < valueList.size()) || (!reverse && index > 0)))
            lastOriginalValue = static_cast<const Value*>(valueList.at(reverse ? index + 1 : index - 1));

        const TimingFunction* originalTimingFunction = originalValue->timingFunction();

        // If there hasn't been a timing function associated with this keyframe, use the
        // animation's timing function, if we have one.
        if (!originalTimingFunction && animation->isTimingFunctionSet())
            originalTimingFunction = animation->timingFunction();

        // Ease is the default timing function.
        blink::WebAnimationCurve::TimingFunctionType timingFunctionType = blink::WebAnimationCurve::TimingFunctionTypeEase;

        bool isUsingCustomBezierTimingFunction = false;
        double x1 = 0;
        double y1 = 0;
        double x2 = 1;
        double y2 = 1;

        if (originalTimingFunction) {
            switch (originalTimingFunction->type()) {
            case TimingFunction::StepsFunction:
                // FIXME: add support for steps timing function.
                return nullptr;
            case TimingFunction::LinearFunction:
                // This doesn't need to be flipped when the animation is reversed.
                timingFunctionType = blink::WebAnimationCurve::TimingFunctionTypeLinear;
                break;
            case TimingFunction::CubicBezierFunction:
                {
                    const CubicBezierTimingFunction* originalBezierTimingFunction = toCubicBezierTimingFunction(originalTimingFunction);
                    isUsingCustomBezierTimingFunction = true;
                    x1 = originalBezierTimingFunction->x1();
                    y1 = originalBezierTimingFunction->y1();
                    x2 = originalBezierTimingFunction->x2();
                    y2 = originalBezierTimingFunction->y2();
                    if (reverse) {
                        // When the animation is reversed, we need to swap the
                        // start and end keyframes, and flip the timing
                        // function in both x and y.
                        double x1Old = x1;
                        double y1Old = y1;
                        x1 = 1 - x2;
                        y1 = 1 - y2;
                        x2 = 1 - x1Old;
                        y2 = 1 - y1Old;
                    }
                    break;
                }
            default:
                ASSERT_NOT_REACHED();
            } // switch
        }

        double duration = (animation && animation->isDurationSet()) ? animation->duration() : 1;
        double keyTime = originalValue->keyTime() * duration;

        if (reverse)
            keyTime = duration - keyTime;

        bool addedKeyframe = false;
        if (isUsingCustomBezierTimingFunction)
            addedKeyframe = appendKeyframeWithCustomBezierTimingFunction<Value, Keyframe, Curve>(curve, keyTime, originalValue, lastOriginalValue, x1, y1, x2, y2, boxSize);
        else
            addedKeyframe = appendKeyframeWithStandardTimingFunction<Value, Keyframe, Curve>(curve, keyTime, originalValue, lastOriginalValue, timingFunctionType, boxSize);

        if (!addedKeyframe)
            return nullptr;
    }

    OwnPtr<blink::WebAnimation> webAnimation = adoptPtr(Platform::current()->compositorSupport()->createAnimation(*curve, targetProperty, animationId));

    int iterations = (animation && animation->isIterationCountSet()) ? animation->iterationCount() : 1;
    webAnimation->setIterations(iterations);
    webAnimation->setAlternatesDirection(alternate);

    // If timeOffset > 0, then the animation has started in the past.
    webAnimation->setTimeOffset(timeOffset);

    return webAnimation.release();
}

PassOwnPtr<blink::WebAnimation> createWebAnimation(const KeyframeValueList& values, const CSSAnimationData* animation, int animationId, double timeOffset, const FloatSize& boxSize)
{
    switch (values.property()) {
    case AnimatedPropertyWebkitTransform: {
        OwnPtr<WebTransformAnimationCurve> curve = adoptPtr(Platform::current()->compositorSupport()->createTransformAnimationCurve());
        return createWebAnimation<TransformAnimationValue, WebTransformKeyframe, WebTransformAnimationCurve>(values, animation, animationId, timeOffset, curve.get(), blink::WebAnimation::TargetPropertyTransform, FloatSize(boxSize));
    }

    case AnimatedPropertyOpacity: {
        OwnPtr<WebFloatAnimationCurve> curve = adoptPtr(Platform::current()->compositorSupport()->createFloatAnimationCurve());
        return createWebAnimation<FloatAnimationValue, WebFloatKeyframe, WebFloatAnimationCurve>(values, animation, animationId, timeOffset, curve.get(), blink::WebAnimation::TargetPropertyOpacity, FloatSize());
    }

    case AnimatedPropertyWebkitFilter: {
        OwnPtr<WebFilterAnimationCurve> curve = adoptPtr(Platform::current()->compositorSupport()->createFilterAnimationCurve());
        return createWebAnimation<FilterAnimationValue, WebFilterKeyframe, WebFilterAnimationCurve>(values, animation, animationId, timeOffset, curve.get(), blink::WebAnimation::TargetPropertyFilter, FloatSize(boxSize));
    }

    case AnimatedPropertyBackgroundColor:
    case AnimatedPropertyInvalid:
        return nullptr;
    }

    return nullptr;
}

} // namespace WebCore
