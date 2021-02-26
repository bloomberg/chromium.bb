/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bench/Benchmark.h"
#include "include/core/SkFont.h"
#include "include/core/SkTypeface.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/GrRecordingContext.h"
#include "src/core/SkUtils.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrRenderTargetContext.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/text/GrTextBlob.h"
#include "src/utils/SkUTF.h"


// From Project Guttenberg. This is UTF-8 text.
static const char* gText =
        "Call me Ishmael.  Some years ago--never mind how long precisely";

class DirectMaskGlyphVertexFillBenchmark : public Benchmark {
    bool isSuitableFor(Backend backend) override {
        return backend == kGPU_Backend;
    }

    const char* onGetName() override {
        return "DirectMaskGlyphVertexFillBenchmark";
    }

    void onPerCanvasPreDraw(SkCanvas* canvas) override {
        auto typeface = SkTypeface::MakeFromName("monospace", SkFontStyle());
        SkFont font(typeface);

        SkMatrix view = SkMatrix::I();
        size_t len = strlen(gText);
        SkGlyphRunBuilder builder;
        SkPaint paint;
        builder.drawTextUTF8(paint, font, gText, len, {100, 100});
        auto glyphRunList = builder.useGlyphRunList();
        SkASSERT(!glyphRunList.empty());
        fBlob = GrTextBlob::Make(glyphRunList, view);
        SkSurfaceProps props;
        if (canvas) { canvas->getProps(&props); }
        auto colorSpace = SkColorSpace::MakeSRGB();
        SkGlyphRunListPainter painter{props, kUnknown_SkColorType,
                                      colorSpace.get(), SkStrikeCache::GlobalStrikeCache()};

        GrSDFTOptions options{256, 256};
        painter.processGlyphRunList(
                glyphRunList, view, props, false, options, fBlob.get());


        SkASSERT(fBlob->subRunList().head() != nullptr);
        GrAtlasSubRun* subRun = static_cast<GrAtlasSubRun*>(fBlob->subRunList().head());
        subRun->testingOnly_packedGlyphIDToGrGlyph(&fCache);
        fVertices.reset(new char[subRun->vertexStride() * subRun->glyphCount() * 4]);
    }

    void onDraw(int loops, SkCanvas* canvas) override {
        GrAtlasSubRun* subRun = static_cast<GrAtlasSubRun*>(fBlob->subRunList().head());

        SkIRect clip = SkIRect::MakeEmpty();
        SkPaint paint;
        GrColor grColor = SkColorToPremulGrColor(paint.getColor());

        for (int loop = 0; loop < loops; loop++) {
            subRun->fillVertexData(fVertices.get(), 0, subRun->glyphCount(),
                                   grColor, SkMatrix::I(), {100, 100}, clip);
        }
    }

private:
    sk_sp<GrTextBlob> fBlob;
    GrStrikeCache fCache;
    std::unique_ptr<char[]> fVertices;
};

DEF_BENCH(return new DirectMaskGlyphVertexFillBenchmark{});
