// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformValue_h
#define TransformValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/StyleValue.h"

namespace blink {

class CORE_EXPORT TransformValue final : public StyleValue {
    WTF_MAKE_NONCOPYABLE(TransformValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    static TransformValue* create()
    {
        return new TransformValue();
    }

    bool is2D() const;

    String cssString() const override;

    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override;

    StyleValueType type() const override { return TransformValueType; }

private:
    TransformValue() {}
};

} // namespace blink

#endif
