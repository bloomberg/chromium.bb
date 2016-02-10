// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerspectiveTransformComponent_h
#define PerspectiveTransformComponent_h

#include "core/CoreExport.h"
#include "core/css/cssom/LengthValue.h"
#include "core/css/cssom/TransformComponent.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT PerspectiveTransformComponent : public TransformComponent {
    WTF_MAKE_NONCOPYABLE(PerspectiveTransformComponent);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PerspectiveTransformComponent* create(const LengthValue*, ExceptionState&);

    // Bindings require a non const return value.
    LengthValue* length() const { return const_cast<LengthValue*>(m_length.get()); }

    TransformComponentType type() const override { return PerspectiveType; }

    // TODO: Implement asMatrix for PerspectiveTransformComponent.
    MatrixTransformComponent* asMatrix() const override { return nullptr; }

    PassRefPtrWillBeRawPtr<CSSFunctionValue> toCSSValue() const override;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_length);
        TransformComponent::trace(visitor);
    }

private:
    PerspectiveTransformComponent(const LengthValue* length) : m_length(length) {}

    Member<const LengthValue> m_length;
};

} // namespace blink

#endif
