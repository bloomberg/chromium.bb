/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkTestSVGTypeface.h"

#ifdef SK_XML

#include "Resources.h"
#include "SkAdvancedTypefaceMetrics.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColor.h"
#include "SkData.h"
#include "SkEncodedImageFormat.h"
#include "SkFontDescriptor.h"
#include "SkFontStyle.h"
#include "SkGeometry.h"
#include "SkGlyph.h"
#include "SkImage.h"
#include "SkImageInfo.h"
#include "SkMask.h"
#include "SkMatrix.h"
#include "SkNoDrawCanvas.h"
#include "SkOTUtils.h"
#include "SkPaintPriv.h"
#include "SkPath.h"
#include "SkPathPriv.h"
#include "SkPathEffect.h"
#include "SkPathOps.h"
#include "SkPixmap.h"
#include "SkPointPriv.h"
#include "SkRRect.h"
#include "SkSVGDOM.h"
#include "SkScalerContext.h"
#include "SkSize.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "SkTDArray.h"
#include "SkTemplates.h"
#include "SkUtils.h"

#include <utility>

class SkDescriptor;

SkTestSVGTypeface::SkTestSVGTypeface(const char* name,
                                     int upem,
                                     const SkFontMetrics& fontMetrics,
                                     const SkSVGTestTypefaceGlyphData* data, int dataCount,
                                     const SkFontStyle& style)
    : SkTypeface(style, false)
    , fName(name)
    , fUpem(upem)
    , fFontMetrics(fontMetrics)
    , fGlyphs(new Glyph[dataCount])
    , fGlyphCount(dataCount)
{
    for (int i = 0; i < dataCount; ++i) {
        const SkSVGTestTypefaceGlyphData& datum = data[i];
        std::unique_ptr<SkStreamAsset> stream = GetResourceAsStream(datum.fSvgResourcePath);
        fCMap.set(datum.fUnicode, i);
        fGlyphs[i].fAdvance = datum.fAdvance;
        fGlyphs[i].fOrigin = datum.fOrigin;
        if (!stream) {
            continue;
        }
        sk_sp<SkSVGDOM> svg = SkSVGDOM::MakeFromStream(*stream.get());
        if (!svg) {
            continue;
        }

        const SkSize& sz = svg->containerSize();
        if (sz.isEmpty()) {
            continue;
        }

        fGlyphs[i].fSvg = std::move(svg);
    }
}

SkTestSVGTypeface::~SkTestSVGTypeface() {}

SkTestSVGTypeface::Glyph::Glyph() : fOrigin{0,0}, fAdvance(0) {}
SkTestSVGTypeface::Glyph::~Glyph() {}

void SkTestSVGTypeface::getAdvance(SkGlyph* glyph) const {
    SkGlyphID glyphID = glyph->getGlyphID();
    glyphID = glyphID < fGlyphCount ? glyphID : 0;

    glyph->fAdvanceX = fGlyphs[glyphID].fAdvance;
    glyph->fAdvanceY = 0;
}

void SkTestSVGTypeface::getFontMetrics(SkFontMetrics* metrics) const {
    *metrics = fFontMetrics;
}

void SkTestSVGTypeface::onFilterRec(SkScalerContextRec* rec) const {
    rec->setHinting(kNo_SkFontHinting);
}

void SkTestSVGTypeface::getGlyphToUnicodeMap(SkUnichar* glyphToUnicode) const {
    SkDEBUGCODE(unsigned glyphCount = this->countGlyphs());
    fCMap.foreach([=](const SkUnichar& c, const SkGlyphID& g) {
        SkASSERT(g < glyphCount);
        glyphToUnicode[g] = c;
    });
}

std::unique_ptr<SkAdvancedTypefaceMetrics> SkTestSVGTypeface::onGetAdvancedMetrics() const {
    std::unique_ptr<SkAdvancedTypefaceMetrics> info(new SkAdvancedTypefaceMetrics);
    info->fFontName = fName;
    return info;
}

void SkTestSVGTypeface::onGetFontDescriptor(SkFontDescriptor* desc, bool* isLocal) const {
    desc->setFamilyName(fName.c_str());
    desc->setStyle(this->fontStyle());
    *isLocal = false;
}

int SkTestSVGTypeface::onCharsToGlyphs(const void* chars, Encoding encoding,
                                    uint16_t glyphs[], int glyphCount) const {
    auto utf8  = (const      char*)chars;
    auto utf16 = (const  uint16_t*)chars;
    auto utf32 = (const SkUnichar*)chars;

    for (int i = 0; i < glyphCount; i++) {
        SkUnichar ch;
        switch (encoding) {
            case kUTF8_Encoding:  ch =  SkUTF8_NextUnichar(&utf8 ); break;
            case kUTF16_Encoding: ch = SkUTF16_NextUnichar(&utf16); break;
            case kUTF32_Encoding: ch =                    *utf32++; break;
        }
        if (glyphs) {
            SkGlyphID* g = fCMap.find(ch);
            glyphs[i] = g ? *g : 0;
        }
    }
    return glyphCount;
}

void SkTestSVGTypeface::onGetFamilyName(SkString* familyName) const {
    *familyName = fName;
}

SkTypeface::LocalizedStrings* SkTestSVGTypeface::onCreateFamilyNameIterator() const {
    SkString familyName(fName);
    SkString language("und"); //undetermined
    return new SkOTUtils::LocalizedStrings_SingleName(familyName, language);
}

class SkTestSVGScalerContext : public SkScalerContext {
public:
    SkTestSVGScalerContext(sk_sp<SkTestSVGTypeface> face, const SkScalerContextEffects& effects,
                           const SkDescriptor* desc)
        : SkScalerContext(std::move(face), effects, desc)
    {
        fRec.getSingleMatrix(&fMatrix);
        SkScalar upem = this->geTestSVGTypeface()->fUpem;
        fMatrix.preScale(1.f/upem, 1.f/upem);
    }

protected:
    SkTestSVGTypeface* geTestSVGTypeface() const {
        return static_cast<SkTestSVGTypeface*>(this->getTypeface());
    }

    unsigned generateGlyphCount() override {
        return this->geTestSVGTypeface()->onCountGlyphs();
    }

    uint16_t generateCharToGlyph(SkUnichar u) override {
        uint16_t g;
        (void) this->geTestSVGTypeface()->onCharsToGlyphs(&u, SkTypeface::kUTF32_Encoding, &g, 1);
        return g;
    }

    bool generateAdvance(SkGlyph* glyph) override {
        this->geTestSVGTypeface()->getAdvance(glyph);

        const SkVector advance = fMatrix.mapXY(SkFloatToScalar(glyph->fAdvanceX),
                                               SkFloatToScalar(glyph->fAdvanceY));
        glyph->fAdvanceX = SkScalarToFloat(advance.fX);
        glyph->fAdvanceY = SkScalarToFloat(advance.fY);
        return true;
    }

