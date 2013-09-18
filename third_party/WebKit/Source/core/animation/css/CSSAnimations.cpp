/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/animation/css/CSSAnimations.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeAnimationEffect.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/dom/EventNames.h"
#include "core/dom/WebKitAnimationEvent.h"
#include "core/platform/animation/CSSAnimationDataList.h"
#include "core/platform/animation/TimingFunction.h"
#include "wtf/HashSet.h"

namespace {

using namespace WebCore;

bool isEarlierPhase(TimedItem::Phase target, TimedItem::Phase reference)
{
    ASSERT(target != TimedItem::PhaseNone);
    ASSERT(reference != TimedItem::PhaseNone);
    return target < reference;
}

bool isLaterPhase(TimedItem::Phase target, TimedItem::Phase reference)
{
    ASSERT(target != TimedItem::PhaseNone);
    ASSERT(reference != TimedItem::PhaseNone);
    return target > reference;
}

} // namespace

namespace WebCore {

// Returns the default timing function.
const PassRefPtr<TimingFunction> timingFromAnimationData(const CSSAnimationData* animationData, Timing& timing)
{
    if (animationData->isDelaySet())
        timing.startDelay = animationData->delay();
    if (animationData->isDurationSet()) {
        timing.iterationDuration = animationData->duration();
        timing.hasIterationDuration = true;
    }
    if (animationData->isIterationCountSet()) {
        if (animationData->iterationCount() == CSSAnimationData::IterationCountInfinite)
            timing.iterationCount = std::numeric_limits<double>::infinity();
        else
            timing.iterationCount = animationData->iterationCount();
    }
    if (animationData->isFillModeSet()) {
        switch (animationData->fillMode()) {
        case AnimationFillModeForwards:
            timing.fillMode = Timing::FillModeForwards;
            break;
        case AnimationFillModeBackwards:
            timing.fillMode = Timing::FillModeBackwards;
            break;
        case AnimationFillModeBoth:
            timing.fillMode = Timing::FillModeBoth;
            break;
        case AnimationFillModeNone:
            timing.fillMode = Timing::FillModeNone;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
    if (animationData->isDirectionSet()) {
        switch (animationData->direction()) {
        case CSSAnimationData::AnimationDirectionNormal:
            timing.direction = Timing::PlaybackDirectionNormal;
            break;
        case CSSAnimationData::AnimationDirectionAlternate:
            timing.direction = Timing::PlaybackDirectionAlternate;
            break;
        case CSSAnimationData::AnimationDirectionReverse:
            timing.direction = Timing::PlaybackDirectionReverse;
            break;
        case CSSAnimationData::AnimationDirectionAlternateReverse:
            timing.direction = Timing::PlaybackDirectionAlternateReverse;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return animationData->isTimingFunctionSet() ? animationData->timingFunction() : CSSAnimationData::initialAnimationTimingFunction();
}

CSSAnimationUpdateScope::CSSAnimationUpdateScope(Element* target)
    : m_target(target)
{
    if (!m_target)
        return;
    ActiveAnimations* activeAnimations = m_target->activeAnimations();
    CSSAnimations* cssAnimations = activeAnimations ? activeAnimations->cssAnimations() : 0;
    // It's possible than an update was created outside an update scope. That's harmless
    // but we must clear it now to avoid applying it if an updated replacement is not
    // created in this scope.
    if (cssAnimations)
        cssAnimations->setPendingUpdate(nullptr);
}

CSSAnimationUpdateScope::~CSSAnimationUpdateScope()
{
    if (!m_target)
        return;
    ActiveAnimations* activeAnimations = m_target->activeAnimations();
    CSSAnimations* cssAnimations = activeAnimations ? activeAnimations->cssAnimations() : 0;
    if (cssAnimations)
        cssAnimations->maybeApplyPendingUpdate(m_target);
}

bool CSSAnimations::needsUpdate(const Element* element, const RenderStyle* style)
{
    ActiveAnimations* activeAnimations = element->activeAnimations();
    const CSSAnimationDataList* animations = style->animations();
    const CSSAnimations* cssAnimations = activeAnimations ? activeAnimations->cssAnimations() : 0;
    EDisplay display = style->display();
    return (display != NONE && animations && animations->size()) || (cssAnimations && !cssAnimations->isEmpty());
}

PassOwnPtr<CSSAnimationUpdate> CSSAnimations::calculateUpdate(const Element* element, const RenderStyle* style, const CSSAnimations* cssAnimations, const CSSAnimationDataList* animationDataList, StyleResolver* resolver)
{
    OwnPtr<CSSAnimationUpdate> update;
    HashSet<AtomicString> inactive;
    if (cssAnimations)
        for (AnimationMap::const_iterator iter = cssAnimations->m_animations.begin(); iter != cssAnimations->m_animations.end(); ++iter)
            inactive.add(iter->key);

    RefPtr<MutableStylePropertySet> newStyles;
    if (style->display() != NONE) {
        for (size_t i = 0; animationDataList && i < animationDataList->size(); ++i) {
            const CSSAnimationData* animationData = animationDataList->animation(i);
            if (animationData->isNoneAnimation())
                continue;
            ASSERT(animationData->isValidAnimation());
            AtomicString animationName(animationData->name());

            if (cssAnimations) {
                AnimationMap::const_iterator existing(cssAnimations->m_animations.find(animationName));
                if (existing != cssAnimations->m_animations.end()) {
                    // FIXME: The play-state of this animation might have changed, record the change in the update.
                    inactive.remove(animationName);
                    continue;
                }
            }

            Timing timing;
            RefPtr<TimingFunction> defaultTimingFunction = timingFromAnimationData(animationData, timing);
            KeyframeAnimationEffect::KeyframeVector keyframes;
            resolver->resolveKeyframes(element, style, animationName, defaultTimingFunction.get(), keyframes, timing.timingFunction);
            if (!keyframes.isEmpty()) {
                if (!update)
                    update = adoptPtr(new CSSAnimationUpdate());
                // FIXME: crbug.com/268791 - Keyframes are already normalized, perhaps there should be a flag on KeyframeAnimationEffect to skip normalization.
                update->startAnimation(animationName, InertAnimation::create(KeyframeAnimationEffect::create(keyframes), timing).get());
            }
        }
    }

    if (!inactive.isEmpty() && !update)
        update = adoptPtr(new CSSAnimationUpdate());
    for (HashSet<AtomicString>::const_iterator iter = inactive.begin(); iter != inactive.end(); ++iter)
        update->cancelAnimation(*iter, cssAnimations->m_animations.get(*iter));

    return update.release();
}

void CSSAnimations::maybeApplyPendingUpdate(Element* element)
{
    if (!element->renderer())
        m_pendingUpdate = nullptr;

    if (!m_pendingUpdate)
        return;

    OwnPtr<CSSAnimationUpdate> update = m_pendingUpdate.release();

    for (Vector<AtomicString>::const_iterator iter = update->cancelledAnimationNames().begin(); iter != update->cancelledAnimationNames().end(); ++iter)
        m_animations.take(*iter)->cancel();

    // FIXME: Apply updates to play-state.

    for (Vector<CSSAnimationUpdate::NewAnimation>::const_iterator iter = update->newAnimations().begin(); iter != update->newAnimations().end(); ++iter) {
        OwnPtr<CSSAnimations::EventDelegate> eventDelegate = adoptPtr(new EventDelegate(element, iter->name));
        RefPtr<Animation> animation = Animation::create(element, iter->animation->effect(), iter->animation->specified(), eventDelegate.release());
        RefPtr<Player> player = element->document().timeline()->play(animation.get());
        m_animations.set(iter->name, player.get());
    }
}

void CSSAnimations::cancel()
{
    for (AnimationMap::iterator iter = m_animations.begin(); iter != m_animations.end(); ++iter)
        iter->value->cancel();

    m_animations.clear();
    m_pendingUpdate = nullptr;
}

void CSSAnimations::EventDelegate::maybeDispatch(Document::ListenerType listenerType, AtomicString& eventName, double elapsedTime)
{
    if (m_target->document().hasListenerType(listenerType))
        m_target->document().timeline()->addEventToDispatch(m_target, WebKitAnimationEvent::create(eventName, m_name, elapsedTime));
}

void CSSAnimations::EventDelegate::onEventCondition(const TimedItem* timedItem, bool isFirstSample, TimedItem::Phase previousPhase, double previousIteration)
{
    // Events for a single document are queued and dispatched as a group at
    // the end of DocumentTimeline::serviceAnimations.
    // FIXME: Events which are queued outside of serviceAnimations should
    // trigger a timer to dispatch when control is released.
    const TimedItem::Phase currentPhase = timedItem->phase();
    const double currentIteration = timedItem->currentIteration();

    // Note that the elapsedTime is measured from when the animation starts playing.
    if (!isFirstSample && previousPhase == TimedItem::PhaseActive && currentPhase == TimedItem::PhaseActive && previousIteration != currentIteration) {
        ASSERT(!isNull(previousIteration));
        ASSERT(!isNull(currentIteration));
        // We fire only a single event for all iterations thast terminate
        // between a single pair of samples. See http://crbug.com/275263. For
        // compatibility with the existing implementation, this event uses
        // the elapsedTime for the first iteration in question.
        ASSERT(timedItem->specified().hasIterationDuration);
        const double elapsedTime = timedItem->specified().iterationDuration * (previousIteration + 1);
        maybeDispatch(Document::ANIMATIONITERATION_LISTENER, eventNames().animationiterationEvent, elapsedTime);
        return;
    }
    if ((isFirstSample || previousPhase == TimedItem::PhaseBefore) && isLaterPhase(currentPhase, TimedItem::PhaseBefore)) {
        ASSERT(timedItem->specified().startDelay > 0 || isFirstSample);
        // The spec states that the elapsed time should be
        // 'delay < 0 ? -delay : 0', but we always use 0 to match the existing
        // implementation. See crbug.com/279611
        maybeDispatch(Document::ANIMATIONSTART_LISTENER, eventNames().animationstartEvent, 0);
    }
    if ((isFirstSample || isEarlierPhase(previousPhase, TimedItem::PhaseAfter)) && currentPhase == TimedItem::PhaseAfter)
        maybeDispatch(Document::ANIMATIONEND_LISTENER, eventNames().animationendEvent, timedItem->activeDuration());
}

bool CSSAnimations::isAnimatableProperty(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBaselineShift:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBottom:
    case CSSPropertyBoxShadow:
    case CSSPropertyClip:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyFillOpacity:
    case CSSPropertyFlex:
    case CSSPropertyFloodColor:
    case CSSPropertyFloodOpacity:
    case CSSPropertyFontSize:
    case CSSPropertyHeight:
    case CSSPropertyKerning:
    case CSSPropertyLeft:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLightingColor:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyleImage:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyOpacity:
    case CSSPropertyOrphans:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyRight:
    case CSSPropertyStopColor:
    case CSSPropertyStopOpacity:
    case CSSPropertyStroke:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyStrokeMiterlimit:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyStrokeWidth:
    case CSSPropertyTextIndent:
    case CSSPropertyTextShadow:
    case CSSPropertyTop:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitBoxShadow:
    case CSSPropertyWebkitClipPath:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyWebkitFilter:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitShapeInside:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTransform:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWidows:
    case CSSPropertyWidth:
    case CSSPropertyWordSpacing:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
        return true;
    default:
        return false;
    }
}

} // namespace WebCore
