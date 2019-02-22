/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "sk_tool_utils.h"

#include "SkColorPriv.h"
#include "SkImageGenerator.h"
#include "SkPath.h"
#include "SkYUVAIndex.h"

#if SK_SUPPORT_GPU
#include "GrBackendSurface.h"
#include "GrContextPriv.h"
#include "GrGpu.h"
#endif

static const int kTileWidthHeight = 128;
static const int kLabelWidth = 64;
static const int kLabelHeight = 32;
static const int kPad = 1;

enum YUVFormat {
    // 4:4:4 formats, 32 bpp
    AYUV_YUVFormat,  // 8-bit YUVA values all interleaved

    // 4:2:0 formats, 12 bpp
    kNV12_YUVFormat, // 8-bit Y plane + 2x2 down sampled interleaved U/V planes
    kNV21_YUVFormat, // same as kNV12 but w/ U/V reversed in the interleaved plane

    kI420_YUVFormat, // 8-bit Y plane + 2x2 down sampled U and V planes
    kYV12_YUVFormat, // 8-bit Y plane + 2x2 down sampled V and U planes

    kLast_YUVFormat = kYV12_YUVFormat
};

// All the planes we need to construct the various YUV formats
struct PlaneData {
   SkBitmap fYFull;
   SkBitmap fUFull;
   SkBitmap fVFull;
   SkBitmap fAFull;
   SkBitmap fUQuarter; // 2x2 downsampled U channel
   SkBitmap fVQuarter; // 2x2 downsampled V channel
};

// Add a portion of a circle to 'path'. The points 'o1' and 'o2' are on the border of the circle
// and have tangents 'v1' and 'v2'.
static void add_arc(SkPath* path,
                    const SkPoint& o1, const SkVector& v1,
                    const SkPoint& o2, const SkVector& v2,
                    SkTDArray<SkRect>* circles, bool takeLongWayRound) {

    SkVector v3 = { -v1.fY, v1.fX };
    SkVector v4 = { v2.fY, -v2.fX };

    SkScalar t = ((o2.fX - o1.fX) * v4.fY - (o2.fY - o1.fY) * v4.fX) / v3.cross(v4);
    SkPoint center = { o1.fX + t * v3.fX, o1.fY + t * v3.fY };

    SkRect r = { center.fX - t, center.fY - t, center.fX + t, center.fY + t };

    if (circles) {
        circles->push_back(r);
    }

    SkVector startV = o1 - center, endV = o2 - center;
    startV.normalize();
    endV.normalize();

    SkScalar startDeg = SkRadiansToDegrees(SkScalarATan2(startV.fY, startV.fX));
    SkScalar endDeg = SkRadiansToDegrees(SkScalarATan2(endV.fY, endV.fX));

    startDeg += 360.0f;
    startDeg = fmodf(startDeg, 360.0f);

    endDeg += 360.0f;
    endDeg = fmodf(endDeg, 360.0f);

    if (endDeg < startDeg) {
        endDeg += 360.0f;
    }

    SkScalar sweepDeg = SkTAbs(endDeg - startDeg);
    if (!takeLongWayRound) {
        sweepDeg = sweepDeg - 360;
    }

    path->arcTo(r, startDeg, sweepDeg, false);
}

