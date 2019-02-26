/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkGlyphCache.h"

#include "Benchmark.h"
#include "SkCanvas.h"
#include "SkStrikeCache.h"
#include "SkGraphics.h"
#include "SkTaskGroup.h"
#include "SkTypeface.h"
#include "sk_tool_utils.h"


static void do_font_stuff(SkFont* font) {
    SkPaint defaultPaint;
    for (SkScalar i = 8; i < 64; i++) {
        font->setSize(i);
        auto cache = SkStrikeCache::FindOrCreateStrikeExclusive(
                *font,  defaultPaint, SkSurfaceProps(0, kUnknown_SkPixelGeometry),
                SkScalerContextFlags::kNone, SkMatrix::I());
        uint16_t glyphs['z'];
        for (int c = ' '; c < 'z'; c++) {
            glyphs[c] = cache->unicharToGlyph(c);
        }
        for (int lookups = 0; lookups < 10; lookups++) {
            for (int c = ' '; c < 'z'; c++) {
                const SkGlyph& g = cache->getGlyphIDMetrics(glyphs[c]);
                cache->findImage(g);
            }
        }

    }
}

class SkGlyphCacheBasic : public Benchmark {
public:
    explicit SkGlyphCacheBasic(size_t cacheSize) : fCacheSize(cacheSize) { }

protected:
    const char* onGetName() override {
        fName.printf("SkGlyphCacheBasic%dK", (int)(fCacheSize >> 10));
        return fName.c_str();
    }

    bool isSuitableFor(Backend backend) override {
        return backend == kNonRendering_Backend;
    }

    void onDraw(int loops, SkCanvas*) override {
        size_t oldCacheLimitSize = SkGraphics::GetFontCacheLimit();
        SkGraphics::SetFontCacheLimit(fCacheSize);
        SkFont font;
        font.setEdging(SkFont::Edging::kAntiAlias);
        font.setSubpixel(true);
        font.setTypeface(sk_tool_utils::create_portable_typeface("serif", SkFontStyle::Italic()));

        for (int work = 0; work < loops; work++) {
            do_font_stuff(&font);
        }
        SkGraphics::SetFontCacheLimit(oldCacheLimitSize);
    }

private:
    typedef Benchmark INHERITED;
    const size_t fCacheSize;
    SkString fName;
};

class SkGlyphCacheStressTest : public Benchmark {
public:
    explicit SkGlyphCacheStressTest(int cacheSize) : fCacheSize(cacheSize) { }

protected:
    const char* onGetName() override {
        fName.printf("SkGlyphCacheStressTest%dK", (int)(fCacheSize >> 10));
        return fName.c_str();
    }

    bool isSuitableFor(Backend backend) override {
        return backend == kNonRendering_Backend;
    }

    void onDraw(int loops, SkCanvas*) override {
        size_t oldCacheLimitSize = SkGraphics::GetFontCacheLimit();
        SkGraphics::SetFontCacheLimit(fCacheSize);
        sk_sp<SkTypeface> typefaces[] =
            {sk_tool_utils::create_portable_typeface("serif", SkFontStyle::Italic()),
             sk_tool_utils::create_portable_typeface("sans-serif", SkFontStyle::Italic())};

        for (int work = 0; work < loops; work++) {
            SkTaskGroup().batch(16, [&](int threadIndex) {
                SkFont font;
                font.setEdging(SkFont::Edging::kAntiAlias);
                font.setSubpixel(true);
                font.setTypeface(typefaces[threadIndex % 2]);
                do_font_stuff(&font);
            });
        }
        SkGraphics::SetFontCacheLimit(oldCacheLimitSize);
    }

private:
    typedef Benchmark INHERITED;
    const size_t fCacheSize;
    SkString fName;
};

DEF_BENCH( return new SkGlyphCacheBasic(256 * 1024); )
DEF_BENCH( return new SkGlyphCacheBasic(32 * 1024 * 1024); )
DEF_BENCH( return new SkGlyphCacheStressTest(256 * 1024); )
DEF_BENCH( return new SkGlyphCacheStressTest(32 * 1024 * 1024); )
