/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Resources.h"
#include "SkCanvas.h"
#include "SkFont.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "SkTextBlob.h"
#include "SkTo.h"
#include "SkTypeface.h"
#include "gm.h"
#include "sk_tool_utils.h"

class DFTextGM : public skiagm::GM {
public:
    DFTextGM() {
        this->setBGColor(0xFFFFFFFF);
    }

protected:
    void onOnceBeforeDraw() override {
        fEmojiTypeface = sk_tool_utils::emoji_typeface();
        fEmojiText = sk_tool_utils::emoji_sample_text();
    }

    SkString onShortName() override {
        SkString name("dftext");
        name.append(sk_tool_utils::platform_font_manager());
        return name;
    }

    SkISize onISize() override {
        return SkISize::Make(1024, 768);
    }

    virtual void onDraw(SkCanvas* inputCanvas) override {
        SkScalar textSizes[] = { 9.0f, 9.0f*2.0f, 9.0f*5.0f, 9.0f*2.0f*5.0f };
        SkScalar scales[] = { 2.0f*5.0f, 5.0f, 2.0f, 1.0f };

        // set up offscreen rendering with distance field text
        GrContext* ctx = inputCanvas->getGrContext();
        SkISize size = onISize();
        SkImageInfo info = SkImageInfo::MakeN32(size.width(), size.height(), kPremul_SkAlphaType,
                                                inputCanvas->imageInfo().refColorSpace());
        SkSurfaceProps props(SkSurfaceProps::kUseDeviceIndependentFonts_Flag,
                             SkSurfaceProps::kLegacyFontHost_InitType);
        auto surface(SkSurface::MakeRenderTarget(ctx, SkBudgeted::kNo, info, 0, &props));
        SkCanvas* canvas = surface ? surface->getCanvas() : inputCanvas;
        // init our new canvas with the old canvas's matrix
        canvas->setMatrix(inputCanvas->getTotalMatrix());
        // apply global scale to test glyph positioning
        canvas->scale(1.05f, 1.05f);
        canvas->clear(0xffffffff);

        SkPaint paint;
        paint.setAntiAlias(true);

        SkFont font(sk_tool_utils::create_portable_typeface("serif", SkFontStyle()));
        font.setSubpixel(true);

        const char* text = "Hamburgefons";
        const size_t textLen = strlen(text);

        // check scaling up
        SkScalar x = SkIntToScalar(0);
        SkScalar y = SkIntToScalar(78);
        for (size_t i = 0; i < SK_ARRAY_COUNT(textSizes); ++i) {
            SkAutoCanvasRestore acr(canvas, true);
            canvas->translate(x, y);
            canvas->scale(scales[i], scales[i]);
            font.setSize(textSizes[i]);
            canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, 0, 0, font, paint);
            y += font.getMetrics(nullptr)*scales[i];
        }

