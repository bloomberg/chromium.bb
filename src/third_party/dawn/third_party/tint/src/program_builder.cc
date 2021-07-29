// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/program_builder.h"

#include "src/ast/assignment_statement.h"
#include "src/ast/call_statement.h"
#include "src/ast/variable_decl_statement.h"
#include "src/debug.h"
#include "src/demangler.h"
#include "src/sem/expression.h"
#include "src/sem/variable.h"

namespace tint {

ProgramBuilder::VarOptionals::~VarOptionals() = default;

ProgramBuilder::ProgramBuilder()
    : id_(ProgramID::New()),
      ast_(ast_nodes_.Create<ast::Module>(id_, Source{})) {}

ProgramBuilder::ProgramBuilder(ProgramBuilder&& rhs)
    : id_(std::move(rhs.id_)),
      types_(std::move(rhs.types_)),
      ast_nodes_(std::move(rhs.ast_nodes_)),
      sem_nodes_(std::move(rhs.sem_nodes_)),
      ast_(rhs.ast_),
      sem_(std::move(rhs.sem_)),
      symbols_(std::move(rhs.symbols_)),
      diagnostics_(std::move(rhs.diagnostics_)),
      transforms_applied_(std::move(rhs.transforms_applied_)) {
  rhs.MarkAsMoved();
}

ProgramBuilder::~ProgramBuilder() = default;

ProgramBuilder& ProgramBuilder::operator=(ProgramBuilder&& rhs) {
  rhs.MarkAsMoved();
  AssertNotMoved();
  id_ = std::move(rhs.id_);
  types_ = std::move(rhs.types_);
  ast_nodes_ = std::move(rhs.ast_nodes_);
  sem_nodes_ = std::move(rhs.sem_nodes_);
  ast_ = rhs.ast_;
  sem_ = std::move(rhs.sem_);
  symbols_ = std::move(rhs.symbols_);
  diagnostics_ = std::move(rhs.diagnostics_);
  transforms_applied_ = std::move(rhs.transforms_applied_);
  return *this;
}

ProgramBuilder ProgramBuilder::Wrap(const Program* program) {
  ProgramBuilder builder;
  builder.id_ = program->ID();
  builder.types_ = sem::Manager::Wrap(program->Types());
  builder.ast_ = builder.create<ast::Module>(
      program->AST().source(), program->AST().GlobalDeclarations());
  builder.sem_ = sem::Info::Wrap(program->Sem());
  builder.symbols_ = program->Symbols();
  builder.diagnostics_ = program->Diagnostics();
  builder.transforms_applied_ = program->TransformsApplied();
  return builder;
}

bool ProgramBuilder::IsValid() const {
  return !diagnostics_.contains_errors();
}

std::string ProgramBuilder::str(const ast::Node* node) const {
  return Demangler().Demangle(Symbols(), node->str(Sem()));
}

void ProgramBuilder::MarkAsMoved() {
  AssertNotMoved();
  moved_ = true;
}

void ProgramBuilder::AssertNotMoved() const {
  if (moved_) {
    TINT_ICE(ProgramBuilder, const_cast<ProgramBuilder*>(this)->diagnostics_)
        << "Attempting to use ProgramBuilder after it has been moved";
  }
}

sem::Type* ProgramBuilder::TypeOf(const ast::Expression* expr) const {
  auto* sem = Sem().Get(expr);
  return sem ? sem->Type() : nullptr;
}

sem::Type* ProgramBuilder::TypeOf(const ast::Variable* var) const {
  auto* sem = Sem().Get(var);
  return sem ? sem->Type() : nullptr;
}

const sem::Type* ProgramBuilder::TypeOf(const ast::Type* type) const {
  return Sem().Get(type);
}

const sem::Type* ProgramBuilder::TypeOf(const ast::TypeDecl* type_decl) const {
  return Sem().Get(type_decl);
}

ast::TypeName* ProgramBuilder::TypesBuilder::Of(ast::TypeDecl* decl) const {
  return type_name(decl->name());
}

const ast::TypeName* ProgramBuilder::TypesBuilder::Of(
    const ast::TypeDecl* decl) const {
  return type_name(decl->name());
}

ProgramBuilder::TypesBuilder::TypesBuilder(ProgramBuilder* pb) : builder(pb) {}

ast::Statement* ProgramBuilder::WrapInStatement(ast::Literal* lit) {
  return WrapInStatement(create<ast::ScalarConstructorExpression>(lit));
}

ast::Statement* ProgramBuilder::WrapInStatement(ast::Expression* expr) {
  if (auto* ce = expr->As<ast::CallExpression>()) {
    return Ignore(ce);
  }
  // Create a temporary variable of inferred type from expr.
  return Decl(Const(symbols_.New(), nullptr, expr));
}

ast::VariableDeclStatement* ProgramBuilder::WrapInStatement(ast::Variable* v) {
  return create<ast::VariableDeclStatement>(v);
}

ast::Statement* ProgramBuilder::WrapInStatement(ast::Statement* stmt) {
  return stmt;
}

ast::Function* ProgramBuilder::WrapInFunction(ast::StatementList stmts) {
  return Func("test_function", {}, ty.void_(), std::move(stmts),
              {create<ast::StageDecoration>(ast::PipelineStage::kCompute),
               WorkgroupSize(1, 1, 1)});
}

}  // namespace tint
