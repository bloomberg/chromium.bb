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
#include "core/animation/AnimatableImage.h"

#include "core/css/CSSImageValue.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "wtf/MathExtras.h"

namespace WebCore {

PassRefPtr<AnimatableValue> AnimatableImage::interpolateTo(const AnimatableValue* value, double fraction) const
{
    const AnimatableImage* image = toAnimatableImage(value);
    RefPtr<CSSValue> fromValue = this->toCSSValue();
    if (!fromValue->isImageValue() && !fromValue->isImageGeneratorValue()) {
        if (!m_image->isImageResource())
            return defaultInterpolateTo(this, value, fraction);
        ImageResource* resource = static_cast<ImageResource*>(m_image->data());
        fromValue = CSSImageValue::create(resource->url(), m_image.get());
    }
    RefPtr<CSSValue> toValue = image->toCSSValue();
    if (!toValue->isImageValue() && !toValue->isImageGeneratorValue()) {
        if (!image->m_image->isImageResource())
            return defaultInterpolateTo(this, value, fraction);
        ImageResource* resource = static_cast<ImageResource*>(image->m_image->data());
        toValue = CSSImageValue::create(resource->url(), image->m_image.get());
    }
    RefPtr<CSSCrossfadeValue> crossfadeValue = CSSCrossfadeValue::create(fromValue, toValue);
    crossfadeValue->setPercentage(CSSPrimitiveValue::create(clampTo<double>(fraction, 0.0, 1.0), CSSPrimitiveValue::CSS_NUMBER));
    return create(StyleGeneratedImage::create(crossfadeValue.get()).get());
}

PassRefPtr<AnimatableValue> AnimatableImage::addWith(const AnimatableValue* value) const
{
    // FIXME: Correct procedure is defined here: http://dev.w3.org/fxtf/web-animations/#the--image--type
    return defaultAddWith(this, value);
}

bool AnimatableImage::equalTo(const AnimatableValue* value) const
{
    return StyleImage::imagesEquivalent(m_image.get(), toAnimatableImage(value)->m_image.get());
}

}
