/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrD3DPipelineStateBuilder_DEFINED
#define GrD3DPipelineStateBuilder_DEFINED

#include "src/gpu/GrPipeline.h"
#include "src/gpu/GrSPIRVUniformHandler.h"
#include "src/gpu/GrSPIRVVaryingHandler.h"
#include "src/gpu/d3d/GrD3DPipelineState.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/sksl/ir/SkSLProgram.h"

class GrProgramDesc;
class GrD3DGpu;
class GrVkRenderPass;
class SkReader32;

class GrD3DPipelineStateBuilder : public GrGLSLProgramBuilder {
public:
    /** Generates a pipeline state.
     *
     * The GrD3DPipelineState implements what is specified in the GrPipeline and
     * GrPrimitiveProcessor as input. After successful generation, the builder result objects are
     * available to be used.
     * @return the created pipeline if generation was successful; nullptr otherwise
     */
    static sk_sp<GrD3DPipelineState> MakePipelineState(GrD3DGpu*, GrRenderTarget*,
                                                       const GrProgramDesc&,
                                                       const GrProgramInfo&);

    const GrCaps* caps() const override;

    GrD3DGpu* gpu() const { return fGpu; }

    void finalizeFragmentOutputColor(GrShaderVar& outputColor) override;
    void finalizeFragmentSecondaryColor(GrShaderVar& outputColor) override;

private:
    GrD3DPipelineStateBuilder(GrD3DGpu*, GrRenderTarget*, const GrProgramDesc&,
                              const GrProgramInfo&);

    sk_sp<GrD3DPipelineState> finalize();

    void compileD3DProgram(SkSL::Program::Kind kind,
                           const SkSL::String& sksl,
                           const SkSL::Program::Settings& settings,
                           ID3DBlob** shader,
                           SkSL::Program::Inputs* outInputs);

    GrGLSLUniformHandler* uniformHandler() override { return &fUniformHandler; }
    const GrGLSLUniformHandler* uniformHandler() const override { return &fUniformHandler; }
    GrGLSLVaryingHandler* varyingHandler() override { return &fVaryingHandler; }

    GrD3DGpu* fGpu;
    GrSPIRVVaryingHandler fVaryingHandler;
    GrSPIRVUniformHandler fUniformHandler;

    typedef GrGLSLProgramBuilder INHERITED;
};

#endif
