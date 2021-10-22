/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_Device_DEFINED
#define skgpu_Device_DEFINED

#include "src/core/SkDevice.h"

namespace skgpu {

class Context;
class DrawContext;

class Device final : public SkBaseDevice  {
public:
    static sk_sp<Device> Make(sk_sp<Context>, const SkImageInfo&);

    sk_sp<Context> refContext() { return fContext; }

protected:
    // Clipping
    void onSave() override {}
    void onRestore() override {}

    bool onClipIsAA() const override { return false; }
    bool onClipIsWideOpen() const override { return false; }
    ClipType onGetClipType() const override { return ClipType::kEmpty; }
    SkIRect onDevClipBounds() const override { return {}; }

    void onClipRect(const SkRect& rect, SkClipOp, bool aa) override {}
    void onClipRRect(const SkRRect& rrect, SkClipOp, bool aa) override {}
    void onClipPath(const SkPath& path, SkClipOp, bool aa) override {}

    // Drawing
    void drawPaint(const SkPaint& paint) override;
    void drawRect(const SkRect& r, const SkPaint& paint) override;
    void drawOval(const SkRect& oval, const SkPaint& paint) override;
    void drawRRect(const SkRRect& rr, const SkPaint& paint) override;
    void drawPoints(SkCanvas::PointMode mode, size_t count,
                    const SkPoint[], const SkPaint& paint) override;
    void drawPath(const SkPath& path, const SkPaint& paint, bool pathIsMutable = false) override;

    // No need to specialize drawDRRect, drawArc, drawRegion, drawPatch as the default impls all
    // route to drawPath, drawRect, or drawVertices as desired.

    // Pixel management
    sk_sp<SkSurface> makeSurface(const SkImageInfo&, const SkSurfaceProps&) override;
    SkBaseDevice* onCreateDevice(const CreateInfo&, const SkPaint*) override;

    bool onReadPixels(const SkPixmap&, int x, int y) override;

    /*
     * TODO: These functions are not in scope to be implemented yet, but will need to be. Call them
     * out explicitly so it's easy to keep tabs on how close feature-complete actually is.
     */

    void onAsRgnClip(SkRegion*) const override {}
    void onClipShader(sk_sp<SkShader>) override {}
    void onClipRegion(const SkRegion& deviceRgn, SkClipOp) override {}
    void onReplaceClip(const SkIRect& rect) override {}

    bool onWritePixels(const SkPixmap&, int x, int y) override { return false; }

    // TODO: This will likely be implemented with the same primitive building block that drawRect
    // and drawRRect will rely on.
    void drawEdgeAAQuad(const SkRect& rect, const SkPoint clip[4],
                        SkCanvas::QuadAAFlags aaFlags, const SkColor4f& color,
                        SkBlendMode mode) override {}

    void drawEdgeAAImageSet(const SkCanvas::ImageSetEntry[], int count,
                            const SkPoint dstClips[], const SkMatrix preViewMatrices[],
                            const SkSamplingOptions&, const SkPaint&,
                            SkCanvas::SrcRectConstraint) override {}

    // TODO: These image drawing APIs can likely be implemented with the same primitive building
    // block that drawEdgeAAImageSet will use.
    void drawImageRect(const SkImage*, const SkRect* src, const SkRect& dst,
                       const SkSamplingOptions&, const SkPaint&,
                       SkCanvas::SrcRectConstraint) override {}
    void drawImageLattice(const SkImage*, const SkCanvas::Lattice&,
                          const SkRect& dst, SkFilterMode, const SkPaint&) override {}
    void drawAtlas(const SkImage* atlas, const SkRSXform[], const SkRect[],
                   const SkColor[], int count, SkBlendMode, const SkSamplingOptions&,
                   const SkPaint&) override {}

    void drawDrawable(SkDrawable*, const SkMatrix*, SkCanvas*) override {}
    void drawVertices(const SkVertices*, SkBlendMode, const SkPaint&) override {}
    void drawShadow(const SkPath&, const SkDrawShadowRec&) override {}
    void onDrawGlyphRunList(const SkGlyphRunList& glyphRunList, const SkPaint& paint) override {}

    void drawDevice(SkBaseDevice*, const SkSamplingOptions&, const SkPaint&) override {}
    void drawSpecial(SkSpecialImage*, const SkMatrix& localToDevice,
                     const SkSamplingOptions&, const SkPaint&) override {}

    sk_sp<SkSpecialImage> makeSpecial(const SkBitmap&) override;
    sk_sp<SkSpecialImage> makeSpecial(const SkImage*) override;
    sk_sp<SkSpecialImage> snapSpecial(const SkIRect& subset, bool forceCopy = false) override;

private:
    Device(sk_sp<Context>, sk_sp<DrawContext>);

    sk_sp<Context> fContext;
    sk_sp<DrawContext> fDC;
};

} // namespace skgpu

#endif // skgpu_Device_DEFINED
