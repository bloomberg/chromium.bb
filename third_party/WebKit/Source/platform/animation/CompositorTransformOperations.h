// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorTransformOperations_h
#define CompositorTransformOperations_h

#include "cc/animation/transform_operations.h"
#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"

class SkMatrix44;

namespace blink {

class PLATFORM_EXPORT CompositorTransformOperations {
    WTF_MAKE_NONCOPYABLE(CompositorTransformOperations);
public:
    CompositorTransformOperations();
    virtual ~CompositorTransformOperations();

    const cc::TransformOperations& asTransformOperations() const;

    // Returns true if these operations can be blended. It will only return
    // false if we must resort to matrix interpolation, and matrix interpolation
    // fails (this can happen if either matrix cannot be decomposed).
    virtual bool canBlendWith(const CompositorTransformOperations& other) const;

    virtual void appendTranslate(double x, double y, double z);
    virtual void appendRotate(double x, double y, double z, double degrees);
    virtual void appendScale(double x, double y, double z);
    virtual void appendSkew(double x, double y);
    virtual void appendPerspective(double depth);
    virtual void appendMatrix(const SkMatrix44&);
    virtual void appendIdentity();

    virtual bool isIdentity() const;

private:
    cc::TransformOperations m_transformOperations;
};

} // namespace blink

#endif // CompositorTransformOperations_h
