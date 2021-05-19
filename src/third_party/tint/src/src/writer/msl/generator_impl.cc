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

#include "src/writer/msl/generator_impl.h"

#include <algorithm>
#include <iomanip>
#include <utility>
#include <vector>

#include "src/ast/bool_literal.h"
#include "src/ast/call_statement.h"
#include "src/ast/constant_id_decoration.h"
#include "src/ast/fallthrough_statement.h"
#include "src/ast/float_literal.h"
#include "src/ast/module.h"
#include "src/ast/sint_literal.h"
#include "src/ast/uint_literal.h"
#include "src/ast/variable_decl_statement.h"
#include "src/semantic/array.h"
#include "src/semantic/call.h"
#include "src/semantic/function.h"
#include "src/semantic/member_accessor_expression.h"
#include "src/semantic/struct.h"
#include "src/semantic/variable.h"
#include "src/type/access_control_type.h"
#include "src/type/alias_type.h"
#include "src/type/array_type.h"
#include "src/type/bool_type.h"
#include "src/type/depth_texture_type.h"
#include "src/type/f32_type.h"
#include "src/type/i32_type.h"
#include "src/type/matrix_type.h"
#include "src/type/multisampled_texture_type.h"
#include "src/type/pointer_type.h"
#include "src/type/sampled_texture_type.h"
#include "src/type/storage_texture_type.h"
#include "src/type/u32_type.h"
#include "src/type/vector_type.h"
#include "src/type/void_type.h"
#include "src/writer/float_to_string.h"

