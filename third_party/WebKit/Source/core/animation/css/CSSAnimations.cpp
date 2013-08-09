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

#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeAnimationEffect.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/AnimationEvent.h"
#include "core/dom/Element.h"
#include "core/dom/EventNames.h"
#include "core/platform/animation/CSSAnimationDataList.h"
#include "core/platform/animation/TimingFunction.h"
#include "wtf/HashSet.h"

namespace WebCore {

void timingFromAnimationData(const CSSAnimationData* animationData, Timing& timing)
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
    if (animationData->isTimingFunctionSet()) {
        if (!animationData->timingFunction()->isLinearTimingFunction())
            timing.timingFunction = animationData->timingFunction();
    } else {
        // CSS default is ease, default in model is linear.
        timing.timingFunction = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease);
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
}

bool CSSAnimations::needsUpdate(const Element* element, const RenderStyle* style)
{
    ActiveAnimations* activeAnimations = element->activeAnimations();
    const CSSAnimationDataList* animations = style->animations();
    const CSSAnimations* cssAnimations = activeAnimations ? activeAnimations->cssAnimations() : 0;
    EDisplay display = style->display();
    return (display != NONE && animations && animations->size()) || (cssAnimations && !cssAnimations->isEmpty());
}

PassOwnPtr<CSSAnimationUpdate> CSSAnimations::calculateUpdate(const Element* element, EDisplay display, const CSSAnimations* cssAnimations, const CSSAnimationDataList* animationDataList, StyleResolver* resolver)
{
    OwnPtr<CSSAnimationUpdate> update;
    HashSet<StringImpl*> inactive;
    if (cssAnimations)
        for (AnimationMap::const_iterator iter = cssAnimations->m_animations.begin(); iter != cssAnimations->m_animations.end(); ++iter)
            inactive.add(iter->key);

    RefPtr<MutableStylePropertySet> newStyles;
    if (display != NONE) {
        for (size_t i = 0; animationDataList && i < animationDataList->size(); ++i) {
            const CSSAnimationData* animationData = animationDataList->animation(i);
            if (animationData->isNoneAnimation())
                continue;
            ASSERT(animationData->isValidAnimation());
            AtomicString animationName(animationData->name());

            if (cssAnimations) {
                AnimationMap::const_iterator existing(cssAnimations->m_animations.find(animationName.impl()));
                if (existing != cssAnimations->m_animations.end()) {
                    inactive.remove(animationName.impl());
                    continue;
                }
            }

            // If there's a delay, no styles will apply yet.
            if (animationData->isDelaySet() && animationData->delay()) {
                RELEASE_ASSERT_WITH_MESSAGE(animationData->delay() > 0, "Web Animations not yet implemented: Negative delay");
                continue;
            }

            const StylePropertySet* keyframeStyles = resolver->firstKeyframeStyles(element, animationName.impl());
            if (keyframeStyles) {
                if (!update)
                    update = adoptPtr(new CSSAnimationUpdate());
                update->addStyles(keyframeStyles);
            }
        }
    }

    if (!inactive.isEmpty() && !update)
        update = adoptPtr(new CSSAnimationUpdate());
    for (HashSet<StringImpl*>::const_iterator iter = inactive.begin(); iter != inactive.end(); ++iter)
        update->cancel(cssAnimations->m_animations.get(*iter));

    return update.release();
}

void CSSAnimations::update(Element* element, const RenderStyle* style)
{
    const CSSAnimationDataList* animationDataList = style->animations();
    HashSet<StringImpl*> inactive;
    for (AnimationMap::const_iterator iter = m_animations.begin(); iter != m_animations.end(); ++iter)
        inactive.add(iter->key);

    if (style->display() != NONE) {
        for (size_t i = 0; animationDataList && i < animationDataList->size(); ++i) {
            const CSSAnimationData* animationData = animationDataList->animation(i);
            if (animationData->isNoneAnimation())
                continue;
            ASSERT(animationData->isValidAnimation());
            AtomicString animationName(animationData->name());

            AnimationMap::const_iterator existing(m_animations.find(animationName.impl()));
            if (existing != m_animations.end()) {
                bool paused = animationData->playState() == AnimPlayStatePaused;
                existing->value->setPaused(paused);
                inactive.remove(animationName.impl());
                continue;
            }

            KeyframeAnimationEffect::KeyframeVector keyframes;
            element->document()->styleResolver()->resolveKeyframes(element, style, animationName.impl(), keyframes);
            if (!keyframes.isEmpty()) {
                Timing timing;
                timingFromAnimationData(animationData, timing);
                OwnPtr<CSSAnimations::EventDelegate> eventDelegate = adoptPtr(new EventDelegate(element, animationName));
                // FIXME: crbug.com/268791 - Keyframes are already normalized, perhaps there should be a flag on KeyframeAnimationEffect to skip normalization.
                m_animations.set(animationName.impl(), element->document()->timeline()->play(
                    Animation::create(element, KeyframeAnimationEffect::create(keyframes), timing, eventDelegate.release()).get()).get());
            }
        }
    }

    for (HashSet<StringImpl*>::const_iterator iter = inactive.begin(); iter != inactive.end(); ++iter)
        m_animations.take(*iter)->cancel();
}

void CSSAnimations::cancel()
{
    for (AnimationMap::iterator iter = m_animations.begin(); iter != m_animations.end(); ++iter)
        iter->value->cancel();

    m_animations.clear();
}

void CSSAnimations::EventDelegate::maybeDispatch(Document::ListenerType listenerType, AtomicString& eventName, double elapsedTime)
{
    if (m_target->document()->hasListenerType(listenerType))
        m_target->document()->timeline()->addEventToDispatch(m_target, AnimationEvent::create(eventName, m_name, elapsedTime));
}

void CSSAnimations::EventDelegate::onEventCondition(bool wasInPlay, bool isInPlay, double previousIteration, double currentIteration)
{
    // Events for a single document are queued and dispatched as a group at
    // the end of DocumentTimeline::serviceAnimations.
    // FIXME: Events which are queued outside of serviceAnimations should
    // trigger a timer to dispatch when control is released.
    // FIXME: Receive TimedItem as param in order to produce correct elapsed time value.
    double elapsedTime = 0;
    if (!wasInPlay && isInPlay) {
        maybeDispatch(Document::ANIMATIONSTART_LISTENER, eventNames().webkitAnimationStartEvent, elapsedTime);
        return;
    }
    if (wasInPlay && isInPlay && currentIteration != previousIteration) {
        maybeDispatch(Document::ANIMATIONITERATION_LISTENER, eventNames().webkitAnimationIterationEvent, elapsedTime);
        return;
    }
    if (wasInPlay && !isInPlay) {
        maybeDispatch(Document::ANIMATIONEND_LISTENER, eventNames().webkitAnimationEndEvent, elapsedTime);
        return;
    }
}

} // namespace WebCore