static SkPath create_splat(const SkPoint& o, SkScalar innerRadius, SkScalar outerRadius,
                           SkScalar ratio, int numLobes, SkTDArray<SkRect>* circles) {
    if (numLobes <= 1) {
        return SkPath();
    }

    SkPath p;

    int numDivisions = 2 * numLobes;
    SkScalar fullLobeDegrees = 360.0f / numLobes;
    SkScalar outDegrees = ratio * fullLobeDegrees / (ratio + 1.0f);
    SkScalar innerDegrees = fullLobeDegrees / (ratio + 1.0f);
    SkMatrix outerStep, innerStep;
    outerStep.setRotate(outDegrees);
    innerStep.setRotate(innerDegrees);
    SkVector curV = SkVector::Make(0.0f, 1.0f);

    if (circles) {
        circles->push_back(SkRect::MakeLTRB(o.fX - innerRadius, o.fY - innerRadius,
                                            o.fX + innerRadius, o.fY + innerRadius));
    }

    p.moveTo(o.fX + innerRadius * curV.fX, o.fY + innerRadius * curV.fY);

    for (int i = 0; i < numDivisions; ++i) {

        SkVector nextV;
        if (0 == (i % 2)) {
            nextV = outerStep.mapVector(curV.fX, curV.fY);

            SkPoint top = SkPoint::Make(o.fX + outerRadius * curV.fX,
                                        o.fY + outerRadius * curV.fY);
            SkPoint nextTop = SkPoint::Make(o.fX + outerRadius * nextV.fX,
                                            o.fY + outerRadius * nextV.fY);

            p.lineTo(top);
            add_arc(&p, top, curV, nextTop, nextV, circles, true);
        } else {
            nextV = innerStep.mapVector(curV.fX, curV.fY);

            SkPoint bot = SkPoint::Make(o.fX + innerRadius * curV.fX,
                                        o.fY + innerRadius * curV.fY);
            SkPoint nextBot = SkPoint::Make(o.fX + innerRadius * nextV.fX,
                                            o.fY + innerRadius * nextV.fY);

            p.lineTo(bot);
            add_arc(&p, bot, curV, nextBot, nextV, nullptr, false);
        }

        curV = nextV;
    }

    p.close();

    return p;
}

static SkBitmap make_bitmap(const SkPath& path, const SkTDArray<SkRect>& circles, bool opaque) {
    const SkColor kGreen  = sk_tool_utils::color_to_565(SkColorSetARGB(0xFF, 178, 240, 104));
    const SkColor kBlue   = sk_tool_utils::color_to_565(SkColorSetARGB(0xFF, 173, 167, 252));
    const SkColor kYellow = sk_tool_utils::color_to_565(SkColorSetARGB(0xFF, 255, 221, 117));

    SkImageInfo ii = SkImageInfo::MakeN32(kTileWidthHeight, kTileWidthHeight, kPremul_SkAlphaType);

    SkBitmap bm;
    bm.allocPixels(ii);

    std::unique_ptr<SkCanvas> canvas = SkCanvas::MakeRasterDirectN32(ii.width(), ii.height(),
                                                                     (SkPMColor*)bm.getPixels(),
                                                                     bm.rowBytes());

    canvas->clear(opaque ? kGreen : SK_ColorTRANSPARENT);

    SkPaint paint;
    paint.setAntiAlias(false); // serialize-8888 doesn't seem to work well w/ partial transparency
    paint.setColor(kBlue);

    canvas->drawPath(path, paint);

    paint.setColor(opaque ? kYellow : SK_ColorTRANSPARENT);
    paint.setBlendMode(SkBlendMode::kSrc);
    for (int i = 0; i < circles.count(); ++i) {
        SkRect r = circles[i];
        r.inset(r.width()/4, r.height()/4);
        canvas->drawOval(r, paint);
    }

    return bm;
}

static void convert_rgba_to_yuva_601_shared(SkColor col, uint8_t yuv[4],
                                            uint8_t off, uint8_t range) {
    static const float Kr = 0.299f;
    static const float Kb = 0.114f;
    static const float Kg = 1.0f - Kr - Kb;

    float r = SkColorGetR(col) / 255.0f;
    float g = SkColorGetG(col) / 255.0f;
    float b = SkColorGetB(col) / 255.0f;

    float Ey = Kr * r + Kg * g + Kb * b;
    float Ecb = (b - Ey) / 1.402f;
    float Ecr = (r - Ey) / 1.772;

    yuv[0] = SkScalarRoundToInt( range * Ey + off );
    yuv[1] = SkScalarRoundToInt( 224 * Ecb + 128 );
    yuv[2] = SkScalarRoundToInt( 224 * Ecr + 128 );
    yuv[3] = SkColorGetA(col);
}

static void convert_rgba_to_yuva_jpeg(SkColor col, uint8_t yuv[4]) {
    // full swing from 0..255
    convert_rgba_to_yuva_601_shared(col, yuv, 0, 255);
}

static void convert_rgba_to_yuva_601(SkColor col, uint8_t yuv[4]) {
    // partial swing from 16..235
    convert_rgba_to_yuva_601_shared(col, yuv, 16, 219);

}