    void generateMetrics(SkGlyph* glyph) override {
        SkGlyphID glyphID = glyph->getGlyphID();
        glyphID = glyphID < this->geTestSVGTypeface()->fGlyphCount ? glyphID : 0;

        glyph->zeroMetrics();
        glyph->fMaskFormat = SkMask::kARGB32_Format;
        this->generateAdvance(glyph);

        SkTestSVGTypeface::Glyph& glyphData = this->geTestSVGTypeface()->fGlyphs[glyphID];
        if (!glyphData.fSvg) {
            return;
        }

        SkSize containerSize = glyphData.fSvg->containerSize();
        SkRect newBounds = SkRect::MakeXYWH(glyphData.fOrigin.fX, -glyphData.fOrigin.fY,
                                            containerSize.fWidth, containerSize.fHeight);
        fMatrix.mapRect(&newBounds);
        SkScalar dx = SkFixedToScalar(glyph->getSubXFixed());
        SkScalar dy = SkFixedToScalar(glyph->getSubYFixed());
        newBounds.offset(dx, dy);

        SkIRect ibounds;
        newBounds.roundOut(&ibounds);
        glyph->fLeft = ibounds.fLeft;
        glyph->fTop = ibounds.fTop;
        glyph->fWidth = ibounds.width();
        glyph->fHeight = ibounds.height();
    }

    void generateImage(const SkGlyph& glyph) override {
        SkGlyphID glyphID = glyph.getGlyphID();
        glyphID = glyphID < this->geTestSVGTypeface()->fGlyphCount ? glyphID : 0;

        SkBitmap bm;
        // TODO: this should be SkImageInfo::MakeS32 when that passes all the tests.
        bm.installPixels(SkImageInfo::MakeN32(glyph.fWidth, glyph.fHeight, kPremul_SkAlphaType),
                         glyph.fImage, glyph.rowBytes());
        bm.eraseColor(0);

        SkTestSVGTypeface::Glyph& glyphData = this->geTestSVGTypeface()->fGlyphs[glyphID];

        SkScalar dx = SkFixedToScalar(glyph.getSubXFixed());
        SkScalar dy = SkFixedToScalar(glyph.getSubYFixed());

        SkCanvas canvas(bm);
        canvas.translate(-glyph.fLeft, -glyph.fTop);
        canvas.translate(dx, dy);
        canvas.concat(fMatrix);
        canvas.translate(glyphData.fOrigin.fX, -glyphData.fOrigin.fY);

        if (glyphData.fSvg) {
            SkAutoExclusive lock(glyphData.fSvgMutex);
            glyphData.fSvg->render(&canvas);
        }
    }

    bool generatePath(SkGlyphID glyph, SkPath* path) override {
        path->reset();
        return false;
    }

    void generateFontMetrics(SkFontMetrics* metrics) override {
        this->geTestSVGTypeface()->getFontMetrics(metrics);
        SkPaintPriv::ScaleFontMetrics(metrics, fMatrix.getScaleY());
    }

private:
    SkMatrix fMatrix;
};

SkScalerContext* SkTestSVGTypeface::onCreateScalerContext(
    const SkScalerContextEffects& e, const SkDescriptor* desc) const
{
    return new SkTestSVGScalerContext(sk_ref_sp(const_cast<SkTestSVGTypeface*>(this)), e, desc);
}

// Recommended that the first four be .notdef, .null, CR, space
constexpr const static SkSVGTestTypefaceGlyphData gGlyphs[] = {
    {"fonts/svg/notdef.svg", {100,800}, 800, 0x0}, // .notdef
    {"fonts/svg/empty.svg", {0,0}, 800, 0x0020}, // space
    {"fonts/svg/diamond.svg", {100, 800}, 800, 0x2662}, // ♢
    {"fonts/svg/smile.svg", {0,800}, 800, 0x1F600}, // 😀
};

sk_sp<SkTestSVGTypeface> SkTestSVGTypeface::Default() {
    SkFontMetrics metrics;
    metrics.fFlags = SkFontMetrics::kUnderlineThicknessIsValid_Flag |
                     SkFontMetrics::kUnderlinePositionIsValid_Flag  |
                     SkFontMetrics::kStrikeoutThicknessIsValid_Flag |
                     SkFontMetrics::kStrikeoutPositionIsValid_Flag;
    metrics.fTop = -800;
    metrics.fAscent = -800;
    metrics.fDescent = 200;
    metrics.fBottom = 200;
    metrics.fLeading = 100;
    metrics.fAvgCharWidth = 1000;
    metrics.fMaxCharWidth = 1000;
    metrics.fXMin = 0;
    metrics.fXMax = 1000;
    metrics.fXHeight = 500;
    metrics.fCapHeight = 700;
    metrics.fUnderlineThickness = 40;
    metrics.fUnderlinePosition = 20;
    metrics.fStrikeoutThickness = 20;
    metrics.fStrikeoutPosition = -400;
    return sk_make_sp<SkTestSVGTypeface>("Emoji", 1000, metrics, gGlyphs, SK_ARRAY_COUNT(gGlyphs),
                                         SkFontStyle::Normal());
}

