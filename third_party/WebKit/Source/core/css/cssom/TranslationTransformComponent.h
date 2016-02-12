// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TranslationTransformComponent_h
#define TranslationTransformComponent_h

#include "core/css/cssom/LengthValue.h"
#include "core/css/cssom/TransformComponent.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT TranslationTransformComponent final : public TransformComponent {
    WTF_MAKE_NONCOPYABLE(TranslationTransformComponent);
    DEFINE_WRAPPERTYPEINFO();
public:
    static TranslationTransformComponent* create(LengthValue* x, LengthValue* y, ExceptionState&)
    {
        return new TranslationTransformComponent(x, y, nullptr);
    }
    static TranslationTransformComponent* create(LengthValue* x, LengthValue* y, LengthValue* z, ExceptionState&);

    LengthValue* x() const { return m_x; }
    LengthValue* y() const { return m_y; }
    LengthValue* z() const { return m_z; }

    TransformComponentType type() const override { return is2D() ? TranslationType : Translation3DType; }

    // TODO: Implement asMatrix for TranslationTransformComponent.
    MatrixTransformComponent* asMatrix() const override { return nullptr; }

    PassRefPtrWillBeRawPtr<CSSFunctionValue> toCSSValue() const override;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_x);
        visitor->trace(m_y);
        visitor->trace(m_z);
        TransformComponent::trace(visitor);
    }

private:
    TranslationTransformComponent(LengthValue* x, LengthValue* y, LengthValue* z)
        : TransformComponent()
        , m_x(x)
        , m_y(y)
        , m_z(z)
    { }

    bool is2D() const { return !m_z; }

    Member<LengthValue> m_x;
    Member<LengthValue> m_y;
    Member<LengthValue> m_z;
};

} // namespace blink

#endif
