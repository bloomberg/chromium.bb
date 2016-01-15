// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutPaintShim_h
#define LineLayoutPaintShim_h

#include "core/layout/api/LineLayoutItem.h"

namespace blink {

class LayoutObject;

// TODO(pilgrim): Remove this kludge once Paint objects have a real API and no longer need access to the underlying LayoutObject.
class LineLayoutPaintShim {
public:
    static LayoutObject* layoutObjectFrom(LineLayoutItem item)
    {
        return item.layoutObject();
    }

    static const LayoutObject* constLayoutObjectFrom(LineLayoutItem item)
    {
        return item.layoutObject();
    }
};

} // namespace blink

#endif // LineLayoutPaintShim_h