void SkTestSVGTypeface::exportTtxCommon(SkWStream* out, const char* type,
                                        const SkTArray<GlyfInfo>* glyfInfo) const
{
    int totalGlyphs = fGlyphCount;
    out->writeText("  <GlyphOrder>\n");
    for (int i = 0; i < fGlyphCount; ++i) {
        out->writeText("    <GlyphID name=\"glyf");
        out->writeHexAsText(i, 4);
        out->writeText("\"/>\n");
    }
    if (glyfInfo) {
        for (int i = 0; i < fGlyphCount; ++i) {
            for (int j = 0; j < (*glyfInfo)[i].fLayers.count(); ++j) {
                out->writeText("    <GlyphID name=\"glyf");
                    out->writeHexAsText(i, 4);
                    out->writeText("l");
                    out->writeHexAsText(j, 4);
                    out->writeText("\"/>\n");
                ++totalGlyphs;
            }
        }
    }
    out->writeText("  </GlyphOrder>\n");

    out->writeText("  <head>\n");
    out->writeText("    <tableVersion value=\"1.0\"/>\n");
    out->writeText("    <fontRevision value=\"1.0\"/>\n");
    out->writeText("    <checkSumAdjustment value=\"0xa9c3274\"/>\n");
    out->writeText("    <magicNumber value=\"0x5f0f3cf5\"/>\n");
    out->writeText("    <flags value=\"00000000 00011011\"/>\n");
    out->writeText("    <unitsPerEm value=\"");
        out->writeDecAsText(fUpem);
        out->writeText("\"/>\n");
    out->writeText("    <created value=\"Thu Feb 15 12:55:49 2018\"/>\n");
    out->writeText("    <modified value=\"Thu Feb 15 12:55:49 2018\"/>\n");
    // TODO: not recalculated for bitmap fonts?
    out->writeText("    <xMin value=\"");
        out->writeScalarAsText(fFontMetrics.fXMin);
        out->writeText("\"/>\n");
    out->writeText("    <yMin value=\"");
        out->writeScalarAsText(-fFontMetrics.fBottom);
        out->writeText("\"/>\n");
    out->writeText("    <xMax value=\"");
        out->writeScalarAsText(fFontMetrics.fXMax);
        out->writeText("\"/>\n");
    out->writeText("    <yMax value=\"");
        out->writeScalarAsText(-fFontMetrics.fTop);
        out->writeText("\"/>\n");

    char macStyle[16] = {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};
    if (this->fontStyle().weight() >= SkFontStyle::Bold().weight()) {
        macStyle[0xF - 0x0] = '1'; // Bold
    }
    switch (this->fontStyle().slant()) {
        case SkFontStyle::kUpright_Slant:
            break;
        case SkFontStyle::kItalic_Slant:
            macStyle[0xF - 0x1] = '1'; // Italic
            break;
        case SkFontStyle::kOblique_Slant:
            macStyle[0xF - 0x1] = '1'; // Italic
            break;
        default:
            SK_ABORT("Unknown slant.");
    }
    if (this->fontStyle().width() <= SkFontStyle::kCondensed_Width) {
        macStyle[0xF - 0x5] = '1'; // Condensed
    } else if (this->fontStyle().width() >= SkFontStyle::kExpanded_Width) {
        macStyle[0xF - 0x6] = '1'; // Extended
    }
    out->writeText("    <macStyle value=\"");
        out->write(macStyle, 8);
        out->writeText(" ");
        out->write(macStyle + 8, 8);
        out->writeText("\"/>\n");
    out->writeText("    <lowestRecPPEM value=\"8\"/>\n");
    out->writeText("    <fontDirectionHint value=\"2\"/>\n");
    out->writeText("    <indexToLocFormat value=\"0\"/>\n");
    out->writeText("    <glyphDataFormat value=\"0\"/>\n");
    out->writeText("  </head>\n");

    out->writeText("  <hhea>\n");
    out->writeText("    <tableVersion value=\"0x00010000\"/>\n");
    out->writeText("    <ascent value=\"");
        out->writeDecAsText(-fFontMetrics.fAscent);
        out->writeText("\"/>\n");
    out->writeText("    <descent value=\"");
        out->writeDecAsText(-fFontMetrics.fDescent);
        out->writeText("\"/>\n");
    out->writeText("    <lineGap value=\"");
        out->writeDecAsText(fFontMetrics.fLeading);
        out->writeText("\"/>\n");
    out->writeText("    <advanceWidthMax value=\"0\"/>\n");
    out->writeText("    <minLeftSideBearing value=\"0\"/>\n");
    out->writeText("    <minRightSideBearing value=\"0\"/>\n");
    out->writeText("    <xMaxExtent value=\"");
        out->writeScalarAsText(fFontMetrics.fXMax - fFontMetrics.fXMin);
        out->writeText("\"/>\n");
    out->writeText("    <caretSlopeRise value=\"1\"/>\n");
    out->writeText("    <caretSlopeRun value=\"0\"/>\n");
    out->writeText("    <caretOffset value=\"0\"/>\n");
    out->writeText("    <reserved0 value=\"0\"/>\n");
    out->writeText("    <reserved1 value=\"0\"/>\n");
    out->writeText("    <reserved2 value=\"0\"/>\n");
    out->writeText("    <reserved3 value=\"0\"/>\n");
    out->writeText("    <metricDataFormat value=\"0\"/>\n");
    out->writeText("    <numberOfHMetrics value=\"0\"/>\n");
    out->writeText("  </hhea>\n");

    // Some of this table is going to be re-calculated, but we have to write it out anyway.
    out->writeText("  <maxp>\n");
    out->writeText("    <tableVersion value=\"0x10000\"/>\n");
    out->writeText("    <numGlyphs value=\"");
        out->writeDecAsText(totalGlyphs);
        out->writeText("\"/>\n");
    out->writeText("    <maxPoints value=\"4\"/>\n");
    out->writeText("    <maxContours value=\"1\"/>\n");
    out->writeText("    <maxCompositePoints value=\"0\"/>\n");
    out->writeText("    <maxCompositeContours value=\"0\"/>\n");
    out->writeText("    <maxZones value=\"1\"/>\n");
    out->writeText("    <maxTwilightPoints value=\"0\"/>\n");
    out->writeText("    <maxStorage value=\"0\"/>\n");
    out->writeText("    <maxFunctionDefs value=\"10\"/>\n");
    out->writeText("    <maxInstructionDefs value=\"0\"/>\n");
    out->writeText("    <maxStackElements value=\"512\"/>\n");
    out->writeText("    <maxSizeOfInstructions value=\"24\"/>\n");
    out->writeText("    <maxComponentElements value=\"0\"/>\n");
    out->writeText("    <maxComponentDepth value=\"0\"/>\n");
    out->writeText("  </maxp>\n");

    out->writeText("  <OS_2>\n");
    out->writeText("    <version value=\"4\"/>\n");
    out->writeText("    <xAvgCharWidth value=\"");
        out->writeScalarAsText(fFontMetrics.fAvgCharWidth);
        out->writeText("\"/>\n");
    out->writeText("    <usWeightClass value=\"");
        out->writeDecAsText(this->fontStyle().weight());
        out->writeText("\"/>\n");
    out->writeText("    <usWidthClass value=\"");
        out->writeDecAsText(this->fontStyle().width());
        out->writeText("\"/>\n");
    out->writeText("    <fsType value=\"00000000 00000000\"/>\n");
    out->writeText("    <ySubscriptXSize value=\"665\"/>\n");
    out->writeText("    <ySubscriptYSize value=\"716\"/>\n");
    out->writeText("    <ySubscriptXOffset value=\"0\"/>\n");
    out->writeText("    <ySubscriptYOffset value=\"143\"/>\n");
    out->writeText("    <ySuperscriptXSize value=\"665\"/>\n");
    out->writeText("    <ySuperscriptYSize value=\"716\"/>\n");
    out->writeText("    <ySuperscriptXOffset value=\"0\"/>\n");
    out->writeText("    <ySuperscriptYOffset value=\"491\"/>\n");
    out->writeText("    <yStrikeoutSize value=\"");
        out->writeScalarAsText(fFontMetrics.fStrikeoutThickness);
        out->writeText("\"/>\n");
    out->writeText("    <yStrikeoutPosition value=\"");
        out->writeScalarAsText(-fFontMetrics.fStrikeoutPosition);
        out->writeText("\"/>\n");
    out->writeText("    <sFamilyClass value=\"0\"/>\n");
    out->writeText("    <panose>\n");
    out->writeText("      <bFamilyType value=\"0\"/>\n");
    out->writeText("      <bSerifStyle value=\"0\"/>\n");
    out->writeText("      <bWeight value=\"0\"/>\n");
    out->writeText("      <bProportion value=\"0\"/>\n");
    out->writeText("      <bContrast value=\"0\"/>\n");
    out->writeText("      <bStrokeVariation value=\"0\"/>\n");
    out->writeText("      <bArmStyle value=\"0\"/>\n");
    out->writeText("      <bLetterForm value=\"0\"/>\n");
    out->writeText("      <bMidline value=\"0\"/>\n");
    out->writeText("      <bXHeight value=\"0\"/>\n");
    out->writeText("    </panose>\n");
    out->writeText("    <ulUnicodeRange1 value=\"00000000 00000000 00000000 00000001\"/>\n");
    out->writeText("    <ulUnicodeRange2 value=\"00010000 00000000 00000000 00000000\"/>\n");
    out->writeText("    <ulUnicodeRange3 value=\"00000000 00000000 00000000 00000000\"/>\n");
    out->writeText("    <ulUnicodeRange4 value=\"00000000 00000000 00000000 00000000\"/>\n");
    out->writeText("    <achVendID value=\"Skia\"/>\n");
    char fsSelection[16] = {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};
    fsSelection[0xF - 0x7] = '1'; // Use typo metrics
    if (this->fontStyle().weight() >= SkFontStyle::Bold().weight()) {
        fsSelection[0xF - 0x5] = '1'; // Bold
    }
    switch (this->fontStyle().slant()) {
        case SkFontStyle::kUpright_Slant:
            if (this->fontStyle().weight() < SkFontStyle::Bold().weight()) {
                fsSelection[0xF - 0x6] = '1'; // Not bold or italic, is regular
            }
            break;
        case SkFontStyle::kItalic_Slant:
            fsSelection[0xF - 0x0] = '1'; // Italic
            break;
        case SkFontStyle::kOblique_Slant:
            fsSelection[0xF - 0x0] = '1'; // Italic
            fsSelection[0xF - 0x9] = '1'; // Oblique
            break;
        default:
            SK_ABORT("Unknown slant.");
    }
    out->writeText("    <fsSelection value=\"");
        out->write(fsSelection, 8);
        out->writeText(" ");
        out->write(fsSelection + 8, 8);
        out->writeText("\"/>\n");
    out->writeText("    <usFirstCharIndex value=\"0\"/>\n");
    out->writeText("    <usLastCharIndex value=\"0\"/>\n");
    out->writeText("    <sTypoAscender value=\"");
        out->writeScalarAsText(-fFontMetrics.fAscent);
        out->writeText("\"/>\n");
    out->writeText("    <sTypoDescender value=\"");
        out->writeScalarAsText(-fFontMetrics.fDescent);
        out->writeText("\"/>\n");
    out->writeText("    <sTypoLineGap value=\"");
        out->writeScalarAsText(fFontMetrics.fLeading);
        out->writeText("\"/>\n");
    out->writeText("    <usWinAscent value=\"");
        out->writeScalarAsText(-fFontMetrics.fAscent);
        out->writeText("\"/>\n");
    out->writeText("    <usWinDescent value=\"");
        out->writeScalarAsText(fFontMetrics.fDescent);
        out->writeText("\"/>\n");
    out->writeText("    <ulCodePageRange1 value=\"00000000 00000000 00000000 00000000\"/>\n");
    out->writeText("    <ulCodePageRange2 value=\"00000000 00000000 00000000 00000000\"/>\n");
    out->writeText("    <sxHeight value=\"");
        out->writeScalarAsText(fFontMetrics.fXHeight);
        out->writeText("\"/>\n");
    out->writeText("    <sCapHeight value=\"");
        out->writeScalarAsText(fFontMetrics.fCapHeight);
        out->writeText("\"/>\n");
    out->writeText("    <usDefaultChar value=\"0\"/>\n");
    out->writeText("    <usBreakChar value=\"32\"/>\n");
    out->writeText("    <usMaxContext value=\"0\"/>\n");
    out->writeText("  </OS_2>\n");

    out->writeText("  <hmtx>\n");
    for (int i = 0; i < fGlyphCount; ++i) {
        out->writeText("    <mtx name=\"glyf");
        out->writeHexAsText(i, 4);
        out->writeText("\" width=\"");
        out->writeDecAsText(fGlyphs[i].fAdvance);
        out->writeText("\" lsb=\"");
        int lsb = fGlyphs[i].fOrigin.fX;
        if (glyfInfo) {
            lsb += (*glyfInfo)[i].fBounds.fLeft;
        }
        out->writeDecAsText(lsb);
        out->writeText("\"/>\n");
    }
    if (glyfInfo) {
        for (int i = 0; i < fGlyphCount; ++i) {
            for (int j = 0; j < (*glyfInfo)[i].fLayers.count(); ++j) {
                out->writeText("    <mtx name=\"glyf");
                    out->writeHexAsText(i, 4);
                    out->writeText("l");
                    out->writeHexAsText(j, 4);
                    out->writeText("\" width=\"");
                    out->writeDecAsText(fGlyphs[i].fAdvance);
                    out->writeText("\" lsb=\"");
                    int32_t lsb = fGlyphs[i].fOrigin.fX + (*glyfInfo)[i].fLayers[j].fBounds.fLeft;
                    out->writeDecAsText(lsb);
                    out->writeText("\"/>\n");
            }
        }
    }
    out->writeText("  </hmtx>\n");

    bool hasNonBMP = false;
    out->writeText("  <cmap>\n");
    out->writeText("    <tableVersion version=\"0\"/>\n");
    out->writeText("    <cmap_format_4 platformID=\"3\" platEncID=\"1\" language=\"0\">\n");
    fCMap.foreach([&out, &hasNonBMP](const SkUnichar& c, const SkGlyphID& g) {
        if (0xFFFF < c) {
            hasNonBMP = true;
            return;
        }
        out->writeText("      <map code=\"0x");
        out->writeHexAsText(c, 4);
        out->writeText("\" name=\"glyf");
        out->writeHexAsText(g, 4);
        out->writeText("\"/>\n");
    });
    out->writeText("    </cmap_format_4>\n");
    if (hasNonBMP) {
        out->writeText("    <cmap_format_12 platformID=\"3\" platEncID=\"10\" format=\"12\" reserved=\"0\" length=\"1\" language=\"0\" nGroups=\"0\">\n");
        fCMap.foreach([&out](const SkUnichar& c, const SkGlyphID& g) {
            out->writeText("      <map code=\"0x");
            out->writeHexAsText(c, 6);
            out->writeText("\" name=\"glyf");
            out->writeHexAsText(g, 4);
            out->writeText("\"/>\n");
        });
        out->writeText("    </cmap_format_12>\n");
    }
    out->writeText("  </cmap>\n");

    out->writeText("  <name>\n");
    out->writeText("    <namerecord nameID=\"1\" platformID=\"3\" platEncID=\"1\" langID=\"0x409\">\n");
    out->writeText("      ");
      out->writeText(fName.c_str());
      out->writeText(" ");
      out->writeText(type);
      out->writeText("\n");
    out->writeText("    </namerecord>\n");
    out->writeText("    <namerecord nameID=\"2\" platformID=\"3\" platEncID=\"1\" langID=\"0x409\">\n");
    out->writeText("      Regular\n");
    out->writeText("    </namerecord>\n");
    out->writeText("  </name>\n");

    out->writeText("  <post>\n");
    out->writeText("    <formatType value=\"3.0\"/>\n");
    out->writeText("    <italicAngle value=\"0.0\"/>\n");
    out->writeText("    <underlinePosition value=\"");
        out->writeScalarAsText(fFontMetrics.fUnderlinePosition);
        out->writeText("\"/>\n");
    out->writeText("    <underlineThickness value=\"");
        out->writeScalarAsText(fFontMetrics.fUnderlineThickness);
        out->writeText("\"/>\n");
    out->writeText("    <isFixedPitch value=\"0\"/>\n");
    out->writeText("    <minMemType42 value=\"0\"/>\n");
    out->writeText("    <maxMemType42 value=\"0\"/>\n");
    out->writeText("    <minMemType1 value=\"0\"/>\n");
    out->writeText("    <maxMemType1 value=\"0\"/>\n");
    out->writeText("  </post>\n");
}

