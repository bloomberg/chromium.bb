// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollAnchor_h
#define ScrollAnchor_h

#include "core/CoreExport.h"
#include "platform/geometry/DoublePoint.h"
#include "platform/heap/Handle.h"

namespace blink {

class LayoutObject;
class ScrollableArea;

// Scrolls to compensate for layout movements (bit.ly/scroll-anchoring).
class CORE_EXPORT ScrollAnchor final {
    DISALLOW_NEW();

public:
    ScrollAnchor(ScrollableArea*);
    ~ScrollAnchor();

    // The LayoutObject we are currently anchored to. Lazily computed during
    // save() and cached until the next call to clear().
    const LayoutObject* anchorObject() const { return m_anchorObject; }

    // Invalidates the anchor.
    void clear() { m_anchorObject = nullptr; }

    // Records the anchor's location in relation to the scroller. Should be
    // called when the scroller is about to be laid out.
    void save();

    // Scrolls to compensate for any change in the anchor's relative location
    // since the most recent call to save(). Should be called immediately after
    // the scroller has been laid out.
    void restore();

    DEFINE_INLINE_TRACE() { visitor->trace(m_scroller); }

private:
    // The scroller that owns and is adjusted by this ScrollAnchor.
    RawPtrWillBeMember<ScrollableArea> m_scroller;

    // The LayoutObject we should anchor to. Lazily computed.
    const LayoutObject* m_anchorObject;

    // Location of m_layoutObject relative to scroller at time of save().
    DoublePoint m_savedRelativeOffset;
};

} // namespace blink

#endif // ScrollAnchor_h