namespace tint {
namespace writer {
namespace msl {
namespace {

const char kInStructNameSuffix[] = "in";
const char kOutStructNameSuffix[] = "out";
const char kTintStructInVarPrefix[] = "_tint_in";
const char kTintStructOutVarPrefix[] = "_tint_out";

bool last_is_break_or_fallthrough(const ast::BlockStatement* stmts) {
  if (stmts->empty()) {
    return false;
  }

  return stmts->last()->Is<ast::BreakStatement>() ||
         stmts->last()->Is<ast::FallthroughStatement>();
}

}  // namespace

GeneratorImpl::GeneratorImpl(const Program* program)
    : TextGenerator(), program_(program) {}

GeneratorImpl::~GeneratorImpl() = default;

bool GeneratorImpl::Generate() {
  out_ << "#include <metal_stdlib>" << std::endl << std::endl;
  out_ << "using namespace metal;" << std::endl;

  for (auto* global : program_->AST().GlobalVariables()) {
    auto* sem = program_->Sem().Get(global);
    global_variables_.set(global->symbol(), sem);
  }

  for (auto* const ty : program_->AST().ConstructedTypes()) {
    if (!EmitConstructedType(ty)) {
      return false;
    }
  }
  if (!program_->AST().ConstructedTypes().empty()) {
    out_ << std::endl;
  }

  for (auto* var : program_->AST().GlobalVariables()) {
    if (!var->is_const()) {
      continue;
    }
    if (!EmitProgramConstVariable(var)) {
      return false;
    }
  }

  // Make sure all entry point data is emitted before the entry point functions
  for (auto* func : program_->AST().Functions()) {
    if (!func->IsEntryPoint()) {
      continue;
    }

    if (!EmitEntryPointData(func)) {
      return false;
    }
  }

  for (auto* func : program_->AST().Functions()) {
    if (!EmitFunction(func)) {
      return false;
    }
  }

  for (auto* func : program_->AST().Functions()) {
    if (!func->IsEntryPoint()) {
      continue;
    }
    if (!EmitEntryPointFunction(func)) {
      return false;
    }
    out_ << std::endl;
  }

  return true;
}

bool GeneratorImpl::EmitConstructedType(const type::Type* ty) {
  make_indent();

  if (auto* alias = ty->As<type::Alias>()) {
    out_ << "typedef ";
    if (!EmitType(alias->type(), "")) {
      return false;
    }
    out_ << " " << program_->Symbols().NameFor(alias->symbol()) << ";"
         << std::endl;
  } else if (auto* str = ty->As<type::Struct>()) {
    if (!EmitStructType(str)) {
      return false;
    }
  } else {
    diagnostics_.add_error("unknown alias type: " + ty->type_name());
    return false;
  }

  return true;
}

bool GeneratorImpl::EmitArrayAccessor(ast::ArrayAccessorExpression* expr) {
  if (!EmitExpression(expr->array())) {
    return false;
  }
  out_ << "[";

  if (!EmitExpression(expr->idx_expr())) {
    return false;
  }
  out_ << "]";

  return true;
}

bool GeneratorImpl::EmitBitcast(ast::BitcastExpression* expr) {
  out_ << "as_type<";
  if (!EmitType(expr->type(), "")) {
    return false;
  }

  out_ << ">(";
  if (!EmitExpression(expr->expr())) {
    return false;
  }

  out_ << ")";
  return true;
}

bool GeneratorImpl::EmitAssign(ast::AssignmentStatement* stmt) {
  make_indent();

  if (!EmitExpression(stmt->lhs())) {
    return false;
  }

  out_ << " = ";

  if (!EmitExpression(stmt->rhs())) {
    return false;
  }

  out_ << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitBinary(ast::BinaryExpression* expr) {
  out_ << "(";

  if (!EmitExpression(expr->lhs())) {
    return false;
  }
  out_ << " ";

  switch (expr->op()) {
    case ast::BinaryOp::kAnd:
      out_ << "&";
      break;
    case ast::BinaryOp::kOr:
      out_ << "|";
      break;
    case ast::BinaryOp::kXor:
      out_ << "^";
      break;
    case ast::BinaryOp::kLogicalAnd:
      out_ << "&&";
      break;
    case ast::BinaryOp::kLogicalOr:
      out_ << "||";
      break;
    case ast::BinaryOp::kEqual:
      out_ << "==";
      break;
    case ast::BinaryOp::kNotEqual:
      out_ << "!=";
      break;
    case ast::BinaryOp::kLessThan:
      out_ << "<";
      break;
    case ast::BinaryOp::kGreaterThan:
      out_ << ">";
      break;
    case ast::BinaryOp::kLessThanEqual:
      out_ << "<=";
      break;
    case ast::BinaryOp::kGreaterThanEqual:
      out_ << ">=";
      break;
    case ast::BinaryOp::kShiftLeft:
      out_ << "<<";
      break;
    case ast::BinaryOp::kShiftRight:
      // TODO(dsinclair): MSL is based on C++14, and >> in C++14 has
      // implementation-defined behaviour for negative LHS.  We may have to
      // generate extra code to implement WGSL-specified behaviour for negative
      // LHS.
      out_ << R"(>>)";
      break;

    case ast::BinaryOp::kAdd:
      out_ << "+";
      break;
    case ast::BinaryOp::kSubtract:
      out_ << "-";
      break;
    case ast::BinaryOp::kMultiply:
      out_ << "*";
      break;
    case ast::BinaryOp::kDivide:
      out_ << "/";
      break;
    case ast::BinaryOp::kModulo:
      out_ << "%";
      break;
    case ast::BinaryOp::kNone:
      diagnostics_.add_error("missing binary operation type");
      return false;
  }
  out_ << " ";

  if (!EmitExpression(expr->rhs())) {
    return false;
  }

  out_ << ")";
  return true;
}

bool GeneratorImpl::EmitBreak(ast::BreakStatement*) {
  make_indent();
  out_ << "break;" << std::endl;
  return true;
}

std::string GeneratorImpl::current_ep_var_name(VarType type) {
  std::string name = "";
  switch (type) {
    case VarType::kIn: {
      auto in_it = ep_sym_to_in_data_.find(current_ep_sym_);
      if (in_it != ep_sym_to_in_data_.end()) {
        name = in_it->second.var_name;
      }
      break;
    }
    case VarType::kOut: {
      auto out_it = ep_sym_to_out_data_.find(current_ep_sym_);
      if (out_it != ep_sym_to_out_data_.end()) {
        name = out_it->second.var_name;
      }
      break;
    }
  }
  return name;
}

bool GeneratorImpl::EmitCall(ast::CallExpression* expr) {
  auto* ident = expr->func()->As<ast::IdentifierExpression>();

  if (ident == nullptr) {
    diagnostics_.add_error("invalid function name");
    return 0;
  }

  auto* call = program_->Sem().Get(expr);
  if (auto* intrinsic = call->Target()->As<semantic::Intrinsic>()) {
    if (intrinsic->IsTexture()) {
      return EmitTextureCall(expr, intrinsic);
    }
    if (intrinsic->Type() == semantic::IntrinsicType::kPack2x16Float ||
        intrinsic->Type() == semantic::IntrinsicType::kUnpack2x16Float) {
      make_indent();
      if (intrinsic->Type() == semantic::IntrinsicType::kPack2x16Float) {
        out_ << "as_type<uint>(half2(";
      } else {
        out_ << "float2(as_type<half2>(";
      }
      if (!EmitExpression(expr->params()[0])) {
        return false;
      }
      out_ << "))";
      return true;
    }
    // TODO(crbug.com/tint/661): Combine sequential barriers to a single
    // instruction.
    if (intrinsic->Type() == semantic::IntrinsicType::kStorageBarrier) {
      make_indent();
      out_ << "threadgroup_barrier(mem_flags::mem_device)";
      return true;
    }
    if (intrinsic->Type() == semantic::IntrinsicType::kWorkgroupBarrier) {
      make_indent();
      out_ << "threadgroup_barrier(mem_flags::mem_threadgroup)";
      return true;
    }
    auto name = generate_builtin_name(intrinsic);
    if (name.empty()) {
      return false;
    }

    make_indent();
    out_ << name << "(";

    bool first = true;
    const auto& params = expr->params();
    for (auto* param : params) {
      if (!first) {
        out_ << ", ";
      }
      first = false;

      if (!EmitExpression(param)) {
        return false;
      }
    }

    out_ << ")";
    return true;
  }

  auto name = program_->Symbols().NameFor(ident->symbol());
  auto caller_sym = ident->symbol();
  auto it = ep_func_name_remapped_.find(current_ep_sym_.to_str() + "_" +
                                        caller_sym.to_str());
  if (it != ep_func_name_remapped_.end()) {
    name = it->second;
  }

  auto* func = program_->AST().Functions().Find(ident->symbol());
  if (func == nullptr) {
    diagnostics_.add_error("Unable to find function: " +
                           program_->Symbols().NameFor(ident->symbol()));
    return false;
  }

  out_ << name << "(";

  bool first = true;
  if (has_referenced_in_var_needing_struct(func)) {
    auto var_name = current_ep_var_name(VarType::kIn);
    if (!var_name.empty()) {
      out_ << var_name;
      first = false;
    }
  }
  if (has_referenced_out_var_needing_struct(func)) {
    auto var_name = current_ep_var_name(VarType::kOut);
    if (!var_name.empty()) {
      if (!first) {
        out_ << ", ";
      }
      first = false;
      out_ << var_name;
    }
  }

  auto* func_sem = program_->Sem().Get(func);
  for (const auto& data : func_sem->ReferencedBuiltinVariables()) {
    auto* var = data.first;
    if (var->StorageClass() != ast::StorageClass::kInput) {
      continue;
    }
    if (!first) {
      out_ << ", ";
    }
    first = false;
    out_ << program_->Symbols().NameFor(var->Declaration()->symbol());
  }

  for (const auto& data : func_sem->ReferencedUniformVariables()) {
    auto* var = data.first;
    if (!first) {
      out_ << ", ";
    }
    first = false;
    out_ << program_->Symbols().NameFor(var->Declaration()->symbol());
  }

  for (const auto& data : func_sem->ReferencedStorageBufferVariables()) {
    auto* var = data.first;
    if (!first) {
      out_ << ", ";
    }
    first = false;
    out_ << program_->Symbols().NameFor(var->Declaration()->symbol());
  }

  const auto& params = expr->params();
  for (auto* param : params) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    if (!EmitExpression(param)) {
      return false;
    }
  }

  out_ << ")";

  return true;
}

bool GeneratorImpl::EmitTextureCall(ast::CallExpression* expr,
                                    const semantic::Intrinsic* intrinsic) {
  using Usage = semantic::Parameter::Usage;

  auto parameters = intrinsic->Parameters();
  auto arguments = expr->params();

  // Returns the argument with the given usage
  auto arg = [&](Usage usage) {
    int idx = semantic::IndexOf(parameters, usage);
    return (idx >= 0) ? arguments[idx] : nullptr;
  };

  auto* texture = arg(Usage::kTexture);
  if (!texture) {
    TINT_ICE(diagnostics_) << "missing texture arg";
    return false;
  }

  auto* texture_type = TypeOf(texture)->UnwrapAll()->As<type::Texture>();

  switch (intrinsic->Type()) {
    case semantic::IntrinsicType::kTextureDimensions: {
      std::vector<const char*> dims;
      switch (texture_type->dim()) {
        case type::TextureDimension::kNone:
          diagnostics_.add_error("texture dimension is kNone");
          return false;
        case type::TextureDimension::k1d:
          dims = {"width"};
          break;
        case type::TextureDimension::k2d:
        case type::TextureDimension::k2dArray:
          dims = {"width", "height"};
          break;
        case type::TextureDimension::k3d:
          dims = {"width", "height", "depth"};
          break;
        case type::TextureDimension::kCube:
        case type::TextureDimension::kCubeArray:
          // width == height == depth for cubes
          // See https://github.com/gpuweb/gpuweb/issues/1345
          dims = {"width", "height", "height"};
          break;
      }

      auto get_dim = [&](const char* name) {
        if (!EmitExpression(texture)) {
          return false;
        }
        out_ << ".get_" << name << "(";
        if (auto* level = arg(Usage::kLevel)) {
          if (!EmitExpression(level)) {
            return false;
          }
        }
        out_ << ")";
        return true;
      };

      if (dims.size() == 1) {
        out_ << "int(";
        get_dim(dims[0]);
        out_ << ")";
      } else {
        EmitType(TypeOf(expr), "");
        out_ << "(";
        for (size_t i = 0; i < dims.size(); i++) {
          if (i > 0) {
            out_ << ", ";
          }
          get_dim(dims[i]);
        }
        out_ << ")";
      }
      return true;
    }
    case semantic::IntrinsicType::kTextureNumLayers: {
      out_ << "int(";
      if (!EmitExpression(texture)) {
        return false;
      }
      out_ << ".get_array_size())";
      return true;
    }
    case semantic::IntrinsicType::kTextureNumLevels: {
      out_ << "int(";
      if (!EmitExpression(texture)) {
        return false;
      }
      out_ << ".get_num_mip_levels())";
      return true;
    }
    case semantic::IntrinsicType::kTextureNumSamples: {
      out_ << "int(";
      if (!EmitExpression(texture)) {
        return false;
      }
      out_ << ".get_num_samples())";
      return true;
    }
    default:
      break;
  }

  if (!EmitExpression(texture))
    return false;

  bool lod_param_is_named = true;

  switch (intrinsic->Type()) {
    case semantic::IntrinsicType::kTextureSample:
    case semantic::IntrinsicType::kTextureSampleBias:
    case semantic::IntrinsicType::kTextureSampleLevel:
    case semantic::IntrinsicType::kTextureSampleGrad:
      out_ << ".sample(";
      break;
    case semantic::IntrinsicType::kTextureSampleCompare:
      out_ << ".sample_compare(";
      break;
    case semantic::IntrinsicType::kTextureLoad:
      out_ << ".read(";
      lod_param_is_named = false;
      break;
    case semantic::IntrinsicType::kTextureStore:
      out_ << ".write(";
      break;
    default:
      TINT_UNREACHABLE(diagnostics_)
          << "Unhandled texture intrinsic '" << intrinsic->str() << "'";
      return false;
  }

  bool first_arg = true;
  auto maybe_write_comma = [&] {
    if (!first_arg) {
      out_ << ", ";
    }
    first_arg = false;
  };

  for (auto usage :
       {Usage::kValue, Usage::kSampler, Usage::kCoords, Usage::kArrayIndex,
        Usage::kDepthRef, Usage::kSampleIndex}) {
    if (auto* e = arg(usage)) {
      maybe_write_comma();
      if (!EmitExpression(e))
        return false;
    }
  }

  if (auto* bias = arg(Usage::kBias)) {
    maybe_write_comma();
    out_ << "bias(";
    if (!EmitExpression(bias)) {
      return false;
    }
    out_ << ")";
  }
  if (auto* level = arg(Usage::kLevel)) {
    maybe_write_comma();
    if (lod_param_is_named) {
      out_ << "level(";
    }
    if (!EmitExpression(level)) {
      return false;
    }
    if (lod_param_is_named) {
      out_ << ")";
    }
  }
  if (auto* ddx = arg(Usage::kDdx)) {
    auto dim = texture_type->dim();
    switch (dim) {
      case type::TextureDimension::k2d:
      case type::TextureDimension::k2dArray:
        maybe_write_comma();
        out_ << "gradient2d(";
        break;
      case type::TextureDimension::k3d:
        maybe_write_comma();
        out_ << "gradient3d(";
        break;
      case type::TextureDimension::kCube:
      case type::TextureDimension::kCubeArray:
        maybe_write_comma();
        out_ << "gradientcube(";
        break;
      default: {
        std::stringstream err;
        err << "MSL does not support gradients for " << dim << " textures";
        diagnostics_.add_error(err.str());
        return false;
      }
    }
    if (!EmitExpression(ddx)) {
      return false;
    }
    out_ << ", ";
    if (!EmitExpression(arg(Usage::kDdy))) {
      return false;
    }
    out_ << ")";
  }

  if (auto* offset = arg(Usage::kOffset)) {
    maybe_write_comma();
    if (!EmitExpression(offset)) {
      return false;
    }
  }

  out_ << ")";

  return true;
}

std::string GeneratorImpl::generate_builtin_name(
    const semantic::Intrinsic* intrinsic) {
  std::string out = "";
  switch (intrinsic->Type()) {
    case semantic::IntrinsicType::kAcos:
    case semantic::IntrinsicType::kAll:
    case semantic::IntrinsicType::kAny:
    case semantic::IntrinsicType::kAsin:
    case semantic::IntrinsicType::kAtan:
    case semantic::IntrinsicType::kAtan2:
    case semantic::IntrinsicType::kCeil:
    case semantic::IntrinsicType::kCos:
    case semantic::IntrinsicType::kCosh:
    case semantic::IntrinsicType::kCross:
    case semantic::IntrinsicType::kDeterminant:
    case semantic::IntrinsicType::kDistance:
    case semantic::IntrinsicType::kDot:
    case semantic::IntrinsicType::kExp:
    case semantic::IntrinsicType::kExp2:
    case semantic::IntrinsicType::kFloor:
    case semantic::IntrinsicType::kFma:
    case semantic::IntrinsicType::kFract:
    case semantic::IntrinsicType::kLength:
    case semantic::IntrinsicType::kLdexp:
    case semantic::IntrinsicType::kLog:
    case semantic::IntrinsicType::kLog2:
    case semantic::IntrinsicType::kMix:
    case semantic::IntrinsicType::kNormalize:
    case semantic::IntrinsicType::kPow:
    case semantic::IntrinsicType::kReflect:
    case semantic::IntrinsicType::kSelect:
    case semantic::IntrinsicType::kSin:
    case semantic::IntrinsicType::kSinh:
    case semantic::IntrinsicType::kSqrt:
    case semantic::IntrinsicType::kStep:
    case semantic::IntrinsicType::kTan:
    case semantic::IntrinsicType::kTanh:
    case semantic::IntrinsicType::kTrunc:
    case semantic::IntrinsicType::kSign:
    case semantic::IntrinsicType::kClamp:
      out += intrinsic->str();
      break;
    case semantic::IntrinsicType::kAbs:
      if (intrinsic->ReturnType()->is_float_scalar_or_vector()) {
        out += "fabs";
      } else {
        out += "abs";
      }
      break;
    case semantic::IntrinsicType::kCountOneBits:
      out += "popcount";
      break;
    case semantic::IntrinsicType::kDpdx:
    case semantic::IntrinsicType::kDpdxCoarse:
    case semantic::IntrinsicType::kDpdxFine:
      out += "dfdx";
      break;
    case semantic::IntrinsicType::kDpdy:
    case semantic::IntrinsicType::kDpdyCoarse:
    case semantic::IntrinsicType::kDpdyFine:
      out += "dfdy";
      break;
    case semantic::IntrinsicType::kFwidth:
    case semantic::IntrinsicType::kFwidthCoarse:
    case semantic::IntrinsicType::kFwidthFine:
      out += "fwidth";
      break;
    case semantic::IntrinsicType::kIsFinite:
      out += "isfinite";
      break;
    case semantic::IntrinsicType::kIsInf:
      out += "isinf";
      break;
    case semantic::IntrinsicType::kIsNan:
      out += "isnan";
      break;
    case semantic::IntrinsicType::kIsNormal:
      out += "isnormal";
      break;
    case semantic::IntrinsicType::kMax:
      if (intrinsic->ReturnType()->is_float_scalar_or_vector()) {
        out += "fmax";
      } else {
        out += "max";
      }
      break;
    case semantic::IntrinsicType::kMin:
      if (intrinsic->ReturnType()->is_float_scalar_or_vector()) {
        out += "fmin";
      } else {
        out += "min";
      }
      break;
    case semantic::IntrinsicType::kFaceForward:
      out += "faceforward";
      break;
    case semantic::IntrinsicType::kPack4x8Snorm:
      out += "pack_float_to_snorm4x8";
      break;
    case semantic::IntrinsicType::kPack4x8Unorm:
      out += "pack_float_to_unorm4x8";
      break;
    case semantic::IntrinsicType::kPack2x16Snorm:
      out += "pack_float_to_snorm2x16";
      break;
    case semantic::IntrinsicType::kPack2x16Unorm:
      out += "pack_float_to_unorm2x16";
      break;
    case semantic::IntrinsicType::kReverseBits:
      out += "reverse_bits";
      break;
    case semantic::IntrinsicType::kRound:
      out += "rint";
      break;
    case semantic::IntrinsicType::kSmoothStep:
      out += "smoothstep";
      break;
    case semantic::IntrinsicType::kInverseSqrt:
      out += "rsqrt";
      break;
    case semantic::IntrinsicType::kUnpack4x8Snorm:
      out += "unpack_snorm4x8_to_float";
      break;
    case semantic::IntrinsicType::kUnpack4x8Unorm:
      out += "unpack_unorm4x8_to_float";
      break;
    case semantic::IntrinsicType::kUnpack2x16Snorm:
      out += "unpack_snorm2x16_to_float";
      break;
    case semantic::IntrinsicType::kUnpack2x16Unorm:
      out += "unpack_unorm2x16_to_float";
      break;
    default:
      diagnostics_.add_error("Unknown import method: " +
                             std::string(intrinsic->str()));
      return "";
  }
  return out;
}

bool GeneratorImpl::EmitCase(ast::CaseStatement* stmt) {
  make_indent();

  if (stmt->IsDefault()) {
    out_ << "default:";
  } else {
    bool first = true;
    for (auto* selector : stmt->selectors()) {
      if (!first) {
        out_ << std::endl;
        make_indent();
      }
      first = false;

      out_ << "case ";
      if (!EmitLiteral(selector)) {
        return false;
      }
      out_ << ":";
    }
  }

  out_ << " {" << std::endl;

  increment_indent();

  for (auto* s : *stmt->body()) {
    if (!EmitStatement(s)) {
      return false;
    }
  }

  if (!last_is_break_or_fallthrough(stmt->body())) {
    make_indent();
    out_ << "break;" << std::endl;
  }

  decrement_indent();
  make_indent();
  out_ << "}" << std::endl;

  return true;
}

bool GeneratorImpl::EmitConstructor(ast::ConstructorExpression* expr) {
  if (auto* scalar = expr->As<ast::ScalarConstructorExpression>()) {
    return EmitScalarConstructor(scalar);
  }
  return EmitTypeConstructor(expr->As<ast::TypeConstructorExpression>());
}

bool GeneratorImpl::EmitContinue(ast::ContinueStatement*) {
  make_indent();
  out_ << "continue;" << std::endl;
  return true;
}

bool GeneratorImpl::EmitTypeConstructor(ast::TypeConstructorExpression* expr) {
  if (expr->type()->IsAnyOf<type::Array, type::Struct>()) {
    out_ << "{";
  } else {
    if (!EmitType(expr->type(), "")) {
      return false;
    }
    out_ << "(";
  }

  // If the type constructor is empty then we need to construct with the zero
  // value for all components.
  if (expr->values().empty()) {
    if (!EmitZeroValue(expr->type())) {
      return false;
    }
  } else {
    bool first = true;
    for (auto* e : expr->values()) {
      if (!first) {
        out_ << ", ";
      }
      first = false;

      if (!EmitExpression(e)) {
        return false;
      }
    }
  }

  if (expr->type()->IsAnyOf<type::Array, type::Struct>()) {
    out_ << "}";
  } else {
    out_ << ")";
  }
  return true;
}

bool GeneratorImpl::EmitZeroValue(type::Type* type) {
  if (type->Is<type::Bool>()) {
    out_ << "false";
  } else if (type->Is<type::F32>()) {
    out_ << "0.0f";
  } else if (type->Is<type::I32>()) {
    out_ << "0";
  } else if (type->Is<type::U32>()) {
    out_ << "0u";
  } else if (auto* vec = type->As<type::Vector>()) {
    return EmitZeroValue(vec->type());
  } else if (auto* mat = type->As<type::Matrix>()) {
    return EmitZeroValue(mat->type());
  } else if (auto* arr = type->As<type::Array>()) {
    out_ << "{";
    if (!EmitZeroValue(arr->type())) {
      return false;
    }
    out_ << "}";
  } else if (type->As<type::Struct>()) {
    out_ << "{}";
  } else {
    diagnostics_.add_error("Invalid type for zero emission: " +
                           type->type_name());
    return false;
  }
  return true;
}

bool GeneratorImpl::EmitScalarConstructor(
    ast::ScalarConstructorExpression* expr) {
  return EmitLiteral(expr->literal());
}

bool GeneratorImpl::EmitLiteral(ast::Literal* lit) {
  if (auto* l = lit->As<ast::BoolLiteral>()) {
    out_ << (l->IsTrue() ? "true" : "false");
  } else if (auto* fl = lit->As<ast::FloatLiteral>()) {
    out_ << FloatToString(fl->value()) << "f";
  } else if (auto* sl = lit->As<ast::SintLiteral>()) {
    out_ << sl->value();
  } else if (auto* ul = lit->As<ast::UintLiteral>()) {
    out_ << ul->value() << "u";
  } else {
    diagnostics_.add_error("unknown literal type");
    return false;
  }
  return true;
}

// TODO(crbug.com/tint/697): Remove this when we remove support for entry point
// params as module-scope globals.
bool GeneratorImpl::EmitEntryPointData(ast::Function* func) {
  auto* func_sem = program_->Sem().Get(func);

  std::vector<std::pair<const ast::Variable*, uint32_t>> in_locations;
  std::vector<std::pair<const ast::Variable*, ast::Decoration*>> out_variables;

  for (auto data : func_sem->ReferencedLocationVariables()) {
    auto* var = data.first;
    auto* deco = data.second;

    if (var->StorageClass() == ast::StorageClass::kInput) {
      in_locations.push_back({var->Declaration(), deco->value()});
    } else if (var->StorageClass() == ast::StorageClass::kOutput) {
      out_variables.push_back({var->Declaration(), deco});
    }
  }

  for (auto data : func_sem->ReferencedBuiltinVariables()) {
    auto* var = data.first;
    auto* deco = data.second;

    if (var->StorageClass() == ast::StorageClass::kOutput) {
      out_variables.push_back({var->Declaration(), deco});
    }
  }

  if (!in_locations.empty()) {
    auto in_struct_name =
        program_->Symbols().NameFor(func->symbol()) + "_" + kInStructNameSuffix;
    auto* in_var_name = kTintStructInVarPrefix;
    ep_sym_to_in_data_[func->symbol()] = {in_struct_name, in_var_name};

    make_indent();
    out_ << "struct " << in_struct_name << " {" << std::endl;

    increment_indent();

    for (auto& data : in_locations) {
      auto* var = data.first;
      uint32_t loc = data.second;

      make_indent();
      if (!EmitType(program_->Sem().Get(var)->Type(),
                    program_->Symbols().NameFor(var->symbol()))) {
        return false;
      }

      out_ << " " << program_->Symbols().NameFor(var->symbol()) << " [[";
      if (func->pipeline_stage() == ast::PipelineStage::kVertex) {
        out_ << "attribute(" << loc << ")";
      } else if (func->pipeline_stage() == ast::PipelineStage::kFragment) {
        out_ << "user(locn" << loc << ")";
      } else {
        diagnostics_.add_error("invalid location variable for pipeline stage");
        return false;
      }
      out_ << "]];" << std::endl;
    }
    decrement_indent();
    make_indent();

    out_ << "};" << std::endl << std::endl;
  }

  if (!out_variables.empty()) {
    auto out_struct_name = program_->Symbols().NameFor(func->symbol()) + "_" +
                           kOutStructNameSuffix;
    auto* out_var_name = kTintStructOutVarPrefix;
    ep_sym_to_out_data_[func->symbol()] = {out_struct_name, out_var_name};

    make_indent();
    out_ << "struct " << out_struct_name << " {" << std::endl;

    increment_indent();
    for (auto& data : out_variables) {
      auto* var = data.first;
      auto* deco = data.second;

      make_indent();
      if (!EmitType(program_->Sem().Get(var)->Type(),
                    program_->Symbols().NameFor(var->symbol()))) {
        return false;
      }

      out_ << " " << program_->Symbols().NameFor(var->symbol()) << " [[";

      if (auto* location = deco->As<ast::LocationDecoration>()) {
        auto loc = location->value();
        if (func->pipeline_stage() == ast::PipelineStage::kVertex) {
          out_ << "user(locn" << loc << ")";
        } else if (func->pipeline_stage() == ast::PipelineStage::kFragment) {
          out_ << "color(" << loc << ")";
        } else {
          diagnostics_.add_error(
              "invalid location variable for pipeline stage");
          return false;
        }
      } else if (auto* builtin = deco->As<ast::BuiltinDecoration>()) {
        auto attr = builtin_to_attribute(builtin->value());
        if (attr.empty()) {
          diagnostics_.add_error("unsupported builtin");
          return false;
        }
        out_ << attr;
      } else {
        diagnostics_.add_error(
            "unsupported variable decoration for entry point output");
        return false;
      }
      out_ << "]];" << std::endl;
    }
    decrement_indent();
    make_indent();
    out_ << "};" << std::endl << std::endl;
  }

  return true;
}

bool GeneratorImpl::EmitExpression(ast::Expression* expr) {
  if (auto* a = expr->As<ast::ArrayAccessorExpression>()) {
    return EmitArrayAccessor(a);
  }
  if (auto* b = expr->As<ast::BinaryExpression>()) {
    return EmitBinary(b);
  }
  if (auto* b = expr->As<ast::BitcastExpression>()) {
    return EmitBitcast(b);
  }
  if (auto* c = expr->As<ast::CallExpression>()) {
    return EmitCall(c);
  }
  if (auto* c = expr->As<ast::ConstructorExpression>()) {
    return EmitConstructor(c);
  }
  if (auto* i = expr->As<ast::IdentifierExpression>()) {
    return EmitIdentifier(i);
  }
  if (auto* m = expr->As<ast::MemberAccessorExpression>()) {
    return EmitMemberAccessor(m);
  }
  if (auto* u = expr->As<ast::UnaryOpExpression>()) {
    return EmitUnaryOp(u);
  }

  diagnostics_.add_error("unknown expression type: " + program_->str(expr));
  return false;
}

void GeneratorImpl::EmitStage(ast::PipelineStage stage) {
  switch (stage) {
    case ast::PipelineStage::kFragment:
      out_ << "fragment";
      break;
    case ast::PipelineStage::kVertex:
      out_ << "vertex";
      break;
    case ast::PipelineStage::kCompute:
      out_ << "kernel";
      break;
    case ast::PipelineStage::kNone:
      break;
  }
  return;
}

bool GeneratorImpl::has_referenced_in_var_needing_struct(ast::Function* func) {
  auto* func_sem = program_->Sem().Get(func);
  for (auto data : func_sem->ReferencedLocationVariables()) {
    auto* var = data.first;
    if (var->StorageClass() == ast::StorageClass::kInput) {
      return true;
    }
  }
  return false;
}

bool GeneratorImpl::has_referenced_out_var_needing_struct(ast::Function* func) {
  auto* func_sem = program_->Sem().Get(func);

  for (auto data : func_sem->ReferencedLocationVariables()) {
    auto* var = data.first;
    if (var->StorageClass() == ast::StorageClass::kOutput) {
      return true;
    }
  }

  for (auto data : func_sem->ReferencedBuiltinVariables()) {
    auto* var = data.first;
    if (var->StorageClass() == ast::StorageClass::kOutput) {
      return true;
    }
  }
  return false;
}

bool GeneratorImpl::has_referenced_var_needing_struct(ast::Function* func) {
  return has_referenced_in_var_needing_struct(func) ||
         has_referenced_out_var_needing_struct(func);
}

bool GeneratorImpl::EmitFunction(ast::Function* func) {
  auto* func_sem = program_->Sem().Get(func);

  make_indent();

  // Entry points will be emitted later, skip for now.
  if (func->IsEntryPoint()) {
    return true;
  }

  // TODO(dsinclair): This could be smarter. If the input/outputs for multiple
  // entry points are the same we could generate a single struct and then have
  // this determine it's the same struct and just emit once.
  bool emit_duplicate_functions = func_sem->AncestorEntryPoints().size() > 0 &&
                                  has_referenced_var_needing_struct(func);

  if (emit_duplicate_functions) {
    for (const auto& ep_sym : func_sem->AncestorEntryPoints()) {
      if (!EmitFunctionInternal(func, emit_duplicate_functions, ep_sym)) {
        return false;
      }
      out_ << std::endl;
    }
  } else {
    // Emit as non-duplicated
    if (!EmitFunctionInternal(func, false, Symbol())) {
      return false;
    }
    out_ << std::endl;
  }

  return true;
}

bool GeneratorImpl::EmitFunctionInternal(ast::Function* func,
                                         bool emit_duplicate_functions,
                                         Symbol ep_sym) {
  auto* func_sem = program_->Sem().Get(func);

  auto name = func->symbol().to_str();
  if (!EmitType(func->return_type(), "")) {
    return false;
  }

  out_ << " ";
  if (emit_duplicate_functions) {
    auto func_name = name;
    auto ep_name = ep_sym.to_str();
    name = program_->Symbols().NameFor(func->symbol()) + "_" +
           program_->Symbols().NameFor(ep_sym);
    ep_func_name_remapped_[ep_name + "_" + func_name] = name;
  } else {
    name = program_->Symbols().NameFor(func->symbol());
  }
  out_ << name << "(";

  bool first = true;

  // If we're emitting duplicate functions that means the function takes
  // the stage_in or stage_out value from the entry point, emit them.
  //
  // We emit both of them if they're there regardless of if they're both used.
  if (emit_duplicate_functions) {
    auto in_it = ep_sym_to_in_data_.find(ep_sym);
    if (in_it != ep_sym_to_in_data_.end()) {
      out_ << "thread " << in_it->second.struct_name << "& "
           << in_it->second.var_name;
      first = false;
    }

    auto out_it = ep_sym_to_out_data_.find(ep_sym);
    if (out_it != ep_sym_to_out_data_.end()) {
      if (!first) {
        out_ << ", ";
      }
      out_ << "thread " << out_it->second.struct_name << "& "
           << out_it->second.var_name;
      first = false;
    }
  }

  for (const auto& data : func_sem->ReferencedBuiltinVariables()) {
    auto* var = data.first;
    if (var->StorageClass() != ast::StorageClass::kInput) {
      continue;
    }
    if (!first) {
      out_ << ", ";
    }
    first = false;

    out_ << "thread ";
    if (!EmitType(var->Type(), "")) {
      return false;
    }
    out_ << "& " << program_->Symbols().NameFor(var->Declaration()->symbol());
  }

  for (const auto& data : func_sem->ReferencedUniformVariables()) {
    auto* var = data.first;
    if (!first) {
      out_ << ", ";
    }
    first = false;

    out_ << "constant ";
    // TODO(dsinclair): Can arrays be uniform? If so, fix this ...
    if (!EmitType(var->Type(), "")) {
      return false;
    }
    out_ << "& " << program_->Symbols().NameFor(var->Declaration()->symbol());
  }

  for (const auto& data : func_sem->ReferencedStorageBufferVariables()) {
    auto* var = data.first;
    if (!first) {
      out_ << ", ";
    }
    first = false;

    auto* ac = var->Type()->As<type::AccessControl>();
    if (ac == nullptr) {
      diagnostics_.add_error(
          "invalid type for storage buffer, expected access control");
      return false;
    }
    if (ac->IsReadOnly()) {
      out_ << "const ";
    }

    out_ << "device ";
    if (!EmitType(ac->type(), "")) {
      return false;
    }
    out_ << "& " << program_->Symbols().NameFor(var->Declaration()->symbol());
  }

  for (auto* v : func->params()) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    auto* type = program_->Sem().Get(v)->Type();

    if (!EmitType(type, program_->Symbols().NameFor(v->symbol()))) {
      return false;
    }
    // Array name is output as part of the type
    if (!type->Is<type::Array>()) {
      out_ << " " << program_->Symbols().NameFor(v->symbol());
    }
  }