static void convert_rgba_to_yuva_709(SkColor col, uint8_t yuv[4]) {
    static const float Kr = 0.2126f;
    static const float Kb = 0.0722f;
    static const float Kg = 1.0f - Kr - Kb;

    float r = SkColorGetR(col) / 255.0f;
    float g = SkColorGetG(col) / 255.0f;
    float b = SkColorGetB(col) / 255.0f;

    float Ey = Kr * r + Kg * g + Kb * b;
    float Ecb = (b - Ey) / 1.8556f;
    float Ecr = (r - Ey) / 1.5748;

    yuv[0] = SkScalarRoundToInt( 219 * Ey +  16 );
    yuv[1] = SkScalarRoundToInt( 224 * Ecb + 128 );
    yuv[2] = SkScalarRoundToInt( 224 * Ecr + 128 );

    yuv[3] = SkColorGetA(col);
}


static SkPMColor convert_yuva_to_rgba_jpeg(uint8_t y, uint8_t u, uint8_t v, uint8_t a) {
    int c = y;
    int d = u - 128;
    int e = v - 128;

    uint8_t r = SkScalarPin(SkScalarRoundToInt( 1.0f * c                   +  1.402f    * e ),
                            0, 255);
    uint8_t g = SkScalarPin(SkScalarRoundToInt( 1.0f * c - (0.344136f * d) - (0.714136f * e)),
                            0, 255);
    uint8_t b = SkScalarPin(SkScalarRoundToInt( 1.0f * c +  1.773f    * d                   ),
                            0, 255);

    return SkPremultiplyARGBInline(a, r, g, b);
}

static SkPMColor convert_yuva_to_rgba_601(uint8_t y, uint8_t u, uint8_t v, uint8_t a) {
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;

    uint8_t r = SkScalarPin(SkScalarRoundToInt( 1.164f * c                +  1.596f * e ), 0, 255);
    uint8_t g = SkScalarPin(SkScalarRoundToInt( 1.164f * c - (0.391f * d) - (0.813f * e)), 0, 255);
    uint8_t b = SkScalarPin(SkScalarRoundToInt( 1.164f * c +  2.018f * d                ), 0, 255);

    return SkPremultiplyARGBInline(a, r, g, b);
}

static SkPMColor convert_yuva_to_rgba_709(uint8_t y, uint8_t u, uint8_t v, uint8_t a) {
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;

    uint8_t r = SkScalarPin(SkScalarRoundToInt( 1.164f * c                +  1.793f * e ), 0, 255);
    uint8_t g = SkScalarPin(SkScalarRoundToInt( 1.164f * c - (0.213f * d) - (0.533f * e)), 0, 255);
    uint8_t b = SkScalarPin(SkScalarRoundToInt( 1.164f * c +  2.112f * d                ), 0, 255);

    return SkPremultiplyARGBInline(a, r, g, b);
}

static void extract_planes(const SkBitmap& bm, SkYUVColorSpace yuvColorSpace, PlaneData* planes) {
    SkASSERT(!(bm.width() % 2));
    SkASSERT(!(bm.height() % 2));

    planes->fYFull.allocPixels(SkImageInfo::MakeA8(bm.width(), bm.height()));
    planes->fUFull.allocPixels(SkImageInfo::MakeA8(bm.width(), bm.height()));
    planes->fVFull.allocPixels(SkImageInfo::MakeA8(bm.width(), bm.height()));
    planes->fAFull.allocPixels(SkImageInfo::MakeA8(bm.width(), bm.height()));
    planes->fUQuarter.allocPixels(SkImageInfo::MakeA8(bm.width()/2, bm.height()/2));
    planes->fVQuarter.allocPixels(SkImageInfo::MakeA8(bm.width()/2, bm.height()/2));

    for (int y = 0; y < bm.height(); ++y) {
        for (int x = 0; x < bm.width(); ++x) {
            SkColor col = bm.getColor(x, y);

            uint8_t yuva[4];

            if (kJPEG_SkYUVColorSpace == yuvColorSpace) {
                convert_rgba_to_yuva_jpeg(col, yuva);
            } else if (kRec601_SkYUVColorSpace == yuvColorSpace) {
                convert_rgba_to_yuva_601(col, yuva);
            } else {
                SkASSERT(kRec709_SkYUVColorSpace == yuvColorSpace);
                convert_rgba_to_yuva_709(col, yuva);
            }

            *planes->fYFull.getAddr8(x, y) = yuva[0];
            *planes->fUFull.getAddr8(x, y) = yuva[1];
            *planes->fVFull.getAddr8(x, y) = yuva[2];
            *planes->fAFull.getAddr8(x, y) = yuva[3];
        }
    }

    for (int y = 0; y < bm.height()/2; ++y) {
        for (int x = 0; x < bm.width()/2; ++x) {
            uint32_t uAccum = 0, vAccum = 0;

            uAccum += *planes->fUFull.getAddr8(2*x, 2*y);
            uAccum += *planes->fUFull.getAddr8(2*x+1, 2*y);
            uAccum += *planes->fUFull.getAddr8(2*x, 2*y+1);
            uAccum += *planes->fUFull.getAddr8(2*x+1, 2*y+1);

            *planes->fUQuarter.getAddr8(x, y) = uAccum / 4.0f;

            vAccum += *planes->fVFull.getAddr8(2*x, 2*y);
            vAccum += *planes->fVFull.getAddr8(2*x+1, 2*y);
            vAccum += *planes->fVFull.getAddr8(2*x, 2*y+1);
            vAccum += *planes->fVFull.getAddr8(2*x+1, 2*y+1);

            *planes->fVQuarter.getAddr8(x, y) = vAccum / 4.0f;
        }
    }
}

