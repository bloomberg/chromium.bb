// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFilterKeyframe_h
#define CompositorFilterKeyframe_h

#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT CompositorFilterKeyframe {
public:
    CompositorFilterKeyframe(double time, PassOwnPtr<CompositorFilterOperations>);
    ~CompositorFilterKeyframe();

    double time() const { return m_time; }

    const CompositorFilterOperations& value() const { return *m_value.get(); }

private:
    double m_time;
    OwnPtr<CompositorFilterOperations> m_value;
};

} // namespace blink

#endif // CompositorFilterKeyframe_h