void SkTestSVGTypeface::exportTtxCbdt(SkWStream* out) const {
    out->writeText("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    out->writeText("<ttFont sfntVersion=\"\\x00\\x01\\x00\\x00\" ttLibVersion=\"3.19\">\n");
    this->exportTtxCommon(out, "CBDT");

    int strikeSizes[3] = { 16, 64, 128 };

    SkPaint paint;
    paint.setTypeface(sk_ref_sp(const_cast<SkTestSVGTypeface*>(this)));
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

    out->writeText("  <CBDT>\n");
    out->writeText("    <header version=\"2.0\"/>\n");
    for (size_t strikeIndex = 0; strikeIndex < SK_ARRAY_COUNT(strikeSizes); ++strikeIndex) {
        paint.setTextSize(strikeSizes[strikeIndex]);
        out->writeText("    <strikedata index=\"");
            out->writeDecAsText(strikeIndex);
            out->writeText("\">\n");
        for (int i = 0; i < fGlyphCount; ++i) {
            SkGlyphID gid = i;
            SkScalar advance;
            SkRect bounds;
            paint.getTextWidths(&gid, sizeof(gid), &advance, &bounds);
            SkIRect ibounds = bounds.roundOut();
            if (ibounds.isEmpty()) {
                continue;
            }
            SkImageInfo image_info = SkImageInfo::MakeN32Premul(ibounds.width(), ibounds.height());
            sk_sp<SkSurface> surface(SkSurface::MakeRaster(image_info));
            SkASSERT(surface);
            SkCanvas* canvas = surface->getCanvas();
            canvas->clear(0);
            SkPixmap pix;
            surface->peekPixels(&pix);
            canvas->drawText(&gid, sizeof(gid), -bounds.fLeft, -bounds.fTop, paint);
            canvas->flush();
            sk_sp<SkImage> image = surface->makeImageSnapshot();
            sk_sp<SkData> data = image->encodeToData(SkEncodedImageFormat::kPNG, 100);

            out->writeText("      <cbdt_bitmap_format_17 name=\"glyf");
                out->writeHexAsText(i, 4);
                out->writeText("\">\n");
            out->writeText("        <SmallGlyphMetrics>\n");
            out->writeText("          <height value=\"");
                out->writeDecAsText(image->height());
                out->writeText("\"/>\n");
            out->writeText("          <width value=\"");
                out->writeDecAsText(image->width());
                out->writeText("\"/>\n");
            out->writeText("          <BearingX value=\"");
                out->writeDecAsText(bounds.fLeft);
                out->writeText("\"/>\n");
            out->writeText("          <BearingY value=\"");
                out->writeScalarAsText(-bounds.fTop);
                out->writeText("\"/>\n");
            out->writeText("          <Advance value=\"");
                out->writeScalarAsText(advance);
                out->writeText("\"/>\n");
            out->writeText("        </SmallGlyphMetrics>\n");
            out->writeText("        <rawimagedata>");
            uint8_t const * bytes = data->bytes();
            for (size_t i = 0; i < data->size(); ++i) {
                if ((i % 0x10) == 0x0) {
                    out->writeText("\n          ");
                } else if (((i - 1) % 0x4) == 0x3) {
                    out->writeText(" ");
                }
                out->writeHexAsText(bytes[i], 2);
            }
            out->writeText("\n");
            out->writeText("        </rawimagedata>\n");
            out->writeText("      </cbdt_bitmap_format_17>\n");
        }
        out->writeText("    </strikedata>\n");
    }
    out->writeText("  </CBDT>\n");

    SkFontMetrics fm;
    out->writeText("  <CBLC>\n");
    out->writeText("    <header version=\"2.0\"/>\n");
    for (size_t strikeIndex = 0; strikeIndex < SK_ARRAY_COUNT(strikeSizes); ++strikeIndex) {
        paint.setTextSize(strikeSizes[strikeIndex]);
        paint.getFontMetrics(&fm);
        out->writeText("    <strike index=\"");
            out->writeDecAsText(strikeIndex);
            out->writeText("\">\n");
        out->writeText("      <bitmapSizeTable>\n");
        out->writeText("        <sbitLineMetrics direction=\"hori\">\n");
        out->writeText("          <ascender value=\"");
            out->writeScalarAsText(-fm.fTop);
            out->writeText("\"/>\n");
        out->writeText("          <descender value=\"");
            out->writeScalarAsText(-fm.fBottom);
            out->writeText("\"/>\n");
        out->writeText("          <widthMax value=\"");
            out->writeScalarAsText(fm.fXMax - fm.fXMin);
            out->writeText("\"/>\n");
        out->writeText("          <caretSlopeNumerator value=\"0\"/>\n");
        out->writeText("          <caretSlopeDenominator value=\"0\"/>\n");
        out->writeText("          <caretOffset value=\"0\"/>\n");
        out->writeText("          <minOriginSB value=\"0\"/>\n");
        out->writeText("          <minAdvanceSB value=\"0\"/>\n");
        out->writeText("          <maxBeforeBL value=\"0\"/>\n");
        out->writeText("          <minAfterBL value=\"0\"/>\n");
        out->writeText("          <pad1 value=\"0\"/>\n");
        out->writeText("          <pad2 value=\"0\"/>\n");
        out->writeText("        </sbitLineMetrics>\n");
        out->writeText("        <sbitLineMetrics direction=\"vert\">\n");
        out->writeText("          <ascender value=\"");
            out->writeScalarAsText(-fm.fTop);
            out->writeText("\"/>\n");
        out->writeText("          <descender value=\"");
            out->writeScalarAsText(-fm.fBottom);
            out->writeText("\"/>\n");
        out->writeText("          <widthMax value=\"");
            out->writeScalarAsText(fm.fXMax - fm.fXMin);
            out->writeText("\"/>\n");
        out->writeText("          <caretSlopeNumerator value=\"0\"/>\n");
        out->writeText("          <caretSlopeDenominator value=\"0\"/>\n");
        out->writeText("          <caretOffset value=\"0\"/>\n");
        out->writeText("          <minOriginSB value=\"0\"/>\n");
        out->writeText("          <minAdvanceSB value=\"0\"/>\n");
        out->writeText("          <maxBeforeBL value=\"0\"/>\n");
        out->writeText("          <minAfterBL value=\"0\"/>\n");
        out->writeText("          <pad1 value=\"0\"/>\n");
        out->writeText("          <pad2 value=\"0\"/>\n");
        out->writeText("        </sbitLineMetrics>\n");
        out->writeText("        <colorRef value=\"0\"/>\n");
        out->writeText("        <startGlyphIndex value=\"1\"/>\n");
        out->writeText("        <endGlyphIndex value=\"1\"/>\n");
        out->writeText("        <ppemX value=\"");
            out->writeDecAsText(strikeSizes[strikeIndex]);
            out->writeText("\"/>\n");
        out->writeText("        <ppemY value=\"");
            out->writeDecAsText(strikeSizes[strikeIndex]);
            out->writeText("\"/>\n");
        out->writeText("        <bitDepth value=\"32\"/>\n");
        out->writeText("        <flags value=\"1\"/>\n");
        out->writeText("      </bitmapSizeTable>\n");
        out->writeText("      <eblc_index_sub_table_1 imageFormat=\"17\" firstGlyphIndex=\"1\" lastGlyphIndex=\"1\">\n");
        for (int i = 0; i < fGlyphCount; ++i) {
            SkGlyphID gid = i;
            SkRect bounds;
            paint.getTextWidths(&gid, sizeof(gid), nullptr, &bounds);
            if (bounds.isEmpty()) {
                continue;
            }
            out->writeText("        <glyphLoc name=\"glyf");
                out->writeHexAsText(i, 4);
                out->writeText("\"/>\n");
        }
        out->writeText("      </eblc_index_sub_table_1>\n");
        out->writeText("    </strike>\n");
    }
    out->writeText("  </CBLC>\n");

    out->writeText("</ttFont>\n");
}

