// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FieldsetPainter_h
#define FieldsetPainter_h

namespace blink {

struct PaintInfo;
class LayoutPoint;
class RenderFieldset;

class FieldsetPainter {
public:
    FieldsetPainter(RenderFieldset& renderFieldset) : m_renderFieldset(renderFieldset) { }

    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
    void paintMask(const PaintInfo&, const LayoutPoint&);

private:
    RenderFieldset& m_renderFieldset;
};

} // namespace blink

#endif // FieldsetPainter_h
