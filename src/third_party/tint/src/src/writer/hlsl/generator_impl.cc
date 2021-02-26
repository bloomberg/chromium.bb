/// Copyright 2020 The Tint Authors.
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

#include "src/writer/hlsl/generator_impl.h"

#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#include "src/ast/array_accessor_expression.h"
#include "src/ast/assignment_statement.h"
#include "src/ast/binary_expression.h"
#include "src/ast/bitcast_expression.h"
#include "src/ast/bool_literal.h"
#include "src/ast/call_expression.h"
#include "src/ast/call_statement.h"
#include "src/ast/case_statement.h"
#include "src/ast/decorated_variable.h"
#include "src/ast/else_statement.h"
#include "src/ast/float_literal.h"
#include "src/ast/identifier_expression.h"
#include "src/ast/if_statement.h"
#include "src/ast/loop_statement.h"
#include "src/ast/member_accessor_expression.h"
#include "src/ast/return_statement.h"
#include "src/ast/sint_literal.h"
#include "src/ast/struct.h"
#include "src/ast/switch_statement.h"
#include "src/ast/type/access_control_type.h"
#include "src/ast/type/alias_type.h"
#include "src/ast/type/array_type.h"
#include "src/ast/type/f32_type.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type/matrix_type.h"
#include "src/ast/type/sampler_type.h"
#include "src/ast/type/struct_type.h"
#include "src/ast/type/texture_type.h"
#include "src/ast/type/vector_type.h"
#include "src/ast/uint_literal.h"
#include "src/ast/unary_op_expression.h"
#include "src/ast/variable_decl_statement.h"