  out_ << ") ";

  current_ep_sym_ = ep_sym;

  if (!EmitBlockAndNewline(func->body())) {
    return false;
  }

  current_ep_sym_ = Symbol();

  return true;
}

std::string GeneratorImpl::builtin_to_attribute(ast::Builtin builtin) const {
  switch (builtin) {
    case ast::Builtin::kPosition:
      return "position";
    case ast::Builtin::kVertexIndex:
      return "vertex_id";
    case ast::Builtin::kInstanceIndex:
      return "instance_id";
    case ast::Builtin::kFrontFacing:
      return "front_facing";
    case ast::Builtin::kFragCoord:
      return "position";
    case ast::Builtin::kFragDepth:
      return "depth(any)";
    case ast::Builtin::kLocalInvocationId:
      return "thread_position_in_threadgroup";
    case ast::Builtin::kLocalInvocationIndex:
      return "thread_index_in_threadgroup";
    case ast::Builtin::kGlobalInvocationId:
      return "thread_position_in_grid";
    case ast::Builtin::kSampleIndex:
      return "sample_id";
    case ast::Builtin::kSampleMaskIn:
      return "sample_mask";
    case ast::Builtin::kSampleMaskOut:
      return "sample_mask";
    default:
      break;
  }
  return "";
}

bool GeneratorImpl::EmitEntryPointFunction(ast::Function* func) {
  auto* func_sem = program_->Sem().Get(func);

  make_indent();

  current_ep_sym_ = func->symbol();

  EmitStage(func->pipeline_stage());
  out_ << " ";

  // This is an entry point, the return type is the entry point output structure
  // if one exists, or void otherwise.
  auto out_data = ep_sym_to_out_data_.find(current_ep_sym_);
  bool has_out_data = out_data != ep_sym_to_out_data_.end();
  if (has_out_data) {
    // TODO(crbug.com/tint/697): Remove this.
    if (!func->return_type()->Is<type::Void>()) {
      TINT_ICE(diagnostics_) << "Mixing module-scope variables and return "
                                "types for shader outputs";
    }
    out_ << out_data->second.struct_name;
  } else {
    out_ << func->return_type()->FriendlyName(program_->Symbols());
  }
  out_ << " " << program_->Symbols().NameFor(func->symbol()) << "(";

  bool first = true;
  // TODO(crbug.com/tint/697): Remove this.
  auto in_data = ep_sym_to_in_data_.find(current_ep_sym_);
  if (in_data != ep_sym_to_in_data_.end()) {
    out_ << in_data->second.struct_name << " " << in_data->second.var_name
         << " [[stage_in]]";
    first = false;
  }

  // Emit entry point parameters.
  for (auto* var : func->params()) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    auto* type = program_->Sem().Get(var)->Type();

    if (!EmitType(type, "")) {
      return false;
    }

    out_ << " " << program_->Symbols().NameFor(var->symbol());

    if (type->Is<type::Struct>()) {
      out_ << " [[stage_in]]";
    } else {
      auto& decos = var->decorations();
      bool builtin_found = false;
      for (auto* deco : decos) {
        auto* builtin = deco->As<ast::BuiltinDecoration>();
        if (!builtin) {
          continue;
        }

        builtin_found = true;

        auto attr = builtin_to_attribute(builtin->value());
        if (attr.empty()) {
          diagnostics_.add_error("unknown builtin");
          return false;
        }
        out_ << " [[" << attr << "]]";
      }
      if (!builtin_found) {
        TINT_ICE(diagnostics_) << "Unsupported entry point parameter";
      }
    }
  }

