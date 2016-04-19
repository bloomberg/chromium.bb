// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasRenderingContext2D_h
#define OffscreenCanvasRenderingContext2D_h

#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "modules/canvas2d/BaseRenderingContext2D.h"
#include "modules/offscreencanvas/OffscreenCanvasRenderingContext.h"
#include "modules/offscreencanvas/OffscreenCanvasRenderingContextFactory.h"

namespace blink {

class MODULES_EXPORT OffscreenCanvasRenderingContext2D final : public OffscreenCanvasRenderingContext, public BaseRenderingContext2D {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(OffscreenCanvasRenderingContext2D);
public:
    class Factory : public OffscreenCanvasRenderingContextFactory {
    public:
        Factory() {}
        ~Factory() override {}

        OffscreenCanvasRenderingContext* create(OffscreenCanvas* canvas, const CanvasContextCreationAttributes& attrs) override
        {
            return new OffscreenCanvasRenderingContext2D(canvas, attrs);
        }

        OffscreenCanvasRenderingContext::ContextType getContextType() const override
        {
            return OffscreenCanvasRenderingContext::Context2d;
        }

        void onError(OffscreenCanvas* canvas, const String& error) override {}
    };

    // OffscreenCanvasRenderingContext implementation
    ~OffscreenCanvasRenderingContext2D() override;
    ContextType getContextType() const override { return Context2d; }
    bool is2d() const override { return true; }

    // BaseRenderingContext2D implementation
    bool originClean() const final;
    void setOriginTainted() final;
    bool wouldTaintOrigin(CanvasImageSource*) final;

    int width() const final;
    int height() const final;

    bool hasImageBuffer() const final;
    ImageBuffer* imageBuffer() const final;

    bool parseColorOrCurrentColor(Color&, const String& colorString) const final;

    SkCanvas* drawingCanvas() const final;
    SkCanvas* existingDrawingCanvas() const final;
    void disableDeferral(DisableDeferralReason) final;

    AffineTransform baseTransform() const final;
    void didDraw(const SkIRect& dirtyRect) final;

    bool stateHasFilter() final;
    SkImageFilter* stateGetFilter() final;
    void snapshotStateForFilter() final { }

    void validateStateStack() final;

    bool hasAlpha() const override { return m_hasAlpha; }
    bool isContextLost() const override;

    ImageBitmap* transferToImageBitmap(ExceptionState&) final;

protected:
    OffscreenCanvasRenderingContext2D(OffscreenCanvas*, const CanvasContextCreationAttributes& attrs);
    DECLARE_VIRTUAL_TRACE();

private:
    bool m_hasAlpha;
    bool m_originClean = true;
    OwnPtr<ImageBuffer> m_imageBuffer;
};

} // namespace blink

#endif // OffscreenCanvasRenderingContext2D_h
