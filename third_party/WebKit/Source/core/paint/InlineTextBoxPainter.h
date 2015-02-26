// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineTextBoxPainter_h
#define InlineTextBoxPainter_h

#include "core/layout/style/LayoutStyleConstants.h"

namespace blink {

struct CompositionUnderline;
struct PaintInfo;

class Color;
class DocumentMarker;
class FloatPoint;
class FloatRect;
class Font;
class GraphicsContext;
class InlineTextBox;
class LayoutPoint;
class LayoutStyle;
class LayoutTextCombine;

class InlineTextBoxPainter {
public:
    InlineTextBoxPainter(InlineTextBox& inlineTextBox) : m_inlineTextBox(inlineTextBox) { }

    void paint(const PaintInfo&, const LayoutPoint&);
    void paintDocumentMarkers(GraphicsContext*, const FloatPoint& boxOrigin, const LayoutStyle&, const Font&, bool background);
    void paintDocumentMarker(GraphicsContext*, const FloatPoint& boxOrigin, DocumentMarker*, const LayoutStyle&, const Font&, bool grammar);
    void paintTextMatchMarker(GraphicsContext*, const FloatPoint& boxOrigin, DocumentMarker*, const LayoutStyle&, const Font&);

    static void removeFromTextBlobCache(InlineTextBox&);

private:
    enum class PaintOptions { Normal, CombinedText };

    void paintCompositionBackgrounds(GraphicsContext*, const FloatPoint& boxOrigin, const LayoutStyle&, const Font&, bool useCustomUnderlines);
    void paintSingleCompositionBackgroundRun(GraphicsContext*, const FloatPoint& boxOrigin, const LayoutStyle&, const Font&, Color backgroundColor, int startPos, int endPos);
    template <PaintOptions>
    void paintSelection(GraphicsContext*, const FloatRect& boxRect, const LayoutStyle&, const Font&, Color textColor, LayoutTextCombine* = nullptr);
    void paintDecoration(GraphicsContext*, const FloatPoint& boxOrigin, TextDecoration);
    void paintCompositionUnderline(GraphicsContext*, const FloatPoint& boxOrigin, const CompositionUnderline&);
    unsigned underlinePaintStart(const CompositionUnderline&);
    unsigned underlinePaintEnd(const CompositionUnderline&);

    InlineTextBox& m_inlineTextBox;
};

} // namespace blink

#endif // InlineTextBoxPainter_h