// Recombine the separate planes into some YUV format
static void create_YUV(const PlaneData& planes, YUVFormat yuvFormat,
                       SkBitmap resultBMs[], SkYUVAIndex yuvaIndices[4], bool opaque) {
    int nextLayer = 0;

    switch (yuvFormat) {
        case AYUV_YUVFormat: {
            SkBitmap yuvaFull;

            yuvaFull.allocPixels(SkImageInfo::MakeN32(planes.fYFull.width(), planes.fYFull.height(),
                                                      kUnpremul_SkAlphaType));

            for (int y = 0; y < planes.fYFull.height(); ++y) {
                for (int x = 0; x < planes.fYFull.width(); ++x) {

                    uint8_t Y = *planes.fYFull.getAddr8(x, y);
                    uint8_t U = *planes.fUFull.getAddr8(x, y);
                    uint8_t V = *planes.fVFull.getAddr8(x, y);
                    uint8_t A = *planes.fAFull.getAddr8(x, y);

                    // NOT premul!
                    *yuvaFull.getAddr32(x, y) = SkColorSetARGB(A, Y, U, V);
                }
            }

            resultBMs[nextLayer++] = yuvaFull;

            yuvaIndices[0].fIndex = 0;
            yuvaIndices[0].fChannel = SkColorChannel::kR;
            yuvaIndices[1].fIndex = 0;
            yuvaIndices[1].fChannel = SkColorChannel::kG;
            yuvaIndices[2].fIndex = 0;
            yuvaIndices[2].fChannel = SkColorChannel::kB;
            yuvaIndices[3].fIndex = 0;
            yuvaIndices[3].fChannel = SkColorChannel::kA;
            break;
        }
        case kNV12_YUVFormat: {
            SkBitmap uvQuarter;

            // There isn't a RG color type. Approx w/ RGBA.
            uvQuarter.allocPixels(SkImageInfo::MakeN32(planes.fYFull.width()/2,
                                                       planes.fYFull.height()/2,
                                                       kUnpremul_SkAlphaType));

            for (int y = 0; y < planes.fYFull.height()/2; ++y) {
                for (int x = 0; x < planes.fYFull.width()/2; ++x) {
                    uint8_t U = *planes.fUQuarter.getAddr8(x, y);
                    uint8_t V = *planes.fVQuarter.getAddr8(x, y);

                    // NOT premul!
                    *uvQuarter.getAddr32(x, y) = SkColorSetARGB(0, U, V, 0);
                }
            }

            resultBMs[nextLayer++] = planes.fYFull;
            resultBMs[nextLayer++] = uvQuarter;

            yuvaIndices[0].fIndex = 0;
            yuvaIndices[0].fChannel = SkColorChannel::kA;
            yuvaIndices[1].fIndex = 1;
            yuvaIndices[1].fChannel = SkColorChannel::kR;
            yuvaIndices[2].fIndex = 1;
            yuvaIndices[2].fChannel = SkColorChannel::kG;
            break;
        }
        case kNV21_YUVFormat: {
            SkBitmap vuQuarter;

            // There isn't a RG color type. Approx w/ RGBA.
            vuQuarter.allocPixels(SkImageInfo::MakeN32(planes.fYFull.width()/2,
                                                       planes.fYFull.height()/2,
                                                       kUnpremul_SkAlphaType));

            for (int y = 0; y < planes.fYFull.height()/2; ++y) {
                for (int x = 0; x < planes.fYFull.width()/2; ++x) {
                    uint8_t U = *planes.fUQuarter.getAddr8(x, y);
                    uint8_t V = *planes.fVQuarter.getAddr8(x, y);

                    // NOT premul!
                    *vuQuarter.getAddr32(x, y) = SkColorSetARGB(0, V, U, 0);
                }
            }

            resultBMs[nextLayer++] = planes.fYFull;
            resultBMs[nextLayer++] = vuQuarter;

            yuvaIndices[0].fIndex = 0;
            yuvaIndices[0].fChannel = SkColorChannel::kA;
            yuvaIndices[1].fIndex = 1;
            yuvaIndices[1].fChannel = SkColorChannel::kG;
            yuvaIndices[2].fIndex = 1;
            yuvaIndices[2].fChannel = SkColorChannel::kR;
            break;
        }
        case kI420_YUVFormat:
            resultBMs[nextLayer++] = planes.fYFull;
            resultBMs[nextLayer++] = planes.fUQuarter;
            resultBMs[nextLayer++] = planes.fVQuarter;

            yuvaIndices[0].fIndex = 0;
            yuvaIndices[0].fChannel = SkColorChannel::kA;
            yuvaIndices[1].fIndex = 1;
            yuvaIndices[1].fChannel = SkColorChannel::kA;
            yuvaIndices[2].fIndex = 2;
            yuvaIndices[2].fChannel = SkColorChannel::kA;
            break;
        case kYV12_YUVFormat:
            resultBMs[nextLayer++] = planes.fYFull;
            resultBMs[nextLayer++] = planes.fVQuarter;
            resultBMs[nextLayer++] = planes.fUQuarter;

            yuvaIndices[0].fIndex = 0;
            yuvaIndices[0].fChannel = SkColorChannel::kA;
            yuvaIndices[1].fIndex = 2;
            yuvaIndices[1].fChannel = SkColorChannel::kA;
            yuvaIndices[2].fIndex = 1;
            yuvaIndices[2].fChannel = SkColorChannel::kA;
            break;
    }

    if (AYUV_YUVFormat != yuvFormat) {
        if (opaque) {
            yuvaIndices[3].fIndex = -1;
        } else {
            resultBMs[nextLayer] = planes.fAFull;

            yuvaIndices[3].fIndex = nextLayer;
            yuvaIndices[3].fChannel = SkColorChannel::kA;
        }
    }

}