  // TODO(crbug.com/tint/697): Remove this.
  for (auto data : func_sem->ReferencedBuiltinVariables()) {
    auto* var = data.first;
    if (var->StorageClass() != ast::StorageClass::kInput) {
      continue;
    }

    if (!first) {
      out_ << ", ";
    }
    first = false;

    auto* builtin = data.second;

    if (!EmitType(var->Type(), "")) {
      return false;
    }

    auto attr = builtin_to_attribute(builtin->value());
    if (attr.empty()) {
      diagnostics_.add_error("unknown builtin");
      return false;
    }
    out_ << " " << program_->Symbols().NameFor(var->Declaration()->symbol())
         << " [[" << attr << "]]";
  }

  for (auto data : func_sem->ReferencedUniformVariables()) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    auto* var = data.first;
    // TODO(dsinclair): We're using the binding to make up the buffer number but
    // we should instead be using a provided mapping that uses both buffer and
    // set. https://bugs.chromium.org/p/tint/issues/detail?id=104
    auto* binding = data.second.binding;
    if (binding == nullptr) {
      diagnostics_.add_error(
          "unable to find binding information for uniform: " +
          program_->Symbols().NameFor(var->Declaration()->symbol()));
      return false;
    }
    // auto* set = data.second.set;

    out_ << "constant ";
    // TODO(dsinclair): Can you have a uniform array? If so, this needs to be
    // updated to handle arrays property.
    if (!EmitType(var->Type(), "")) {
      return false;
    }
    out_ << "& " << program_->Symbols().NameFor(var->Declaration()->symbol())
         << " [[buffer(" << binding->value() << ")]]";
  }

