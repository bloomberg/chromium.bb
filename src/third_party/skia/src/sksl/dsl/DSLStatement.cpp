/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/sksl/DSLStatement.h"

#include "include/private/SkSLDefines.h"
#include "include/sksl/DSLBlock.h"
#include "include/sksl/DSLExpression.h"
#include "include/sksl/SkSLPosition.h"
#include "src/sksl/SkSLThreadContext.h"
#include "src/sksl/ir/SkSLBlock.h"
#include "src/sksl/ir/SkSLExpression.h"
#include "src/sksl/ir/SkSLExpressionStatement.h"
#include "src/sksl/ir/SkSLNop.h"

#if !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU
#include "src/gpu/ganesh/GrFragmentProcessor.h"
#include "src/gpu/ganesh/glsl/GrGLSLFragmentShaderBuilder.h"
#endif

namespace SkSL {

namespace dsl {

DSLStatement::DSLStatement() {}

DSLStatement::DSLStatement(DSLBlock block)
    : fStatement(block.release()) {}

DSLStatement::DSLStatement(DSLExpression expr) {
    std::unique_ptr<SkSL::Expression> skslExpr = expr.release();
    if (skslExpr) {
        fStatement = SkSL::ExpressionStatement::Make(ThreadContext::Context(), std::move(skslExpr));
    }
}

DSLStatement::DSLStatement(std::unique_ptr<SkSL::Expression> expr)
    : fStatement(SkSL::ExpressionStatement::Make(ThreadContext::Context(), std::move(expr))) {
    SkASSERT(this->hasValue());
}

DSLStatement::DSLStatement(std::unique_ptr<SkSL::Statement> stmt)
    : fStatement(std::move(stmt)) {
    SkASSERT(this->hasValue());
}

DSLStatement::DSLStatement(DSLPossibleExpression expr, Position pos)
    : DSLStatement(DSLExpression(std::move(expr), pos)) {}

DSLStatement::DSLStatement(DSLPossibleStatement stmt, Position pos) {
    ThreadContext::ReportErrors(pos);
    if (stmt.hasValue()) {
        fStatement = std::move(stmt.fStatement);
    } else {
        fStatement = SkSL::Nop::Make();
    }
    if (pos.valid() && !fStatement->fPosition.valid()) {
        fStatement->fPosition = pos;
    }
}

DSLStatement::~DSLStatement() {
#if !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU
    if (fStatement && ThreadContext::InFragmentProcessor()) {
        ThreadContext::CurrentEmitArgs()->fFragBuilder->codeAppend(this->release());
        return;
    }
#endif
    SkASSERTF(!fStatement || !ThreadContext::Settings().fAssertDSLObjectsReleased,
              "Statement destroyed without being incorporated into program (see "
              "ProgramSettings::fAssertDSLObjectsReleased)");
}

DSLPossibleStatement::DSLPossibleStatement(std::unique_ptr<SkSL::Statement> statement)
    : fStatement(std::move(statement)) {}

DSLPossibleStatement::~DSLPossibleStatement() {
    if (fStatement) {
        // this handles incorporating the expression into the output tree
        DSLStatement(std::move(fStatement));
    }
}

DSLStatement operator,(DSLStatement left, DSLStatement right) {
    Position pos = left.fStatement->fPosition;
    StatementArray stmts;
    stmts.reserve_back(2);
    stmts.push_back(left.release());
    stmts.push_back(right.release());
    return DSLStatement(SkSL::Block::Make(pos, std::move(stmts), Block::Kind::kCompoundStatement));
}

} // namespace dsl

} // namespace SkSL