/**
 * UnitsPerEm is generally 1000 here. Versions of macOS older than 10.13
 * have problems in CoreText determining the glyph bounds of bitmap glyphs
 * with unitsPerEm set to 1024 or numbers not divisible by 100 when the
 * contour is not closed. The bounds of sbix fonts on macOS appear to be those
 * of the outline in the 'glyf' table. If this countour is closed it will be
 * drawn, as the 'glyf' outline is to be drawn on top of any bitmap. (There is
 * a bit which is supposed to control this, but it cannot be relied on.) So
 * make the glyph contour a degenerate line with points at the edge of the
 * bounding box of the glyph.
 */
void SkTestSVGTypeface::exportTtxSbix(SkWStream* out) const {
    out->writeText("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    out->writeText("<ttFont sfntVersion=\"\\x00\\x01\\x00\\x00\" ttLibVersion=\"3.19\">\n");
    this->exportTtxCommon(out, "sbix");

    SkPaint paint;
    paint.setTypeface(sk_ref_sp(const_cast<SkTestSVGTypeface*>(this)));
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

    out->writeText("  <glyf>\n");
    for (int i = 0; i < fGlyphCount; ++i) {
        const SkTestSVGTypeface::Glyph& glyphData = this->fGlyphs[i];

        SkSize containerSize = glyphData.fSvg ? glyphData.fSvg->containerSize()
                                              : SkSize::MakeEmpty();
        SkRect bounds = SkRect::MakeXYWH(glyphData.fOrigin.fX, -glyphData.fOrigin.fY,
                                         containerSize.fWidth, containerSize.fHeight);
        SkIRect ibounds = bounds.roundOut();
        out->writeText("    <TTGlyph name=\"glyf");
            out->writeHexAsText(i, 4);
            out->writeText("\" xMin=\"");
            out->writeDecAsText(ibounds.fLeft);
            out->writeText("\" yMin=\"");
            out->writeDecAsText(-ibounds.fBottom);
            out->writeText("\" xMax=\"");
            out->writeDecAsText(ibounds.fRight);
            out->writeText("\" yMax=\"");
            out->writeDecAsText(-ibounds.fTop);
            out->writeText("\">\n");
        out->writeText("      <contour>\n");
        out->writeText("        <pt x=\"");
            out->writeDecAsText(ibounds.fLeft);
            out->writeText("\" y=\"");
            out->writeDecAsText(-ibounds.fBottom);
            out->writeText("\" on=\"1\"/>\n");
        out->writeText("      </contour>\n");
        out->writeText("      <contour>\n");
        out->writeText("        <pt x=\"");
            out->writeDecAsText(ibounds.fRight);
            out->writeText("\" y=\"");
            out->writeDecAsText(-ibounds.fTop);
            out->writeText("\" on=\"1\"/>\n");
        out->writeText("      </contour>\n");
        out->writeText("      <instructions/>\n");
        out->writeText("    </TTGlyph>\n");
    }
    out->writeText("  </glyf>\n");

    // The loca table will be re-calculated, but if we don't write one we don't get one.
    out->writeText("  <loca/>\n");

    int strikeSizes[3] = { 16, 64, 128 };

    out->writeText("  <sbix>\n");
    out->writeText("    <version value=\"1\"/>\n");
    out->writeText("    <flags value=\"00000000 00000001\"/>\n");
    for (size_t strikeIndex = 0; strikeIndex < SK_ARRAY_COUNT(strikeSizes); ++strikeIndex) {
        paint.setTextSize(strikeSizes[strikeIndex]);
        out->writeText("    <strike>\n");
        out->writeText("      <ppem value=\"");
            out->writeDecAsText(strikeSizes[strikeIndex]);
            out->writeText("\"/>\n");
        out->writeText("      <resolution value=\"72\"/>\n");
        for (int i = 0; i < fGlyphCount; ++i) {
            SkGlyphID gid = i;
            SkScalar advance;
            SkRect bounds;
            paint.getTextWidths(&gid, sizeof(gid), &advance, &bounds);
            SkIRect ibounds = bounds.roundOut();
            if (ibounds.isEmpty()) {
                continue;
            }
            SkImageInfo image_info = SkImageInfo::MakeN32Premul(ibounds.width(), ibounds.height());
            sk_sp<SkSurface> surface(SkSurface::MakeRaster(image_info));
            SkASSERT(surface);
            SkCanvas* canvas = surface->getCanvas();
            canvas->clear(0);
            SkPixmap pix;
            surface->peekPixels(&pix);
            canvas->drawText(&gid, sizeof(gid), -bounds.fLeft, -bounds.fTop, paint);
            canvas->flush();
            sk_sp<SkImage> image = surface->makeImageSnapshot();
            sk_sp<SkData> data = image->encodeToData(SkEncodedImageFormat::kPNG, 100);

            out->writeText("      <glyph name=\"glyf");
                out->writeHexAsText(i, 4);
                out->writeText("\" graphicType=\"png \" originOffsetX=\"");
                out->writeDecAsText(bounds.fLeft);
                out->writeText("\" originOffsetY=\"");
                out->writeScalarAsText(bounds.fBottom);
                out->writeText("\">\n");

            out->writeText("        <hexdata>");
            uint8_t const * bytes = data->bytes();
            for (size_t i = 0; i < data->size(); ++i) {
                if ((i % 0x10) == 0x0) {
                    out->writeText("\n          ");
                } else if (((i - 1) % 0x4) == 0x3) {
                    out->writeText(" ");
                }
                out->writeHexAsText(bytes[i], 2);
            }
            out->writeText("\n");
            out->writeText("        </hexdata>\n");
            out->writeText("      </glyph>\n");
        }
        out->writeText("    </strike>\n");
    }
    out->writeText("  </sbix>\n");
    out->writeText("</ttFont>\n");
}