  for (auto data : func_sem->ReferencedStorageBufferVariables()) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    auto* var = data.first;
    // TODO(dsinclair): We're using the binding to make up the buffer number but
    // we should instead be using a provided mapping that uses both buffer and
    // set. https://bugs.chromium.org/p/tint/issues/detail?id=104
    auto* binding = data.second.binding;
    // auto* set = data.second.set;

    auto* ac = var->Type()->As<type::AccessControl>();
    if (ac == nullptr) {
      diagnostics_.add_error(
          "invalid type for storage buffer, expected access control");
      return false;
    }
    if (ac->IsReadOnly()) {
      out_ << "const ";
    }

    out_ << "device ";
    if (!EmitType(ac->type(), "")) {
      return false;
    }
    out_ << "& " << program_->Symbols().NameFor(var->Declaration()->symbol())
         << " [[buffer(" << binding->value() << ")]]";
  }

  out_ << ") {" << std::endl;

  increment_indent();

  if (has_out_data) {
    make_indent();
    out_ << out_data->second.struct_name << " " << out_data->second.var_name
         << " = {};" << std::endl;
  }

  generating_entry_point_ = true;
  for (auto* s : *func->body()) {
    if (!EmitStatement(s)) {
      return false;
    }
  }
  auto* last_statement = func->get_last_statement();
  if (last_statement == nullptr ||
      !last_statement->Is<ast::ReturnStatement>()) {
    ast::ReturnStatement ret(Source{});
    if (!EmitStatement(&ret)) {
      return false;
    }
  }
  generating_entry_point_ = false;

  decrement_indent();
  make_indent();
  out_ << "}" << std::endl;

  current_ep_sym_ = Symbol();
  return true;
}

