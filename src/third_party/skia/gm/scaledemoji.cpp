/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "sk_tool_utils.h"

#include "Resources.h"
#include "SkCanvas.h"
#include "SkFontMetrics.h"
#include "SkStream.h"
#include "SkTextBlob.h"
#include "SkTo.h"
#include "SkTypeface.h"

static sk_sp<SkTextBlob> make_hpos_test_blob_utf8(const char* text, const SkFont& font) {
    constexpr SkTextEncoding enc = SkTextEncoding::kUTF8;
    SkTextBlobBuilder builder;
    size_t len = strlen(text);
    int glyphCount = font.countText(text, len, enc);
    const auto& buffer = builder.allocRunPosH(font, glyphCount, 0);
    (void)font.textToGlyphs(text, len, enc, buffer.glyphs, glyphCount);
    font.getXPos(buffer.glyphs, glyphCount, buffer.pos);
    return builder.make();
}

namespace skiagm {

class ScaledEmojiGM : public GM {
public:
    ScaledEmojiGM() { }

protected:
    struct EmojiFont {
        sk_sp<SkTypeface> fTypeface;
        const char* fText;
    } fEmojiFont;

    void onOnceBeforeDraw() override {
        fEmojiFont.fTypeface = sk_tool_utils::emoji_typeface();
        fEmojiFont.fText = sk_tool_utils::emoji_sample_text();
    }

    SkString onShortName() override {
        SkString name("scaledemoji");
        name.append(sk_tool_utils::platform_font_manager());
        return name;
    }

    SkISize onISize() override { return SkISize::Make(1200, 1200); }

    void onDraw(SkCanvas* canvas) override {

        canvas->drawColor(SK_ColorGRAY);

        SkPaint paint;
        SkFont font(fEmojiFont.fTypeface);
        font.setEdging(SkFont::Edging::kAlias);

        const char* text = fEmojiFont.fText;

        // draw text at different point sizes
        // Testing GPU bitmap path, SDF path with no scaling,
        // SDF path with scaling, path rendering with scaling
        SkFontMetrics metrics;
        SkScalar y = 0;
        for (SkScalar textSize : { 70, 180, 270, 340 }) {
            font.setSize(textSize);
            font.getMetrics(&metrics);
            y += -metrics.fAscent;
            canvas->drawSimpleText(text, strlen(text), kUTF8_SkTextEncoding, 10, y, font, paint);
            y += metrics.fDescent + metrics.fLeading;
        }

    }

private:
    typedef GM INHERITED;
};

class ScaledEmojiPosGM : public GM {
public:
    ScaledEmojiPosGM() {}

protected:
    struct EmojiFont {
        sk_sp<SkTypeface> fTypeface;
        const char* fText;
    } fEmojiFont;

    void onOnceBeforeDraw() override {
        fEmojiFont.fTypeface = sk_tool_utils::emoji_typeface();
        fEmojiFont.fText = sk_tool_utils::emoji_sample_text();
    }

    SkString onShortName() override {
        SkString name("scaledemojipos");
        name.append(sk_tool_utils::platform_font_manager());
        return name;
    }

    SkISize onISize() override { return SkISize::Make(1200, 1200); }

    void onDraw(SkCanvas* canvas) override {

        canvas->drawColor(SK_ColorGRAY);

        SkPaint paint;
        SkFont font;
        font.setTypeface(fEmojiFont.fTypeface);
        const char* text = fEmojiFont.fText;

        // draw text at different point sizes
        // Testing GPU bitmap path, SDF path with no scaling,
        // SDF path with scaling, path rendering with scaling
        SkFontMetrics metrics;
        SkScalar y = 0;
        for (SkScalar textSize : { 70, 180, 270, 340 }) {
            font.setSize(textSize);
            font.getMetrics(&metrics);
            y += -metrics.fAscent;

            sk_sp<SkTextBlob> blob = make_hpos_test_blob_utf8(text, font);
            // Draw with an origin.
            canvas->drawTextBlob(blob, 10, y, paint);

            // Draw with shifted canvas.
            canvas->save();
            canvas->translate(750, 0);
            canvas->drawTextBlob(blob, 10, y, paint);
            canvas->restore();

            y += metrics.fDescent + metrics.fLeading;
        }

    }

private:
    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM(return new ScaledEmojiGM;)
DEF_GM(return new ScaledEmojiPosGM;)

}
