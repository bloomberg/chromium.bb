/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrSkSLFP_DEFINED
#define GrSkSLFP_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/effects/SkRuntimeEffect.h"
#include "include/gpu/GrContextOptions.h"
#include "src/gpu/GrFragmentProcessor.h"
#include <atomic>

class GrContext_Base;
class GrShaderCaps;
class SkData;
class SkRuntimeEffect;

class GrSkSLFP : public GrFragmentProcessor {
public:
    /**
     * Creates a new fragment processor from an SkRuntimeEffect and a data blob containing values
     * for all of the 'uniform' variables in the SkSL source. The layout of the uniforms blob is
     * dictated by the SkRuntimeEffect.
     */
    static std::unique_ptr<GrSkSLFP> Make(GrContext_Base* context,
                                          sk_sp<SkRuntimeEffect> effect,
                                          const char* name,
                                          sk_sp<SkData> uniforms);

    const char* name() const override;

    void addChild(std::unique_ptr<GrFragmentProcessor> child);

    std::unique_ptr<GrFragmentProcessor> clone() const override;

private:
    using ShaderErrorHandler = GrContextOptions::ShaderErrorHandler;

    GrSkSLFP(ShaderErrorHandler* shaderErrorHandler, sk_sp<SkRuntimeEffect> effect,
             const char* name, sk_sp<SkData> uniforms);

    GrSkSLFP(const GrSkSLFP& other);

    std::unique_ptr<GrGLSLFragmentProcessor> onMakeProgramImpl() const override;

    void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;

    bool onIsEqual(const GrFragmentProcessor&) const override;

    SkPMColor4f constantOutputForConstantInput(const SkPMColor4f&) const override;

    ShaderErrorHandler*       fShaderErrorHandler;

    sk_sp<SkRuntimeEffect> fEffect;
    const char*            fName;
    sk_sp<SkData>          fUniforms;

    GR_DECLARE_FRAGMENT_PROCESSOR_TEST

    using INHERITED = GrFragmentProcessor;

    friend class GrGLSLSkSLFP;

    friend class GrSkSLFPFactory;
};

class GrRuntimeFPBuilder : public SkRuntimeEffectBuilder<std::unique_ptr<GrFragmentProcessor>> {
public:
    ~GrRuntimeFPBuilder();

    template <const char* CODE> static GrRuntimeFPBuilder Make() {
        static const SkRuntimeEffect::Result gResult = SkRuntimeEffect::Make(SkString(CODE));
#ifdef SK_DEBUG
        if (!gResult.effect) {
            SK_ABORT("Code failed: %s", gResult.errorText.c_str());
        }
#endif
        return GrRuntimeFPBuilder(gResult.effect);
    }

    std::unique_ptr<GrFragmentProcessor> makeFP(GrRecordingContext*);

private:
    explicit GrRuntimeFPBuilder(sk_sp<SkRuntimeEffect>);
    GrRuntimeFPBuilder(GrRuntimeFPBuilder&& that) = default;

    using INHERITED = SkRuntimeEffectBuilder<std::unique_ptr<GrFragmentProcessor>>;
};

#endif