bool GeneratorImpl::global_is_in_struct(const semantic::Variable* var) const {
  bool in_or_out_struct_has_location =
      var != nullptr && var->Declaration()->HasLocationDecoration() &&
      (var->StorageClass() == ast::StorageClass::kInput ||
       var->StorageClass() == ast::StorageClass::kOutput);
  bool in_struct_has_builtin =
      var != nullptr && var->Declaration()->HasBuiltinDecoration() &&
      var->StorageClass() == ast::StorageClass::kOutput;
  return in_or_out_struct_has_location || in_struct_has_builtin;
}

bool GeneratorImpl::EmitIdentifier(ast::IdentifierExpression* expr) {
  auto* ident = expr->As<ast::IdentifierExpression>();
  const semantic::Variable* var = nullptr;
  if (global_variables_.get(ident->symbol(), &var)) {
    if (global_is_in_struct(var)) {
      auto var_type = var->StorageClass() == ast::StorageClass::kInput
                          ? VarType::kIn
                          : VarType::kOut;
      auto name = current_ep_var_name(var_type);
      if (name.empty()) {
        diagnostics_.add_error("unable to find entry point data for variable");
        return false;
      }
      out_ << name << ".";
    }
  }

  out_ << program_->Symbols().NameFor(ident->symbol());

  return true;
}

bool GeneratorImpl::EmitLoop(ast::LoopStatement* stmt) {
  loop_emission_counter_++;

  std::string guard =
      "tint_msl_is_first_" + std::to_string(loop_emission_counter_);

  if (stmt->has_continuing()) {
    make_indent();

    // Continuing variables get their own scope.
    out_ << "{" << std::endl;
    increment_indent();

    make_indent();
    out_ << "bool " << guard << " = true;" << std::endl;

    // A continuing block may use variables declared in the method body. As a
    // first pass, if we have a continuing, we pull all declarations outside
    // the for loop into the continuing scope. Then, the variable declarations
    // will be turned into assignments.
    for (auto* s : *(stmt->body())) {
      if (auto* decl = s->As<ast::VariableDeclStatement>()) {
        if (!EmitVariable(program_->Sem().Get(decl->variable()), true)) {
          return false;
        }
      }
    }
  }

  make_indent();
  out_ << "for(;;) {" << std::endl;
  increment_indent();

  if (stmt->has_continuing()) {
    make_indent();
    out_ << "if (!" << guard << ") ";

    if (!EmitBlockAndNewline(stmt->continuing())) {
      return false;
    }

    make_indent();
    out_ << guard << " = false;" << std::endl;
    out_ << std::endl;
  }

  for (auto* s : *(stmt->body())) {
    // If we have a continuing block we've already emitted the variable
    // declaration before the loop, so treat it as an assignment.
    auto* decl = s->As<ast::VariableDeclStatement>();
    if (decl != nullptr && stmt->has_continuing()) {
      make_indent();

      auto* var = decl->variable();
      out_ << program_->Symbols().NameFor(var->symbol()) << " = ";
      if (var->constructor() != nullptr) {
        if (!EmitExpression(var->constructor())) {
          return false;
        }
      } else {
        if (!EmitZeroValue(program_->Sem().Get(var)->Type())) {
          return false;
        }
      }
      out_ << ";" << std::endl;
      continue;
    }

    if (!EmitStatement(s)) {
      return false;
    }
  }

  decrement_indent();
  make_indent();
  out_ << "}" << std::endl;

  // Close the scope for any continuing variables.
  if (stmt->has_continuing()) {
    decrement_indent();
    make_indent();
    out_ << "}" << std::endl;
  }

  return true;
}

bool GeneratorImpl::EmitDiscard(ast::DiscardStatement*) {
  make_indent();
  // TODO(dsinclair): Verify this is correct when the discard semantics are
  // defined for WGSL (https://github.com/gpuweb/gpuweb/issues/361)
  out_ << "discard_fragment();" << std::endl;
  return true;
}

bool GeneratorImpl::EmitElse(ast::ElseStatement* stmt) {
  if (stmt->HasCondition()) {
    out_ << " else if (";
    if (!EmitExpression(stmt->condition())) {
      return false;
    }
    out_ << ") ";
  } else {
    out_ << " else ";
  }

  return EmitBlock(stmt->body());
}

bool GeneratorImpl::EmitIf(ast::IfStatement* stmt) {
  make_indent();

  out_ << "if (";
  if (!EmitExpression(stmt->condition())) {
    return false;
  }
  out_ << ") ";

  if (!EmitBlock(stmt->body())) {
    return false;
  }

  for (auto* e : stmt->else_statements()) {
    if (!EmitElse(e)) {
      return false;
    }
  }
  out_ << std::endl;

  return true;
}

bool GeneratorImpl::EmitMemberAccessor(ast::MemberAccessorExpression* expr) {
  if (!EmitExpression(expr->structure())) {
    return false;
  }

  out_ << ".";

  // Swizzles get written out directly
  if (program_->Sem().Get(expr)->IsSwizzle()) {
    out_ << program_->Symbols().NameFor(expr->member()->symbol());
  } else if (!EmitExpression(expr->member())) {
    return false;
  }

  return true;
}

bool GeneratorImpl::EmitReturn(ast::ReturnStatement* stmt) {
  make_indent();

  out_ << "return";

  // TODO(crbug.com/tint/697): Remove this conditional.
  if (generating_entry_point_) {
    auto out_data = ep_sym_to_out_data_.find(current_ep_sym_);
    if (out_data != ep_sym_to_out_data_.end()) {
      out_ << " " << out_data->second.var_name;
    }
  }
  if (stmt->has_value()) {
    out_ << " ";
    if (!EmitExpression(stmt->value())) {
      return false;
    }
  }
  out_ << ";" << std::endl;
  return true;
}

bool GeneratorImpl::EmitBlock(const ast::BlockStatement* stmt) {
  out_ << "{" << std::endl;
  increment_indent();

  for (auto* s : *stmt) {
    if (!EmitStatement(s)) {
      return false;
    }
  }

  decrement_indent();
  make_indent();
  out_ << "}";

  return true;
}

bool GeneratorImpl::EmitBlockAndNewline(const ast::BlockStatement* stmt) {
  const bool result = EmitBlock(stmt);
  if (result) {
    out_ << std::endl;
  }
  return result;
}

bool GeneratorImpl::EmitIndentedBlockAndNewline(ast::BlockStatement* stmt) {
  make_indent();
  const bool result = EmitBlock(stmt);
  if (result) {
    out_ << std::endl;
  }
  return result;
}

