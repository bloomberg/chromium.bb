/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLAnalysis.h"
#include "src/sksl/SkSLContext.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/ir/SkSLBlock.h"
#include "src/sksl/ir/SkSLExpressionStatement.h"
#include "src/sksl/ir/SkSLForStatement.h"
#include "src/sksl/ir/SkSLSymbolTable.h"
#include "src/sksl/ir/SkSLType.h"
#include "src/sksl/ir/SkSLVarDeclarations.h"

namespace SkSL {

static bool is_vardecl_block_initializer(const Statement* stmt) {
    if (!stmt) {
        return false;
    }
    if (!stmt->is<SkSL::Block>()) {
        return false;
    }
    const SkSL::Block& b = stmt->as<SkSL::Block>();
    if (b.isScope()) {
        return false;
    }
    for (const auto& child : b.children()) {
        if (!child->is<SkSL::VarDeclaration>()) {
            return false;
        }
    }
    return true;
}

static bool is_simple_initializer(const Statement* stmt) {
    return !stmt || stmt->isEmpty() || stmt->is<SkSL::VarDeclaration>() ||
           stmt->is<SkSL::ExpressionStatement>();
}

std::unique_ptr<Statement> ForStatement::clone() const {
    std::unique_ptr<LoopUnrollInfo> unrollInfo;
    if (fUnrollInfo) {
        unrollInfo = std::make_unique<LoopUnrollInfo>(*fUnrollInfo);
    }

    return std::make_unique<ForStatement>(
            fOffset,
            this->initializer() ? this->initializer()->clone() : nullptr,
            this->test() ? this->test()->clone() : nullptr,
            this->next() ? this->next()->clone() : nullptr,
            this->statement()->clone(),
            std::move(unrollInfo),
            SymbolTable::WrapIfBuiltin(this->symbols()));
}

String ForStatement::description() const {
    String result("for (");
    if (this->initializer()) {
        result += this->initializer()->description();
    } else {
        result += ";";
    }
    result += " ";
    if (this->test()) {
        result += this->test()->description();
    }
    result += "; ";
    if (this->next()) {
        result += this->next()->description();
    }
    result += ") " + this->statement()->description();
    return result;
}

std::unique_ptr<Statement> ForStatement::Convert(const Context& context, int offset,
                                                 std::unique_ptr<Statement> initializer,
                                                 std::unique_ptr<Expression> test,
                                                 std::unique_ptr<Expression> next,
                                                 std::unique_ptr<Statement> statement,
                                                 std::shared_ptr<SymbolTable> symbolTable) {
    bool isSimpleInitializer = is_simple_initializer(initializer.get());
    bool isVardeclBlockInitializer =
            !isSimpleInitializer && is_vardecl_block_initializer(initializer.get());

    if (!isSimpleInitializer && !isVardeclBlockInitializer) {
        context.fErrors->error(initializer->fOffset, "invalid for loop initializer");
        return nullptr;
    }

    if (test) {
        test = context.fTypes.fBool->coerceExpression(std::move(test), context);
        if (!test) {
            return nullptr;
        }
    }

    if (next) {
        // The type of the next-expression doesn't matter, but it needs to be a complete expression.
        // Report an error on intermediate expressions like FunctionReference or TypeReference.
        const Type& nextType = next->type();
        next = nextType.coerceExpression(std::move(next), context);
        if (!next) {
            return nullptr;
        }
    }

    std::unique_ptr<LoopUnrollInfo> unrollInfo;
    if (context.fConfig->strictES2Mode()) {
        unrollInfo = Analysis::GetLoopUnrollInfo(offset, initializer.get(), test.get(),
                                                 next.get(), statement.get(), context.fErrors);
        if (!unrollInfo) {
            return nullptr;
        }
    }

    if (isVardeclBlockInitializer) {
        // If the initializer statement of a for loop contains multiple variables, this causes
        // difficulties for several of our backends; e.g. Metal doesn't have a way to express arrays
        // of different size in the same decl-stmt, because the array-size is part of the type. It's
        // conceptually equivalent to synthesize a scope, declare the variables, and then emit a for
        // statement with an empty init-stmt. (Note that we can't just do this transformation
        // unilaterally for all for-statements, because the resulting for loop isn't ES2-compliant.)
        StatementArray scope;
        scope.push_back(std::move(initializer));
        scope.push_back(ForStatement::Make(context, offset, /*initializer=*/nullptr,
                                           std::move(test), std::move(next), std::move(statement),
                                           std::move(unrollInfo), std::move(symbolTable)));
        return Block::Make(offset, std::move(scope));
    }

    if (Analysis::DetectVarDeclarationWithoutScope(*statement, context.fErrors)) {
        return nullptr;
    }

    return ForStatement::Make(context, offset, std::move(initializer), std::move(test),
                              std::move(next), std::move(statement), std::move(unrollInfo),
                              std::move(symbolTable));
}

std::unique_ptr<Statement> ForStatement::ConvertWhile(const Context& context, int offset,
                                                      std::unique_ptr<Expression> test,
                                                      std::unique_ptr<Statement> statement,
                                                      std::shared_ptr<SymbolTable> symbolTable) {
    if (context.fConfig->strictES2Mode()) {
        context.fErrors->error(offset, "while loops are not supported");
        return nullptr;
    }
    return ForStatement::Convert(context, offset, /*initializer=*/nullptr, std::move(test),
                                 /*next=*/nullptr, std::move(statement), std::move(symbolTable));
}

std::unique_ptr<Statement> ForStatement::Make(const Context& context, int offset,
                                              std::unique_ptr<Statement> initializer,
                                              std::unique_ptr<Expression> test,
                                              std::unique_ptr<Expression> next,
                                              std::unique_ptr<Statement> statement,
                                              std::unique_ptr<LoopUnrollInfo> unrollInfo,
                                              std::shared_ptr<SymbolTable> symbolTable) {
    SkASSERT(is_simple_initializer(initializer.get()) ||
             is_vardecl_block_initializer(initializer.get()));
    SkASSERT(!test || test->type() == *context.fTypes.fBool);
    SkASSERT(!Analysis::DetectVarDeclarationWithoutScope(*statement));

    // If the caller didn't provide us with unroll info, we can compute it here if needed.
    if (!unrollInfo && context.fConfig->strictES2Mode()) {
        unrollInfo = Analysis::GetLoopUnrollInfo(offset, initializer.get(), test.get(),
                                                 next.get(), statement.get(), /*errors=*/nullptr);
        SkASSERT(unrollInfo);
    }

    return std::make_unique<ForStatement>(offset, std::move(initializer), std::move(test),
                                          std::move(next), std::move(statement),
                                          std::move(unrollInfo), std::move(symbolTable));
}

}  // namespace SkSL