namespace {

void convert_noninflect_cubic_to_quads(const SkPoint p[4],
                                       SkScalar toleranceSqd,
                                       SkTArray<SkPoint, true>* quads,
                                       int sublevel = 0)
{
    // Notation: Point a is always p[0]. Point b is p[1] unless p[1] == p[0], in which case it is
    // p[2]. Point d is always p[3]. Point c is p[2] unless p[2] == p[3], in which case it is p[1].

    SkVector ab = p[1] - p[0];
    SkVector dc = p[2] - p[3];

    if (SkPointPriv::LengthSqd(ab) < SK_ScalarNearlyZero) {
        if (SkPointPriv::LengthSqd(dc) < SK_ScalarNearlyZero) {
            SkPoint* degQuad = quads->push_back_n(3);
            degQuad[0] = p[0];
            degQuad[1] = p[0];
            degQuad[2] = p[3];
            return;
        }
        ab = p[2] - p[0];
    }
    if (SkPointPriv::LengthSqd(dc) < SK_ScalarNearlyZero) {
        dc = p[1] - p[3];
    }

    static const SkScalar kLengthScale = 3 * SK_Scalar1 / 2;
    static const int kMaxSubdivs = 10;

    ab.scale(kLengthScale);
    dc.scale(kLengthScale);

    // e0 and e1 are extrapolations along vectors ab and dc.
    SkVector c0 = p[0];
    c0 += ab;
    SkVector c1 = p[3];
    c1 += dc;

    SkScalar dSqd = sublevel > kMaxSubdivs ? 0 : SkPointPriv::DistanceToSqd(c0, c1);
    if (dSqd < toleranceSqd) {
        SkPoint cAvg = c0;
        cAvg += c1;
        cAvg.scale(SK_ScalarHalf);

        SkPoint* pts = quads->push_back_n(3);
        pts[0] = p[0];
        pts[1] = cAvg;
        pts[2] = p[3];
        return;
    }
    SkPoint choppedPts[7];
    SkChopCubicAtHalf(p, choppedPts);
    convert_noninflect_cubic_to_quads(choppedPts + 0, toleranceSqd, quads, sublevel + 1);
    convert_noninflect_cubic_to_quads(choppedPts + 3, toleranceSqd, quads, sublevel + 1);
}

void convertCubicToQuads(const SkPoint p[4], SkScalar tolScale, SkTArray<SkPoint, true>* quads) {
    if (!p[0].isFinite() || !p[1].isFinite() || !p[2].isFinite() || !p[3].isFinite()) {
        return;
    }
    SkPoint chopped[10];
    int count = SkChopCubicAtInflections(p, chopped);

    const SkScalar tolSqd = SkScalarSquare(tolScale);

    for (int i = 0; i < count; ++i) {
        SkPoint* cubic = chopped + 3*i;
        convert_noninflect_cubic_to_quads(cubic, tolSqd, quads);
    }
}

void path_to_quads(const SkPath& path, SkPath* quadPath) {
    quadPath->reset();
    SkTArray<SkPoint, true> qPts;
    SkAutoConicToQuads converter;
    const SkPoint* quadPts;
    SkPath::RawIter iter(path);
    uint8_t verb;
    SkPoint pts[4];
    while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {
        switch (verb) {
            case SkPath::kMove_Verb:
                quadPath->moveTo(pts[0].fX, pts[0].fY);
                break;
            case SkPath::kLine_Verb:
                quadPath->lineTo(pts[1].fX, pts[1].fY);
                break;
            case SkPath::kQuad_Verb:
                quadPath->quadTo(pts[1].fX, pts[1].fY, pts[2].fX, pts[2].fY);
                break;
            case SkPath::kCubic_Verb:
                qPts.reset();
                convertCubicToQuads(pts, SK_Scalar1, &qPts);
                for (int i = 0; i < qPts.count(); i += 3) {
                    quadPath->quadTo(qPts[i+1].fX, qPts[i+1].fY, qPts[i+2].fX, qPts[i+2].fY);
                }
                break;
            case SkPath::kConic_Verb:
                quadPts = converter.computeQuads(pts, iter.conicWeight(), SK_Scalar1);
                for (int i = 0; i < converter.countQuads(); ++i) {
                    quadPath->quadTo(quadPts[i*2+1].fX, quadPts[i*2+1].fY,
                                     quadPts[i*2+2].fX, quadPts[i*2+2].fY);
                }
                break;
            case SkPath::kClose_Verb:
                 quadPath->close();
                break;
            default:
                SkDEBUGFAIL("bad verb");
                return;
        }
    }
}

class SkCOLRCanvas : public SkNoDrawCanvas {
public:
    SkCOLRCanvas(SkRect glyphBounds, SkGlyphID glyphId,
                 SkTestSVGTypeface::GlyfInfo* glyf, SkTHashMap<SkColor, int>* colors,
                 SkWStream* out)
        : SkNoDrawCanvas(glyphBounds.roundOut().width(), glyphBounds.roundOut().height())
        , fOut(out)
        , fGlyphId(glyphId)
        , fBaselineOffset(glyphBounds.top())
        , fLayerId(0)
        , fGlyf(glyf)
        , fColors(colors)
    { }