bool GeneratorImpl::EmitStatement(ast::Statement* stmt) {
  if (auto* a = stmt->As<ast::AssignmentStatement>()) {
    return EmitAssign(a);
  }
  if (auto* b = stmt->As<ast::BlockStatement>()) {
    return EmitIndentedBlockAndNewline(b);
  }
  if (auto* b = stmt->As<ast::BreakStatement>()) {
    return EmitBreak(b);
  }
  if (auto* c = stmt->As<ast::CallStatement>()) {
    make_indent();
    if (!EmitCall(c->expr())) {
      return false;
    }
    out_ << ";" << std::endl;
    return true;
  }
  if (auto* c = stmt->As<ast::ContinueStatement>()) {
    return EmitContinue(c);
  }
  if (auto* d = stmt->As<ast::DiscardStatement>()) {
    return EmitDiscard(d);
  }
  if (stmt->As<ast::FallthroughStatement>()) {
    make_indent();
    out_ << "/* fallthrough */" << std::endl;
    return true;
  }
  if (auto* i = stmt->As<ast::IfStatement>()) {
    return EmitIf(i);
  }
  if (auto* l = stmt->As<ast::LoopStatement>()) {
    return EmitLoop(l);
  }
  if (auto* r = stmt->As<ast::ReturnStatement>()) {
    return EmitReturn(r);
  }
  if (auto* s = stmt->As<ast::SwitchStatement>()) {
    return EmitSwitch(s);
  }
  if (auto* v = stmt->As<ast::VariableDeclStatement>()) {
    auto* var = program_->Sem().Get(v->variable());
    return EmitVariable(var, false);
  }

  diagnostics_.add_error("unknown statement type: " + program_->str(stmt));
  return false;
}

bool GeneratorImpl::EmitSwitch(ast::SwitchStatement* stmt) {
  make_indent();

  out_ << "switch(";
  if (!EmitExpression(stmt->condition())) {
    return false;
  }
  out_ << ") {" << std::endl;

  increment_indent();

  for (auto* s : stmt->body()) {
    if (!EmitCase(s)) {
      return false;
    }
  }

  decrement_indent();
  make_indent();
  out_ << "}" << std::endl;

  return true;
}

bool GeneratorImpl::EmitType(type::Type* type, const std::string& name) {
  std::string access_str = "";
  if (auto* ac = type->As<type::AccessControl>()) {
    if (ac->access_control() == ast::AccessControl::kReadOnly) {
      access_str = "read";
    } else if (ac->access_control() == ast::AccessControl::kWriteOnly) {
      access_str = "write";
    } else {
      diagnostics_.add_error("Invalid access control for storage texture");
      return false;
    }

    type = ac->type();
  }

  if (auto* alias = type->As<type::Alias>()) {
    out_ << program_->Symbols().NameFor(alias->symbol());
  } else if (auto* ary = type->As<type::Array>()) {
    type::Type* base_type = ary;
    std::vector<uint32_t> sizes;
    while (auto* arr = base_type->As<type::Array>()) {
      if (arr->IsRuntimeArray()) {
        sizes.push_back(1);
      } else {
        sizes.push_back(arr->size());
      }
      base_type = arr->type();
    }
    if (!EmitType(base_type, "")) {
      return false;
    }
    if (!name.empty()) {
      out_ << " " << name;
    }
    for (uint32_t size : sizes) {
      out_ << "[" << size << "]";
    }
  } else if (type->Is<type::Bool>()) {
    out_ << "bool";
  } else if (type->Is<type::F32>()) {
    out_ << "float";
  } else if (type->Is<type::I32>()) {
    out_ << "int";
  } else if (auto* mat = type->As<type::Matrix>()) {
    if (!EmitType(mat->type(), "")) {
      return false;
    }
    out_ << mat->columns() << "x" << mat->rows();
  } else if (auto* ptr = type->As<type::Pointer>()) {
    // TODO(dsinclair): Storage class?
    if (!EmitType(ptr->type(), "")) {
      return false;
    }
    out_ << "*";
  } else if (type->Is<type::Sampler>()) {
    out_ << "sampler";
  } else if (auto* str = type->As<type::Struct>()) {
    // The struct type emits as just the name. The declaration would be emitted
    // as part of emitting the constructed types.
    out_ << program_->Symbols().NameFor(str->symbol());
  } else if (auto* tex = type->As<type::Texture>()) {
    if (tex->Is<type::DepthTexture>()) {
      out_ << "depth";
    } else {
      out_ << "texture";
    }

    switch (tex->dim()) {
      case type::TextureDimension::k1d:
        out_ << "1d";
        break;
      case type::TextureDimension::k2d:
        out_ << "2d";
        break;
      case type::TextureDimension::k2dArray:
        out_ << "2d_array";
        break;
      case type::TextureDimension::k3d:
        out_ << "3d";
        break;
      case type::TextureDimension::kCube:
        out_ << "cube";
        break;
      case type::TextureDimension::kCubeArray:
        out_ << "cube_array";
        break;
      default:
        diagnostics_.add_error("Invalid texture dimensions");
        return false;
    }
    if (tex->Is<type::MultisampledTexture>()) {
      out_ << "_ms";
    }
    out_ << "<";
    if (tex->Is<type::DepthTexture>()) {
      out_ << "float, access::sample";
    } else if (auto* storage = tex->As<type::StorageTexture>()) {
      if (!EmitType(storage->type(), "")) {
        return false;
      }
      out_ << ", access::" << access_str;
    } else if (auto* ms = tex->As<type::MultisampledTexture>()) {
      if (!EmitType(ms->type(), "")) {
        return false;
      }
      out_ << ", access::sample";
    } else if (auto* sampled = tex->As<type::SampledTexture>()) {
      if (!EmitType(sampled->type(), "")) {
        return false;
      }
      out_ << ", access::sample";
    } else {
      diagnostics_.add_error("invalid texture type");
      return false;
    }
    out_ << ">";

  } else if (type->Is<type::U32>()) {
    out_ << "uint";
  } else if (auto* vec = type->As<type::Vector>()) {
    if (!EmitType(vec->type(), "")) {
      return false;
    }
    out_ << vec->size();
  } else if (type->Is<type::Void>()) {
    out_ << "void";
  } else {
    diagnostics_.add_error("unknown type in EmitType: " + type->type_name());
    return false;
  }

  return true;
}

bool GeneratorImpl::EmitPackedType(type::Type* type, const std::string& name) {
  if (auto* alias = type->As<type::Alias>()) {
    return EmitPackedType(alias->type(), name);
  }

  if (auto* vec = type->As<type::Vector>()) {
    out_ << "packed_";
    if (!EmitType(vec->type(), "")) {
      return false;
    }
    out_ << vec->size();
    return true;
  }

  return EmitType(type, name);
}

