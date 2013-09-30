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

#ifndef AnimatableValue_h
#define AnimatableValue_h

#include "core/css/CSSValue.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class AnimatableValue : public RefCounted<AnimatableValue> {
public:
    virtual ~AnimatableValue() { }

    static const AnimatableValue* neutralValue();

    static PassRefPtr<AnimatableValue> interpolate(const AnimatableValue*, const AnimatableValue*, double fraction);
    // For noncommutative values read add(A, B) to mean the value A with B composed onto it.
    static PassRefPtr<AnimatableValue> add(const AnimatableValue*, const AnimatableValue*);

    bool isColor() const { return m_type == TypeColor; }
    bool isImage() const { return m_type == TypeImage; }
    bool isLengthBox() const { return m_type == TypeLengthBox; }
    bool isLengthSize() const { return m_type == TypeLengthSize; }
    bool isNumber() const { return m_type == TypeNumber; }
    bool isNeutral() const { return m_type == TypeNeutral; }
    bool isTransform() const { return m_type == TypeTransform; }
    bool isUnknown() const { return m_type == TypeUnknown; }
    bool isVisibility() const { return m_type == TypeVisibility; }

protected:
    enum AnimatableType {
        TypeColor,
        TypeImage,
        TypeLengthBox,
        TypeLengthSize,
        TypeNeutral,
        TypeNumber,
        TypeTransform,
        TypeUnknown,
        TypeVisibility,
    };

    AnimatableValue(AnimatableType type) : m_type(type) { }

    bool isSameType(const AnimatableValue* value) const
    {
        ASSERT(value);
        return value->m_type == m_type;
    }

    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const = 0;
    static PassRefPtr<AnimatableValue> defaultInterpolateTo(const AnimatableValue* left, const AnimatableValue* right, double fraction) { return takeConstRef((fraction < 0.5) ? left : right); }

    // For noncommutative values read A->addWith(B) to mean the value A with B composed onto it.
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const;
    static PassRefPtr<AnimatableValue> defaultAddWith(const AnimatableValue* left, const AnimatableValue* right) { return takeConstRef(right); }

    template <class T>
    static PassRefPtr<T> takeConstRef(const T* value) { return PassRefPtr<T>(const_cast<T*>(value)); }

    const AnimatableType m_type;
};

} // namespace WebCore

#endif // AnimatableValue_h