    void writePoint(SkScalar x, SkScalar y, bool on) {
        fOut->writeText("        <pt x=\"");
        fOut->writeDecAsText(SkScalarRoundToInt(x));
        fOut->writeText("\" y=\"");
        fOut->writeDecAsText(SkScalarRoundToInt(y));
        fOut->writeText("\" on=\"");
        fOut->write8(on ? '1' : '0');
        fOut->writeText("\"/>\n");
    }
    SkIRect writePath(const SkPath& path, bool layer) {
        // Convert to quads.
        SkPath quads;
        path_to_quads(path, &quads);

        SkRect bounds = quads.computeTightBounds();
        SkIRect ibounds = bounds.roundOut();
        // The bounds will be re-calculated anyway.
        fOut->writeText("    <TTGlyph name=\"glyf");
            fOut->writeHexAsText(fGlyphId, 4);
            if (layer) {
                fOut->writeText("l");
                fOut->writeHexAsText(fLayerId, 4);
            }
            fOut->writeText("\" xMin=\"");
            fOut->writeDecAsText(ibounds.fLeft);
            fOut->writeText("\" yMin=\"");
            fOut->writeDecAsText(ibounds.fTop);
            fOut->writeText("\" xMax=\"");
            fOut->writeDecAsText(ibounds.fRight);
            fOut->writeText("\" yMax=\"");
            fOut->writeDecAsText(ibounds.fBottom);
            fOut->writeText("\">\n");

        SkPath::RawIter iter(quads);
        uint8_t verb;
        SkPoint pts[4];
        bool contourOpen = false;
        while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {
            switch (verb) {
                case SkPath::kMove_Verb:
                    if (contourOpen) {
                        fOut->writeText("      </contour>\n");
                        contourOpen = false;
                    }
                    break;
                case SkPath::kLine_Verb:
                    if (!contourOpen) {
                        fOut->writeText("      <contour>\n");
                        this->writePoint(pts[0].fX, pts[0].fY, true);
                        contourOpen = true;
                    }
                    this->writePoint(pts[1].fX, pts[1].fY, true);
                    break;
                case SkPath::kQuad_Verb:
                    if (!contourOpen) {
                        fOut->writeText("      <contour>\n");
                        this->writePoint(pts[0].fX, pts[0].fY, true);
                        contourOpen = true;
                    }
                    this->writePoint(pts[1].fX, pts[1].fY, false);
                    this->writePoint(pts[2].fX, pts[2].fY, true);
                    break;
                case SkPath::kClose_Verb:
                    if (contourOpen) {
                        fOut->writeText("      </contour>\n");
                        contourOpen = false;
                    }
                    break;
                default:
                    SkDEBUGFAIL("bad verb");
                    return ibounds;
            }
        }
        if (contourOpen) {
            fOut->writeText("      </contour>\n");
        }

        // Required to write out an instructions tag.
        fOut->writeText("      <instructions/>\n");
        fOut->writeText("    </TTGlyph>\n");
        return ibounds;
    }

    void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
        SkPath path;
        path.addRect(rect);
        this->drawPath(path, paint);
    }

