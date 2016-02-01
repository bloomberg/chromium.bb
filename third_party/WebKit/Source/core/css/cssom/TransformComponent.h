// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformComponent_h
#define TransformComponent_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/CSSFunctionValue.h"

namespace blink {

class CORE_EXPORT TransformComponent : public GarbageCollectedFinalized<TransformComponent>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(TransformComponent);
    DEFINE_WRAPPERTYPEINFO();
public:
    enum TransformComponentType {
        MatrixType, PerspectiveType, RotationType, ScaleType, SkewType, TranslateType,
        Matrix3DType, Rotation3DType, Scale3DType, Translate3DType
    };

    virtual ~TransformComponent() { }

    virtual TransformComponentType type() const = 0;

    bool is2DComponent() const
    {
        TransformComponentType transformType = type();
        return transformType != Matrix3DType
            && transformType != PerspectiveType
            && transformType != Rotation3DType
            && transformType != Scale3DType
            && transformType != Translate3DType
            && transformType != PerspectiveType;
    }

    String cssString() const
    {
        return toCSSValue()->cssText();
    }

    virtual PassRefPtrWillBeRawPtr<CSSFunctionValue> toCSSValue() const = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    TransformComponent() { }
};

} // namespace blink

#endif
