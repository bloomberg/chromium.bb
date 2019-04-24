/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "sk_tool_utils.h"

#include "Resources.h"
#include "SkCanvas.h"
#include "SkGradientShader.h"
#include "SkRandomScalerContext.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "SkTextBlob.h"
#include "SkTypeface.h"

#include "GrContext.h"

namespace skiagm {
class TextBlobRandomFont : public GpuGM {
public:
    // This gm tests that textblobs can be translated and scaled with a font that returns random
    // but deterministic masks
    TextBlobRandomFont() { }

protected:
    void onOnceBeforeDraw() override {
        SkTextBlobBuilder builder;

        const char* text = "The quick brown fox jumps over the lazy dog.";

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(SK_ColorMAGENTA);

        // make textbloben
        SkFont font;
        font.setSize(32);
        font.setEdging(SkFont::Edging::kSubpixelAntiAlias);

        // Setup our random scaler context
        auto typeface = sk_tool_utils::create_portable_typeface("sans-serif", SkFontStyle::Bold());
        if (!typeface) {
            typeface = SkTypeface::MakeDefault();
        }
        font.setTypeface(sk_make_sp<SkRandomTypeface>(std::move(typeface), paint, false));

        SkScalar y = 0;
        SkRect bounds;
        font.measureText(text, strlen(text), kUTF8_SkTextEncoding, &bounds);
        y -= bounds.fTop;
        sk_tool_utils::add_to_text_blob(&builder, text, font, 0, y);
        y += bounds.fBottom;

        // A8
        const char* bigtext1 = "The quick brown fox";
        const char* bigtext2 = "jumps over the lazy dog.";
        font.setSize(160);
        font.setSubpixel(false);
        font.setEdging(SkFont::Edging::kAntiAlias);
        font.measureText(bigtext1, strlen(bigtext1), kUTF8_SkTextEncoding, &bounds);
        y -= bounds.fTop;
        sk_tool_utils::add_to_text_blob(&builder, bigtext1, font, 0, y);
        y += bounds.fBottom;

        font.measureText(bigtext2, strlen(bigtext2), kUTF8_SkTextEncoding, &bounds);
        y -= bounds.fTop;
        sk_tool_utils::add_to_text_blob(&builder, bigtext2, font, 0, y);
        y += bounds.fBottom;

        // color emoji
        if (sk_sp<SkTypeface> origEmoji = sk_tool_utils::emoji_typeface()) {
            font.setTypeface(sk_make_sp<SkRandomTypeface>(origEmoji, paint, false));
            const char* emojiText = sk_tool_utils::emoji_sample_text();
            font.measureText(emojiText, strlen(emojiText), kUTF8_SkTextEncoding, &bounds);
            y -= bounds.fTop;
            sk_tool_utils::add_to_text_blob(&builder, emojiText, font, 0, y);
            y += bounds.fBottom;
        }

        // build
        fBlob = builder.make();
    }

    SkString onShortName() override {
        return SkString("textblobrandomfont");
    }

    SkISize onISize() override {
        return SkISize::Make(kWidth, kHeight);
    }

    DrawResult onDraw(GrContext* context, GrRenderTargetContext*, SkCanvas* canvas,
                      SkString* errorMsg) override {
        // This GM exists to test a specific feature of the GPU backend.
        // This GM uses sk_tool_utils::makeSurface which doesn't work well with vias.
        // This GM uses SkRandomTypeface which doesn't work well with serialization.
        canvas->drawColor(SK_ColorWHITE);

        SkImageInfo info = SkImageInfo::Make(kWidth, kHeight, canvas->imageInfo().colorType(),
                                             kPremul_SkAlphaType,
                                             canvas->imageInfo().refColorSpace());
        SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
        auto surface(sk_tool_utils::makeSurface(canvas, info, &props));
        if (!surface) {
            *errorMsg = "This test requires a surface";
            return DrawResult::kFail;
        }

        SkPaint paint;
        paint.setAntiAlias(true);

        SkCanvas* surfaceCanvas = surface->getCanvas();

        SkScalar stride = SkScalarCeilToScalar(fBlob->bounds().height());
        SkScalar yOffset = 5;

        canvas->save();
        // Originally we would alternate between rotating and not to force blob regeneration,
        // but that code seems to have rotted. Keeping the rotate to match the old GM as
        // much as possible, and it seems like a reasonable stress test for transformed
        // color emoji.
        canvas->rotate(-0.05f);
        canvas->drawTextBlob(fBlob, 10, yOffset, paint);
        yOffset += stride;
        canvas->restore();

        // Rotate in the surface canvas, not the final canvas, to avoid aliasing
        surfaceCanvas->rotate(-0.05f);
        surfaceCanvas->drawTextBlob(fBlob, 10, yOffset, paint);
        surface->draw(canvas, 0, 0, nullptr);
        yOffset += stride;

        // free gpu resources and verify
        context->freeGpuResources();

        canvas->rotate(-0.05f);
        canvas->drawTextBlob(fBlob, 10, yOffset, paint);
        yOffset += stride;
        return DrawResult::kOk;
    }

private:
    sk_sp<SkTextBlob> fBlob;

    static constexpr int kWidth = 2000;
    static constexpr int kHeight = 1600;

    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM(return new TextBlobRandomFont;)
}