    void onDrawOval(const SkRect& oval, const SkPaint& paint) override {
        SkPath path;
        path.addOval(oval);
        this->drawPath(path, paint);
    }

    void onDrawArc(const SkRect& oval, SkScalar startAngle, SkScalar sweepAngle, bool useCenter,
                   const SkPaint& paint) override
    {
        SkPath path;
        bool fillNoPathEffect = SkPaint::kFill_Style == paint.getStyle() && !paint.getPathEffect();
        SkPathPriv::CreateDrawArcPath(&path, oval, startAngle, sweepAngle, useCenter,
                                    fillNoPathEffect);
        this->drawPath(path, paint);
    }

    void onDrawRRect(const SkRRect& rrect, const SkPaint& paint) override {
        SkPath path;
        path.addRRect(rrect);
        this->drawPath(path, paint);
    }

    void onDrawPath(const SkPath& platonicPath, const SkPaint& originalPaint) override {
        SkPaint paint = originalPaint;
        SkPath path = platonicPath;

        // Apply the path effect.
        if (paint.getPathEffect() || paint.getStyle() != SkPaint::kFill_Style) {
            bool fill = paint.getFillPath(path, &path);

            paint.setPathEffect(nullptr);
            if (fill) {
                paint.setStyle(SkPaint::kFill_Style);
            } else {
                paint.setStyle(SkPaint::kStroke_Style);
                paint.setStrokeWidth(0);
            }
        }

        // Apply the matrix.
        SkMatrix m = this->getTotalMatrix();
        // If done to the canvas then everything would get clipped out.
        m.postTranslate(0, fBaselineOffset); // put the baseline at 0
        m.postScale(1, -1); // and flip it since OpenType is y-up.
        path.transform(m);

        // While creating the default glyf, union with dark colors and intersect with bright colors.
        SkColor color = paint.getColor();
        if ((SkColorGetR(color) + SkColorGetG(color) + SkColorGetB(color)) / 3 > 0x20) {
            fBasePath.add(path, SkPathOp::kDifference_SkPathOp);
        } else {
            fBasePath.add(path, SkPathOp::kUnion_SkPathOp);
        }

        SkIRect bounds = this->writePath(path, true);

        // The CPAL table has the concept of a 'current color' which is index 0xFFFF.
        // Mark any layer drawn in 'currentColor' as having this special index.
        // The value of 'currentColor' here should a color which causes this layer to union into the
        // default glyf.
        constexpr SkColor currentColor = 0xFF2B0000;

        int colorIndex;
        if (color == currentColor) {
            colorIndex = 0xFFFF;
        } else {
            int* colorIndexPtr = fColors->find(color);
            if (colorIndexPtr) {
                colorIndex = *colorIndexPtr;
            } else {
                colorIndex = fColors->count();
                fColors->set(color, colorIndex);
            }
        }
        fGlyf->fLayers.emplace_back(colorIndex, bounds);

        ++fLayerId;
    }

    void finishGlyph() {
        SkPath baseGlyph;
        fBasePath.resolve(&baseGlyph);
        fGlyf->fBounds = this->writePath(baseGlyph, false);
    }

private:
    SkWStream * const fOut;
    SkGlyphID fGlyphId;
    SkScalar fBaselineOffset;
    int fLayerId;
    SkOpBuilder fBasePath;
    SkTestSVGTypeface::GlyfInfo* fGlyf;
    SkTHashMap<SkColor, int>* fColors;
};

} // namespace

void SkTestSVGTypeface::exportTtxColr(SkWStream* out) const {
    out->writeText("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    out->writeText("<ttFont sfntVersion=\"\\x00\\x01\\x00\\x00\" ttLibVersion=\"3.19\">\n");

    SkTHashMap<SkColor, int> colors;
    SkTArray<GlyfInfo> glyfInfos(fGlyphCount);

    // Need to know all the glyphs up front for the common tables.
    SkDynamicMemoryWStream glyfOut;
    glyfOut.writeText("  <glyf>\n");
    for (int i = 0; i < fGlyphCount; ++i) {
        const SkTestSVGTypeface::Glyph& glyphData = this->fGlyphs[i];

        SkSize containerSize = glyphData.fSvg ? glyphData.fSvg->containerSize()
                                              : SkSize::MakeEmpty();
        SkRect bounds = SkRect::MakeXYWH(glyphData.fOrigin.fX, -glyphData.fOrigin.fY,
                                         containerSize.fWidth, containerSize.fHeight);
        SkCOLRCanvas canvas(bounds, i, &glyfInfos.emplace_back(), &colors, &glyfOut);
        if (glyphData.fSvg) {
            glyphData.fSvg->render(&canvas);
        }
        canvas.finishGlyph();
    }
    glyfOut.writeText("  </glyf>\n");

    this->exportTtxCommon(out, "COLR", &glyfInfos);

    // The loca table will be re-calculated, but if we don't write one we don't get one.
    out->writeText("  <loca/>\n");

    std::unique_ptr<SkStreamAsset> glyfStream = glyfOut.detachAsStream();
    out->writeStream(glyfStream.get(), glyfStream->getLength());

    out->writeText("  <COLR>\n");
    out->writeText("    <version value=\"0\"/>\n");
    for (int i = 0; i < fGlyphCount; ++i) {
        if (glyfInfos[i].fBounds.isEmpty() || glyfInfos[i].fLayers.empty()) {
            continue;
        }
        out->writeText("    <ColorGlyph name=\"glyf");
            out->writeHexAsText(i, 4);
            out->writeText("\">\n");
        for (int j = 0; j < glyfInfos[i].fLayers.count(); ++j) {
            const int colorIndex = glyfInfos[i].fLayers[j].fLayerColorIndex;
            out->writeText("      <layer colorID=\"");
                out->writeDecAsText(colorIndex);
                out->writeText("\" name=\"glyf");
                out->writeHexAsText(i, 4);
                out->writeText("l");
                out->writeHexAsText(j, 4);
                out->writeText("\"/>\n");
        }
        out->writeText("    </ColorGlyph>\n");
    }
    out->writeText("  </COLR>\n");

    // The colors must be written in order, the 'index' is ignored by ttx.
    SkAutoTMalloc<SkColor> colorsInOrder(colors.count());
    colors.foreach([&colorsInOrder](const SkColor& c, const int* i) {
        colorsInOrder[*i] = c;
    });
    out->writeText("  <CPAL>\n");
    out->writeText("    <version value=\"0\"/>\n");
    out->writeText("    <numPaletteEntries value=\"");
        out->writeDecAsText(colors.count());
        out->writeText("\"/>\n");
    out->writeText("    <palette index=\"0\">\n");
    for (int i = 0; i < colors.count(); ++i) {
        SkColor c = colorsInOrder[i];
        out->writeText("      <color index=\"");
            out->writeDecAsText(i);
            out->writeText("\" value=\"#");
            out->writeHexAsText(SkColorGetR(c), 2);
            out->writeHexAsText(SkColorGetG(c), 2);
            out->writeHexAsText(SkColorGetB(c), 2);
            out->writeHexAsText(SkColorGetA(c), 2);
            out->writeText("\"/>\n");
    }
    out->writeText("    </palette>\n");
    out->writeText("  </CPAL>\n");

    out->writeText("</ttFont>\n");
}
#endif  // SK_XML
