// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockFlowPainter_h
#define BlockFlowPainter_h

namespace blink {

class FloatingObject;
class LayoutPoint;
struct PaintInfo;
class RenderBlockFlow;

class BlockFlowPainter {
public:
    BlockFlowPainter(RenderBlockFlow& renderBlockFlow) : m_renderBlockFlow(renderBlockFlow) { }
    void paintFloats(PaintInfo&, const LayoutPoint&, bool preservePhase);

private:
    RenderBlockFlow& m_renderBlockFlow;
};

} // namespace blink

#endif // BlockFlowPainter_h
