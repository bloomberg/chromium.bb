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
#include "core/animation/EffectInput.h"

#include "bindings/v8/Dictionary.h"
#include "core/animation/AnimationHelpers.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"

namespace WebCore {

// FIXME: Remove this once we've removed the dependency on Element.
static bool checkDocumentAndRenderer(Element& element)
{
    if (!element.inActiveDocument())
        return false;
    element.document().updateRenderTreeIfNeeded();
    return element.renderer();
}

PassRefPtrWillBeRawPtr<AnimationEffect> EffectInput::convert(Element* element, const Vector<Dictionary>& keyframeDictionaryVector, ExceptionState& exceptionState, bool unsafe)
{
    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!unsafe && !checkDocumentAndRenderer(*element))
        return nullptr;

    StyleSheetContents* styleSheetContents = element->document().elementSheet().contents();
    StringKeyframeVector keyframes;

    for (size_t i = 0; i < keyframeDictionaryVector.size(); ++i) {
        RefPtrWillBeRawPtr<StringKeyframe> keyframe = StringKeyframe::create();
        keyframes.append(keyframe);

        double offset;
        if (keyframeDictionaryVector[i].get("offset", offset))
            keyframe->setOffset(offset);

        String compositeString;
        keyframeDictionaryVector[i].get("composite", compositeString);
        if (compositeString == "add")
            keyframe->setComposite(AnimationEffect::CompositeAdd);

        String timingFunctionString;
        if (keyframeDictionaryVector[i].get("easing", timingFunctionString)) {
            RefPtrWillBeRawPtr<CSSValue> timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue(timingFunctionString);
            if (timingFunctionValue)
                keyframe->setEasing(CSSToStyleMap::animationTimingFunction(timingFunctionValue.get(), false));
        }

        Vector<String> keyframeProperties;
        keyframeDictionaryVector[i].getOwnPropertyNames(keyframeProperties);
        for (size_t j = 0; j < keyframeProperties.size(); ++j) {
            String property = keyframeProperties[j];
            CSSPropertyID id = camelCaseCSSPropertyNameToID(property);
            if (id == CSSPropertyInvalid)
                continue;
            String value;
            keyframeDictionaryVector[i].get(property, value);
            keyframe->setPropertyValue(id, value, styleSheetContents);
        }
    }

    RefPtrWillBeRawPtr<StringKeyframeEffectModel> keyframeEffectModel = StringKeyframeEffectModel::create(keyframes);
    if (!keyframeEffectModel->isReplaceOnly()) {
        exceptionState.throwDOMException(NotSupportedError, "Partial keyframes are not supported.");
        return nullptr;
    }
    keyframeEffectModel->forceConversionsToAnimatableValues(element);

    return keyframeEffectModel;
}

} // namespace WebCore
