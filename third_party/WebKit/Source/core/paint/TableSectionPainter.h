// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableSectionPainter_h
#define TableSectionPainter_h

namespace blink {

class LayoutPoint;
struct PaintInfo;
class RenderTableCell;
class RenderTableSection;

class TableSectionPainter {
public:
    TableSectionPainter(RenderTableSection& renderTableSection) : m_renderTableSection(renderTableSection) { }

    void paint(PaintInfo&, const LayoutPoint&);
    void paintObject(PaintInfo&, const LayoutPoint&);

private:
    void paintCell(RenderTableCell*, PaintInfo&, const LayoutPoint&);

    RenderTableSection& m_renderTableSection;
};

} // namespace blink

#endif // TableSectionPainter_h
