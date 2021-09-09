/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/dsl/priv/DSLFPs.h"

#include "src/sksl/dsl/priv/DSLWriter.h"
#include "src/sksl/ir/SkSLCodeStringExpression.h"

#if !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU

namespace SkSL {

namespace dsl {

void StartFragmentProcessor(GrGLSLFragmentProcessor* processor,
                            GrGLSLFragmentProcessor::EmitArgs* emitArgs) {
    DSLWriter::StartFragmentProcessor(processor, emitArgs);
}

void EndFragmentProcessor() {
    DSLWriter::EndFragmentProcessor();
}

DSLVar sk_SampleCoord() {
    return DSLVar("sk_SampleCoord");
}

DSLExpression SampleChild(int index, DSLExpression sampleExpr) {
    std::unique_ptr<SkSL::Expression> expr = sampleExpr.release();
    if (expr) {
        SkASSERT(expr->type().isVector());
        SkASSERT(expr->type().componentType().isFloat());
    }

    GrGLSLFragmentProcessor* proc = DSLWriter::CurrentProcessor();
    GrGLSLFragmentProcessor::EmitArgs& emitArgs = *DSLWriter::CurrentEmitArgs();
    SkString code;
    switch (expr ? expr->type().columns() : 0) {
        default:
            SkASSERTF(0, "unsupported SampleChild type: %s", expr->type().description().c_str());
            [[fallthrough]];
        case 0:
            code = proc->invokeChild(index, emitArgs);
            break;
        case 2:
            code = proc->invokeChild(index, emitArgs, /*skslCoords=*/expr->description());
            break;
        case 4:
            code = proc->invokeChild(index, /*inputColor=*/expr->description().c_str(), emitArgs);
            break;
    }

    return DSLExpression(std::make_unique<SkSL::CodeStringExpression>(
            code.c_str(), DSLWriter::Context().fTypes.fHalf4.get()));
}

GrGLSLUniformHandler::UniformHandle VarUniformHandle(const DSLVar& var) {
    return DSLWriter::VarUniformHandle(var);
}

} // namespace dsl

} // namespace SkSL

#endif // !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU
