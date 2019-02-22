/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkData.h"
#include "SkCanvas.h"
#include "SkGraphics.h"
#include "SkImageGenerator.h"
#include "SkImageInfoPriv.h"
#include "Test.h"

#if defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)
    #include "SkImageGeneratorCG.h"
#elif defined(SK_BUILD_FOR_WIN)
    #include "SkImageGeneratorWIC.h"
#endif

static bool gMyFactoryWasCalled;

static std::unique_ptr<SkImageGenerator> my_factory(sk_sp<SkData>) {
    gMyFactoryWasCalled = true;
    return nullptr;
}

static void test_imagegenerator_factory(skiatest::Reporter* reporter) {
    // just need a non-empty data to test things
    sk_sp<SkData> data(SkData::MakeWithCString("test_imagegenerator_factory"));

    gMyFactoryWasCalled = false;

    REPORTER_ASSERT(reporter, !gMyFactoryWasCalled);

    std::unique_ptr<SkImageGenerator> gen = SkImageGenerator::MakeFromEncoded(data);
    REPORTER_ASSERT(reporter, nullptr == gen);
    REPORTER_ASSERT(reporter, !gMyFactoryWasCalled);

    // Test is racy, in that it hopes no other thread is changing this global...
    auto prev = SkGraphics::SetImageGeneratorFromEncodedDataFactory(my_factory);
    gen = SkImageGenerator::MakeFromEncoded(data);
    REPORTER_ASSERT(reporter, nullptr == gen);
    REPORTER_ASSERT(reporter, gMyFactoryWasCalled);

    // This just verifies that the signatures match.
#if defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)
    SkGraphics::SetImageGeneratorFromEncodedDataFactory(SkImageGeneratorCG::MakeFromEncodedCG);
#elif defined(SK_BUILD_FOR_WIN)
    SkGraphics::SetImageGeneratorFromEncodedDataFactory(SkImageGeneratorWIC::MakeFromEncodedWIC);
#endif

    SkGraphics::SetImageGeneratorFromEncodedDataFactory(prev);
}

class MyImageGenerator : public SkImageGenerator {
public:
    MyImageGenerator() : SkImageGenerator(SkImageInfo::MakeN32Premul(0, 0)) {}
};

DEF_TEST(ImageGenerator, reporter) {
    MyImageGenerator ig;
    SkYUVSizeInfo sizeInfo;
    sizeInfo.fSizes[SkYUVSizeInfo::kY] = SkISize::Make(200, 200);
    sizeInfo.fSizes[SkYUVSizeInfo::kU] = SkISize::Make(100, 100);
    sizeInfo.fSizes[SkYUVSizeInfo::kV] = SkISize::Make( 50,  50);
    sizeInfo.fWidthBytes[SkYUVSizeInfo::kY] = 0;
    sizeInfo.fWidthBytes[SkYUVSizeInfo::kU] = 0;
    sizeInfo.fWidthBytes[SkYUVSizeInfo::kV] = 0;
    void* planes[3] = { nullptr };
    SkYUVColorSpace colorSpace;

    // Check that the YUV decoding API does not cause any crashes
    ig.queryYUV8(&sizeInfo, nullptr);
    ig.queryYUV8(&sizeInfo, &colorSpace);
    sizeInfo.fWidthBytes[SkYUVSizeInfo::kY] = 250;
    sizeInfo.fWidthBytes[SkYUVSizeInfo::kU] = 250;
    sizeInfo.fWidthBytes[SkYUVSizeInfo::kV] = 250;
    int dummy;
    planes[SkYUVSizeInfo::kY] = planes[SkYUVSizeInfo::kU] = planes[SkYUVSizeInfo::kV] = &dummy;
    ig.getYUV8Planes(sizeInfo, planes);

    // Suppressed due to https://code.google.com/p/skia/issues/detail?id=4339
    if (false) {
        test_imagegenerator_factory(reporter);
    }
}

#include "SkAutoMalloc.h"
#include "SkPictureRecorder.h"

static sk_sp<SkPicture> make_picture() {
    SkPictureRecorder recorder;
    recorder.beginRecording(100, 100)->drawColor(SK_ColorRED);
    return recorder.finishRecordingAsPicture();
}

DEF_TEST(PictureImageGenerator, reporter) {
    const struct {
        SkColorType fColorType;
        SkAlphaType fAlphaType;
    } recs[] = {
        { kRGBA_8888_SkColorType, kPremul_SkAlphaType },
        { kBGRA_8888_SkColorType, kPremul_SkAlphaType },
        { kRGBA_F16_SkColorType,  kPremul_SkAlphaType },
        { kRGBA_F32_SkColorType,  kPremul_SkAlphaType },
        { kRGBA_1010102_SkColorType, kPremul_SkAlphaType },

        { kRGBA_8888_SkColorType, kUnpremul_SkAlphaType },
        { kBGRA_8888_SkColorType, kUnpremul_SkAlphaType },
        { kRGBA_F16_SkColorType,  kUnpremul_SkAlphaType },
        { kRGBA_F32_SkColorType,  kUnpremul_SkAlphaType },
        { kRGBA_1010102_SkColorType, kUnpremul_SkAlphaType },
    };

    auto colorspace = SkColorSpace::MakeSRGB();
    auto picture = make_picture();
    auto gen = SkImageGenerator::MakeFromPicture({100, 100}, picture, nullptr, nullptr,
                                                 SkImage::BitDepth::kU8, colorspace);

    // worst case for all requests
    SkAutoMalloc storage(100 * 100 * SkColorTypeBytesPerPixel(kRGBA_F32_SkColorType));

    for (const auto& rec : recs) {
        SkImageInfo info = SkImageInfo::Make(100, 100, rec.fColorType, rec.fAlphaType, colorspace);
        REPORTER_ASSERT(reporter, gen->getPixels(info, storage.get(), info.minRowBytes()));
    }
}

