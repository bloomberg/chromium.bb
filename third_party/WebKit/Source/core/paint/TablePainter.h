// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TablePainter_h
#define TablePainter_h

namespace blink {

class LayoutPoint;
struct PaintInfo;
class RenderTable;

class TablePainter {
public:
    TablePainter(RenderTable& renderTable) : m_renderTable(renderTable) { }

    void paintObject(const PaintInfo&, const LayoutPoint&);
    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
    void paintMask(const PaintInfo&, const LayoutPoint&);

private:
    RenderTable& m_renderTable;
};

} // namespace blink

#endif // TablePainter_h
