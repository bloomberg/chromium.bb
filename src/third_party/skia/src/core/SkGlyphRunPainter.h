/*
 * Copyright 2018 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkGlyphRunPainter_DEFINED
#define SkGlyphRunPainter_DEFINED

#include "include/core/SkSurfaceProps.h"
#include "src/core/SkDistanceFieldGen.h"
#include "src/core/SkGlyphRun.h"
#include "src/core/SkScalerContext.h"
#include "src/core/SkTextBlobPriv.h"

#if SK_SUPPORT_GPU
#include "src/gpu/ganesh/text/GrSDFTControl.h"
class GrColorInfo;
namespace skgpu { namespace v1 { class SurfaceDrawContext; }}
#endif

class SkGlyphRunPainterInterface;
class SkStrikeSpec;
class GrSDFTMatrixRange;

// round and ignorePositionMask are used to calculate the subpixel position of a glyph.
// The per component (x or y) calculation is:
//
//   subpixelOffset = (floor((viewportPosition + rounding) & mask) >> 14) & 3
//
// where mask is either 0 or ~0, and rounding is either
// 1/2 for non-subpixel or 1/8 for subpixel.
struct SkGlyphPositionRoundingSpec {
    SkGlyphPositionRoundingSpec(bool isSubpixel, SkAxisAlignment axisAlignment);
    const SkVector halfAxisSampleFreq;
    const SkIPoint ignorePositionMask;
    const SkIPoint ignorePositionFieldMask;

private:
    static SkVector HalfAxisSampleFreq(bool isSubpixel, SkAxisAlignment axisAlignment);
    static SkIPoint IgnorePositionMask(bool isSubpixel, SkAxisAlignment axisAlignment);
    static SkIPoint IgnorePositionFieldMask(bool isSubpixel, SkAxisAlignment axisAlignment);
};

class SkStrikeCommon {
public:
    // An atlas consists of plots, and plots hold glyphs. The minimum a plot can be is 256x256.
    // This means that the maximum size a glyph can be is 256x256.
    inline static constexpr uint16_t kSkSideTooBigForAtlas = 256;
};

// -- SkGlyphRunListPainterCPU ---------------------------------------------------------------------
class SkGlyphRunListPainterCPU {
public:
    class BitmapDevicePainter {
    public:
        BitmapDevicePainter() = default;
        BitmapDevicePainter(const BitmapDevicePainter&) = default;
        virtual ~BitmapDevicePainter() = default;

        virtual void paintMasks(SkDrawableGlyphBuffer* accepted, const SkPaint& paint) const = 0;
        virtual void drawBitmap(const SkBitmap&, const SkMatrix&, const SkRect* dstOrNull,
                                const SkSamplingOptions&, const SkPaint&) const = 0;
    };

    SkGlyphRunListPainterCPU(const SkSurfaceProps& props,
                             SkColorType colorType,
                             SkColorSpace* cs);

    void drawForBitmapDevice(
            SkCanvas* canvas, const BitmapDevicePainter* bitmapDevice,
            const SkGlyphRunList& glyphRunList, const SkPaint& paint, const SkMatrix& drawMatrix);
private:
    // The props as on the actual device.
    const SkSurfaceProps fDeviceProps;

    // The props for when the bitmap device can't draw LCD text.
    const SkSurfaceProps fBitmapFallbackProps;
    const SkColorType fColorType;
    const SkScalerContextFlags fScalerContextFlags;
};

#if SK_SUPPORT_GPU
class SkGlyphRunListPainter {
public:
    SkGlyphRunListPainter(const SkSurfaceProps& props,
                          const SkColorSpace* colorSpace,
                          SkStrikeForGPUCacheInterface* strikeCache);
    // The following ctor is used exclusively by the GPU, and will always use the global
    // strike cache.
    explicit SkGlyphRunListPainter(const skgpu::v1::SurfaceDrawContext&);

    // A nullptr for process means that the calls to the cache will be performed, but none of the
    // callbacks will be called.
    // N.B. The positionMatrix has already been translated to the glyph run list origin.
    void categorizeGlyphRunList(SkGlyphRunPainterInterface* process,
                                const SkGlyphRunList& glyphRunList,
                                const SkMatrix& positionMatrix,
                                const SkPaint& drawPaint,
                                const GrSDFTControl& control,
                                const char* tag = nullptr);

private:
    SkGlyphRunListPainter(const SkSurfaceProps& props,
                          SkScalerContextFlags flags,
                          SkStrikeForGPUCacheInterface* strikeCache);

    // The props as on the actual device.
    [[maybe_unused]] const SkSurfaceProps fDeviceProps;
    [[maybe_unused]] const SkScalerContextFlags fScalerContextFlags;

    [[maybe_unused]] SkStrikeForGPUCacheInterface* const fStrikeCache;
};

// SkGlyphRunPainterInterface are all the ways that Ganesh generates glyphs. The first
// distinction is between Device and Source.
// * Device - the data in the cache is scaled to the device. There is no transformation from the
//   cache to the screen.
// * Source - the data in the cache needs to be scaled from the cache to source space using the
//   factor cacheToSourceScale. When drawn the system must combine cacheToSourceScale and the
//   deviceView matrix to transform the cache data onto the screen. This allows zooming and
//   simple animation to reuse the same glyph data by just changing the transform.
//
// In addition to transformation type above, Masks, Paths, SDFT, and Fallback (or really the
// rendering method of last resort) are the different
// formats of data used from the cache.
class SkGlyphRunPainterInterface {
public:
    virtual ~SkGlyphRunPainterInterface() = default;

    virtual void processDeviceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                    sk_sp<SkStrike>&& strike) = 0;

    virtual void processSourceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                    sk_sp<SkStrike>&& strike,
                                    SkScalar strikeToSourceScale) = 0;

    virtual void processSourcePaths(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                    const SkFont& runFont,
                                    const SkDescriptor& descriptor,
                                    SkScalar strikeToSourceScale) = 0;

    virtual void processSourceDrawables(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                        sk_sp<SkStrike>&& strike,
                                        const SkDescriptor& descriptor,
                                        SkScalar strikeToSourceScale) = 0;

    virtual void processSourceSDFT(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                   sk_sp<SkStrike>&& strike,
                                   SkScalar strikeToSourceScale,
                                   const SkFont& runFont,
                                   const GrSDFTMatrixRange& matrixRange) = 0;
};
#endif  // SK_SUPPORT_GPU
#endif  // SkGlyphRunPainter_DEFINED
