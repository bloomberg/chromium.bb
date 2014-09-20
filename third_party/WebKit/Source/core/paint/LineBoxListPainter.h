// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineBoxListPainter_h
#define LineBoxListPainter_h

#include "core/rendering/style/RenderStyleConstants.h"

namespace blink {

class LayoutPoint;
struct PaintInfo;
class RenderBoxModelObject;
class RenderLineBoxList;

class LineBoxListPainter {
public:
    LineBoxListPainter(RenderLineBoxList& renderLineBoxList) : m_renderLineBoxList(renderLineBoxList) { }

    void paint(RenderBoxModelObject*, PaintInfo&, const LayoutPoint&) const;

private:
    RenderLineBoxList& m_renderLineBoxList;
};

} // namespace blink

#endif // LineBoxListPainter_h
