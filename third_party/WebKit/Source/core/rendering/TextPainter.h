// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextPainter_h
#define TextPainter_h

#include "core/rendering/style/RenderStyleConstants.h"
#include "platform/fonts/TextBlob.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/Color.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Font;
class GraphicsContext;
class GraphicsContextStateSaver;
class RenderCombineText;
class ShadowList;
class TextRun;
struct TextRunPaintInfo;

class TextPainter {
public:
    struct Style;

    TextPainter(GraphicsContext*, const Font&, const TextRun&, const FloatPoint& textOrigin, const FloatRect& textBounds, bool horizontal);
    ~TextPainter();

    void setEmphasisMark(const AtomicString&, TextEmphasisPosition);
    void setCombinedText(RenderCombineText* combinedText) { m_combinedText = combinedText; }

    static void updateGraphicsContext(GraphicsContext*, const Style&, bool horizontal, GraphicsContextStateSaver&);

    void paint(int startOffset, int endOffset, int length, const Style&, TextBlobPtr* cachedTextBlob = 0);

    struct Style {
        Color fillColor;
        Color strokeColor;
        Color emphasisMarkColor;
        float strokeWidth;
        const ShadowList* shadow;

        bool operator==(const Style& other)
        {
            return fillColor == other.fillColor
                && strokeColor == other.strokeColor
                && emphasisMarkColor == other.emphasisMarkColor
                && strokeWidth == other.strokeWidth
                && shadow == other.shadow;
        }
        bool operator!=(const Style& other) { return !(*this == other); }
    };

private:
    void updateGraphicsContext(const Style& style, GraphicsContextStateSaver& saver)
    {
        updateGraphicsContext(m_graphicsContext, style, m_horizontal, saver);
    }

    enum PaintInternalStep { PaintText, PaintEmphasisMark };

    template <PaintInternalStep step>
    void paintInternalRun(TextRunPaintInfo&, int from, int to, TextBlobPtr* cachedTextBlob = 0);

    template <PaintInternalStep step>
    void paintInternal(int startOffset, int endOffset, int truncationPoint, TextBlobPtr* cachedTextBlob = 0);

    void paintEmphasisMarkForCombinedText();

    GraphicsContext* m_graphicsContext;
    const Font& m_font;
    const TextRun& m_run;
    FloatPoint m_textOrigin;
    FloatRect m_textBounds;
    bool m_horizontal;
    AtomicString m_emphasisMark;
    int m_emphasisMarkOffset;
    RenderCombineText* m_combinedText;
};

} // namespace blink

#endif // TextPainter_h