static uint8_t look_up(float x1, float y1, const SkBitmap& bm, SkColorChannel  channel) {
    uint8_t result;

    int x = SkScalarFloorToInt(x1 * bm.width());
    int y = SkScalarFloorToInt(y1 * bm.height());

    if (kAlpha_8_SkColorType == bm.colorType()) {
        SkASSERT(SkColorChannel::kA == channel);
        result = *bm.getAddr8(x, y);
    } else {
        SkASSERT(kN32_SkColorType == bm.colorType());

        switch (channel) {
            case SkColorChannel::kR:
                result = SkColorGetR(bm.getColor(x, y));
                break;
            case SkColorChannel::kG:
                result = SkColorGetG(bm.getColor(x, y));
                break;
            case SkColorChannel::kB:
                result = SkColorGetB(bm.getColor(x, y));
                break;
            case SkColorChannel::kA:
                result = SkColorGetA(bm.getColor(x, y));
                break;
        }
    }

    return result;
}

class YUVGenerator : public SkImageGenerator {
public:
    YUVGenerator(const SkImageInfo& ii,
                 YUVFormat yuvFormat,
                 SkYUVColorSpace yuvColorSpace,
                 SkYUVAIndex yuvaIndices[4],
                 SkBitmap bitmaps[4])
            : SkImageGenerator(ii)
            , fYUVFormat(yuvFormat)
            , fYUVColorSpace(yuvColorSpace) {
        memcpy(fYUVAIndices, yuvaIndices, sizeof(fYUVAIndices));

        bool used[4] = { false, false, false, false };
        for (int i = 0; i < 4; ++i) {
            if (yuvaIndices[i].fIndex >= 0) {
                SkASSERT(yuvaIndices[i].fIndex < 4);
                used[yuvaIndices[i].fIndex] = true;
            } else {
                SkASSERT(3 == i); // only the 'A' channel can be unspecified
            }
        }

        for (int i = 0; i < 4; ++i) {
            if (used[i]) {
                fYUVBitmaps[i] = bitmaps[i];
            }
        }
    }

protected:
    bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                     const Options&) override {

        if (kUnknown_SkColorType == fFlattened.colorType()) {
            fFlattened.allocPixels(info);

            for (int y = 0; y < info.height(); ++y) {
                for (int x = 0; x < info.width(); ++x) {

                    float x1 = (x + 0.5f) / info.width();
                    float y1 = (y + 0.5f) / info.height();

                    uint8_t Y = look_up(x1, y1,
                                        fYUVBitmaps[fYUVAIndices[0].fIndex],
                                        fYUVAIndices[0].fChannel);

                    uint8_t U = look_up(x1, y1,
                                        fYUVBitmaps[fYUVAIndices[1].fIndex],
                                        fYUVAIndices[1].fChannel);


                    uint8_t V = look_up(x1, y1,
                                        fYUVBitmaps[fYUVAIndices[2].fIndex],
                                        fYUVAIndices[2].fChannel);

                    uint8_t A = 255;
                    if (fYUVAIndices[3].fIndex >= 0) {
                        A = look_up(x1, y1,
                                    fYUVBitmaps[fYUVAIndices[3].fIndex],
                                    fYUVAIndices[3].fChannel);
                    }

                    // Making premul here.
                    switch (fYUVColorSpace) {
                        case kJPEG_SkYUVColorSpace:
                            *fFlattened.getAddr32(x, y) = convert_yuva_to_rgba_jpeg(Y, U, V, A);
                            break;
                        case kRec601_SkYUVColorSpace:
                            *fFlattened.getAddr32(x, y) = convert_yuva_to_rgba_601(Y, U, V, A);
                            break;
                        case kRec709_SkYUVColorSpace:
                            *fFlattened.getAddr32(x, y) = convert_yuva_to_rgba_709(Y, U, V, A);
                            break;
                    }
                }
            }
        }

        return fFlattened.readPixels(info, pixels, rowBytes, 0, 0);
    }

    bool onQueryYUV8(SkYUVSizeInfo* size, SkYUVColorSpace* yuvColorSpace) const override {
        if (kI420_YUVFormat != fYUVFormat && kYV12_YUVFormat != fYUVFormat) {
            return false; // currently this API only supports planar formats
        }

        *yuvColorSpace = fYUVColorSpace;
        size->fSizes[0].fWidth = fYUVBitmaps[fYUVAIndices[0].fIndex].width();
        size->fSizes[0].fHeight = fYUVBitmaps[fYUVAIndices[0].fIndex].height();
        size->fWidthBytes[0] = fYUVBitmaps[fYUVAIndices[0].fIndex].rowBytes();

        size->fSizes[1].fWidth = fYUVBitmaps[fYUVAIndices[1].fIndex].width();
        size->fSizes[1].fHeight = fYUVBitmaps[fYUVAIndices[1].fIndex].height();
        size->fWidthBytes[1] = fYUVBitmaps[fYUVAIndices[1].fIndex].rowBytes();

        size->fSizes[2].fWidth = fYUVBitmaps[fYUVAIndices[2].fIndex].width();
        size->fSizes[2].fHeight = fYUVBitmaps[fYUVAIndices[2].fIndex].height();
        size->fWidthBytes[2] = fYUVBitmaps[fYUVAIndices[2].fIndex].rowBytes();
        return true;
    }

    bool onGetYUV8Planes(const SkYUVSizeInfo&, void* planes[3]) override {
        planes[0] = fYUVBitmaps[fYUVAIndices[0].fIndex].getAddr(0, 0);
        planes[1] = fYUVBitmaps[fYUVAIndices[1].fIndex].getAddr(0, 0);
        planes[2] = fYUVBitmaps[fYUVAIndices[2].fIndex].getAddr(0, 0);
        return true;
    }

