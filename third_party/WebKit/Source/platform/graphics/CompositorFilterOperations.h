// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFilterOperations_h
#define CompositorFilterOperations_h

#include "cc/output/filter_operations.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/IntPoint.h"
#include "platform/graphics/Color.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "wtf/Noncopyable.h"

class SkImageFilter;

namespace blink {

// An ordered list of filter operations.
class PLATFORM_EXPORT CompositorFilterOperations {
    WTF_MAKE_NONCOPYABLE(CompositorFilterOperations);
public:
    CompositorFilterOperations();
    virtual ~CompositorFilterOperations();

    const cc::FilterOperations& asFilterOperations() const;

    virtual void appendGrayscaleFilter(float amount);
    virtual void appendSepiaFilter(float amount);
    virtual void appendSaturateFilter(float amount);
    virtual void appendHueRotateFilter(float amount);
    virtual void appendInvertFilter(float amount);
    virtual void appendBrightnessFilter(float amount);
    virtual void appendContrastFilter(float amount);
    virtual void appendOpacityFilter(float amount);
    virtual void appendBlurFilter(float amount);
    virtual void appendDropShadowFilter(IntPoint offset, float stdDeviation, Color);
    virtual void appendColorMatrixFilter(SkScalar matrix[20]);
    virtual void appendZoomFilter(float amount, int inset);
    virtual void appendSaturatingBrightnessFilter(float amount);

    // This grabs a ref on the passed-in filter.
    virtual void appendReferenceFilter(SkImageFilter*);

    virtual void clear();
    virtual bool isEmpty() const;

private:
    cc::FilterOperations m_filterOperations;
};

} // namespace blink

#endif // CompositorFilterOperations_h
