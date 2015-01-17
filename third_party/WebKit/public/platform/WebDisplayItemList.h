// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDisplayItemList_h
#define WebDisplayItemList_h

#include "WebBlendMode.h"
#include "WebFloatPoint.h"
#include "WebFloatRect.h"
#include "WebRect.h"
#include "WebVector.h"

#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRegion.h"

class SkImageFilter;
class SkMatrix44;
class SkPicture;

namespace blink {

// An ordered list of items representing content to be rendered (stored in
// 'drawing' items) and operations to be performed when rendering this content
// (stored in 'clip', 'transform', 'transparency', and 'filter' items). See
// http://dev.chromium.org/blink/slimming-paint for more details.
class WebDisplayItemList {
public:
    virtual ~WebDisplayItemList() { }

    // This grabs a ref on the passed-in SkPicture.
    virtual void appendDrawingItem(SkPicture*, const WebFloatPoint& location) = 0;

    virtual void appendClipItem(const WebRect&, const WebVector<SkRRect>&) = 0;
    virtual void appendEndClipItem() = 0;
    virtual void appendClipPathItem(const SkPath&, SkRegion::Op, bool antialias) = 0;
    virtual void appendEndClipPathItem() = 0;
    virtual void appendFloatClipItem(const WebFloatRect&) = 0;
    virtual void appendEndFloatClipItem() = 0;
    virtual void appendTransformItem(const SkMatrix44&) = 0;
    virtual void appendEndTransformItem() = 0;
    virtual void appendTransparencyItem(float opacity, WebBlendMode) = 0;
    virtual void appendEndTransparencyItem() = 0;

    // This grabs a ref on the passed-in filter.
    virtual void appendFilterItem(SkImageFilter*, const WebFloatRect& bounds) = 0;
    virtual void appendEndFilterItem() = 0;
};

} // namespace blink

#endif // WebDisplayItemList_h