bool GeneratorImpl::EmitStructType(const type::Struct* str) {
  // TODO(dsinclair): Block decoration?
  // if (str->impl()->decoration() != ast::Decoration::kNone) {
  // }
  out_ << "struct " << program_->Symbols().NameFor(str->symbol()) << " {"
       << std::endl;

  auto* sem_str = program_->Sem().Get(str);
  if (!sem_str) {
    TINT_ICE(diagnostics_) << "struct  missing semantic info";
    return false;
  }

  bool is_host_shareable = sem_str->IsHostShareable();

  // Emits a `/* 0xnnnn */` byte offset comment for a struct member.
  auto add_byte_offset_comment = [&](uint32_t offset) {
    std::ios_base::fmtflags saved_flag_state(out_.flags());
    out_ << "/* 0x" << std::hex << std::setfill('0') << std::setw(4) << offset
         << " */ ";
    out_.flags(saved_flag_state);
  };

  uint32_t pad_count = 0;
  auto add_padding = [&](uint32_t size) {
    out_ << "int8_t _tint_pad_" << pad_count << "[" << size << "];"
         << std::endl;
    pad_count++;
  };

  increment_indent();
  uint32_t msl_offset = 0;
  for (auto* mem : str->impl()->members()) {
    make_indent();

    auto* sem_mem = program_->Sem().Get(mem);
    if (!sem_mem) {
      TINT_ICE(diagnostics_) << "struct member missing semantic info";
      return false;
    }

    auto wgsl_offset = sem_mem->Offset();

    if (is_host_shareable) {
      if (wgsl_offset < msl_offset) {
        // Unimplementable layout
        TINT_ICE(diagnostics_)
            << "Structure member WGSL offset (" << wgsl_offset
            << ") is behind MSL offset (" << msl_offset << ")";
        return false;
      }

      // Generate padding if required
      if (auto padding = wgsl_offset - msl_offset) {
        add_byte_offset_comment(msl_offset);
        add_padding(padding);
        msl_offset += padding;
        make_indent();
      }

      add_byte_offset_comment(msl_offset);

      if (!EmitPackedType(mem->type(),
                          program_->Symbols().NameFor(mem->symbol()))) {
        return false;
      }
    } else {
      if (!EmitType(mem->type(), program_->Symbols().NameFor(mem->symbol()))) {
        return false;
      }
    }

    auto* ty = mem->type()->UnwrapAliasIfNeeded();

    // Array member name will be output with the type
    if (!ty->Is<type::Array>()) {
      out_ << " " << program_->Symbols().NameFor(mem->symbol());
    }

    // Emit decorations
    for (auto* deco : mem->decorations()) {
      if (auto* builtin = deco->As<ast::BuiltinDecoration>()) {
        auto attr = builtin_to_attribute(builtin->value());
        if (attr.empty()) {
          diagnostics_.add_error("unknown builtin");
          return false;
        }
        out_ << " [[" << attr << "]]";
      } else if (auto* loc = deco->As<ast::LocationDecoration>()) {
        auto& pipeline_stage_uses =
            program_->Sem().Get(str)->PipelineStageUses();
        if (pipeline_stage_uses.size() != 1) {
          TINT_ICE(diagnostics_) << "invalid entry point IO struct uses";
        }

        if (pipeline_stage_uses.count(
                semantic::PipelineStageUsage::kVertexInput)) {
          out_ << " [[attribute(" + std::to_string(loc->value()) + ")]]";
        } else if (pipeline_stage_uses.count(
                       semantic::PipelineStageUsage::kVertexOutput)) {
          out_ << " [[user(locn" + std::to_string(loc->value()) + ")]]";
        } else if (pipeline_stage_uses.count(
                       semantic::PipelineStageUsage::kFragmentInput)) {
          out_ << " [[user(locn" + std::to_string(loc->value()) + ")]]";
        } else if (pipeline_stage_uses.count(
                       semantic::PipelineStageUsage::kFragmentOutput)) {
          out_ << " [[color(" + std::to_string(loc->value()) + ")]]";
        } else {
          TINT_ICE(diagnostics_) << "invalid use of location decoration";
        }
      }
    }

    out_ << ";" << std::endl;

    if (is_host_shareable) {
      // Calculate new MSL offset
      auto size_align = MslPackedTypeSizeAndAlign(ty);
      if (msl_offset % size_align.align) {
        TINT_ICE(diagnostics_) << "Misaligned MSL structure member "
                               << ty->FriendlyName(program_->Symbols()) << " "
                               << program_->Symbols().NameFor(mem->symbol());
        return false;
      }
      msl_offset += size_align.size;
    }
  }

  if (is_host_shareable && sem_str->Size() != msl_offset) {
    make_indent();
    add_byte_offset_comment(msl_offset);
    add_padding(sem_str->Size() - msl_offset);
  }

  decrement_indent();
  make_indent();

  out_ << "};" << std::endl;
  return true;
}

bool GeneratorImpl::EmitUnaryOp(ast::UnaryOpExpression* expr) {
  switch (expr->op()) {
    case ast::UnaryOp::kNot:
      out_ << "!";
      break;
    case ast::UnaryOp::kNegation:
      out_ << "-";
      break;
  }
  out_ << "(";

  if (!EmitExpression(expr->expr())) {
    return false;
  }

  out_ << ")";

  return true;
}

bool GeneratorImpl::EmitVariable(const semantic::Variable* var,
                                 bool skip_constructor) {
  make_indent();

  auto* decl = var->Declaration();

  // TODO(dsinclair): Handle variable decorations
  if (!decl->decorations().empty()) {
    diagnostics_.add_error("Variable decorations are not handled yet");
    return false;
  }
  if (decl->is_const()) {
    out_ << "const ";
  }
  if (!EmitType(var->Type(), program_->Symbols().NameFor(decl->symbol()))) {
    return false;
  }
  if (!var->Type()->Is<type::Array>()) {
    out_ << " " << program_->Symbols().NameFor(decl->symbol());
  }

  if (!skip_constructor) {
    out_ << " = ";
    if (decl->constructor() != nullptr) {
      if (!EmitExpression(decl->constructor())) {
        return false;
      }
    } else if (var->StorageClass() == ast::StorageClass::kPrivate ||
               var->StorageClass() == ast::StorageClass::kFunction ||
               var->StorageClass() == ast::StorageClass::kNone ||
               var->StorageClass() == ast::StorageClass::kOutput) {
      if (!EmitZeroValue(var->Type())) {
        return false;
      }
    }
  }
  out_ << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitProgramConstVariable(const ast::Variable* var) {
  make_indent();

  for (auto* d : var->decorations()) {
    if (!d->Is<ast::ConstantIdDecoration>()) {
      diagnostics_.add_error("Decorated const values not valid");
      return false;
    }
  }
  if (!var->is_const()) {
    diagnostics_.add_error("Expected a const value");
    return false;
  }

  out_ << "constant ";
  auto* type = program_->Sem().Get(var)->Type();
  if (!EmitType(type, program_->Symbols().NameFor(var->symbol()))) {
    return false;
  }
  if (!type->Is<type::Array>()) {
    out_ << " " << program_->Symbols().NameFor(var->symbol());
  }

  if (var->HasConstantIdDecoration()) {
    out_ << " [[function_constant(" << var->constant_id() << ")]]";
  } else if (var->constructor() != nullptr) {
    out_ << " = ";
    if (!EmitExpression(var->constructor())) {
      return false;
    }
  }
  out_ << ";" << std::endl;

  return true;
}

GeneratorImpl::SizeAndAlign GeneratorImpl::MslPackedTypeSizeAndAlign(
    type::Type* ty) {
  ty = ty->UnwrapAliasIfNeeded();

  if (ty->IsAnyOf<type::U32, type::I32, type::F32>()) {
    // https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
    // 2.1 Scalar Data Types
    return {4, 4};
  }

  if (auto* vec = ty->As<type::Vector>()) {
    // https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
    // 2.2.3 Packed Vector Types
    auto num_els = vec->size();
    auto* el_ty = vec->type()->UnwrapAll();
    if (el_ty->IsAnyOf<type::U32, type::I32, type::F32>()) {
      return SizeAndAlign{num_els * 4, 4};
    }
  }

  if (auto* mat = ty->As<type::Matrix>()) {
    // https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
    // 2.3 Matrix Data Types
    auto cols = mat->columns();
    auto rows = mat->rows();
    auto* el_ty = mat->type()->UnwrapAll();
    if (el_ty->IsAnyOf<type::U32, type::I32, type::F32>()) {
      static constexpr SizeAndAlign table[] = {
          /* float2x2 */ {16, 8},
          /* float2x3 */ {32, 16},
          /* float2x4 */ {32, 16},
          /* float3x2 */ {24, 8},
          /* float3x3 */ {48, 16},
          /* float3x4 */ {48, 16},
          /* float4x2 */ {32, 8},
          /* float4x3 */ {64, 16},
          /* float4x4 */ {64, 16},
      };
      if (cols >= 2 && cols <= 4 && rows >= 2 && rows <= 4) {
        return table[(3 * (cols - 2)) + (rows - 2)];
      }
    }
  }

  if (auto* arr = ty->As<type::Array>()) {
    auto* sem = program_->Sem().Get(arr);
    if (!sem) {
      TINT_ICE(diagnostics_) << "Array missing semantic info";
      return {};
    }
    auto el_size_align = MslPackedTypeSizeAndAlign(arr->type());
    if (sem->Stride() != el_size_align.size) {
      // TODO(crbug.com/tint/649): transform::Msl needs to replace these arrays
      // with a new array type that has the element type padded to the required
      // stride.
      TINT_UNIMPLEMENTED(diagnostics_)
          << "Arrays with custom strides not yet implemented";
      return {};
    }
    auto num_els = std::max<uint32_t>(arr->size(), 1);
    return SizeAndAlign{el_size_align.size * num_els, el_size_align.align};
  }

  if (auto* str = ty->As<type::Struct>()) {
    // TODO(crbug.com/tint/650): There's an assumption here that MSL's default
    // structure size and alignment matches WGSL's. We need to confirm this.
    auto* sem = program_->Sem().Get(str);
    if (!sem) {
      TINT_ICE(diagnostics_) << "Array missing semantic info";
      return {};
    }
    return SizeAndAlign{sem->Size(), sem->Align()};
  }

  TINT_UNREACHABLE(diagnostics_) << "Unhandled type " << ty->TypeInfo().name;
  return {};
}

}  // namespace msl
}  // namespace writer
}  // namespace tint