private:
    YUVFormat       fYUVFormat;
    SkYUVColorSpace fYUVColorSpace;
    SkYUVAIndex     fYUVAIndices[4];
    SkBitmap        fYUVBitmaps[4];
    SkBitmap        fFlattened;

};

static sk_sp<SkImage> make_yuv_gen_image(const SkImageInfo& ii,
                                         YUVFormat yuvFormat,
                                         SkYUVColorSpace yuvColorSpace,
                                         SkYUVAIndex yuvaIndices[4],
                                         SkBitmap bitmaps[]) {
     std::unique_ptr<SkImageGenerator> gen(new YUVGenerator(ii, yuvFormat, yuvColorSpace,
                                                            yuvaIndices, bitmaps));

    return SkImage::MakeFromGenerator(std::move(gen));
}

static void draw_col_label(SkCanvas* canvas, int x, int yuvColorSpace, bool opaque) {
    static const char* kYUVColorSpaceNames[] = { "JPEG", "601", "709" };
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(kYUVColorSpaceNames) == kLastEnum_SkYUVColorSpace+1);

    SkPaint textPaint;
    textPaint.setTextAlign(SkPaint::kCenter_Align);
    sk_tool_utils::set_portable_typeface(&textPaint, nullptr, SkFontStyle::Bold());
    textPaint.setTextSize(16);

    SkRect textRect;
    SkString colLabel;

    colLabel.printf("%s", kYUVColorSpaceNames[yuvColorSpace]);
    textPaint.measureText(colLabel.c_str(), colLabel.size(), &textRect);
    int y = textRect.height();

    canvas->drawText(colLabel.c_str(), colLabel.size(), x, y, textPaint);

    colLabel.printf("%s", opaque ? "Opaque" : "Transparent");
    textPaint.measureText(colLabel.c_str(), colLabel.size(), &textRect);
    y += textRect.height();

    canvas->drawText(colLabel.c_str(), colLabel.size(), x, y, textPaint);
}

