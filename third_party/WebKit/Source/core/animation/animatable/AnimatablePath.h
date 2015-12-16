// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimatablePath_h
#define AnimatablePath_h

#include "core/CoreExport.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/css/CSSPathValue.h"

namespace blink {

class CORE_EXPORT AnimatablePath final : public AnimatableValue {
public:
    ~AnimatablePath() override { }
    static PassRefPtr<AnimatablePath> create(PassRefPtrWillBeRawPtr<CSSPathValue> pathValue)
    {
        return adoptRef(new AnimatablePath(pathValue));
    }

    CSSPathValue* pathValue() const { return m_pathValue.get(); }

protected:
    PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const override;
    bool usesDefaultInterpolationWith(const AnimatableValue*) const override;

private:
    explicit AnimatablePath(PassRefPtrWillBeRawPtr<CSSPathValue> pathValue)
        : m_pathValue(pathValue)
    {
        ASSERT(m_pathValue);
    }
    AnimatableType type() const override { return TypePath; }
    bool equalTo(const AnimatableValue*) const override;
    const RefPtrWillBePersistent<CSSPathValue> m_pathValue;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatablePath, isPath());

} // namespace blink

#endif // AnimatablePath_h
