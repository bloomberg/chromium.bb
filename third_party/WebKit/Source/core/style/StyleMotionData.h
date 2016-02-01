// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleMotionData_h
#define StyleMotionData_h

#include "core/style/StyleMotionPath.h"
#include "core/style/StyleMotionRotation.h"
#include "platform/Length.h"
#include "wtf/Allocator.h"

namespace blink {

class StyleMotionData {
    DISALLOW_NEW();
public:
    StyleMotionData(StyleMotionPath* path, const Length& offset, StyleMotionRotation rotation)
        : m_path(path)
        , m_offset(offset)
        , m_rotation(rotation)
    {
    }

    bool operator==(const StyleMotionData&) const;

    bool operator!=(const StyleMotionData& o) const { return !(*this == o); }

    // Must be public for SET_VAR in ComputedStyle.h
    RefPtr<StyleMotionPath> m_path; // nullptr indicates path is 'none'
    Length m_offset;
    StyleMotionRotation m_rotation;
};

} // namespace blink

#endif // StyleMotionData_h
