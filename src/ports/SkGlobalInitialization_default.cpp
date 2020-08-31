/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkFlattenable.h"

#if defined(SK_DISABLE_EFFECT_DESERIALIZATION)

    void SkFlattenable::PrivateInitializer::InitEffects() {}
    void SkFlattenable::PrivateInitializer::InitImageFilters() {}

#else

    #include "include/core/SkBBHFactory.h"
    #include "include/core/SkColorFilter.h"
    #include "include/core/SkPathEffect.h"
    #include "include/effects/Sk1DPathEffect.h"
    #include "include/effects/Sk2DPathEffect.h"
    #include "include/effects/SkCornerPathEffect.h"
    #include "include/effects/SkDiscretePathEffect.h"
    #include "include/effects/SkGradientShader.h"
    #include "include/effects/SkHighContrastFilter.h"
    #include "include/effects/SkLayerDrawLooper.h"
    #include "include/effects/SkLumaColorFilter.h"
    #include "include/effects/SkOverdrawColorFilter.h"
    #include "include/effects/SkPerlinNoiseShader.h"
    #include "include/effects/SkRuntimeEffect.h"
    #include "include/effects/SkShaderMaskFilter.h"
    #include "include/effects/SkTableColorFilter.h"
    #include "src/core/SkColorFilter_Matrix.h"
    #include "src/core/SkRecordedDrawable.h"
    #include "src/effects/SkDashImpl.h"
    #include "src/effects/SkEmbossMaskFilter.h"
    #include "src/effects/SkOpPE.h"
    #include "src/effects/SkTrimPE.h"
    #include "src/shaders/SkBitmapProcShader.h"
    #include "src/shaders/SkColorFilterShader.h"
    #include "src/shaders/SkColorShader.h"
    #include "src/shaders/SkComposeShader.h"
    #include "src/shaders/SkEmptyShader.h"
    #include "src/shaders/SkImageShader.h"
    #include "src/shaders/SkLocalMatrixShader.h"
    #include "src/shaders/SkPictureShader.h"
    #include "src/shaders/SkShaderBase.h"

    #include "include/effects/SkImageFilters.h"
    #include "src/core/SkLocalMatrixImageFilter.h"
    #include "src/core/SkMatrixImageFilter.h"

    /*
     *  Register most effects for deserialization.
     *
     *  None of these are strictly required for Skia to operate, so if you're
     *  not using deserialization yourself, you can define
     *  SK_DISABLE_EFFECT_SERIALIZATION, or modify/replace this file as needed.
     */
    void SkFlattenable::PrivateInitializer::InitEffects() {
        // Shaders.
        SK_REGISTER_FLATTENABLE(SkColor4Shader);
        SK_REGISTER_FLATTENABLE(SkColorFilterShader);
        SK_REGISTER_FLATTENABLE(SkColorShader);
        SK_REGISTER_FLATTENABLE(SkShader_Blend);
        SK_REGISTER_FLATTENABLE(SkShader_Lerp);
        SK_REGISTER_FLATTENABLE(SkEmptyShader);
        SK_REGISTER_FLATTENABLE(SkLocalMatrixShader);
        SK_REGISTER_FLATTENABLE(SkPictureShader);
        SkGradientShader::RegisterFlattenables();
        SkPerlinNoiseShader::RegisterFlattenables();
        SkShaderBase::RegisterFlattenables();

        // Color filters.
        SkColorFilter_Matrix::RegisterFlattenables();
        SK_REGISTER_FLATTENABLE(SkLumaColorFilter);
        SkColorFilter::RegisterFlattenables();
        SkHighContrastFilter::RegisterFlattenables();
        SkTableColorFilter::RegisterFlattenables();

        // Shader & color filter.
        SkRuntimeEffect::RegisterFlattenables();

        // Mask filters.
        SK_REGISTER_FLATTENABLE(SkEmbossMaskFilter);
        SkMaskFilter::RegisterFlattenables();
        SkShaderMaskFilter::RegisterFlattenables();

        // Path effects.
        SK_REGISTER_FLATTENABLE(SkCornerPathEffect);
        SK_REGISTER_FLATTENABLE(SkDashImpl);
        SK_REGISTER_FLATTENABLE(SkDiscretePathEffect);
        SK_REGISTER_FLATTENABLE(SkLine2DPathEffect);
        SK_REGISTER_FLATTENABLE(SkMatrixPE);
        SK_REGISTER_FLATTENABLE(SkOpPE);
        SK_REGISTER_FLATTENABLE(SkPath1DPathEffect);
        SK_REGISTER_FLATTENABLE(SkPath2DPathEffect);
        SK_REGISTER_FLATTENABLE(SkStrokePE);
        SK_REGISTER_FLATTENABLE(SkTrimPE);
        SkPathEffect::RegisterFlattenables();

        // Misc.
        SK_REGISTER_FLATTENABLE(SkLayerDrawLooper);
        SK_REGISTER_FLATTENABLE(SkRecordedDrawable);
    }

    /*
     *  Register SkImageFilters for deserialization.
     *
     *  None of these are strictly required for Skia to operate, so if you're
     *  not using deserialization yourself, you can define
     *  SK_DISABLE_EFFECT_SERIALIZATION, or modify/replace this file as needed.
     */
    void SkFlattenable::PrivateInitializer::InitImageFilters() {
        SkImageFilters::RegisterFlattenables();
        SK_REGISTER_FLATTENABLE(SkLocalMatrixImageFilter);
        SK_REGISTER_FLATTENABLE(SkMatrixImageFilter);
    }

#endif
