// Copyright 2021 Google LLC.
#ifndef Painter_DEFINED
#define Painter_DEFINED
#include "experimental/sktext/include/Text.h"
#include "experimental/sktext/include/Types.h"
#include "include/core/SkCanvas.h"

namespace skia {
namespace text {

    struct DecoratedBlock {
        DecoratedBlock(uint32_t count, SkPaint fg, SkPaint bg)
                : charCount(count)
                , foregroundPaint(std::move(fg))
                , backgroundPaint(std::move(bg)) { }
        uint32_t    charCount;
        SkPaint foregroundPaint;
        SkPaint backgroundPaint;
    };

    class TrivialFontChain : public FontChain {
    public:
        TrivialFontChain(const char* ff, SkScalar size, SkFontStyle fontStyle)
                : fTypeface(sk_sp<SkTypeface>(SkFontMgr::RefDefault()->matchFamilyStyle(ff, SkFontStyle::Normal())))
                , fSize(size)
                , fFontStyle(fontStyle) { }
        size_t count() const override { return (size_t)1; }
        sk_sp<SkTypeface> operator[](size_t index) const  override {
            SkASSERT(index == 0);
            return fTypeface;
        }
        float size() const override { return fSize; }

        sk_sp<SkTypeface> getTypeface() const { return fTypeface; }

    private:
        sk_sp<SkTypeface> fTypeface;
        SkScalar fSize;
        SkFontStyle fFontStyle;
    };

    class Paint : public FormattedText::Visitor {
    public:
        void paint(SkCanvas* canvas, SkPoint xy, FormattedText* formattedText, SkSpan<DecoratedBlock> decoratedBlocks);
        // Simplification (using default font manager, default font family and default everything possible)
        static bool drawText(std::u16string text, SkCanvas* canvas, SkScalar x, SkScalar y);
        static bool drawText(std::u16string text, SkCanvas* canvas, SkScalar width);
        static bool drawText(std::u16string text, SkCanvas* canvas,
                             TextDirection textDirection, TextAlign textAlign,
                             SkPaint foreground, SkPaint background,
                             const SkString& fontFamily, SkScalar fontSize, SkFontStyle fontStyle,
                             SkScalar x, SkScalar y);
        static bool drawText(std::u16string text, SkCanvas* canvas,
                             TextDirection textDirection, TextAlign textAlign,
                             SkPaint foreground, SkPaint background,
                             const SkString& fontFamily, SkScalar fontSize, SkFontStyle fontStyle,
                             SkSize reqSize, SkScalar x, SkScalar y);

    private:
        static sk_sp<FormattedText> layout(std::u16string text,
                                           TextDirection textDirection, TextAlign textAlign,
                                           SkSize reqSize,
                                           SkSpan<FontBlock> fontBlocks);

        void onBeginLine(TextRange lineText) override;
        void onEndLine(TextRange) override;
        void onGlyphRun(const SkFont& font,
                        TextRange textRange,
                        SkRect boundingRect,
                        int glyphCount,
                        const uint16_t glyphs[],
                        const SkPoint  positions[],
                        const SkPoint offsets[]) override;
        void onPlaceholder(TextRange, const SkRect& bounds) override;

        // We guarantee that the text range will be inside one of the decorated blocks
        DecoratedBlock findDecoratedBlock(TextRange textRange);

        SkCanvas* fCanvas;
        SkPoint fXY;
        SkSpan<FontBlock> fFontBlocks;
        SkSpan<DecoratedBlock> fDecoratedBlocks;
    };
}  // namespace text
} // namespace skia
#endif // Painter_DEFINED