        // check rotation
        for (size_t i = 0; i < 5; ++i) {
            SkScalar rotX = SkIntToScalar(10);
            SkScalar rotY = y;

            SkAutoCanvasRestore acr(canvas, true);
            canvas->translate(SkIntToScalar(10 + i * 200), -80);
            canvas->rotate(SkIntToScalar(i * 5), rotX, rotY);
            for (int ps = 6; ps <= 32; ps += 3) {
                font.setSize(SkIntToScalar(ps));
                canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, rotX, rotY, font, paint);
                rotY += font.getMetrics(nullptr);
            }
        }

        // check scaling down
        font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
        x = SkIntToScalar(680);
        y = SkIntToScalar(20);
        size_t arraySize = SK_ARRAY_COUNT(textSizes);
        for (size_t i = 0; i < arraySize; ++i) {
            SkAutoCanvasRestore acr(canvas, true);
            canvas->translate(x, y);
            SkScalar scaleFactor = SkScalarInvert(scales[arraySize - i - 1]);
            canvas->scale(scaleFactor, scaleFactor);
            font.setSize(textSizes[i]);
            canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, 0, 0, font, paint);
            y += font.getMetrics(nullptr)*scaleFactor;
        }

        // check pos text
        {
            SkAutoCanvasRestore acr(canvas, true);

            canvas->scale(2.0f, 2.0f);

            SkAutoTArray<SkGlyphID> glyphs(SkToInt(textLen));
            int count = font.textToGlyphs(text, textLen, kUTF8_SkTextEncoding, glyphs.get(), textLen);
            SkAutoTArray<SkPoint>  pos(count);
            font.setSize(textSizes[0]);
            font.getPos(glyphs.get(), count, pos.get(), {340, 75});

            auto blob = SkTextBlob::MakeFromPosText(glyphs.get(), count * sizeof(SkGlyphID),
                                                    pos.get(), font, kGlyphID_SkTextEncoding);
            canvas->drawTextBlob(blob, 0, 0, paint);
        }


        // check gamma-corrected blending
        const SkColor fg[] = {
            0xFFFFFFFF,
            0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF,
            0xFFFF0000, 0xFF00FF00, 0xFF0000FF,
            0xFF000000,
        };

        paint.setColor(0xFFF7F3F7);
        SkRect r = SkRect::MakeLTRB(670, 215, 820, 397);
        canvas->drawRect(r, paint);

        x = SkIntToScalar(680);
        y = SkIntToScalar(235);
        font.setSize(SkIntToScalar(19));
        for (size_t i = 0; i < SK_ARRAY_COUNT(fg); ++i) {
            paint.setColor(fg[i]);

            canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, x, y, font, paint);
            y += font.getMetrics(nullptr);
        }

        paint.setColor(0xFF181C18);
        r = SkRect::MakeLTRB(820, 215, 970, 397);
        canvas->drawRect(r, paint);

        x = SkIntToScalar(830);
        y = SkIntToScalar(235);
        font.setSize(SkIntToScalar(19));
        for (size_t i = 0; i < SK_ARRAY_COUNT(fg); ++i) {
            paint.setColor(fg[i]);

            canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, x, y, font, paint);
            y += font.getMetrics(nullptr);
        }

        // check skew
        {
            font.setEdging(SkFont::Edging::kAntiAlias);
            SkAutoCanvasRestore acr(canvas, true);
            canvas->skew(0.0f, 0.151515f);
            font.setSize(SkIntToScalar(32));
            canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, 745, 70, font, paint);
        }
        {
            font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
            SkAutoCanvasRestore acr(canvas, true);
            canvas->skew(0.5f, 0.0f);
            font.setSize(SkIntToScalar(32));
            canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, 580, 125, font, paint);
        }

        // check perspective
        {
            font.setEdging(SkFont::Edging::kAntiAlias);
            SkAutoCanvasRestore acr(canvas, true);
            SkMatrix persp;
            persp.setAll(0.9839f, 0, 0,
                         0.2246f, 0.6829f, 0,
                         0.0002352f, -0.0003844f, 1);
            canvas->concat(persp);
            canvas->translate(1100, -295);
            font.setSize(37.5f);
            canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, 0, 0, font, paint);
        }
        {
            font.setSubpixel(false);
            font.setEdging(SkFont::Edging::kAlias);
            SkAutoCanvasRestore acr(canvas, true);
            SkMatrix persp;
            persp.setAll(0.9839f, 0, 0,
                         0.2246f, 0.6829f, 0,
                         0.0002352f, -0.0003844f, 1);
            canvas->concat(persp);
            canvas->translate(1075, -245);
            canvas->scale(375, 375);
            font.setSize(0.1f);
            canvas->drawSimpleText(text, textLen, kUTF8_SkTextEncoding, 0, 0, font, paint);
        }

        // check color emoji
        if (fEmojiTypeface) {
            SkFont emoiFont;
            emoiFont.setSubpixel(true);
            emoiFont.setTypeface(fEmojiTypeface);
            emoiFont.setSize(SkIntToScalar(19));
            canvas->drawSimpleText(fEmojiText, strlen(fEmojiText), kUTF8_SkTextEncoding, 670, 90, emoiFont, paint);
        }

        // render offscreen buffer
        if (surface) {
            SkAutoCanvasRestore acr(inputCanvas, true);
            // since we prepended this matrix already, we blit using identity
            inputCanvas->resetMatrix();
            inputCanvas->drawImage(surface->makeImageSnapshot().get(), 0, 0, nullptr);
        }
    }

private:
    sk_sp<SkTypeface> fEmojiTypeface;
    const char* fEmojiText;

    typedef skiagm::GM INHERITED;
};

DEF_GM(return new DFTextGM;)
