/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_Image_Graphite_DEFINED
#define skgpu_graphite_Image_Graphite_DEFINED

#include "src/image/SkImage_Base.h"

#include "src/gpu/graphite/TextureProxyView.h"

namespace skgpu::graphite {

class Context;
class Recorder;

class Image final : public SkImage_Base {
public:
    Image(TextureProxyView, const SkColorInfo&);
    ~Image() override;

    bool onReadPixels(GrDirectContext*,
                      const SkImageInfo& dstInfo,
                      void* dstPixels,
                      size_t dstRowBytes,
                      int srcX,
                      int srcY,
                      CachingHint) const override { return false; }
    // Temporary and only for testing purposes.
    // To be removed once asynchronous readback is working.
    bool testingOnly_ReadPixels(Context*,
                                Recorder*,
                                const SkImageInfo& dstInfo,
                                void* dstPixels,
                                size_t dstRowBytes,
                                int srcX,
                                int srcY);

    bool onHasMipmaps() const override { return false; }

    bool isGraphiteBacked() const override { return true; }

    bool getROPixels(GrDirectContext*,
                     SkBitmap*,
                     CachingHint = kAllow_CachingHint) const override { return false; }

    sk_sp<SkImage> onMakeSubset(const SkIRect&, GrDirectContext*) const override {
        return nullptr;
    }

    bool onIsValid(GrRecordingContext*) const override { return true; }

    sk_sp<SkImage> onMakeColorTypeAndColorSpace(SkColorType,
                                                sk_sp<SkColorSpace>,
                                                GrDirectContext*) const override {
        return nullptr;
    }

    sk_sp<SkImage> onReinterpretColorSpace(sk_sp<SkColorSpace>) const override {
        return nullptr;
    }

private:
#if SK_SUPPORT_GPU
    std::unique_ptr<GrFragmentProcessor> onAsFragmentProcessor(
            GrRecordingContext*,
            SkSamplingOptions,
            const SkTileMode[2],
            const SkMatrix&,
            const SkRect* subset,
            const SkRect* domain) const override {
        return nullptr;
    }
    std::tuple<GrSurfaceProxyView, GrColorType> onAsView(
            GrRecordingContext*,
            GrMipmapped,
            GrImageTexGenPolicy policy) const override {
        return {};
    }
#endif

    std::tuple<TextureProxyView, SkColorType> onAsView(Recorder*,
                                                       Mipmapped) const override;

    TextureProxyView fTextureProxyView;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_Image_Graphite_DEFINED
