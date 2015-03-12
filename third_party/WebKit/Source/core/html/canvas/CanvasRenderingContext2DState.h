// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasRenderingContext2DState_h
#define CanvasRenderingContext2DState_h

#include "core/css/CSSFontSelectorClient.h"
#include "core/html/canvas/ClipList.h"
#include "platform/fonts/Font.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Vector.h"

namespace blink {

class CanvasStyle;

class CanvasRenderingContext2DState final : public CSSFontSelectorClient {
public:
    CanvasRenderingContext2DState();
    virtual ~CanvasRenderingContext2DState();

    DECLARE_VIRTUAL_TRACE();

    enum ClipListCopyMode {
        CopyClipList,
        DontCopyClipList
    };

    CanvasRenderingContext2DState(const CanvasRenderingContext2DState&, ClipListCopyMode = CopyClipList);
    CanvasRenderingContext2DState& operator=(const CanvasRenderingContext2DState&);

    // CSSFontSelectorClient implementation
    virtual void fontsNeedUpdate(CSSFontSelector*) override;

    bool hasUnrealizedSaves() const { return m_unrealizedSaveCount; }
    void save() { ++m_unrealizedSaveCount; }
    void restore() { --m_unrealizedSaveCount; }
    void resetUnrealizedSaveCount() { m_unrealizedSaveCount = 0; }

    void setLineDash(const Vector<float>&);
    const Vector<float>& lineDash() const { return m_lineDash; }

    void setLineDashOffset(float offset) { m_lineDashOffset = offset; }
    float lineDashOffset() const { return m_lineDashOffset; }

    // setTransform returns true iff transform is invertible;
    void setTransform(const AffineTransform&);
    void resetTransform();
    AffineTransform transform() const { return m_transform; }
    bool isTransformInvertible() const { return m_isTransformInvertible; }

    void clipPath(const SkPath&, AntiAliasingMode);
    bool hasClip() const { return m_hasClip; }
    bool hasComplexClip() const { return m_hasComplexClip; }
    void playbackClips(SkCanvas* canvas) const { m_clipList.playback(canvas); }

    void setFont(const Font&, CSSFontSelector*);
    const Font& font() const;
    bool hasRealizedFont() const { return m_realizedFont; }
    void setUnparsedFont(const String& font) { m_unparsedFont = font; }
    const String& unparsedFont() const { return m_unparsedFont; }

    void setStrokeStyle(PassRefPtrWillBeRawPtr<CanvasStyle> style) { m_strokeStyle = style; }
    CanvasStyle* strokeStyle() const { return m_strokeStyle.get(); }

    void setFillStyle(PassRefPtrWillBeRawPtr<CanvasStyle> style) { m_fillStyle = style; }
    CanvasStyle* fillStyle() const { return m_fillStyle.get(); }

    enum Direction {
        DirectionInherit,
        DirectionRTL,
        DirectionLTR
    };

    void setDirection(Direction direction) { m_direction = direction; }
    Direction direction() const { return m_direction; }

    void setTextAlign(TextAlign align) { m_textAlign = align; }
    TextAlign textAlign() const { return m_textAlign; }

    void setTextBaseline(TextBaseline baseline) { m_textBaseline = baseline; }
    TextBaseline textBaseline() const { return m_textBaseline; }

    void setLineWidth(float lineWidth) { m_lineWidth = lineWidth; }
    float lineWidth() const { return m_lineWidth; }

    void setLineCap(LineCap lineCap) { m_lineCap = lineCap; }
    LineCap lineCap() const { return m_lineCap; }

    void setLineJoin(LineJoin lineJoin) { m_lineJoin = lineJoin; }
    LineJoin lineJoin() const { return m_lineJoin; }

    void setMiterLimit(float miterLimit) { m_miterLimit = miterLimit; }
    float miterLimit() const { return m_miterLimit; }

    void setShadowOffsetX(float x) { m_shadowOffset.setWidth(x); }
    void setShadowOffsetY(float y) { m_shadowOffset.setHeight(y); }
    const FloatSize& shadowOffset() const { return m_shadowOffset; }

    void setShadowBlur(float shadowBlur) { m_shadowBlur = shadowBlur; }
    float shadowBlur() const { return m_shadowBlur; }

    void setShadowColor(SkColor shadowColor) { m_shadowColor = shadowColor; }
    SkColor shadowColor() const { return m_shadowColor; }

    void setGlobalAlpha(float alpha) { m_globalAlpha = alpha; }
    float globalAlpha() const { return m_globalAlpha; }

    void setGlobalComposite(SkXfermode::Mode mode) { m_globalComposite = mode; }
    SkXfermode::Mode globalComposite() const { return m_globalComposite; }

    void setImageSmoothingEnabled(bool enabled) { m_imageSmoothingEnabled = enabled; }
    bool imageSmoothingEnabled() const { return m_imageSmoothingEnabled; }

    void setUnparsedStrokeColor(const String& color) { m_unparsedStrokeColor = color; }
    const String& unparsedStrokeColor() const { return m_unparsedStrokeColor; }

    void setUnparsedFillColor(const String& color) { m_unparsedFillColor = color; }
    const String& unparsedFillColor() const { return m_unparsedFillColor; }

private:
    unsigned m_unrealizedSaveCount;

    String m_unparsedStrokeColor;
    String m_unparsedFillColor;
    RefPtrWillBeMember<CanvasStyle> m_strokeStyle;
    RefPtrWillBeMember<CanvasStyle> m_fillStyle;

    float m_lineWidth;
    LineCap m_lineCap;
    LineJoin m_lineJoin;
    float m_miterLimit;
    FloatSize m_shadowOffset;
    float m_shadowBlur;
    SkColor m_shadowColor;
    float m_globalAlpha;
    SkXfermode::Mode m_globalComposite;
    AffineTransform m_transform;
    bool m_isTransformInvertible;
    Vector<float> m_lineDash;
    float m_lineDashOffset;
    bool m_imageSmoothingEnabled;

    // Text state.
    TextAlign m_textAlign;
    TextBaseline m_textBaseline;
    Direction m_direction;

    String m_unparsedFont;
    Font m_font;
    bool m_realizedFont;

    bool m_hasClip;
    bool m_hasComplexClip;

    ClipList m_clipList;
};

} // blink

#endif
