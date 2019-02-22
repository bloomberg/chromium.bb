/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Benchmark.h"
#include "Resources.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkRandom.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTemplates.h"
#include "SkTextBlob.h"
#include "SkTypeface.h"

#include "sk_tool_utils.h"

/*
 * A trivial test which benchmarks the performance of a textblob with a single run.
 */
class SkTextBlobBench : public Benchmark {
public:
    SkTextBlobBench() {}

    void onDelayedSetup() override {
        fPaint.setTypeface(sk_tool_utils::create_portable_typeface("serif", SkFontStyle()));
        fPaint.setTextEncoding(SkPaint::kUTF8_TextEncoding);

        // This text seems representative in both length and letter frequency.
        const char* text = "Keep your sentences short, but not overly so.";

        fGlyphs.setCount(fPaint.textToGlyphs(text, strlen(text), nullptr));
        fPaint.textToGlyphs(text, strlen(text), fGlyphs.begin());

        fPaint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        fPaint.setSubpixelText(true);
    }

    sk_sp<SkTextBlob> makeBlob() {
        const SkTextBlobBuilder::RunBuffer& run =
            fBuilder.allocRunPosH(fPaint, fGlyphs.count(), 10, nullptr);
        for (int i = 0; i < fGlyphs.count(); i++) {
            run.glyphs[i] = fGlyphs[i];
            run.   pos[i] = (i+1) * 10.125;
        }
        return fBuilder.make();
    }

private:
    SkTextBlobBuilder    fBuilder;
    SkPaint              fPaint;
    SkTDArray<uint16_t>  fGlyphs;

    typedef Benchmark INHERITED;
};

class TextBlobCachedBench : public SkTextBlobBench {
    const char* onGetName() override {
        return "TextBlobCachedBench";
    }

    void onDraw(int loops, SkCanvas* canvas) override {
        SkPaint paint;

        auto blob = this->makeBlob();
        auto bigLoops = loops * 100;
        for (int i = 0; i < bigLoops; i++) {
            // To ensure maximum caching, we just redraw the blob at the same place everytime
            canvas->drawTextBlob(blob, 0, 0, paint);
        }
    }
};

class TextBlobFirstTimeBench : public SkTextBlobBench {
    const char* onGetName() override {
        return "TextBlobFirstTimeBench";
    }

    void onDraw(int loops, SkCanvas* canvas) override {
        SkPaint paint;

        auto bigLoops = loops * 100;
        for (int i = 0; i < bigLoops; i++) {
            canvas->drawTextBlob(this->makeBlob(), 0, 0, paint);
        }
    }
};

DEF_BENCH( return new TextBlobCachedBench(); )
DEF_BENCH( return new TextBlobFirstTimeBench(); )
