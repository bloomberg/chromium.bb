// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableRowPainter_h
#define TableRowPainter_h

#include "core/rendering/RenderTableRow.h"

namespace blink {

class TableRowPainter {
public:
    TableRowPainter(RenderTableRow& renderTableRow) : m_renderTableRow(renderTableRow) { }

    void paint(PaintInfo&, const LayoutPoint&);
    void paintOutlineForRowIfNeeded(PaintInfo&, const LayoutPoint&);

private:
    RenderTableRow& m_renderTableRow;
};

} // namespace blink

#endif // TableRowPainter_h