static void draw_row_label(SkCanvas* canvas, int y, int yuvFormat) {
    static const char* kYUVFormatNames[] = { "AYUV", "NV12", "NV21", "I420", "YV12" };
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(kYUVFormatNames) == kLast_YUVFormat+1);

    SkPaint textPaint;
    textPaint.setTextAlign(SkPaint::kLeft_Align);
    sk_tool_utils::set_portable_typeface(&textPaint, nullptr, SkFontStyle::Bold());
    textPaint.setTextSize(16);

    SkRect textRect;
    SkString rowLabel;

    rowLabel.printf("%s", kYUVFormatNames[yuvFormat]);
    textPaint.measureText(rowLabel.c_str(), rowLabel.size(), &textRect);
    y += kTileWidthHeight/2 + textRect.height()/2;

    canvas->drawText(rowLabel.c_str(), rowLabel.size(), 0, y, textPaint);
}

namespace skiagm {

// This GM creates an opaque and transparent bitmap, extracts the planes and then recombines
// them into various YUV formats. It then renders the results in the grid:
//
//                   JPEG                   601                     709
//        Transparent  Opaque       Transparent  Opaque        Transparent  Opaque
// AYUV
// NV12
// NV21
// I420
// YV12
class WackyYUVFormatsGM : public GM {
public:
    WackyYUVFormatsGM() {
        this->setBGColor(0xFFCCCCCC);
    }

protected:

    SkString onShortName() override {
        return SkString("wacky_yuv_formats");
    }

    SkISize onISize() override {
        int numCols = 2 * (kLastEnum_SkYUVColorSpace + 1); // opacity x color-space
        int numRows = 1 + (kLast_YUVFormat + 1);  // origin + # yuv formats
        return SkISize::Make(kLabelWidth  + numCols * (kTileWidthHeight + kPad),
                             kLabelHeight + numRows * (kTileWidthHeight + kPad));
    }

