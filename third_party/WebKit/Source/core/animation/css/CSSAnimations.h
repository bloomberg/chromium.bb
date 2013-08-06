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

#ifndef CSSAnimations_h
#define CSSAnimations_h

#include "core/animation/Animation.h"

#include "core/animation/Player.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/platform/animation/CSSAnimationData.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class CSSAnimationDataList;
class Element;
class RenderObject;
class StyleResolver;

class CSSAnimationUpdate FINAL {
public:
    const StylePropertySet* styles() const { return m_styles.get(); }
    // Returns whether player has been cancelled and should be filtered during style application.
    bool isFiltered(const Player* player) const { return m_filtered.contains(player); }
    void cancel(const Player* player)
    {
        m_filtered.add(player);
    }
    void addStyles(const StylePropertySet* styles)
    {
        if (!m_styles)
            m_styles = MutableStylePropertySet::create();
        m_styles->mergeAndOverrideOnConflict(styles);
    }
private:
    HashSet<const Player*> m_filtered;
    RefPtr<MutableStylePropertySet> m_styles;
};

class CSSAnimations FINAL {
public:
    static bool needsUpdate(const Element*, const RenderStyle*);
    static PassOwnPtr<CSSAnimationUpdate> calculateUpdate(const Element*, EDisplay, const CSSAnimations*, const CSSAnimationDataList*, StyleResolver*);
    void update(Element*, const RenderStyle*);
    bool isEmpty() const { return m_animations.isEmpty(); }
    void cancel();
private:
    typedef HashMap<StringImpl*, Player*> AnimationMap;
    AnimationMap m_animations;
    class EventDelegate FINAL : public TimedItemEventDelegate {
    public:
        EventDelegate(Element* target, const AtomicString& name)
            : m_target(target)
            , m_name(name)
        {
        }
        virtual void onEventCondition(bool wasInPlay, bool isInPlay, double previousIteration, double currentIteration) OVERRIDE;
    private:
        void maybeDispatch(Document::ListenerType, AtomicString& eventName, double elapsedTime);
        Element* m_target;
        const AtomicString m_name;
    };
};

} // namespace WebCore

#endif