namespace tint {
namespace writer {
namespace hlsl {
namespace {

const char kInStructNameSuffix[] = "in";
const char kOutStructNameSuffix[] = "out";
const char kTintStructInVarPrefix[] = "tint_in";
const char kTintStructOutVarPrefix[] = "tint_out";
const char kTempNamePrefix[] = "_tint_tmp";

bool last_is_break_or_fallthrough(const ast::BlockStatement* stmts) {
  if (stmts->empty()) {
    return false;
  }

  return stmts->last()->IsBreak() || stmts->last()->IsFallthrough();
}

std::string get_buffer_name(ast::Expression* expr) {
  for (;;) {
    if (expr->IsIdentifier()) {
      return expr->AsIdentifier()->name();
    } else if (expr->IsMemberAccessor()) {
      expr = expr->AsMemberAccessor()->structure();
    } else if (expr->IsArrayAccessor()) {
      expr = expr->AsArrayAccessor()->array();
    } else {
      break;
    }
  }
  return "";
}

uint32_t convert_swizzle_to_index(const std::string& swizzle) {
  if (swizzle == "r" || swizzle == "x") {
    return 0;
  }
  if (swizzle == "g" || swizzle == "y") {
    return 1;
  }
  if (swizzle == "b" || swizzle == "z") {
    return 2;
  }
  if (swizzle == "a" || swizzle == "w") {
    return 3;
  }
  return 0;
}

}  // namespace

GeneratorImpl::GeneratorImpl(ast::Module* module) : module_(module) {}

GeneratorImpl::~GeneratorImpl() = default;

void GeneratorImpl::make_indent(std::ostream& out) {
  for (size_t i = 0; i < indent_; i++) {
    out << " ";
  }
}

bool GeneratorImpl::Generate(std::ostream& out) {
  for (const auto& global : module_->global_variables()) {
    register_global(global.get());
  }

  for (auto* const ty : module_->constructed_types()) {
    if (!EmitConstructedType(out, ty)) {
      return false;
    }
  }
  if (!module_->constructed_types().empty()) {
    out << std::endl;
  }

  for (const auto& var : module_->global_variables()) {
    if (!var->is_const()) {
      continue;
    }
    if (!EmitProgramConstVariable(out, var.get())) {
      return false;
    }
  }

  std::unordered_set<std::string> emitted_globals;
  // Make sure all entry point data is emitted before the entry point functions
  for (const auto& func : module_->functions()) {
    if (!func->IsEntryPoint()) {
      continue;
    }

    if (!EmitEntryPointData(out, func.get(), emitted_globals)) {
      return false;
    }
  }

  for (const auto& func : module_->functions()) {
    if (!EmitFunction(out, func.get())) {
      return false;
    }
  }

  for (const auto& func : module_->functions()) {
    if (!func->IsEntryPoint()) {
      continue;
    }
    if (!EmitEntryPointFunction(out, func.get())) {
      return false;
    }
    out << std::endl;
  }
  return true;
}

void GeneratorImpl::register_global(ast::Variable* global) {
  global_variables_.set(global->name(), global);
}

std::string GeneratorImpl::generate_name(const std::string& prefix) {
  std::string name = prefix;
  uint32_t i = 0;
  while (namer_.IsMapped(name) || namer_.IsRemapped(name)) {
    name = prefix + "_" + std::to_string(i);
    ++i;
  }
  namer_.RegisterRemappedName(name);
  return name;
}

std::string GeneratorImpl::current_ep_var_name(VarType type) {
  std::string name = "";
  switch (type) {
    case VarType::kIn: {
      auto in_it = ep_name_to_in_data_.find(current_ep_name_);
      if (in_it != ep_name_to_in_data_.end()) {
        name = in_it->second.var_name;
      }
      break;
    }
    case VarType::kOut: {
      auto outit = ep_name_to_out_data_.find(current_ep_name_);
      if (outit != ep_name_to_out_data_.end()) {
        name = outit->second.var_name;
      }
      break;
    }
  }
  return name;
}

bool GeneratorImpl::EmitConstructedType(std::ostream& out,
                                        const ast::type::Type* ty) {
  make_indent(out);

  if (ty->IsAlias()) {
    auto* alias = ty->AsAlias();
    // HLSL typedef is for intrinsic types only. For an alias'd struct,
    // generate a secondary struct with the new name.
    if (alias->type()->IsStruct()) {
      if (!EmitStructType(out, alias->type()->AsStruct(), alias->name())) {
        return false;
      }
      return true;
    }
    out << "typedef ";
    if (!EmitType(out, alias->type(), "")) {
      return false;
    }
    out << " " << namer_.NameFor(alias->name()) << ";" << std::endl;
  } else if (ty->IsStruct()) {
    auto* str = ty->AsStruct();
    if (!EmitStructType(out, str, str->name())) {
      return false;
    }
  } else {
    error_ = "unknown constructed type: " + ty->type_name();
    return false;
  }

  return true;
}

bool GeneratorImpl::EmitArrayAccessor(std::ostream& pre,
                                      std::ostream& out,
                                      ast::ArrayAccessorExpression* expr) {
  // Handle writing into a storage buffer array
  if (is_storage_buffer_access(expr)) {
    return EmitStorageBufferAccessor(pre, out, expr, nullptr);
  }

  if (!EmitExpression(pre, out, expr->array())) {
    return false;
  }
  out << "[";

  if (!EmitExpression(pre, out, expr->idx_expr())) {
    return false;
  }
  out << "]";

  return true;
}

bool GeneratorImpl::EmitBitcast(std::ostream& pre,
                                std::ostream& out,
                                ast::BitcastExpression* expr) {
  if (!expr->type()->IsF32() && !expr->type()->IsI32() &&
      !expr->type()->IsU32()) {
    error_ = "Unable to do bitcast to type " + expr->type()->type_name();
    return false;
  }

  out << "as";
  if (!EmitType(out, expr->type(), "")) {
    return false;
  }
  out << "(";
  if (!EmitExpression(pre, out, expr->expr())) {
    return false;
  }
  out << ")";
  return true;
}

bool GeneratorImpl::EmitAssign(std::ostream& out,
                               ast::AssignmentStatement* stmt) {
  make_indent(out);

  std::ostringstream pre;

  // If the LHS is an accessor into a storage buffer then we have to
  // emit a Store operation instead of an ='s.
  if (stmt->lhs()->IsMemberAccessor()) {
    auto* mem = stmt->lhs()->AsMemberAccessor();
    if (is_storage_buffer_access(mem)) {
      std::ostringstream accessor_out;
      if (!EmitStorageBufferAccessor(pre, accessor_out, mem, stmt->rhs())) {
        return false;
      }
      out << pre.str();
      out << accessor_out.str() << ";" << std::endl;
      return true;
    }
  } else if (stmt->lhs()->IsArrayAccessor()) {
    auto* ary = stmt->lhs()->AsArrayAccessor();
    if (is_storage_buffer_access(ary)) {
      std::ostringstream accessor_out;
      if (!EmitStorageBufferAccessor(pre, accessor_out, ary, stmt->rhs())) {
        return false;
      }
      out << pre.str();
      out << accessor_out.str() << ";" << std::endl;
      return true;
    }
  }

  std::ostringstream lhs_out;
  if (!EmitExpression(pre, lhs_out, stmt->lhs())) {
    return false;
  }
  std::ostringstream rhs_out;
  if (!EmitExpression(pre, rhs_out, stmt->rhs())) {
    return false;
  }

  out << pre.str();
  out << lhs_out.str() << " = " << rhs_out.str() << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitBinary(std::ostream& pre,
                               std::ostream& out,
                               ast::BinaryExpression* expr) {
  if (expr->op() == ast::BinaryOp::kLogicalAnd ||
      expr->op() == ast::BinaryOp::kLogicalOr) {
    std::ostringstream lhs_out;
    if (!EmitExpression(pre, lhs_out, expr->lhs())) {
      return false;
    }

    auto name = generate_name(kTempNamePrefix);
    make_indent(pre);
    pre << "bool " << name << " = " << lhs_out.str() << ";" << std::endl;

    make_indent(pre);
    pre << "if (";
    if (expr->op() == ast::BinaryOp::kLogicalOr) {
      pre << "!";
    }
    pre << name << ") {" << std::endl;
    increment_indent();

    std::ostringstream rhs_out;
    if (!EmitExpression(pre, rhs_out, expr->rhs())) {
      return false;
    }

    make_indent(pre);
    pre << name << " = " << rhs_out.str() << ";" << std::endl;

    decrement_indent();
    make_indent(pre);
    pre << "}" << std::endl;

    out << "(" << name << ")";
    return true;
  }

  out << "(";
  if (!EmitExpression(pre, out, expr->lhs())) {
    return false;
  }
  out << " ";

  switch (expr->op()) {
    case ast::BinaryOp::kAnd:
      out << "&";
      break;
    case ast::BinaryOp::kOr:
      out << "|";
      break;
    case ast::BinaryOp::kXor:
      out << "^";
      break;
    case ast::BinaryOp::kLogicalAnd:
    case ast::BinaryOp::kLogicalOr: {
      // These are both handled above.
      assert(false);
      return false;
    }
    case ast::BinaryOp::kEqual:
      out << "==";
      break;
    case ast::BinaryOp::kNotEqual:
      out << "!=";
      break;
    case ast::BinaryOp::kLessThan:
      out << "<";
      break;
    case ast::BinaryOp::kGreaterThan:
      out << ">";
      break;
    case ast::BinaryOp::kLessThanEqual:
      out << "<=";
      break;
    case ast::BinaryOp::kGreaterThanEqual:
      out << ">=";
      break;
    case ast::BinaryOp::kShiftLeft:
      out << "<<";
      break;
    case ast::BinaryOp::kShiftRight:
      // TODO(dsinclair): MSL is based on C++14, and >> in C++14 has
      // implementation-defined behaviour for negative LHS.  We may have to
      // generate extra code to implement WGSL-specified behaviour for negative
      // LHS.
      out << R"(>>)";
      break;

    case ast::BinaryOp::kAdd:
      out << "+";
      break;
    case ast::BinaryOp::kSubtract:
      out << "-";
      break;
    case ast::BinaryOp::kMultiply:
      out << "*";
      break;
    case ast::BinaryOp::kDivide:
      out << "/";
      break;
    case ast::BinaryOp::kModulo:
      out << "%";
      break;
    case ast::BinaryOp::kNone:
      error_ = "missing binary operation type";
      return false;
  }
  out << " ";

  if (!EmitExpression(pre, out, expr->rhs())) {
    return false;
  }

  out << ")";
  return true;
}

bool GeneratorImpl::EmitBlock(std::ostream& out,
                              const ast::BlockStatement* stmt) {
  out << "{" << std::endl;
  increment_indent();

  for (const auto& s : *stmt) {
    if (!EmitStatement(out, s.get())) {
      return false;
    }
  }

  decrement_indent();
  make_indent(out);
  out << "}";

  return true;
}

bool GeneratorImpl::EmitBlockAndNewline(std::ostream& out,
                                        const ast::BlockStatement* stmt) {
  const bool result = EmitBlock(out, stmt);
  if (result) {
    out << std::endl;
  }
  return result;
}

bool GeneratorImpl::EmitIndentedBlockAndNewline(std::ostream& out,
                                                ast::BlockStatement* stmt) {
  make_indent(out);
  const bool result = EmitBlock(out, stmt);
  if (result) {
    out << std::endl;
  }
  return result;
}

bool GeneratorImpl::EmitBreak(std::ostream& out, ast::BreakStatement*) {
  make_indent(out);
  out << "break;" << std::endl;
  return true;
}

std::string GeneratorImpl::generate_intrinsic_name(ast::Intrinsic intrinsic) {
  if (intrinsic == ast::Intrinsic::kAny) {
    return "any";
  }
  if (intrinsic == ast::Intrinsic::kAll) {
    return "all";
  }
  if (intrinsic == ast::Intrinsic::kCountOneBits) {
    return "countbits";
  }
  if (intrinsic == ast::Intrinsic::kDot) {
    return "dot";
  }
  if (intrinsic == ast::Intrinsic::kDpdy) {
    return "ddy";
  }
  if (intrinsic == ast::Intrinsic::kDpdyFine) {
    return "ddy_fine";
  }
  if (intrinsic == ast::Intrinsic::kDpdyCoarse) {
    return "ddy_coarse";
  }
  if (intrinsic == ast::Intrinsic::kDpdx) {
    return "ddx";
  }
  if (intrinsic == ast::Intrinsic::kDpdxFine) {
    return "ddx_fine";
  }
  if (intrinsic == ast::Intrinsic::kDpdxCoarse) {
    return "ddx_coarse";
  }
  if (intrinsic == ast::Intrinsic::kFwidth ||
      intrinsic == ast::Intrinsic::kFwidthFine ||
      intrinsic == ast::Intrinsic::kFwidthCoarse) {
    return "fwidth";
  }
  if (intrinsic == ast::Intrinsic::kIsFinite) {
    return "isfinite";
  }
  if (intrinsic == ast::Intrinsic::kIsInf) {
    return "isinf";
  }
  if (intrinsic == ast::Intrinsic::kIsNan) {
    return "isnan";
  }
  if (intrinsic == ast::Intrinsic::kReverseBits) {
    return "reversebits";
  }
  return "";
}

bool GeneratorImpl::EmitCall(std::ostream& pre,
                             std::ostream& out,
                             ast::CallExpression* expr) {
  if (!expr->func()->IsIdentifier()) {
    error_ = "invalid function name";
    return 0;
  }

  auto* ident = expr->func()->AsIdentifier();
  if (ident->IsIntrinsic()) {
    const auto& params = expr->params();
    if (ident->intrinsic() == ast::Intrinsic::kSelect) {
      error_ = "select not supported in HLSL backend yet";
      return false;
    } else if (ident->intrinsic() == ast::Intrinsic::kIsNormal) {
      error_ = "is_normal not supported in HLSL backend yet";
      return false;
    } else if (ident->intrinsic() == ast::Intrinsic::kOuterProduct) {
      error_ = "outer_product not supported yet";
      return false;
      // TODO(dsinclair): This gets tricky. We need to generate two variables to
      // hold the outer_product expressions, but we maybe inside an expression
      // ourselves. So, this will need to, possibly, output the variables
      // _before_ the expression which contains the outer product.
      //
      // This then has the follow on, what if we have `(false &&
      // outer_product())` in that case, we shouldn't evaluate the expressions
      // at all because of short circuting.
      //
      // So .... this turns out to be hard ...

      // // We create variables to hold the two parameters in case they're
      // // function calls with side effects.
      // auto* param0 = param[0].get();
      // auto* name0 = generate_name("outer_product_expr_0");

      // auto* param1 = param[1].get();
      // auto* name1 = generate_name("outer_product_expr_1");

      // make_indent(out);
      // if (!EmitType(out, expr->result_type(), "")) {
      //   return false;
      // }
      // out << "(";

      // auto param1_type = params[1]->result_type()->UnwrapPtrIfNeeded();
      // if (!param1_type->IsVector()) {
      //   error_ = "invalid param type in outer_product got: " +
      //            param1_type->type_name();
      //   return false;
      // }

      // for (uint32_t i = 0; i < param1_type->AsVector()->size(); ++i) {
      //   if (i > 0) {
      //     out << ", ";
      //   }

      //   if (!EmitExpression(pre, out, params[0].get())) {
      //     return false;
      //   }
      //   out << " * ";

      //   if (!EmitExpression(pre, out, params[1].get())) {
      //     return false;
      //   }
      //   out << "[" << i << "]";
      // }

      // out << ")";
    } else {
      auto name = generate_intrinsic_name(ident->intrinsic());
      if (name.empty()) {
        if (ast::intrinsic::IsTextureIntrinsic(ident->intrinsic())) {
          error_ = "Textures not implemented yet";
          return false;
        }
        name = generate_builtin_name(expr);
        if (name.empty()) {
          return false;
        }
      }

      make_indent(out);
      out << name << "(";

      bool first = true;
      for (const auto& param : params) {
        if (!first) {
          out << ", ";
        }
        first = false;

        if (!EmitExpression(pre, out, param.get())) {
          return false;
        }
      }

      out << ")";
    }
    return true;
  }

  auto name = ident->name();
  auto it = ep_func_name_remapped_.find(current_ep_name_ + "_" + name);
  if (it != ep_func_name_remapped_.end()) {
    name = it->second;
  }

  auto* func = module_->FindFunctionByName(ident->name());
  if (func == nullptr) {
    error_ = "Unable to find function: " + name;
    return false;
  }

  out << name << "(";

  bool first = true;
  if (has_referenced_in_var_needing_struct(func)) {
    auto var_name = current_ep_var_name(VarType::kIn);
    if (!var_name.empty()) {
      out << var_name;
      first = false;
    }
  }
  if (has_referenced_out_var_needing_struct(func)) {
    auto var_name = current_ep_var_name(VarType::kOut);
    if (!var_name.empty()) {
      if (!first) {
        out << ", ";
      }
      first = false;
      out << var_name;
    }
  }

  const auto& params = expr->params();
  for (const auto& param : params) {
    if (!first) {
      out << ", ";
    }
    first = false;

    if (!EmitExpression(pre, out, param.get())) {
      return false;
    }
  }

  out << ")";

  return true;
}

std::string GeneratorImpl::generate_builtin_name(ast::CallExpression* expr) {
  std::string out;
  auto* ident = expr->func()->AsIdentifier();
  switch (ident->intrinsic()) {
    case ast::Intrinsic::kAcos:
    case ast::Intrinsic::kAsin:
    case ast::Intrinsic::kAtan:
    case ast::Intrinsic::kAtan2:
    case ast::Intrinsic::kCeil:
    case ast::Intrinsic::kCos:
    case ast::Intrinsic::kCosh:
    case ast::Intrinsic::kCross:
    case ast::Intrinsic::kDeterminant:
    case ast::Intrinsic::kDistance:
    case ast::Intrinsic::kExp:
    case ast::Intrinsic::kExp2:
    case ast::Intrinsic::kFloor:
    case ast::Intrinsic::kFma:
    case ast::Intrinsic::kLength:
    case ast::Intrinsic::kLog:
    case ast::Intrinsic::kLog2:
    case ast::Intrinsic::kNormalize:
    case ast::Intrinsic::kPow:
    case ast::Intrinsic::kReflect:
    case ast::Intrinsic::kRound:
    case ast::Intrinsic::kSin:
    case ast::Intrinsic::kSinh:
    case ast::Intrinsic::kSqrt:
    case ast::Intrinsic::kStep:
    case ast::Intrinsic::kTan:
    case ast::Intrinsic::kTanh:
    case ast::Intrinsic::kTrunc:
    case ast::Intrinsic::kMix:
    case ast::Intrinsic::kSign:
    case ast::Intrinsic::kAbs:
    case ast::Intrinsic::kMax:
    case ast::Intrinsic::kMin:
    case ast::Intrinsic::kClamp:
      out = ident->name();
      break;
    case ast::Intrinsic::kFaceForward:
      out = "faceforward";
      break;
    case ast::Intrinsic::kFract:
      out = "frac";
      break;
    case ast::Intrinsic::kInverseSqrt:
      out = "rsqrt";
      break;
    case ast::Intrinsic::kSmoothStep:
      out = "smoothstep";
      break;
    default:
      error_ = "Unknown builtin method: " + ident->name();
      return "";
  }

  return out;
}

bool GeneratorImpl::EmitCase(std::ostream& out, ast::CaseStatement* stmt) {
  make_indent(out);

  if (stmt->IsDefault()) {
    out << "default:";
  } else {
    bool first = true;
    for (const auto& selector : stmt->selectors()) {
      if (!first) {
        out << std::endl;
        make_indent(out);
      }
      first = false;

      out << "case ";
      if (!EmitLiteral(out, selector.get())) {
        return false;
      }
      out << ":";
    }
  }

  out << " {" << std::endl;

  increment_indent();

  for (const auto& s : *(stmt->body())) {
    if (!EmitStatement(out, s.get())) {
      return false;
    }
  }

  if (!last_is_break_or_fallthrough(stmt->body())) {
    make_indent(out);
    out << "break;" << std::endl;
  }

  decrement_indent();
  make_indent(out);
  out << "}" << std::endl;

  return true;
}

bool GeneratorImpl::EmitConstructor(std::ostream& pre,
                                    std::ostream& out,
                                    ast::ConstructorExpression* expr) {
  if (expr->IsScalarConstructor()) {
    return EmitScalarConstructor(pre, out, expr->AsScalarConstructor());
  }
  return EmitTypeConstructor(pre, out, expr->AsTypeConstructor());
}

bool GeneratorImpl::EmitScalarConstructor(
    std::ostream&,
    std::ostream& out,
    ast::ScalarConstructorExpression* expr) {
  return EmitLiteral(out, expr->literal());
}

bool GeneratorImpl::EmitTypeConstructor(std::ostream& pre,
                                        std::ostream& out,
                                        ast::TypeConstructorExpression* expr) {
  if (expr->type()->IsArray()) {
    out << "{";
  } else {
    if (!EmitType(out, expr->type(), "")) {
      return false;
    }
    out << "(";
  }

  // If the type constructor is empty then we need to construct with the zero
  // value for all components.
  if (expr->values().empty()) {
    if (!EmitZeroValue(out, expr->type())) {
      return false;
    }
  } else {
    bool first = true;
    for (const auto& e : expr->values()) {
      if (!first) {
        out << ", ";
      }
      first = false;

      if (!EmitExpression(pre, out, e.get())) {
        return false;
      }
    }
  }

  if (expr->type()->IsArray()) {
    out << "}";
  } else {
    out << ")";
  }
  return true;
}

bool GeneratorImpl::EmitContinue(std::ostream& out, ast::ContinueStatement*) {
  make_indent(out);
  out << "continue;" << std::endl;
  return true;
}

bool GeneratorImpl::EmitDiscard(std::ostream& out, ast::DiscardStatement*) {
  make_indent(out);
  // TODO(dsinclair): Verify this is correct when the discard semantics are
  // defined for WGSL (https://github.com/gpuweb/gpuweb/issues/361)
  out << "discard;" << std::endl;
  return true;
}

bool GeneratorImpl::EmitExpression(std::ostream& pre,
                                   std::ostream& out,
                                   ast::Expression* expr) {
  if (expr->IsArrayAccessor()) {
    return EmitArrayAccessor(pre, out, expr->AsArrayAccessor());
  }
  if (expr->IsBinary()) {
    return EmitBinary(pre, out, expr->AsBinary());
  }
  if (expr->IsBitcast()) {
    return EmitBitcast(pre, out, expr->AsBitcast());
  }
  if (expr->IsCall()) {
    return EmitCall(pre, out, expr->AsCall());
  }
  if (expr->IsConstructor()) {
    return EmitConstructor(pre, out, expr->AsConstructor());
  }
  if (expr->IsIdentifier()) {
    return EmitIdentifier(pre, out, expr->AsIdentifier());
  }
  if (expr->IsMemberAccessor()) {
    return EmitMemberAccessor(pre, out, expr->AsMemberAccessor());
  }
  if (expr->IsUnaryOp()) {
    return EmitUnaryOp(pre, out, expr->AsUnaryOp());
  }

  error_ = "unknown expression type: " + expr->str();
  return false;
}

bool GeneratorImpl::global_is_in_struct(ast::Variable* var) const {
  return var->IsDecorated() &&
         (var->AsDecorated()->HasLocationDecoration() ||
          var->AsDecorated()->HasBuiltinDecoration()) &&
         (var->storage_class() == ast::StorageClass::kInput ||
          var->storage_class() == ast::StorageClass::kOutput);
}

bool GeneratorImpl::EmitIdentifier(std::ostream&,
                                   std::ostream& out,
                                   ast::IdentifierExpression* expr) {
  auto* ident = expr->AsIdentifier();
  ast::Variable* var = nullptr;
  if (global_variables_.get(ident->name(), &var)) {
    if (global_is_in_struct(var)) {
      auto var_type = var->storage_class() == ast::StorageClass::kInput
                          ? VarType::kIn
                          : VarType::kOut;
      auto name = current_ep_var_name(var_type);
      if (name.empty()) {
        error_ = "unable to find entry point data for variable";
        return false;
      }
      out << name << ".";
    }
  }
  out << namer_.NameFor(ident->name());

  return true;
}

bool GeneratorImpl::EmitIf(std::ostream& out, ast::IfStatement* stmt) {
  make_indent(out);

  std::ostringstream pre;
  std::ostringstream cond;
  if (!EmitExpression(pre, cond, stmt->condition())) {
    return false;
  }

  std::ostringstream if_out;
  if_out << "if (" << cond.str() << ") ";
  if (!EmitBlock(if_out, stmt->body())) {
    return false;
  }

  for (const auto& e : stmt->else_statements()) {
    if (e->HasCondition()) {
      if_out << " else {" << std::endl;

      increment_indent();

      std::ostringstream else_pre;
      std::ostringstream else_cond_out;
      if (!EmitExpression(else_pre, else_cond_out, e->condition())) {
        return false;
      }
      if_out << else_pre.str();

      make_indent(if_out);
      if_out << "if (" << else_cond_out.str() << ") ";
    } else {
      if_out << " else ";
    }

    if (!EmitBlock(if_out, e->body())) {
      return false;
    }
  }
  if_out << std::endl;

  for (const auto& e : stmt->else_statements()) {
    if (!e->HasCondition()) {
      continue;
    }

    decrement_indent();
    make_indent(if_out);
    if_out << "}" << std::endl;
  }

  out << pre.str();
  out << if_out.str();
  return true;
}

bool GeneratorImpl::has_referenced_in_var_needing_struct(ast::Function* func) {
  for (auto data : func->referenced_location_variables()) {
    auto* var = data.first;
    if (var->storage_class() == ast::StorageClass::kInput) {
      return true;
    }
  }

  for (auto data : func->referenced_builtin_variables()) {
    auto* var = data.first;
    if (var->storage_class() == ast::StorageClass::kInput) {
      return true;
    }
  }
  return false;
}

bool GeneratorImpl::has_referenced_out_var_needing_struct(ast::Function* func) {
  for (auto data : func->referenced_location_variables()) {
    auto* var = data.first;
    if (var->storage_class() == ast::StorageClass::kOutput) {
      return true;
    }
  }

  for (auto data : func->referenced_builtin_variables()) {
    auto* var = data.first;
    if (var->storage_class() == ast::StorageClass::kOutput) {
      return true;
    }
  }
  return false;
}

bool GeneratorImpl::has_referenced_var_needing_struct(ast::Function* func) {
  for (auto data : func->referenced_location_variables()) {
    auto* var = data.first;
    if (var->storage_class() == ast::StorageClass::kOutput ||
        var->storage_class() == ast::StorageClass::kInput) {
      return true;
    }
  }

  for (auto data : func->referenced_builtin_variables()) {
    auto* var = data.first;
    if (var->storage_class() == ast::StorageClass::kOutput ||
        var->storage_class() == ast::StorageClass::kInput) {
      return true;
    }
  }
  return false;
}

bool GeneratorImpl::EmitFunction(std::ostream& out, ast::Function* func) {
  make_indent(out);

  // Entry points will be emitted later, skip for now.
  if (func->IsEntryPoint()) {
    return true;
  }

  // TODO(dsinclair): This could be smarter. If the input/outputs for multiple
  // entry points are the same we could generate a single struct and then have
  // this determine it's the same struct and just emit once.
  bool emit_duplicate_functions = func->ancestor_entry_points().size() > 0 &&
                                  has_referenced_var_needing_struct(func);

  if (emit_duplicate_functions) {
    for (const auto& ep_name : func->ancestor_entry_points()) {
      if (!EmitFunctionInternal(out, func, emit_duplicate_functions, ep_name)) {
        return false;
      }
      out << std::endl;
    }
  } else {
    // Emit as non-duplicated
    if (!EmitFunctionInternal(out, func, false, "")) {
      return false;
    }
    out << std::endl;
  }

  return true;
}

bool GeneratorImpl::EmitFunctionInternal(std::ostream& out,
                                         ast::Function* func,
                                         bool emit_duplicate_functions,
                                         const std::string& ep_name) {
  auto name = func->name();

  if (!EmitType(out, func->return_type(), "")) {
    return false;
  }

  out << " ";

  if (emit_duplicate_functions) {
    name = generate_name(name + "_" + ep_name);
    ep_func_name_remapped_[ep_name + "_" + func->name()] = name;
  } else {
    name = namer_.NameFor(name);
  }

  out << name << "(";

  bool first = true;

  // If we're emitting duplicate functions that means the function takes
  // the stage_in or stage_out value from the entry point, emit them.
  //
  // We emit both of them if they're there regardless of if they're both used.
  if (emit_duplicate_functions) {
    auto in_it = ep_name_to_in_data_.find(ep_name);
    if (in_it != ep_name_to_in_data_.end()) {
      out << "in " << in_it->second.struct_name << " "
          << in_it->second.var_name;
      first = false;
    }

    auto outit = ep_name_to_out_data_.find(ep_name);
    if (outit != ep_name_to_out_data_.end()) {
      if (!first) {
        out << ", ";
      }
      out << "out " << outit->second.struct_name << " "
          << outit->second.var_name;
      first = false;
    }
  }

  for (const auto& v : func->params()) {
    if (!first) {
      out << ", ";
    }
    first = false;

    if (!EmitType(out, v->type(), v->name())) {
      return false;
    }
    // Array name is output as part of the type
    if (!v->type()->IsArray()) {
      out << " " << v->name();
    }
  }

  out << ") ";

  current_ep_name_ = ep_name;

  if (!EmitBlockAndNewline(out, func->body())) {
    return false;
  }

  current_ep_name_ = "";

  return true;
}

bool GeneratorImpl::EmitEntryPointData(
    std::ostream& out,
    ast::Function* func,
    std::unordered_set<std::string>& emitted_globals) {
  std::vector<std::pair<ast::Variable*, ast::VariableDecoration*>> in_variables;
  std::vector<std::pair<ast::Variable*, ast::VariableDecoration*>> outvariables;
  for (auto data : func->referenced_location_variables()) {
    auto* var = data.first;
    auto* deco = data.second;

    if (var->storage_class() == ast::StorageClass::kInput) {
      in_variables.push_back({var, deco});
    } else if (var->storage_class() == ast::StorageClass::kOutput) {
      outvariables.push_back({var, deco});
    }
  }

  for (auto data : func->referenced_builtin_variables()) {
    auto* var = data.first;
    auto* deco = data.second;

    if (var->storage_class() == ast::StorageClass::kInput) {
      in_variables.push_back({var, deco});
    } else if (var->storage_class() == ast::StorageClass::kOutput) {
      outvariables.push_back({var, deco});
    }
  }

  bool emitted_uniform = false;
  for (auto data : func->referenced_uniform_variables()) {
    auto* var = data.first;
    // TODO(dsinclair): We're using the binding to make up the buffer number but
    // we should instead be using a provided mapping that uses both buffer and
    // set. https://bugs.chromium.org/p/tint/issues/detail?id=104
    auto* binding = data.second.binding;
    if (binding == nullptr) {
      error_ = "unable to find binding information for uniform: " + var->name();
      return false;
    }
    // auto* set = data.second.set;

    // If the global has already been emitted we skip it, it's been emitted by
    // a previous entry point.
    if (emitted_globals.count(var->name()) != 0) {
      continue;
    }
    emitted_globals.insert(var->name());

    auto* type = var->type()->UnwrapIfNeeded();
    if (type->IsStruct()) {
      auto* strct = type->AsStruct();

      out << "ConstantBuffer<" << strct->name() << "> " << var->name()
          << " : register(b" << binding->value() << ");" << std::endl;
    } else {
      // TODO(dsinclair): There is outstanding spec work to require all uniform
      // buffers to be [[block]] decorated, which means structs. This is
      // currently not the case, so this code handles the cases where the data
      // is not a block.
      // Relevant: https://github.com/gpuweb/gpuweb/issues/1004
      //           https://github.com/gpuweb/gpuweb/issues/1008
      out << "cbuffer : register(b" << binding->value() << ") {" << std::endl;

      increment_indent();
      make_indent(out);
      if (!EmitType(out, type, "")) {
        return false;
      }
      out << " " << var->name() << ";" << std::endl;
      decrement_indent();
      out << "};" << std::endl;
    }

    emitted_uniform = true;
  }
  if (emitted_uniform) {
    out << std::endl;
  }

  bool emitted_storagebuffer = false;
  for (auto data : func->referenced_storagebuffer_variables()) {
    auto* var = data.first;
    auto* binding = data.second.binding;

    // If the global has already been emitted we skip it, it's been emitted by
    // a previous entry point.
    if (emitted_globals.count(var->name()) != 0) {
      continue;
    }
    emitted_globals.insert(var->name());

    if (!var->type()->IsAccessControl()) {
      error_ = "access control type required for storage buffer";
      return false;
    }
    auto* ac = var->type()->AsAccessControl();

    if (ac->IsReadWrite()) {
      out << "RW";
    }
    out << "ByteAddressBuffer " << var->name() << " : register(u"
        << binding->value() << ");" << std::endl;
    emitted_storagebuffer = true;
  }
  if (emitted_storagebuffer) {
    out << std::endl;
  }

  if (!in_variables.empty()) {
    auto in_struct_name =
        generate_name(func->name() + "_" + kInStructNameSuffix);
    auto in_var_name = generate_name(kTintStructInVarPrefix);
    ep_name_to_in_data_[func->name()] = {in_struct_name, in_var_name};

    make_indent(out);
    out << "struct " << in_struct_name << " {" << std::endl;

    increment_indent();

    for (auto& data : in_variables) {
      auto* var = data.first;
      auto* deco = data.second;

      make_indent(out);
      if (!EmitType(out, var->type(), var->name())) {
        return false;
      }

      out << " " << var->name() << " : ";
      if (deco->IsLocation()) {
        if (func->pipeline_stage() == ast::PipelineStage::kCompute) {
          error_ = "invalid location variable for pipeline stage";
          return false;
        }
        out << "TEXCOORD" << deco->AsLocation()->value();
      } else if (deco->IsBuiltin()) {
        auto attr = builtin_to_attribute(deco->AsBuiltin()->value());
        if (attr.empty()) {
          error_ = "unsupported builtin";
          return false;
        }
        out << attr;
      } else {
        error_ = "unsupported variable decoration for entry point output";
        return false;
      }
      out << ";" << std::endl;
    }
    decrement_indent();
    make_indent(out);

    out << "};" << std::endl << std::endl;
  }

  if (!outvariables.empty()) {
    auto outstruct_name =
        generate_name(func->name() + "_" + kOutStructNameSuffix);
    auto outvar_name = generate_name(kTintStructOutVarPrefix);
    ep_name_to_out_data_[func->name()] = {outstruct_name, outvar_name};

    make_indent(out);
    out << "struct " << outstruct_name << " {" << std::endl;

    increment_indent();
    for (auto& data : outvariables) {
      auto* var = data.first;
      auto* deco = data.second;

      make_indent(out);
      if (!EmitType(out, var->type(), var->name())) {
        return false;
      }

      out << " " << var->name() << " : ";

      if (deco->IsLocation()) {
        auto loc = deco->AsLocation()->value();
        if (func->pipeline_stage() == ast::PipelineStage::kVertex) {
          out << "TEXCOORD" << loc;
        } else if (func->pipeline_stage() == ast::PipelineStage::kFragment) {
          out << "SV_Target" << loc << "";
        } else {
          error_ = "invalid location variable for pipeline stage";
          return false;
        }
      } else if (deco->IsBuiltin()) {
        auto attr = builtin_to_attribute(deco->AsBuiltin()->value());
        if (attr.empty()) {
          error_ = "unsupported builtin";
          return false;
        }
        out << attr;
      } else {
        error_ = "unsupported variable decoration for entry point output";
        return false;
      }
      out << ";" << std::endl;
    }
    decrement_indent();
    make_indent(out);
    out << "};" << std::endl << std::endl;
  }

  return true;
}

std::string GeneratorImpl::builtin_to_attribute(ast::Builtin builtin) const {
  switch (builtin) {
    case ast::Builtin::kPosition:
      return "SV_Position";
    case ast::Builtin::kVertexIdx:
      return "SV_VertexID";
    case ast::Builtin::kInstanceIdx:
      return "SV_InstanceID";
    case ast::Builtin::kFrontFacing:
      return "SV_IsFrontFacing";
    case ast::Builtin::kFragCoord:
      return "SV_Position";
    case ast::Builtin::kFragDepth:
      return "SV_Depth";
    case ast::Builtin::kLocalInvocationId:
      return "SV_GroupThreadID";
    case ast::Builtin::kLocalInvocationIdx:
      return "SV_GroupIndex";
    case ast::Builtin::kGlobalInvocationId:
      return "SV_DispatchThreadID";
    default:
      break;
  }
  return "";
}

bool GeneratorImpl::EmitEntryPointFunction(std::ostream& out,
                                           ast::Function* func) {
  make_indent(out);

  current_ep_name_ = func->name();

  if (func->pipeline_stage() == ast::PipelineStage::kCompute) {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
    std::tie(x, y, z) = func->workgroup_size();
    out << "[numthreads(" << std::to_string(x) << ", " << std::to_string(y)
        << ", " << std::to_string(z) << ")]" << std::endl;
    make_indent(out);
  }

  auto outdata = ep_name_to_out_data_.find(current_ep_name_);
  bool has_outdata = outdata != ep_name_to_out_data_.end();
  if (has_outdata) {
    out << outdata->second.struct_name;
  } else {
    out << "void";
  }
  out << " " << namer_.NameFor(current_ep_name_) << "(";

  auto in_data = ep_name_to_in_data_.find(current_ep_name_);
  if (in_data != ep_name_to_in_data_.end()) {
    out << in_data->second.struct_name << " " << in_data->second.var_name;
  }
  out << ") {" << std::endl;

  increment_indent();

  if (has_outdata) {
    make_indent(out);
    out << outdata->second.struct_name << " " << outdata->second.var_name << ";"
        << std::endl;
  }

  generating_entry_point_ = true;
  for (const auto& s : *(func->body())) {
    if (!EmitStatement(out, s.get())) {
      return false;
    }
  }
  generating_entry_point_ = false;

  decrement_indent();
  make_indent(out);
  out << "}" << std::endl;

  current_ep_name_ = "";

  return true;
}

bool GeneratorImpl::EmitLiteral(std::ostream& out, ast::Literal* lit) {
  if (lit->IsBool()) {
    out << (lit->AsBool()->IsTrue() ? "true" : "false");
  } else if (lit->IsFloat()) {
    auto flags = out.flags();
    auto precision = out.precision();

    out.flags(flags | std::ios_base::showpoint);
    out.precision(std::numeric_limits<float>::max_digits10);

    out << lit->AsFloat()->value() << "f";

    out.precision(precision);
    out.flags(flags);
  } else if (lit->IsSint()) {
    out << lit->AsSint()->value();
  } else if (lit->IsUint()) {
    out << lit->AsUint()->value() << "u";
  } else {
    error_ = "unknown literal type";
    return false;
  }
  return true;
}

bool GeneratorImpl::EmitZeroValue(std::ostream& out, ast::type::Type* type) {
  if (type->IsBool()) {
    out << "false";
  } else if (type->IsF32()) {
    out << "0.0f";
  } else if (type->IsI32()) {
    out << "0";
  } else if (type->IsU32()) {
    out << "0u";
  } else if (type->IsVector()) {
    return EmitZeroValue(out, type->AsVector()->type());
  } else if (type->IsMatrix()) {
    auto* mat = type->AsMatrix();
    for (uint32_t i = 0; i < (mat->rows() * mat->columns()); i++) {
      if (i != 0) {
        out << ", ";
      }
      if (!EmitZeroValue(out, mat->type())) {
        return false;
      }
    }
  } else {
    error_ = "Invalid type for zero emission: " + type->type_name();
    return false;
  }
  return true;
}

bool GeneratorImpl::EmitLoop(std::ostream& out, ast::LoopStatement* stmt) {
  loop_emission_counter_++;

  std::string guard = namer_.NameFor("tint_hlsl_is_first_" +
                                     std::to_string(loop_emission_counter_));

  if (stmt->has_continuing()) {
    make_indent(out);

    // Continuing variables get their own scope.
    out << "{" << std::endl;
    increment_indent();

    make_indent(out);
    out << "bool " << guard << " = true;" << std::endl;

    // A continuing block may use variables declared in the method body. As a
    // first pass, if we have a continuing, we pull all declarations outside
    // the for loop into the continuing scope. Then, the variable declarations
    // will be turned into assignments.
    for (const auto& s : *(stmt->body())) {
      if (!s->IsVariableDecl()) {
        continue;
      }
      if (!EmitVariable(out, s->AsVariableDecl()->variable(), true)) {
        return false;
      }
    }
  }

  make_indent(out);
  out << "for(;;) {" << std::endl;
  increment_indent();

  if (stmt->has_continuing()) {
    make_indent(out);
    out << "if (!" << guard << ") ";

    if (!EmitBlockAndNewline(out, stmt->continuing())) {
      return false;
    }

    make_indent(out);
    out << guard << " = false;" << std::endl;
    out << std::endl;
  }

  for (const auto& s : *(stmt->body())) {
    // If we have a continuing block we've already emitted the variable
    // declaration before the loop, so treat it as an assignment.
    if (s->IsVariableDecl() && stmt->has_continuing()) {
      make_indent(out);

      auto* var = s->AsVariableDecl()->variable();

      std::ostringstream pre;
      std::ostringstream constructor_out;
      if (var->constructor() != nullptr) {
        if (!EmitExpression(pre, constructor_out, var->constructor())) {
          return false;
        }
      }
      out << pre.str();

      out << var->name() << " = ";
      if (var->constructor() != nullptr) {
        out << constructor_out.str();
      } else {
        if (!EmitZeroValue(out, var->type())) {
          return false;
        }
      }
      out << ";" << std::endl;
      continue;
    }

    if (!EmitStatement(out, s.get())) {
      return false;
    }
  }

  decrement_indent();
  make_indent(out);
  out << "}" << std::endl;

  // Close the scope for any continuing variables.
  if (stmt->has_continuing()) {
    decrement_indent();
    make_indent(out);
    out << "}" << std::endl;
  }

  return true;
}

std::string GeneratorImpl::generate_storage_buffer_index_expression(
    std::ostream& pre,
    ast::Expression* expr) {
  std::ostringstream out;
  bool first = true;
  for (;;) {
    if (expr->IsIdentifier()) {
      break;
    }

    if (!first) {
      out << " + ";
    }
    first = false;
    if (expr->IsMemberAccessor()) {
      auto* mem = expr->AsMemberAccessor();
      auto* res_type = mem->structure()->result_type()->UnwrapAll();
      if (res_type->IsStruct()) {
        auto* str_type = res_type->AsStruct()->impl();
        auto* str_member = str_type->get_member(mem->member()->name());

        if (!str_member->has_offset_decoration()) {
          error_ = "missing offset decoration for struct member";
          return "";
        }
        out << str_member->offset();

      } else if (res_type->IsVector()) {
        // This must be a single element swizzle if we've got a vector at this
        // point.
        if (mem->member()->name().size() != 1) {
          error_ =
              "Encountered multi-element swizzle when should have only one "
              "level";
          return "";
        }

        // TODO(dsinclair): All our types are currently 4 bytes (f32, i32, u32)
        // so this is assuming 4. This will need to be fixed when we get f16 or
        // f64 types.
        out << "(4 * " << convert_swizzle_to_index(mem->member()->name())
            << ")";
      } else {
        error_ =
            "Invalid result type for member accessor: " + res_type->type_name();
        return "";
      }

      expr = mem->structure();
    } else if (expr->IsArrayAccessor()) {
      auto* ary = expr->AsArrayAccessor();
      auto* ary_type = ary->array()->result_type()->UnwrapAll();

      out << "(";
      if (ary_type->IsArray()) {
        out << ary_type->AsArray()->array_stride();
      } else if (ary_type->IsVector()) {
        // TODO(dsinclair): This is a hack. Our vectors can only be f32, i32
        // or u32 which are all 4 bytes. When we get f16 or other types we'll
        // have to ask the type for the byte size.
        out << "4";
      } else if (ary_type->IsMatrix()) {
        auto* mat = ary_type->AsMatrix();
        if (mat->columns() == 2) {
          out << "8";
        } else {
          out << "16";
        }
      } else {
        error_ = "Invalid array type in storage buffer access";
        return "";
      }
      out << " * ";
      if (!EmitExpression(pre, out, ary->idx_expr())) {
        return "";
      }
      out << ")";

      expr = ary->array();
    } else {
      error_ = "error emitting storage buffer access";
      return "";
    }
  }

  return out.str();
}

// TODO(dsinclair): This currently only handles loading of 4, 8, 12 or 16 byte
// members. If we need to support larger we'll need to do the loading into
// chunks.
//
// TODO(dsinclair): Need to support loading through a pointer. The pointer is
// just a memory address in the storage buffer, so need to do the correct
// calculation.
bool GeneratorImpl::EmitStorageBufferAccessor(std::ostream& pre,
                                              std::ostream& out,
                                              ast::Expression* expr,
                                              ast::Expression* rhs) {
  auto* result_type = expr->result_type()->UnwrapAll();
  bool is_store = rhs != nullptr;

  std::string access_method = is_store ? "Store" : "Load";
  if (result_type->IsVector()) {
    access_method += std::to_string(result_type->AsVector()->size());
  } else if (result_type->IsMatrix()) {
    access_method += std::to_string(result_type->AsMatrix()->rows());
  }

  // If we aren't storing then we need to put in the outer cast.
  if (!is_store) {
    if (result_type->is_float_scalar_or_vector() || result_type->IsMatrix()) {
      out << "asfloat(";
    } else if (result_type->is_signed_scalar_or_vector()) {
      out << "asint(";
    } else if (result_type->is_unsigned_scalar_or_vector()) {
      out << "asuint(";
    }
  }

  auto buffer_name = get_buffer_name(expr);
  if (buffer_name.empty()) {
    error_ = "error emitting storage buffer access";
    return false;
  }

  auto idx = generate_storage_buffer_index_expression(pre, expr);
  if (idx.empty()) {
    return false;
  }

  if (result_type->IsMatrix()) {
    auto* mat = result_type->AsMatrix();

    // TODO(dsinclair): This is assuming 4 byte elements. Will need to be fixed
    // if we get matrixes of f16 or f64.
    uint32_t stride = mat->rows() == 2 ? 8 : 16;

    if (is_store) {
      if (!EmitType(out, mat, "")) {
        return false;
      }

      auto name = generate_name(kTempNamePrefix);
      out << " " << name << " = ";
      if (!EmitExpression(pre, out, rhs)) {
        return false;
      }
      out << ";" << std::endl;

      for (uint32_t i = 0; i < mat->columns(); i++) {
        if (i > 0) {
          out << ";" << std::endl;
        }

        make_indent(out);
        out << buffer_name << "." << access_method << "(" << idx << " + "
            << (i * stride) << ", asuint(" << name << "[" << i << "]))";
      }

      return true;
    }

    out << "matrix<uint, " << mat->rows() << ", " << mat->columns() << ">(";

    for (uint32_t i = 0; i < mat->columns(); i++) {
      if (i != 0) {
        out << ", ";
      }

      out << buffer_name << "." << access_method << "(" << idx << " + "
          << (i * stride) << ")";
    }

    // Close the matrix type and outer cast
    out << "))";

    return true;
  }

  out << buffer_name << "." << access_method << "(" << idx;
  if (is_store) {
    out << ", asuint(";
    if (!EmitExpression(pre, out, rhs)) {
      return false;
    }
    out << ")";
  }

  out << ")";

  // Close the outer cast.
  if (!is_store) {
    out << ")";
  }

  return true;
}

bool GeneratorImpl::is_storage_buffer_access(
    ast::ArrayAccessorExpression* expr) {
  // We only care about array so we can get to the next part of the expression.
  // If it isn't an array or a member accessor we can stop looking as it won't
  // be a storage buffer.
  auto* ary = expr->array();
  if (ary->IsMemberAccessor()) {
    return is_storage_buffer_access(ary->AsMemberAccessor());
  } else if (ary->IsArrayAccessor()) {
    return is_storage_buffer_access(ary->AsArrayAccessor());
  }
  return false;
}

bool GeneratorImpl::is_storage_buffer_access(
    ast::MemberAccessorExpression* expr) {
  auto* structure = expr->structure();
  auto* data_type = structure->result_type()->UnwrapAll();
  // If the data is a multi-element swizzle then we will not load the swizzle
  // portion through the Load command.
  if (data_type->IsVector() && expr->member()->name().size() > 1) {
    return false;
  }

  // Check if this is a storage buffer variable
  if (structure->IsIdentifier()) {
    auto* ident = expr->structure()->AsIdentifier();
    ast::Variable* var = nullptr;
    if (!global_variables_.get(ident->name(), &var)) {
      return false;
    }
    return var->storage_class() == ast::StorageClass::kStorageBuffer;
  } else if (structure->IsMemberAccessor()) {
    return is_storage_buffer_access(structure->AsMemberAccessor());
  } else if (structure->IsArrayAccessor()) {
    return is_storage_buffer_access(structure->AsArrayAccessor());
  }

  // Technically I don't think this is possible, but if we don't have a struct
  // or array accessor then we can't have a storage buffer I believe.
  return false;
}

bool GeneratorImpl::EmitMemberAccessor(std::ostream& pre,
                                       std::ostream& out,
                                       ast::MemberAccessorExpression* expr) {
  // Look for storage buffer accesses as we have to convert them into Load
  // expressions. Stores will be identified in the assignment emission and a
  // member accessor store of a storage buffer will not get here.
  if (is_storage_buffer_access(expr)) {
    return EmitStorageBufferAccessor(pre, out, expr, nullptr);
  }

  if (!EmitExpression(pre, out, expr->structure())) {
    return false;
  }
  out << ".";
  return EmitExpression(pre, out, expr->member());
}

bool GeneratorImpl::EmitReturn(std::ostream& out, ast::ReturnStatement* stmt) {
  make_indent(out);

  if (generating_entry_point_) {
    out << "return";
    auto outdata = ep_name_to_out_data_.find(current_ep_name_);
    if (outdata != ep_name_to_out_data_.end()) {
      out << " " << outdata->second.var_name;
    }
  } else if (stmt->has_value()) {
    std::ostringstream pre;
    std::ostringstream ret_out;
    if (!EmitExpression(pre, ret_out, stmt->value())) {
      return false;
    }
    out << pre.str();
    out << "return " << ret_out.str();
  } else {
    out << "return";
  }
  out << ";" << std::endl;
  return true;
}

bool GeneratorImpl::EmitStatement(std::ostream& out, ast::Statement* stmt) {
  if (stmt->IsAssign()) {
    return EmitAssign(out, stmt->AsAssign());
  }
  if (stmt->IsBlock()) {
    return EmitIndentedBlockAndNewline(out, stmt->AsBlock());
  }
  if (stmt->IsBreak()) {
    return EmitBreak(out, stmt->AsBreak());
  }
  if (stmt->IsCall()) {
    make_indent(out);
    std::ostringstream pre;
    std::ostringstream call_out;
    if (!EmitCall(pre, call_out, stmt->AsCall()->expr())) {
      return false;
    }
    out << pre.str();
    out << call_out.str() << ";" << std::endl;
    return true;
  }
  if (stmt->IsContinue()) {
    return EmitContinue(out, stmt->AsContinue());
  }
  if (stmt->IsDiscard()) {
    return EmitDiscard(out, stmt->AsDiscard());
  }
  if (stmt->IsFallthrough()) {
    make_indent(out);
    out << "/* fallthrough */" << std::endl;
    return true;
  }
  if (stmt->IsIf()) {
    return EmitIf(out, stmt->AsIf());
  }
  if (stmt->IsLoop()) {
    return EmitLoop(out, stmt->AsLoop());
  }
  if (stmt->IsReturn()) {
    return EmitReturn(out, stmt->AsReturn());
  }
  if (stmt->IsSwitch()) {
    return EmitSwitch(out, stmt->AsSwitch());
  }
  if (stmt->IsVariableDecl()) {
    return EmitVariable(out, stmt->AsVariableDecl()->variable(), false);
  }

  error_ = "unknown statement type: " + stmt->str();
  return false;
}

bool GeneratorImpl::EmitSwitch(std::ostream& out, ast::SwitchStatement* stmt) {
  make_indent(out);

  std::ostringstream pre;
  std::ostringstream cond;
  if (!EmitExpression(pre, cond, stmt->condition())) {
    return false;
  }

  out << pre.str();
  out << "switch(" << cond.str() << ") {" << std::endl;

  increment_indent();

  for (const auto& s : stmt->body()) {
    if (!EmitCase(out, s.get())) {
      return false;
    }
  }

  decrement_indent();
  make_indent(out);
  out << "}" << std::endl;

  return true;
}

bool GeneratorImpl::EmitType(std::ostream& out,
                             ast::type::Type* type,
                             const std::string& name) {
  if (type->IsAlias()) {
    auto* alias = type->AsAlias();
    out << namer_.NameFor(alias->name());
  } else if (type->IsArray()) {
    auto* ary = type->AsArray();

    ast::type::Type* base_type = ary;
    std::vector<uint32_t> sizes;
    while (base_type->IsArray()) {
      if (base_type->AsArray()->IsRuntimeArray()) {
        // TODO(dsinclair): Support runtime arrays
        // https://bugs.chromium.org/p/tint/issues/detail?id=185
        error_ = "runtime array not supported yet.";
        return false;
      } else {
        sizes.push_back(base_type->AsArray()->size());
      }
      base_type = base_type->AsArray()->type();
    }
    if (!EmitType(out, base_type, "")) {
      return false;
    }
    if (!name.empty()) {
      out << " " << namer_.NameFor(name);
    }
    for (uint32_t size : sizes) {
      out << "[" << size << "]";
    }
  } else if (type->IsBool()) {
    out << "bool";
  } else if (type->IsF32()) {
    out << "float";
  } else if (type->IsI32()) {
    out << "int";
  } else if (type->IsMatrix()) {
    auto* mat = type->AsMatrix();
    out << "matrix<";
    if (!EmitType(out, mat->type(), "")) {
      return false;
    }
    out << ", " << mat->rows() << ", " << mat->columns() << ">";
  } else if (type->IsPointer()) {
    // TODO(dsinclair): What do we do with pointers in HLSL?
    // https://bugs.chromium.org/p/tint/issues/detail?id=183
    error_ = "pointers not supported in HLSL";
    return false;
  } else if (type->IsSampler()) {
    auto* sampler = type->AsSampler();
    out << "Sampler";
    if (sampler->IsComparison()) {
      out << "Comparison";
    }
    out << "State";
  } else if (type->IsStruct()) {
    out << type->AsStruct()->name();
  } else if (type->IsTexture()) {
    auto* tex = type->AsTexture();
    if (tex->IsStorage()) {
      out << "RW";
    }
    out << "Texture";

    switch (tex->dim()) {
      case ast::type::TextureDimension::k1d:
        out << "1D";
        break;
      case ast::type::TextureDimension::k1dArray:
        out << "1DArray";
        break;
      case ast::type::TextureDimension::k2d:
        out << "2D";
        break;
      case ast::type::TextureDimension::k2dArray:
        out << "2DArray";
        break;
      case ast::type::TextureDimension::k3d:
        out << "3D";
        break;
      case ast::type::TextureDimension::kCube:
        out << "Cube";
        break;
      case ast::type::TextureDimension::kCubeArray:
        out << "CubeArray";
        break;
      default:
        error_ = "Invalid texture dimensions";
        return false;
    }

  } else if (type->IsU32()) {
    out << "uint";
  } else if (type->IsVector()) {
    auto* vec = type->AsVector();
    out << "vector<";
    if (!EmitType(out, vec->type(), "")) {
      return false;
    }
    out << ", " << vec->size() << ">";
  } else if (type->IsVoid()) {
    out << "void";
  } else {
    error_ = "unknown type in EmitType";
    return false;
  }

  return true;
}

bool GeneratorImpl::EmitStructType(std::ostream& out,
                                   const ast::type::StructType* str,
                                   const std::string& name) {
  // TODO(dsinclair): Block decoration?
  // if (str->impl()->decoration() != ast::StructDecoration::kNone) {
  // }
  out << "struct " << name << " {" << std::endl;

  increment_indent();
  for (const auto& mem : str->impl()->members()) {
    make_indent(out);
    // TODO(dsinclair): Handle [[offset]] annotation on structs
    // https://bugs.chromium.org/p/tint/issues/detail?id=184

    if (!EmitType(out, mem->type(), mem->name())) {
      return false;
    }
    // Array member name will be output with the type
    if (!mem->type()->IsArray()) {
      out << " " << namer_.NameFor(mem->name());
    }
    out << ";" << std::endl;
  }
  decrement_indent();
  make_indent(out);

  out << "};" << std::endl;

  return true;
}

bool GeneratorImpl::EmitUnaryOp(std::ostream& pre,
                                std::ostream& out,
                                ast::UnaryOpExpression* expr) {
  switch (expr->op()) {
    case ast::UnaryOp::kNot:
      out << "!";
      break;
    case ast::UnaryOp::kNegation:
      out << "-";
      break;
  }
  out << "(";

  if (!EmitExpression(pre, out, expr->expr())) {
    return false;
  }

  out << ")";

  return true;
}

bool GeneratorImpl::EmitVariable(std::ostream& out,
                                 ast::Variable* var,
                                 bool skip_constructor) {
  make_indent(out);

  // TODO(dsinclair): Handle variable decorations
  if (var->IsDecorated()) {
    error_ = "Variable decorations are not handled yet";
    return false;
  }

  std::ostringstream constructor_out;
  if (!skip_constructor && var->constructor() != nullptr) {
    constructor_out << " = ";

    std::ostringstream pre;
    if (!EmitExpression(pre, constructor_out, var->constructor())) {
      return false;
    }
    out << pre.str();
  }

  if (var->is_const()) {
    out << "const ";
  }
  if (!EmitType(out, var->type(), var->name())) {
    return false;
  }
  if (!var->type()->IsArray()) {
    out << " " << var->name();
  }
  out << constructor_out.str() << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitProgramConstVariable(std::ostream& out,
                                             const ast::Variable* var) {
  make_indent(out);

  if (var->IsDecorated() && !var->AsDecorated()->HasConstantIdDecoration()) {
    error_ = "Decorated const values not valid";
    return false;
  }
  if (!var->is_const()) {
    error_ = "Expected a const value";
    return false;
  }

  std::ostringstream constructor_out;
  if (var->constructor() != nullptr) {
    std::ostringstream pre;
    if (!EmitExpression(pre, constructor_out, var->constructor())) {
      return false;
    }
    out << pre.str();
  }

  if (var->IsDecorated() && var->AsDecorated()->HasConstantIdDecoration()) {
    auto const_id = var->AsDecorated()->constant_id();

    out << "#ifndef WGSL_SPEC_CONSTANT_" << const_id << std::endl;

    if (var->constructor() != nullptr) {
      out << "#define WGSL_SPEC_CONSTANT_" << const_id << " "
          << constructor_out.str() << std::endl;
    } else {
      out << "#error spec constant required for constant id " << const_id
          << std::endl;
    }
    out << "#endif" << std::endl;
    out << "static const ";
    if (!EmitType(out, var->type(), var->name())) {
      return false;
    }
    out << " " << var->name() << " = WGSL_SPEC_CONSTANT_" << const_id << ";"
        << std::endl;
    out << "#undef WGSL_SPEC_CONSTANT_" << const_id << std::endl;
  } else {
    out << "static const ";
    if (!EmitType(out, var->type(), var->name())) {
      return false;
    }
    if (!var->type()->IsArray()) {
      out << " " << var->name();
    }

    if (var->constructor() != nullptr) {
      out << " = " << constructor_out.str();
    }
    out << ";" << std::endl;
  }

  return true;
}

}  // namespace hlsl
}  // namespace writer
}  // namespace tint
