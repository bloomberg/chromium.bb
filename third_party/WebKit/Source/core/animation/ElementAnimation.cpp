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
#include "core/animation/ElementAnimation.h"

#include "core/animation/DocumentTimeline.h"
#include "core/css/RuntimeCSSEnabled.h"
#include "core/css/resolver/StyleResolver.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>

namespace WebCore {

CSSPropertyID ElementAnimation::camelCaseCSSPropertyNameToID(StringImpl* propertyName)
{
    typedef HashMap<StringImpl*, CSSPropertyID> CSSPropertyIDMap;
    DEFINE_STATIC_LOCAL(CSSPropertyIDMap, map, ());

    if (propertyName->find('-') != kNotFound)
        return CSSPropertyInvalid;

    CSSPropertyID id = map.get(propertyName);

    if (!id) {
        StringBuilder builder;
        size_t position = 0;
        size_t end;
        while ((end = propertyName->find(isASCIIUpper, position)) != kNotFound) {
            builder.append(propertyName->substring(position, end - position) + "-" + toASCIILower((*propertyName)[end]));
            position = end + 1;
        }
        builder.append(propertyName->substring(position));
        // Doesn't handle prefixed properties.
        id = cssPropertyID(builder.toString());
        if (id != CSSPropertyInvalid && RuntimeCSSEnabled::isCSSPropertyEnabled(id))
            map.add(propertyName, id);
    }
    return id;
}

void ElementAnimation::animate(Element* element, Vector<Dictionary> keyframeDictionaryVector, double duration)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());

    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!element->inActiveDocument())
        return;
    element->document().updateStyleIfNeeded();
    if (!element->renderer())
        return;

    startAnimation(element, keyframeDictionaryVector, duration);
}

void ElementAnimation::startAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, double duration)
{
    KeyframeAnimationEffect::KeyframeVector keyframes;
    Vector<RefPtr<MutableStylePropertySet> > propertySetVector;

    for (size_t i = 0; i < keyframeDictionaryVector.size(); ++i) {
        RefPtr<MutableStylePropertySet> propertySet = MutableStylePropertySet::create();
        propertySetVector.append(propertySet);

        RefPtr<Keyframe> keyframe = Keyframe::create();
        keyframes.append(keyframe);

        double offset;
        if (keyframeDictionaryVector[i].get("offset", offset)) {
            keyframe->setOffset(offset);
        } else {
            // FIXME: Web Animations CSS engine does not yet implement handling of
            // keyframes without specified offsets. This check can be removed when
            // that funcitonality is implemented.
            ASSERT_NOT_REACHED();
            return;
        }

        String compositeString;
        keyframeDictionaryVector[i].get("composite", compositeString);
        if (compositeString == "add")
            keyframe->setComposite(AnimationEffect::CompositeAdd);

        Vector<String> keyframeProperties;
        keyframeDictionaryVector[i].getOwnPropertyNames(keyframeProperties);

        for (size_t j = 0; j < keyframeProperties.size(); ++j) {
            String property = keyframeProperties[j];
            CSSPropertyID id = camelCaseCSSPropertyNameToID(property.impl());

            // FIXME: There is no way to store invalid properties or invalid values
            // in a Keyframe object, so for now I just skip over them. Eventually we
            // will need to support getFrames(), which should return exactly the
            // keyframes that were input through the API. We will add a layer to wrap
            // KeyframeAnimationEffect, store input keyframes and implement getFrames.
            if (id == CSSPropertyInvalid || !CSSAnimations::isAnimatableProperty(id))
                continue;

            String value;
            keyframeDictionaryVector[i].get(property, value);
            propertySet->setProperty(id, value);
        }
    }

    // FIXME: Replace this with code that just parses, when that code is available.
    RefPtr<KeyframeAnimationEffect> effect = StyleResolver::createKeyframeAnimationEffect(*element, propertySetVector, keyframes);

    // FIXME: Totally hardcoded Timing for now. Will handle timing parameters later.
    Timing timing;
    // FIXME: Currently there is no way to tell whether or not an iterationDuration
    // has been specified (becauser the default argument is 0). So any animation
    // created using Element.animate() will have a timing with hasIterationDuration()
    // == true.
    timing.hasIterationDuration = true;
    timing.iterationDuration = std::max<double>(duration, 0);

    RefPtr<Animation> animation = Animation::create(element, effect, timing);
    DocumentTimeline* timeline = element->document().timeline();
    ASSERT(timeline);
    timeline->play(animation.get());
}

} // namespace WebCore
