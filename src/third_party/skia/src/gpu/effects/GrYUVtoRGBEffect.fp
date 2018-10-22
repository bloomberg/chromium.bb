/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

in uniform sampler2D ySampler;
in half4x4 ySamplerTransform;
@coordTransform(ySampler) {
    ySamplerTransform
}

in uniform sampler2D uSampler;
in half4x4 uSamplerTransform;
@coordTransform(uSampler) {
    uSamplerTransform
}
@samplerParams(uSampler) {
    uvSamplerParams
}

in uniform sampler2D vSampler;
in half4x4 vSamplerTransform;
@coordTransform(vSampler) {
    vSamplerTransform
}
@samplerParams(vSampler) {
    uvSamplerParams
}

in uniform half4x4 colorSpaceMatrix;
layout(key) in bool nv12;

@constructorParams {
    GrSamplerState uvSamplerParams
}

@class {
    static std::unique_ptr<GrFragmentProcessor> Make(sk_sp<GrTextureProxy> yProxy,
                                                     sk_sp<GrTextureProxy> uProxy,
                                                     sk_sp<GrTextureProxy> vProxy,
                                                     SkYUVColorSpace colorSpace, bool nv12);
    SkString dumpInfo() const override;
}

@cpp {
    static const float kJPEGConversionMatrix[16] = {
        1.0f,  0.0f,       1.402f,   -0.703749f,
        1.0f, -0.344136f, -0.714136f, 0.531211f,
        1.0f,  1.772f,     0.0f,     -0.889475f,
        0.0f,  0.0f,       0.0f,      1.0
    };

    static const float kRec601ConversionMatrix[16] = {
        1.164f,  0.0f,    1.596f, -0.87075f,
        1.164f, -0.391f, -0.813f,  0.52925f,
        1.164f,  2.018f,  0.0f,   -1.08175f,
        0.0f,    0.0f,    0.0f,    1.0}
    ;

    static const float kRec709ConversionMatrix[16] = {
        1.164f,  0.0f,    1.793f, -0.96925f,
        1.164f, -0.213f, -0.533f,  0.30025f,
        1.164f,  2.112f,  0.0f,   -1.12875f,
        0.0f,    0.0f,    0.0f,    1.0f}
    ;

    std::unique_ptr<GrFragmentProcessor> GrYUVtoRGBEffect::Make(sk_sp<GrTextureProxy> yProxy,
                                                                sk_sp<GrTextureProxy> uProxy,
                                                                sk_sp<GrTextureProxy> vProxy,
                                                                SkYUVColorSpace colorSpace,
                                                                bool nv12) {
        SkScalar w[3], h[3];
        w[0] = SkIntToScalar(yProxy->width());
        h[0] = SkIntToScalar(yProxy->height());
        w[1] = SkIntToScalar(uProxy->width());
        h[1] = SkIntToScalar(uProxy->height());
        w[2] = SkIntToScalar(vProxy->width());
        h[2] = SkIntToScalar(vProxy->height());
        SkMatrix yTransform = SkMatrix::I();
        SkMatrix uTransform = SkMatrix::MakeScale(w[1] / w[0], h[1] / h[0]);
        SkMatrix vTransform = SkMatrix::MakeScale(w[2] / w[0], h[2] / h[0]);
        GrSamplerState::Filter uvFilterMode =
            ((uProxy->width()  != yProxy->width()) ||
             (uProxy->height() != yProxy->height()) ||
             (vProxy->width()  != yProxy->width()) ||
             (vProxy->height() != yProxy->height())) ?
            GrSamplerState::Filter::kBilerp :
            GrSamplerState::Filter::kNearest;
        SkMatrix44 mat(SkMatrix44::kUninitialized_Constructor);
        switch (colorSpace) {
            case kJPEG_SkYUVColorSpace:
                mat.setColMajorf(kJPEGConversionMatrix);
                break;
            case kRec601_SkYUVColorSpace:
                mat.setColMajorf(kRec601ConversionMatrix);
                break;
            case kRec709_SkYUVColorSpace:
                mat.setColMajorf(kRec709ConversionMatrix);
                break;
        }
        return std::unique_ptr<GrFragmentProcessor>(
                new GrYUVtoRGBEffect(std::move(yProxy), yTransform, std::move(uProxy), uTransform,
                                     std::move(vProxy), vTransform, mat, nv12,
                                     GrSamplerState(GrSamplerState::WrapMode::kClamp,
                                                    uvFilterMode)));
    }

    SkString GrYUVtoRGBEffect::dumpInfo() const {
        SkString str;
        str.appendf("Y: %d %d U: %d %d V: %d %d\n",
            fYSampler.proxy()->uniqueID().asUInt(),
            fYSampler.proxy()->underlyingUniqueID().asUInt(),
            fUSampler.proxy()->uniqueID().asUInt(),
            fUSampler.proxy()->underlyingUniqueID().asUInt(),
            fVSampler.proxy()->uniqueID().asUInt(),
            fVSampler.proxy()->underlyingUniqueID().asUInt());

        return str;
    }
}

void main() {
    @if (nv12) {
        sk_OutColor = half4(texture(ySampler, sk_TransformedCoords2D[0]).r,
                            texture(uSampler, sk_TransformedCoords2D[1]).rg,
                            1.0) * colorSpaceMatrix;
    } else {
        sk_OutColor = half4(texture(ySampler, sk_TransformedCoords2D[0]).r,
                            texture(uSampler, sk_TransformedCoords2D[1]).r,
                            texture(vSampler, sk_TransformedCoords2D[2]).r,
                            1.0) * colorSpaceMatrix;
    }
}
