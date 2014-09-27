// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReplacedPainter_h
#define ReplacedPainter_h

namespace blink {

struct PaintInfo;
class LayoutPoint;
class RenderReplaced;

class ReplacedPainter {
public:
    ReplacedPainter(RenderReplaced& renderReplaced) : m_renderReplaced(renderReplaced) { }

    void paint(PaintInfo&, const LayoutPoint&);

private:
    RenderReplaced& m_renderReplaced;
};

} // namespace blink

#endif // ReplacedPainter_h