    void onOnceBeforeDraw() override {
        SkPoint origin = { kTileWidthHeight/2.0f, kTileWidthHeight/2.0f };
        float outerRadius = kTileWidthHeight/2.0f - 20.0f;
        float innerRadius = 20.0f;

        {
            // transparent
            SkTDArray<SkRect> circles;
            SkPath path = create_splat(origin, innerRadius, outerRadius, 1.0f, 5, &circles);
            fOriginalBMs[0] = make_bitmap(path, circles, false);
        }

        {
            // opaque
            SkTDArray<SkRect> circles;
            SkPath path = create_splat(origin, innerRadius, outerRadius, 1.0f, 7, &circles);
            fOriginalBMs[1] = make_bitmap(path, circles, true);
        }
    }

    void createImages(GrContext* context) {
        for (bool opaque : { false, true }) {
            for (int cs = kJPEG_SkYUVColorSpace; cs <= kLastEnum_SkYUVColorSpace; ++cs) {
                PlaneData planes;
                extract_planes(fOriginalBMs[opaque], (SkYUVColorSpace) cs, &planes);

                for (int format = AYUV_YUVFormat; format <= kLast_YUVFormat; ++format) {
                    SkBitmap resultBMs[4];
                    SkYUVAIndex yuvaIndices[4];
                    create_YUV(planes, (YUVFormat) format, resultBMs, yuvaIndices, opaque);

#if SK_SUPPORT_GPU
                    if (context) {
                        if (context->abandoned()) {
                            return;
                        }

                        GrGpu* gpu = context->contextPriv().getGpu();
                        if (!gpu) {
                            return;
                        }

                        bool used[4] = { false, false, false, false };
                        for (int i = 0; i < 4; ++i) {
                            if (yuvaIndices[i].fIndex >= 0) {
                                SkASSERT(yuvaIndices[i].fIndex < 4);
                                used[yuvaIndices[i].fIndex] = true;
                            }
                        }

                        GrBackendTexture yuvaTextures[4];

                        for (int i = 0; i < 4; ++i) {
                            if (!used[i]) {
                                continue;
                            }

                            yuvaTextures[i] = gpu->createTestingOnlyBackendTexture(
                                resultBMs[i].getPixels(),
                                resultBMs[i].width(),
                                resultBMs[i].height(),
                                resultBMs[i].colorType(),
                                false,
                                GrMipMapped::kNo,
                                resultBMs[i].rowBytes());
                        }

                        fImages[opaque][cs][format] = SkImage::MakeFromYUVATexturesCopy(
                            context,
                            (SkYUVColorSpace) cs,
                            yuvaTextures,
                            yuvaIndices,
                            { fOriginalBMs[opaque].width(), fOriginalBMs[opaque].height() },
                            kTopLeft_GrSurfaceOrigin);
                    } else
#endif
                    {
                        fImages[opaque][cs][format] = make_yuv_gen_image(
                                                                fOriginalBMs[opaque].info(),
                                                                (YUVFormat) format,
                                                                (SkYUVColorSpace) cs,
                                                                yuvaIndices,
                                                                resultBMs);
                    }
                }
            }
        }
    }

    void onDraw(SkCanvas* canvas) override {
        this->createImages(canvas->getGrContext());

        int x = kLabelWidth;
        for (int cs = kJPEG_SkYUVColorSpace; cs <= kLastEnum_SkYUVColorSpace; ++cs) {
            for (int opaque : { 0, 1 }) {
                int y = kLabelHeight;

                draw_col_label(canvas, x+kTileWidthHeight/2, cs, opaque);

                canvas->drawBitmap(fOriginalBMs[opaque], x, y);
                y += kTileWidthHeight + kPad;

                for (int format = AYUV_YUVFormat; format <= kLast_YUVFormat; ++format) {
                    draw_row_label(canvas, y, format);
                    canvas->drawImage(fImages[opaque][cs][format], x, y);

                    y += kTileWidthHeight + kPad;
                }

                x += kTileWidthHeight + kPad;
            }
        }
    }

private:
    SkBitmap       fOriginalBMs[2];
    sk_sp<SkImage> fImages[2][kLastEnum_SkYUVColorSpace+1][kLast_YUVFormat+1];

    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM(return new WackyYUVFormatsGM;)
}
