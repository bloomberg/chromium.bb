// Copyright 2020 The Tint Authors.
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

#include "src/validator/validator_impl.h"

#include <cassert>
#include <unordered_set>

#include "src/ast/call_statement.h"
#include "src/ast/function.h"
#include "src/ast/int_literal.h"
#include "src/ast/intrinsic.h"
#include "src/ast/sint_literal.h"
#include "src/ast/switch_statement.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type/u32_type.h"
#include "src/ast/type/void_type.h"
#include "src/ast/uint_literal.h"
#include "src/ast/variable_decl_statement.h"

namespace tint {

ValidatorImpl::ValidatorImpl() = default;

ValidatorImpl::~ValidatorImpl() = default;

void ValidatorImpl::set_error(const Source& src, const std::string& msg) {
  error_ += std::to_string(src.range.begin.line) + ":" +
            std::to_string(src.range.begin.column) + ": " + msg;
}

bool ValidatorImpl::Validate(const ast::Module* module) {
  if (!module) {
    return false;
  }
  function_stack_.push_scope();
  if (!ValidateGlobalVariables(module->global_variables())) {
    return false;
  }
  if (!ValidateFunctions(module->functions())) {
    return false;
  }
  if (!ValidateEntryPoint(module->functions())) {
    return false;
  }
  function_stack_.pop_scope();

  return true;
}

bool ValidatorImpl::ValidateGlobalVariables(
    const ast::VariableList& global_vars) {
  for (const auto& var : global_vars) {
    if (variable_stack_.has(var->name())) {
      set_error(var->source(),
                "v-0011: redeclared global identifier '" + var->name() + "'");
      return false;
    }
    if (!var->is_const() && var->storage_class() == ast::StorageClass::kNone) {
      set_error(var->source(),
                "v-0022: global variables must have a storage class");
      return false;
    }
    if (var->is_const() &&
        !(var->storage_class() == ast::StorageClass::kNone)) {
      set_error(var->source(),
                "v-global01: global constants shouldn't have a storage class");
      return false;
    }
    variable_stack_.set_global(var->name(), var.get());
  }
  return true;
}

bool ValidatorImpl::ValidateFunctions(const ast::FunctionList& funcs) {
  for (const auto& func : funcs) {
    if (function_stack_.has(func->name())) {
      set_error(func->source(),
                "v-0016: function names must be unique '" + func->name() + "'");
      return false;
    }

    function_stack_.set(func->name(), func.get());
    current_function_ = func.get();
    if (!ValidateFunction(func.get())) {
      return false;
    }
    current_function_ = nullptr;
  }

  return true;
}

bool ValidatorImpl::ValidateEntryPoint(const ast::FunctionList& funcs) {
  auto shader_is_present = false;
  for (const auto& func : funcs) {
    if (func->IsEntryPoint()) {
      shader_is_present = true;
      if (!func->params().empty()) {
        set_error(func->source(),
                  "v-0023: Entry point function must accept no parameters: '" +
                      func->name() + "'");
        return false;
      }

      if (!func->return_type()->IsVoid()) {
        set_error(func->source(),
                  "v-0024: Entry point function must return void: '" +
                      func->name() + "'");
        return false;
      }
      auto stage_deco_count = 0;
      for (const auto& deco : func->decorations()) {
        if (deco->IsStage()) {
          stage_deco_count++;
        }
      }
      if (stage_deco_count > 1) {
        set_error(
            func->source(),
            "v-0020: only one stage decoration permitted per entry point");
        return false;
      }
    }
  }
  if (!shader_is_present) {
    set_error(Source{},
              "v-0003: At least one of vertex, fragment or compute shader must "
              "be present");
    return false;
  }
  return true;
}

bool ValidatorImpl::ValidateFunction(const ast::Function* func) {
  variable_stack_.push_scope();

  for (const auto& param : func->params()) {
    variable_stack_.set(param->name(), param.get());
  }
  if (!ValidateStatements(func->body())) {
    return false;
  }
  variable_stack_.pop_scope();

  if (!current_function_->return_type()->IsVoid()) {
    if (!func->get_last_statement() ||
        !func->get_last_statement()->IsReturn()) {
      set_error(func->source(),
                "v-0002: non-void function must end with a return statement");
      return false;
    }
  }
  return true;
}

bool ValidatorImpl::ValidateReturnStatement(const ast::ReturnStatement* ret) {
  // TODO(sarahM0): update this when this issue resolves:
  // https://github.com/gpuweb/gpuweb/issues/996
  ast::type::Type* func_type = current_function_->return_type();

  ast::type::VoidType void_type;
  auto* ret_type =
      ret->has_value() ? ret->value()->result_type()->UnwrapAll() : &void_type;

  if (func_type->type_name() != ret_type->type_name()) {
    set_error(ret->source(),
              "v-000y: return statement type must match its function return "
              "type, returned '" +
                  ret_type->type_name() + "', expected '" +
                  func_type->type_name() + "'");
    return false;
  }

  return true;
}

bool ValidatorImpl::ValidateStatements(const ast::BlockStatement* block) {
  if (!block) {
    return false;
  }
  for (const auto& stmt : *block) {
    if (!ValidateStatement(stmt.get())) {
      return false;
    }
  }
  return true;
}

bool ValidatorImpl::ValidateDeclStatement(
    const ast::VariableDeclStatement* decl) {
  auto name = decl->variable()->name();
  bool is_global = false;
  if (variable_stack_.get(name, nullptr, &is_global)) {
    std::string error_number = "v-0014: ";
    if (is_global) {
      error_number = "v-0013: ";
    }
    set_error(decl->source(),
              error_number + "redeclared identifier '" + name + "'");
    return false;
  }
  variable_stack_.set(name, decl->variable());
  return true;
}

bool ValidatorImpl::ValidateStatement(const ast::Statement* stmt) {
  if (!stmt) {
    return false;
  }
  if (stmt->IsVariableDecl()) {
    auto* v = stmt->AsVariableDecl();
    bool constructor_valid =
        v->variable()->has_constructor()
            ? ValidateExpression(v->variable()->constructor())
            : true;

    return constructor_valid && ValidateDeclStatement(stmt->AsVariableDecl());
  }
  if (stmt->IsAssign()) {
    return ValidateAssign(stmt->AsAssign());
  }
  if (stmt->IsReturn()) {
    return ValidateReturnStatement(stmt->AsReturn());
  }
  if (stmt->IsCall()) {
    return ValidateCallExpr(stmt->AsCall()->expr());
  }
  if (stmt->IsSwitch()) {
    return ValidateSwitch(stmt->AsSwitch());
  }
  if (stmt->IsCase()) {
    return ValidateCase(stmt->AsCase());
  }
  return true;
}

bool ValidatorImpl::ValidateSwitch(const ast::SwitchStatement* s) {
  if (!ValidateExpression(s->condition())) {
    return false;
  }

  auto* cond_type = s->condition()->result_type()->UnwrapAll();
  if (!(cond_type->IsI32() || cond_type->IsU32())) {
    set_error(s->condition()->source(),
              "v-0025: switch statement selector expression must be of a "
              "scalar integer type");
    return false;
  }

  int default_counter = 0;
  std::unordered_set<int32_t> selector_set;
  for (const auto& case_stmt : s->body()) {
    if (!ValidateStatement(case_stmt.get())) {
      return false;
    }

    if (case_stmt.get()->IsDefault()) {
      default_counter++;
    }

    for (const auto& selector : case_stmt.get()->selectors()) {
      auto* selector_ptr = selector.get();
      if (cond_type != selector_ptr->type()) {
        set_error(case_stmt.get()->source(),
                  "v-0026: the case selector values must have the same "
                  "type as the selector expression.");
        return false;
      }

      auto v = static_cast<int32_t>(selector_ptr->type()->IsU32()
                                        ? selector_ptr->AsUint()->value()
                                        : selector_ptr->AsSint()->value());
      if (selector_set.count(v)) {
        auto v_str = selector_ptr->type()->IsU32()
                         ? selector_ptr->AsUint()->to_str()
                         : selector_ptr->AsSint()->to_str();
        set_error(case_stmt.get()->source(),
                  "v-0027: a literal value must not appear more than once in "
                  "the case selectors for a switch statement: '" +
                      v_str + "'");
        return false;
      }
      selector_set.emplace(v);
    }
  }

  if (default_counter != 1) {
    set_error(s->source(),
              "v-0008: switch statement must have exactly one default clause");
    return false;
  }

  auto* last_clause = s->body().back().get();
  auto* last_stmt_of_last_clause = last_clause->AsCase()->body()->last();
  if (last_stmt_of_last_clause && last_stmt_of_last_clause->IsFallthrough()) {
    set_error(last_stmt_of_last_clause->source(),
              "v-0028: a fallthrough statement must not appear as "
              "the last statement in last clause of a switch");
    return false;
  }
  return true;
}

bool ValidatorImpl::ValidateCase(const ast::CaseStatement* c) {
  if (!ValidateStatement(c->body())) {
    return false;
  }
  return true;
}

bool ValidatorImpl::ValidateCallExpr(const ast::CallExpression* expr) {
  if (!expr) {
    // TODO(sarahM0): Here and other Validate.*: figure out whether return
    // false or true
    return false;
  }

  if (expr->func()->IsIdentifier()) {
    auto* ident = expr->func()->AsIdentifier();
    auto func_name = ident->name();
    if (ident->IsIntrinsic()) {
      // TODO(sarahM0): validate intrinsics - tied with type-determiner
    } else {
      if (!function_stack_.has(func_name)) {
        set_error(expr->source(),
                  "v-0005: function must be declared before use: '" +
                      func_name + "'");
        return false;
      }
      if (func_name == current_function_->name()) {
        set_error(expr->source(),
                  "v-0004: recursion is not allowed: '" + func_name + "'");
        return false;
      }
    }
  } else {
    set_error(expr->source(), "Invalid function call expression");
    return false;
  }

  return true;
}

bool ValidatorImpl::ValidateAssign(const ast::AssignmentStatement* a) {
  if (!a) {
    return false;
  }
  if (!(ValidateConstant(a))) {
    return false;
  }
  if (!(ValidateExpression(a->lhs()) && ValidateExpression(a->rhs()))) {
    return false;
  }
  if (!ValidateResultTypes(a)) {
    return false;
  }

  return true;
}

bool ValidatorImpl::ValidateConstant(const ast::AssignmentStatement* assign) {
  if (!assign) {
    return false;
  }

  if (assign->lhs()->IsIdentifier()) {
    ast::Variable* var;
    auto* ident = assign->lhs()->AsIdentifier();
    if (variable_stack_.get(ident->name(), &var)) {
      if (var->is_const()) {
        set_error(assign->source(), "v-0021: cannot re-assign a constant: '" +
                                        ident->name() + "'");
        return false;
      }
    }
  }
  return true;
}

bool ValidatorImpl::ValidateResultTypes(const ast::AssignmentStatement* a) {
  if (!a->lhs()->result_type() || !a->rhs()->result_type()) {
    set_error(a->source(), "result_type() is nullptr");
    return false;
  }

  auto* lhs_result_type = a->lhs()->result_type()->UnwrapAll();
  auto* rhs_result_type = a->rhs()->result_type()->UnwrapAll();
  if (lhs_result_type != rhs_result_type) {
    // TODO(sarahM0): figur out what should be the error number.
    set_error(a->source(), "v-000x: invalid assignment of '" +
                               lhs_result_type->type_name() + "' to '" +
                               rhs_result_type->type_name() + "'");
    return false;
  }
  return true;
}

bool ValidatorImpl::ValidateExpression(const ast::Expression* expr) {
  if (!expr) {
    return false;
  }
  if (expr->IsIdentifier()) {
    return ValidateIdentifier(expr->AsIdentifier());
  }

  if (expr->IsCall()) {
    return ValidateCallExpr(expr->AsCall());
  }
  return true;
}

bool ValidatorImpl::ValidateIdentifier(const ast::IdentifierExpression* ident) {
  ast::Variable* var;
  if (!variable_stack_.get(ident->name(), &var)) {
    set_error(ident->source(),
              "v-0006: '" + ident->name() + "' is not declared");
    return false;
  }
  return true;
}

}  // namespace tint
