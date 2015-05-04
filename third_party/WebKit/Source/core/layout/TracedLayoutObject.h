// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedLayoutObject_h
#define TracedLayoutObject_h

#include "platform/EventTracer.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Vector.h"

namespace blink {

class JSONObject;
class LayoutObject;
class LayoutView;

class TracedLayoutObject : public TraceEvent::ConvertableToTraceFormat {
    WTF_MAKE_NONCOPYABLE(TracedLayoutObject);
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> create(const LayoutView&);

    String asTraceFormat() const override;

private:
    explicit TracedLayoutObject(const LayoutObject&);

    PassRefPtr<JSONObject> toJSON() const;

    unsigned long m_address;
    String m_name;
    bool m_positioned;
    bool m_selfNeeds;
    bool m_positionedMovement;
    bool m_childNeeds;
    bool m_posChildNeeds;
    bool m_isTableCell;
    String m_tag;
    String m_id;
    Vector<String> m_classNames;
    LayoutRect m_rect;
    unsigned m_row;
    unsigned m_col;
    unsigned m_rowSpan;
    unsigned m_colSpan;
    Vector<RefPtr<TracedLayoutObject>> m_children;
};

} // namespace blink

#endif // TracedLayoutObject_h
