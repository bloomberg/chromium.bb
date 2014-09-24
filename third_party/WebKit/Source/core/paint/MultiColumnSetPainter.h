// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MultiColumnSetPainter_h
#define MultiColumnSetPainter_h

namespace blink {

struct PaintInfo;
class LayoutPoint;
class RenderMultiColumnSet;

class MultiColumnSetPainter {
public:
    MultiColumnSetPainter(RenderMultiColumnSet& renderMultiColumnSet) : m_renderMultiColumnSet(renderMultiColumnSet) { }
    void paintObject(PaintInfo&, const LayoutPoint& paintOffset);

private:
    void paintColumnRules(PaintInfo&, const LayoutPoint& paintOffset);

    RenderMultiColumnSet& m_renderMultiColumnSet;
};

} // namespace blink

#endif // MultiColumnSetPainter_h
