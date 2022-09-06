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

#include "src/tint/writer/msl/generator_impl.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <utility>
#include <vector>

#include "src/tint/ast/alias.h"
#include "src/tint/ast/bool_literal_expression.h"
#include "src/tint/ast/call_statement.h"
#include "src/tint/ast/disable_validation_attribute.h"
#include "src/tint/ast/fallthrough_statement.h"
#include "src/tint/ast/float_literal_expression.h"
#include "src/tint/ast/id_attribute.h"
#include "src/tint/ast/interpolate_attribute.h"
#include "src/tint/ast/module.h"
#include "src/tint/ast/variable_decl_statement.h"
#include "src/tint/ast/void.h"
#include "src/tint/sem/array.h"
#include "src/tint/sem/atomic.h"
#include "src/tint/sem/bool.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/constant.h"
#include "src/tint/sem/depth_multisampled_texture.h"
#include "src/tint/sem/depth_texture.h"
#include "src/tint/sem/f32.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/i32.h"
#include "src/tint/sem/matrix.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/module.h"
#include "src/tint/sem/multisampled_texture.h"
#include "src/tint/sem/pointer.h"
#include "src/tint/sem/reference.h"
#include "src/tint/sem/sampled_texture.h"
#include "src/tint/sem/storage_texture.h"
#include "src/tint/sem/struct.h"
#include "src/tint/sem/type_constructor.h"
#include "src/tint/sem/type_conversion.h"
#include "src/tint/sem/u32.h"
#include "src/tint/sem/variable.h"
#include "src/tint/sem/vector.h"
#include "src/tint/sem/void.h"
#include "src/tint/transform/array_length_from_uniform.h"
#include "src/tint/transform/builtin_polyfill.h"
#include "src/tint/transform/canonicalize_entry_point_io.h"
#include "src/tint/transform/disable_uniformity_analysis.h"
#include "src/tint/transform/expand_compound_assignment.h"
#include "src/tint/transform/manager.h"
#include "src/tint/transform/module_scope_var_to_entry_point_param.h"
#include "src/tint/transform/promote_initializers_to_const_var.h"
#include "src/tint/transform/promote_side_effects_to_decl.h"
#include "src/tint/transform/remove_phonies.h"
#include "src/tint/transform/simplify_pointers.h"
#include "src/tint/transform/unshadow.h"
#include "src/tint/transform/unwind_discard_functions.h"
#include "src/tint/transform/vectorize_scalar_matrix_constructors.h"
#include "src/tint/transform/wrap_arrays_in_structs.h"
#include "src/tint/transform/zero_init_workgroup_memory.h"
#include "src/tint/utils/defer.h"
#include "src/tint/utils/map.h"
#include "src/tint/utils/scoped_assignment.h"
#include "src/tint/writer/float_to_string.h"
#include "src/tint/writer/generate_external_texture_bindings.h"

namespace tint::writer::msl {
namespace {

bool last_is_break_or_fallthrough(const ast::BlockStatement* stmts) {
    return IsAnyOf<ast::BreakStatement, ast::FallthroughStatement>(stmts->Last());
}

void PrintF32(std::ostream& out, float value) {
    // Note: Currently inf and nan should not be constructable, but this is implemented for the day
    // we support them.
    if (std::isinf(value)) {
        out << (value >= 0 ? "INFINITY" : "-INFINITY");
    } else if (std::isnan(value)) {
        out << "NAN";
    } else {
        out << FloatToString(value) << "f";
    }
}

void PrintI32(std::ostream& out, int32_t value) {
    // MSL (and C++) parse `-2147483648` as a `long` because it parses unary minus and `2147483648`
    // as separate tokens, and the latter doesn't fit into an (32-bit) `int`.
    // WGSL, on the other hand, parses this as an `i32`.
    // To avoid issues with `long` to `int` casts, emit `(-2147483647 - 1)` instead, which ensures
    // the expression type is `int`.
    if (auto int_min = std::numeric_limits<int32_t>::min(); value == int_min) {
        out << "(" << int_min + 1 << " - 1)";
    } else {
        out << value;
    }
}

class ScopedBitCast {
  public:
    ScopedBitCast(GeneratorImpl* generator,
                  std::ostream& stream,
                  const sem::Type* curr_type,
                  const sem::Type* target_type)
        : s(stream) {
        auto* target_vec_type = target_type->As<sem::Vector>();

        // If we need to promote from scalar to vector, bitcast the scalar to the
        // vector element type.
        if (curr_type->is_scalar() && target_vec_type) {
            target_type = target_vec_type->type();
        }

        // Bit cast
        s << "as_type<";
        generator->EmitType(s, target_type, "");
        s << ">(";
    }

    ~ScopedBitCast() { s << ")"; }

  private:
    std::ostream& s;
};

}  // namespace

SanitizedResult::SanitizedResult() = default;
SanitizedResult::~SanitizedResult() = default;
SanitizedResult::SanitizedResult(SanitizedResult&&) = default;

SanitizedResult Sanitize(const Program* in, const Options& options) {
    transform::Manager manager;
    transform::DataMap data;

    manager.Add<transform::DisableUniformityAnalysis>();

    {  // Builtin polyfills
        transform::BuiltinPolyfill::Builtins polyfills;
        polyfills.extract_bits = transform::BuiltinPolyfill::Level::kClampParameters;
        polyfills.first_leading_bit = true;
        polyfills.first_trailing_bit = true;
        polyfills.insert_bits = transform::BuiltinPolyfill::Level::kClampParameters;
        data.Add<transform::BuiltinPolyfill::Config>(polyfills);
        manager.Add<transform::BuiltinPolyfill>();
    }

    // Build the config for the internal ArrayLengthFromUniform transform.
    auto& array_length_from_uniform = options.array_length_from_uniform;
    transform::ArrayLengthFromUniform::Config array_length_from_uniform_cfg(
        array_length_from_uniform.ubo_binding);
    if (!array_length_from_uniform.bindpoint_to_size_index.empty()) {
        // If |array_length_from_uniform| bindings are provided, use that config.
        array_length_from_uniform_cfg.bindpoint_to_size_index =
            array_length_from_uniform.bindpoint_to_size_index;
    } else {
        // If the binding map is empty, use the deprecated |buffer_size_ubo_index|
        // and automatically choose indices using the binding numbers.
        array_length_from_uniform_cfg = transform::ArrayLengthFromUniform::Config(
            sem::BindingPoint{0, options.buffer_size_ubo_index});
        // Use the SSBO binding numbers as the indices for the buffer size lookups.
        for (auto* var : in->AST().GlobalVariables()) {
            auto* global = in->Sem().Get<sem::GlobalVariable>(var);
            if (global && global->StorageClass() == ast::StorageClass::kStorage) {
                array_length_from_uniform_cfg.bindpoint_to_size_index.emplace(
                    global->BindingPoint(), global->BindingPoint().binding);
            }
        }
    }

    // Build the configs for the internal CanonicalizeEntryPointIO transform.
    auto entry_point_io_cfg = transform::CanonicalizeEntryPointIO::Config(
        transform::CanonicalizeEntryPointIO::ShaderStyle::kMsl, options.fixed_sample_mask,
        options.emit_vertex_point_size);

    if (options.generate_external_texture_bindings) {
        auto new_bindings_map = GenerateExternalTextureBindings(in);
        data.Add<transform::MultiplanarExternalTexture::NewBindingPoints>(new_bindings_map);
    }
    manager.Add<transform::MultiplanarExternalTexture>();

    manager.Add<transform::Unshadow>();

    if (!options.disable_workgroup_init) {
        // ZeroInitWorkgroupMemory must come before CanonicalizeEntryPointIO as
        // ZeroInitWorkgroupMemory may inject new builtin parameters.
        manager.Add<transform::ZeroInitWorkgroupMemory>();
    }
    manager.Add<transform::CanonicalizeEntryPointIO>();
    manager.Add<transform::ExpandCompoundAssignment>();
    manager.Add<transform::PromoteSideEffectsToDecl>();
    manager.Add<transform::UnwindDiscardFunctions>();
    manager.Add<transform::PromoteInitializersToConstVar>();

    manager.Add<transform::VectorizeScalarMatrixConstructors>();
    manager.Add<transform::WrapArraysInStructs>();
    manager.Add<transform::RemovePhonies>();
    manager.Add<transform::SimplifyPointers>();
    // ArrayLengthFromUniform must come after SimplifyPointers, as
    // it assumes that the form of the array length argument is &var.array.
    manager.Add<transform::ArrayLengthFromUniform>();
    manager.Add<transform::ModuleScopeVarToEntryPointParam>();
    data.Add<transform::ArrayLengthFromUniform::Config>(std::move(array_length_from_uniform_cfg));
    data.Add<transform::CanonicalizeEntryPointIO::Config>(std::move(entry_point_io_cfg));
    auto out = manager.Run(in, data);

    SanitizedResult result;
    result.program = std::move(out.program);
    if (!result.program.IsValid()) {
        return result;
    }
    if (auto* res = out.data.Get<transform::ArrayLengthFromUniform::Result>()) {
        result.used_array_length_from_uniform_indices = std::move(res->used_size_indices);
    }
    result.needs_storage_buffer_sizes = !result.used_array_length_from_uniform_indices.empty();
    return result;
}

GeneratorImpl::GeneratorImpl(const Program* program) : TextGenerator(program) {}

GeneratorImpl::~GeneratorImpl() = default;

bool GeneratorImpl::Generate() {
    line() << "#include <metal_stdlib>";
    line();
    line() << "using namespace metal;";

    auto helpers_insertion_point = current_buffer_->lines.size();

    auto* mod = builder_.Sem().Module();
    for (auto* decl : mod->DependencyOrderedDeclarations()) {
        bool ok = Switch(
            decl,  //
            [&](const ast::Struct* str) {
                TINT_DEFER(line());
                return EmitTypeDecl(TypeOf(str));
            },
            [&](const ast::Alias*) {
                return true;  // folded away by the writer
            },
            [&](const ast::Variable* var) {
                if (var->is_const) {
                    TINT_DEFER(line());
                    return EmitProgramConstVariable(var);
                }
                // These are pushed into the entry point by sanitizer transforms.
                TINT_ICE(Writer, diagnostics_)
                    << "module-scope variables should have been handled by the MSL "
                       "sanitizer";
                return false;
            },
            [&](const ast::Function* func) {
                TINT_DEFER(line());
                if (func->IsEntryPoint()) {
                    return EmitEntryPointFunction(func);
                }
                return EmitFunction(func);
            },
            [&](const ast::Enable*) {
                // Do nothing for enabling extension in MSL
                return true;
            },
            [&](Default) {
                // These are pushed into the entry point by sanitizer transforms.
                TINT_ICE(Writer, diagnostics_) << "unhandled type: " << decl->TypeInfo().name;
                return false;
            });
        if (!ok) {
            return false;
        }
    }

    if (!invariant_define_name_.empty()) {
        // 'invariant' attribute requires MSL 2.1 or higher.
        // WGSL can ignore the invariant attribute on pre MSL 2.1 devices.
        // See: https://github.com/gpuweb/gpuweb/issues/893#issuecomment-745537465
        line(&helpers_) << "#if __METAL_VERSION__ >= 210";
        line(&helpers_) << "#define " << invariant_define_name_ << " @invariant";
        line(&helpers_) << "#else";
        line(&helpers_) << "#define " << invariant_define_name_;
        line(&helpers_) << "#endif";
        line(&helpers_);
    }

    if (!helpers_.lines.empty()) {
        current_buffer_->Insert("", helpers_insertion_point++, 0);
        current_buffer_->Insert(helpers_, helpers_insertion_point++, 0);
    }

    return true;
}

bool GeneratorImpl::EmitTypeDecl(const sem::Type* ty) {
    if (auto* str = ty->As<sem::Struct>()) {
        if (!EmitStructType(current_buffer_, str)) {
            return false;
        }
    } else {
        diagnostics_.add_error(diag::System::Writer,
                               "unknown alias type: " + ty->FriendlyName(builder_.Symbols()));
        return false;
    }

    return true;
}

bool GeneratorImpl::EmitIndexAccessor(std::ostream& out, const ast::IndexAccessorExpression* expr) {
    bool paren_lhs =
        !expr->object->IsAnyOf<ast::IndexAccessorExpression, ast::CallExpression,
                               ast::IdentifierExpression, ast::MemberAccessorExpression>();

    if (paren_lhs) {
        out << "(";
    }
    if (!EmitExpression(out, expr->object)) {
        return false;
    }
    if (paren_lhs) {
        out << ")";
    }

    out << "[";

    if (!EmitExpression(out, expr->index)) {
        return false;
    }
    out << "]";

    return true;
}

bool GeneratorImpl::EmitBitcast(std::ostream& out, const ast::BitcastExpression* expr) {
    out << "as_type<";
    if (!EmitType(out, TypeOf(expr)->UnwrapRef(), "")) {
        return false;
    }

    out << ">(";
    if (!EmitExpression(out, expr->expr)) {
        return false;
    }

    out << ")";
    return true;
}

bool GeneratorImpl::EmitAssign(const ast::AssignmentStatement* stmt) {
    auto out = line();

    if (!EmitExpression(out, stmt->lhs)) {
        return false;
    }

    out << " = ";

    if (!EmitExpression(out, stmt->rhs)) {
        return false;
    }

    out << ";";

    return true;
}

bool GeneratorImpl::EmitBinary(std::ostream& out, const ast::BinaryExpression* expr) {
    auto emit_op = [&] {
        out << " ";

        switch (expr->op) {
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
                out << "&&";
                break;
            case ast::BinaryOp::kLogicalOr:
                out << "||";
                break;
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
                // generate extra code to implement WGSL-specified behaviour for
                // negative LHS.
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
                diagnostics_.add_error(diag::System::Writer, "missing binary operation type");
                return false;
        }
        out << " ";
        return true;
    };

    auto signed_type_of = [&](const sem::Type* ty) -> const sem::Type* {
        if (ty->is_integer_scalar()) {
            return builder_.create<sem::I32>();
        } else if (auto* v = ty->As<sem::Vector>()) {
            return builder_.create<sem::Vector>(builder_.create<sem::I32>(), v->Width());
        }
        return {};
    };

    auto unsigned_type_of = [&](const sem::Type* ty) -> const sem::Type* {
        if (ty->is_integer_scalar()) {
            return builder_.create<sem::U32>();
        } else if (auto* v = ty->As<sem::Vector>()) {
            return builder_.create<sem::Vector>(builder_.create<sem::U32>(), v->Width());
        }
        return {};
    };

    auto* lhs_type = TypeOf(expr->lhs)->UnwrapRef();
    auto* rhs_type = TypeOf(expr->rhs)->UnwrapRef();

    // Handle fmod
    if (expr->op == ast::BinaryOp::kModulo && lhs_type->is_float_scalar_or_vector()) {
        out << "fmod";
        ScopedParen sp(out);
        if (!EmitExpression(out, expr->lhs)) {
            return false;
        }
        out << ", ";
        if (!EmitExpression(out, expr->rhs)) {
            return false;
        }
        return true;
    }

    // Handle +/-/* of signed values
    if ((expr->IsAdd() || expr->IsSubtract() || expr->IsMultiply()) &&
        lhs_type->is_signed_scalar_or_vector() && rhs_type->is_signed_scalar_or_vector()) {
        // If lhs or rhs is a vector, use that type (support implicit scalar to
        // vector promotion)
        auto* target_type = lhs_type->Is<sem::Vector>()
                                ? lhs_type
                                : (rhs_type->Is<sem::Vector>() ? rhs_type : lhs_type);

        // WGSL defines behaviour for signed overflow, MSL does not. For these
        // cases, bitcast operands to unsigned, then cast result to signed.
        ScopedBitCast outer_int_cast(this, out, target_type, signed_type_of(target_type));
        ScopedParen sp(out);
        {
            ScopedBitCast lhs_uint_cast(this, out, lhs_type, unsigned_type_of(target_type));
            if (!EmitExpression(out, expr->lhs)) {
                return false;
            }
        }
        if (!emit_op()) {
            return false;
        }
        {
            ScopedBitCast rhs_uint_cast(this, out, rhs_type, unsigned_type_of(target_type));
            if (!EmitExpression(out, expr->rhs)) {
                return false;
            }
        }
        return true;
    }

    // Handle left bit shifting a signed value
    // TODO(crbug.com/tint/1077): This may not be necessary. The MSL spec
    // seems to imply that left shifting a signed value is treated the same as
    // left shifting an unsigned value, but we need to make sure.
    if (expr->IsShiftLeft() && lhs_type->is_signed_scalar_or_vector()) {
        // Shift left: discards top bits, so convert first operand to unsigned
        // first, then convert result back to signed
        ScopedBitCast outer_int_cast(this, out, lhs_type, signed_type_of(lhs_type));
        ScopedParen sp(out);
        {
            ScopedBitCast lhs_uint_cast(this, out, lhs_type, unsigned_type_of(lhs_type));
            if (!EmitExpression(out, expr->lhs)) {
                return false;
            }
        }
        if (!emit_op()) {
            return false;
        }
        if (!EmitExpression(out, expr->rhs)) {
            return false;
        }
        return true;
    }

    // Handle '&' and '|' of booleans.
    if ((expr->IsAnd() || expr->IsOr()) && lhs_type->Is<sem::Bool>()) {
        out << "bool";
        ScopedParen sp(out);
        if (!EmitExpression(out, expr->lhs)) {
            return false;
        }
        if (!emit_op()) {
            return false;
        }
        if (!EmitExpression(out, expr->rhs)) {
            return false;
        }
        return true;
    }

    // Emit as usual
    ScopedParen sp(out);
    if (!EmitExpression(out, expr->lhs)) {
        return false;
    }
    if (!emit_op()) {
        return false;
    }
    if (!EmitExpression(out, expr->rhs)) {
        return false;
    }

    return true;
}

bool GeneratorImpl::EmitBreak(const ast::BreakStatement*) {
    line() << "break;";
    return true;
}

bool GeneratorImpl::EmitCall(std::ostream& out, const ast::CallExpression* expr) {
    auto* call = program_->Sem().Get<sem::Call>(expr);
    auto* target = call->Target();
    return Switch(
        target, [&](const sem::Function* func) { return EmitFunctionCall(out, call, func); },
        [&](const sem::Builtin* builtin) { return EmitBuiltinCall(out, call, builtin); },
        [&](const sem::TypeConversion* conv) { return EmitTypeConversion(out, call, conv); },
        [&](const sem::TypeConstructor* ctor) { return EmitTypeConstructor(out, call, ctor); },
        [&](Default) {
            TINT_ICE(Writer, diagnostics_) << "unhandled call target: " << target->TypeInfo().name;
            return false;
        });
}

bool GeneratorImpl::EmitFunctionCall(std::ostream& out,
                                     const sem::Call* call,
                                     const sem::Function*) {
    auto* ident = call->Declaration()->target.name;
    out << program_->Symbols().NameFor(ident->symbol) << "(";

    bool first = true;
    for (auto* arg : call->Arguments()) {
        if (!first) {
            out << ", ";
        }
        first = false;

        if (!EmitExpression(out, arg->Declaration())) {
            return false;
        }
    }

    out << ")";
    return true;
}

bool GeneratorImpl::EmitBuiltinCall(std::ostream& out,
                                    const sem::Call* call,
                                    const sem::Builtin* builtin) {
    auto* expr = call->Declaration();
    if (builtin->IsAtomic()) {
        return EmitAtomicCall(out, expr, builtin);
    }
    if (builtin->IsTexture()) {
        return EmitTextureCall(out, call, builtin);
    }

    auto name = generate_builtin_name(builtin);

    switch (builtin->Type()) {
        case sem::BuiltinType::kDot:
            return EmitDotCall(out, expr, builtin);
        case sem::BuiltinType::kModf:
            return EmitModfCall(out, expr, builtin);
        case sem::BuiltinType::kFrexp:
            return EmitFrexpCall(out, expr, builtin);
        case sem::BuiltinType::kDegrees:
            return EmitDegreesCall(out, expr, builtin);
        case sem::BuiltinType::kRadians:
            return EmitRadiansCall(out, expr, builtin);

        case sem::BuiltinType::kPack2x16float:
        case sem::BuiltinType::kUnpack2x16float: {
            if (builtin->Type() == sem::BuiltinType::kPack2x16float) {
                out << "as_type<uint>(half2(";
            } else {
                out << "float2(as_type<half2>(";
            }
            if (!EmitExpression(out, expr->args[0])) {
                return false;
            }
            out << "))";
            return true;
        }
        // TODO(crbug.com/tint/661): Combine sequential barriers to a single
        // instruction.
        case sem::BuiltinType::kStorageBarrier: {
            out << "threadgroup_barrier(mem_flags::mem_device)";
            return true;
        }
        case sem::BuiltinType::kWorkgroupBarrier: {
            out << "threadgroup_barrier(mem_flags::mem_threadgroup)";
            return true;
        }

        case sem::BuiltinType::kLength: {
            auto* sem = builder_.Sem().Get(expr->args[0]);
            if (sem->Type()->UnwrapRef()->is_scalar()) {
                // Emulate scalar overload using fabs(x).
                name = "fabs";
            }
            break;
        }

        case sem::BuiltinType::kDistance: {
            auto* sem = builder_.Sem().Get(expr->args[0]);
            if (sem->Type()->UnwrapRef()->is_scalar()) {
                // Emulate scalar overload using fabs(x - y);
                out << "fabs";
                ScopedParen sp(out);
                if (!EmitExpression(out, expr->args[0])) {
                    return false;
                }
                out << " - ";
                if (!EmitExpression(out, expr->args[1])) {
                    return false;
                }
                return true;
            }
            break;
        }

        default:
            break;
    }

    if (name.empty()) {
        return false;
    }

    out << name << "(";

    bool first = true;
    for (auto* arg : expr->args) {
        if (!first) {
            out << ", ";
        }
        first = false;

        if (!EmitExpression(out, arg)) {
            return false;
        }
    }

    out << ")";
    return true;
}

bool GeneratorImpl::EmitTypeConversion(std::ostream& out,
                                       const sem::Call* call,
                                       const sem::TypeConversion* conv) {
    if (!EmitType(out, conv->Target(), "")) {
        return false;
    }
    out << "(";

    if (!EmitExpression(out, call->Arguments()[0]->Declaration())) {
        return false;
    }

    out << ")";
    return true;
}

bool GeneratorImpl::EmitTypeConstructor(std::ostream& out,
                                        const sem::Call* call,
                                        const sem::TypeConstructor* ctor) {
    auto* type = ctor->ReturnType();

    if (type->IsAnyOf<sem::Array, sem::Struct>()) {
        out << "{";
    } else {
        if (!EmitType(out, type, "")) {
            return false;
        }
        out << "(";
    }

    int i = 0;
    for (auto* arg : call->Arguments()) {
        if (i > 0) {
            out << ", ";
        }

        if (auto* struct_ty = type->As<sem::Struct>()) {
            // Emit field designators for structures to account for padding members.
            auto* member = struct_ty->Members()[i]->Declaration();
            auto name = program_->Symbols().NameFor(member->symbol);
            out << "." << name << "=";
        }

        if (!EmitExpression(out, arg->Declaration())) {
            return false;
        }

        i++;
    }

    if (type->IsAnyOf<sem::Array, sem::Struct>()) {
        out << "}";
    } else {
        out << ")";
    }
    return true;
}

bool GeneratorImpl::EmitAtomicCall(std::ostream& out,
                                   const ast::CallExpression* expr,
                                   const sem::Builtin* builtin) {
    auto call = [&](const std::string& name, bool append_memory_order_relaxed) {
        out << name;
        {
            ScopedParen sp(out);
            for (size_t i = 0; i < expr->args.size(); i++) {
                auto* arg = expr->args[i];
                if (i > 0) {
                    out << ", ";
                }
                if (!EmitExpression(out, arg)) {
                    return false;
                }
            }
            if (append_memory_order_relaxed) {
                out << ", memory_order_relaxed";
            }
        }
        return true;
    };

    switch (builtin->Type()) {
        case sem::BuiltinType::kAtomicLoad:
            return call("atomic_load_explicit", true);

        case sem::BuiltinType::kAtomicStore:
            return call("atomic_store_explicit", true);

        case sem::BuiltinType::kAtomicAdd:
            return call("atomic_fetch_add_explicit", true);

        case sem::BuiltinType::kAtomicSub:
            return call("atomic_fetch_sub_explicit", true);

        case sem::BuiltinType::kAtomicMax:
            return call("atomic_fetch_max_explicit", true);

        case sem::BuiltinType::kAtomicMin:
            return call("atomic_fetch_min_explicit", true);

        case sem::BuiltinType::kAtomicAnd:
            return call("atomic_fetch_and_explicit", true);

        case sem::BuiltinType::kAtomicOr:
            return call("atomic_fetch_or_explicit", true);

        case sem::BuiltinType::kAtomicXor:
            return call("atomic_fetch_xor_explicit", true);

        case sem::BuiltinType::kAtomicExchange:
            return call("atomic_exchange_explicit", true);

        case sem::BuiltinType::kAtomicCompareExchangeWeak: {
            auto* ptr_ty = TypeOf(expr->args[0])->UnwrapRef()->As<sem::Pointer>();
            auto sc = ptr_ty->StorageClass();
            auto* str = builtin->ReturnType()->As<sem::Struct>();

            auto func = utils::GetOrCreate(
                atomicCompareExchangeWeak_, ACEWKeyType{{sc, str}}, [&]() -> std::string {
                    // Emit the builtin return type unique to this overload. This does not
                    // exist in the AST, so it will not be generated in Generate().
                    if (!EmitStructTypeOnce(&helpers_, builtin->ReturnType()->As<sem::Struct>())) {
                        return "";
                    }

                    auto name = UniqueIdentifier("atomicCompareExchangeWeak");
                    auto& buf = helpers_;
                    auto* atomic_ty = builtin->Parameters()[0]->Type();
                    auto* arg_ty = builtin->Parameters()[1]->Type();

                    {
                        auto f = line(&buf);
                        auto str_name = StructName(builtin->ReturnType()->As<sem::Struct>());
                        f << str_name << " " << name << "(";
                        if (!EmitTypeAndName(f, atomic_ty, "atomic")) {
                            return "";
                        }
                        f << ", ";
                        if (!EmitTypeAndName(f, arg_ty, "compare")) {
                            return "";
                        }
                        f << ", ";
                        if (!EmitTypeAndName(f, arg_ty, "value")) {
                            return "";
                        }
                        f << ") {";
                    }

                    buf.IncrementIndent();
                    TINT_DEFER({
                        buf.DecrementIndent();
                        line(&buf) << "}";
                        line(&buf);
                    });

                    {
                        auto f = line(&buf);
                        if (!EmitTypeAndName(f, arg_ty, "old_value")) {
                            return "";
                        }
                        f << " = compare;";
                    }
                    line(&buf) << "bool exchanged = "
                                  "atomic_compare_exchange_weak_explicit(atomic, "
                                  "&old_value, value, memory_order_relaxed, "
                                  "memory_order_relaxed);";
                    line(&buf) << "return {old_value, exchanged};";
                    return name;
                });

            if (func.empty()) {
                return false;
            }
            return call(func, false);
        }

        default:
            break;
    }

    TINT_UNREACHABLE(Writer, diagnostics_) << "unsupported atomic builtin: " << builtin->Type();
    return false;
}

bool GeneratorImpl::EmitTextureCall(std::ostream& out,
                                    const sem::Call* call,
                                    const sem::Builtin* builtin) {
    using Usage = sem::ParameterUsage;

    auto& signature = builtin->Signature();
    auto* expr = call->Declaration();
    auto& arguments = call->Arguments();

    // Returns the argument with the given usage
    auto arg = [&](Usage usage) {
        int idx = signature.IndexOf(usage);
        return (idx >= 0) ? arguments[idx] : nullptr;
    };

    auto* texture = arg(Usage::kTexture)->Declaration();
    if (!texture) {
        TINT_ICE(Writer, diagnostics_) << "missing texture arg";
        return false;
    }

    auto* texture_type = TypeOf(texture)->UnwrapRef()->As<sem::Texture>();

    // Helper to emit the texture expression, wrapped in parentheses if the
    // expression includes an operator with lower precedence than the member
    // accessor used for the function calls.
    auto texture_expr = [&]() {
        bool paren_lhs =
            !texture->IsAnyOf<ast::IndexAccessorExpression, ast::CallExpression,
                              ast::IdentifierExpression, ast::MemberAccessorExpression>();
        if (paren_lhs) {
            out << "(";
        }
        if (!EmitExpression(out, texture)) {
            return false;
        }
        if (paren_lhs) {
            out << ")";
        }
        return true;
    };

    // MSL requires that `lod` is a constant 0 for 1D textures.
    bool level_is_constant_zero = texture_type->dim() == ast::TextureDimension::k1d;

    switch (builtin->Type()) {
        case sem::BuiltinType::kTextureDimensions: {
            std::vector<const char*> dims;
            switch (texture_type->dim()) {
                case ast::TextureDimension::kNone:
                    diagnostics_.add_error(diag::System::Writer, "texture dimension is kNone");
                    return false;
                case ast::TextureDimension::k1d:
                    dims = {"width"};
                    break;
                case ast::TextureDimension::k2d:
                case ast::TextureDimension::k2dArray:
                case ast::TextureDimension::kCube:
                case ast::TextureDimension::kCubeArray:
                    dims = {"width", "height"};
                    break;
                case ast::TextureDimension::k3d:
                    dims = {"width", "height", "depth"};
                    break;
            }

            auto get_dim = [&](const char* name) {
                if (!texture_expr()) {
                    return false;
                }
                out << ".get_" << name << "(";
                if (level_is_constant_zero) {
                    out << "0";
                } else {
                    if (auto* level = arg(Usage::kLevel)) {
                        if (!EmitExpression(out, level->Declaration())) {
                            return false;
                        }
                    }
                }
                out << ")";
                return true;
            };

            if (dims.size() == 1) {
                out << "int(";
                get_dim(dims[0]);
                out << ")";
            } else {
                EmitType(out, TypeOf(expr)->UnwrapRef(), "");
                out << "(";
                for (size_t i = 0; i < dims.size(); i++) {
                    if (i > 0) {
                        out << ", ";
                    }
                    get_dim(dims[i]);
                }
                out << ")";
            }
            return true;
        }
        case sem::BuiltinType::kTextureNumLayers: {
            out << "int(";
            if (!texture_expr()) {
                return false;
            }
            out << ".get_array_size())";
            return true;
        }
        case sem::BuiltinType::kTextureNumLevels: {
            out << "int(";
            if (!texture_expr()) {
                return false;
            }
            out << ".get_num_mip_levels())";
            return true;
        }
        case sem::BuiltinType::kTextureNumSamples: {
            out << "int(";
            if (!texture_expr()) {
                return false;
            }
            out << ".get_num_samples())";
            return true;
        }
        default:
            break;
    }

    if (!texture_expr()) {
        return false;
    }

    bool lod_param_is_named = true;

    switch (builtin->Type()) {
        case sem::BuiltinType::kTextureSample:
        case sem::BuiltinType::kTextureSampleBias:
        case sem::BuiltinType::kTextureSampleLevel:
        case sem::BuiltinType::kTextureSampleGrad:
            out << ".sample(";
            break;
        case sem::BuiltinType::kTextureSampleCompare:
        case sem::BuiltinType::kTextureSampleCompareLevel:
            out << ".sample_compare(";
            break;
        case sem::BuiltinType::kTextureGather:
            out << ".gather(";
            break;
        case sem::BuiltinType::kTextureGatherCompare:
            out << ".gather_compare(";
            break;
        case sem::BuiltinType::kTextureLoad:
            out << ".read(";
            lod_param_is_named = false;
            break;
        case sem::BuiltinType::kTextureStore:
            out << ".write(";
            break;
        default:
            TINT_UNREACHABLE(Writer, diagnostics_)
                << "Unhandled texture builtin '" << builtin->str() << "'";
            return false;
    }

    bool first_arg = true;
    auto maybe_write_comma = [&] {
        if (!first_arg) {
            out << ", ";
        }
        first_arg = false;
    };

    for (auto usage : {Usage::kValue, Usage::kSampler, Usage::kCoords, Usage::kArrayIndex,
                       Usage::kDepthRef, Usage::kSampleIndex}) {
        if (auto* e = arg(usage)) {
            maybe_write_comma();

            // Cast the coordinates to unsigned integers if necessary.
            bool casted = false;
            if (usage == Usage::kCoords && e->Type()->UnwrapRef()->is_integer_scalar_or_vector()) {
                casted = true;
                switch (texture_type->dim()) {
                    case ast::TextureDimension::k1d:
                        out << "uint(";
                        break;
                    case ast::TextureDimension::k2d:
                    case ast::TextureDimension::k2dArray:
                        out << "uint2(";
                        break;
                    case ast::TextureDimension::k3d:
                        out << "uint3(";
                        break;
                    default:
                        TINT_ICE(Writer, diagnostics_) << "unhandled texture dimensionality";
                        break;
                }
            }

            if (!EmitExpression(out, e->Declaration())) {
                return false;
            }

            if (casted) {
                out << ")";
            }
        }
    }

    if (auto* bias = arg(Usage::kBias)) {
        maybe_write_comma();
        out << "bias(";
        if (!EmitExpression(out, bias->Declaration())) {
            return false;
        }
        out << ")";
    }
    if (auto* level = arg(Usage::kLevel)) {
        maybe_write_comma();
        if (lod_param_is_named) {
            out << "level(";
        }
        if (level_is_constant_zero) {
            out << "0";
        } else {
            if (!EmitExpression(out, level->Declaration())) {
                return false;
            }
        }
        if (lod_param_is_named) {
            out << ")";
        }
    }
    if (builtin->Type() == sem::BuiltinType::kTextureSampleCompareLevel) {
        maybe_write_comma();
        out << "level(0)";
    }
    if (auto* ddx = arg(Usage::kDdx)) {
        auto dim = texture_type->dim();
        switch (dim) {
            case ast::TextureDimension::k2d:
            case ast::TextureDimension::k2dArray:
                maybe_write_comma();
                out << "gradient2d(";
                break;
            case ast::TextureDimension::k3d:
                maybe_write_comma();
                out << "gradient3d(";
                break;
            case ast::TextureDimension::kCube:
            case ast::TextureDimension::kCubeArray:
                maybe_write_comma();
                out << "gradientcube(";
                break;
            default: {
                std::stringstream err;
                err << "MSL does not support gradients for " << dim << " textures";
                diagnostics_.add_error(diag::System::Writer, err.str());
                return false;
            }
        }
        if (!EmitExpression(out, ddx->Declaration())) {
            return false;
        }
        out << ", ";
        if (!EmitExpression(out, arg(Usage::kDdy)->Declaration())) {
            return false;
        }
        out << ")";
    }

    bool has_offset = false;
    if (auto* offset = arg(Usage::kOffset)) {
        has_offset = true;
        maybe_write_comma();
        if (!EmitExpression(out, offset->Declaration())) {
            return false;
        }
    }

    if (auto* component = arg(Usage::kComponent)) {
        maybe_write_comma();
        if (!has_offset) {
            // offset argument may need to be provided if we have a component.
            switch (texture_type->dim()) {
                case ast::TextureDimension::k2d:
                case ast::TextureDimension::k2dArray:
                    out << "int2(0), ";
                    break;
                default:
                    break;  // Other texture dimensions don't have an offset
            }
        }
        auto c = component->ConstantValue().Element<AInt>(0);
        switch (c.value) {
            case 0:
                out << "component::x";
                break;
            case 1:
                out << "component::y";
                break;
            case 2:
                out << "component::z";
                break;
            case 3:
                out << "component::w";
                break;
            default:
                TINT_ICE(Writer, diagnostics_) << "invalid textureGather component: " << c;
                break;
        }
    }

    out << ")";

    return true;
}

bool GeneratorImpl::EmitDotCall(std::ostream& out,
                                const ast::CallExpression* expr,
                                const sem::Builtin* builtin) {
    auto* vec_ty = builtin->Parameters()[0]->Type()->As<sem::Vector>();
    std::string fn = "dot";
    if (vec_ty->type()->is_integer_scalar()) {
        // MSL does not have a builtin for dot() with integer vector types.
        // Generate the helper function if it hasn't been created already
        fn = utils::GetOrCreate(int_dot_funcs_, vec_ty->Width(), [&]() -> std::string {
            TextBuffer b;
            TINT_DEFER(helpers_.Append(b));

            auto fn_name = UniqueIdentifier("tint_dot" + std::to_string(vec_ty->Width()));
            auto v = "vec<T," + std::to_string(vec_ty->Width()) + ">";

            line(&b) << "template<typename T>";
            line(&b) << "T " << fn_name << "(" << v << " a, " << v << " b) {";
            {
                auto l = line(&b);
                l << "  return ";
                for (uint32_t i = 0; i < vec_ty->Width(); i++) {
                    if (i > 0) {
                        l << " + ";
                    }
                    l << "a[" << i << "]*b[" << i << "]";
                }
                l << ";";
            }
            line(&b) << "}";
            return fn_name;
        });
    }

    out << fn << "(";
    if (!EmitExpression(out, expr->args[0])) {
        return false;
    }
    out << ", ";
    if (!EmitExpression(out, expr->args[1])) {
        return false;
    }
    out << ")";
    return true;
}

bool GeneratorImpl::EmitModfCall(std::ostream& out,
                                 const ast::CallExpression* expr,
                                 const sem::Builtin* builtin) {
    return CallBuiltinHelper(
        out, expr, builtin, [&](TextBuffer* b, const std::vector<std::string>& params) {
            auto* ty = builtin->Parameters()[0]->Type();
            auto in = params[0];

            std::string width;
            if (auto* vec = ty->As<sem::Vector>()) {
                width = std::to_string(vec->Width());
            }

            // Emit the builtin return type unique to this overload. This does not
            // exist in the AST, so it will not be generated in Generate().
            if (!EmitStructType(&helpers_, builtin->ReturnType()->As<sem::Struct>())) {
                return false;
            }

            line(b) << "float" << width << " whole;";
            line(b) << "float" << width << " fract = modf(" << in << ", whole);";
            line(b) << "return {fract, whole};";
            return true;
        });
}

bool GeneratorImpl::EmitFrexpCall(std::ostream& out,
                                  const ast::CallExpression* expr,
                                  const sem::Builtin* builtin) {
    return CallBuiltinHelper(
        out, expr, builtin, [&](TextBuffer* b, const std::vector<std::string>& params) {
            auto* ty = builtin->Parameters()[0]->Type();
            auto in = params[0];

            std::string width;
            if (auto* vec = ty->As<sem::Vector>()) {
                width = std::to_string(vec->Width());
            }

            // Emit the builtin return type unique to this overload. This does not
            // exist in the AST, so it will not be generated in Generate().
            if (!EmitStructType(&helpers_, builtin->ReturnType()->As<sem::Struct>())) {
                return false;
            }

            line(b) << "int" << width << " exp;";
            line(b) << "float" << width << " sig = frexp(" << in << ", exp);";
            line(b) << "return {sig, exp};";
            return true;
        });
}

bool GeneratorImpl::EmitDegreesCall(std::ostream& out,
                                    const ast::CallExpression* expr,
                                    const sem::Builtin* builtin) {
    return CallBuiltinHelper(out, expr, builtin,
                             [&](TextBuffer* b, const std::vector<std::string>& params) {
                                 line(b) << "return " << params[0] << " * " << std::setprecision(20)
                                         << sem::kRadToDeg << ";";
                                 return true;
                             });
}

bool GeneratorImpl::EmitRadiansCall(std::ostream& out,
                                    const ast::CallExpression* expr,
                                    const sem::Builtin* builtin) {
    return CallBuiltinHelper(out, expr, builtin,
                             [&](TextBuffer* b, const std::vector<std::string>& params) {
                                 line(b) << "return " << params[0] << " * " << std::setprecision(20)
                                         << sem::kDegToRad << ";";
                                 return true;
                             });
}

std::string GeneratorImpl::generate_builtin_name(const sem::Builtin* builtin) {
    std::string out = "";
    switch (builtin->Type()) {
        case sem::BuiltinType::kAcos:
        case sem::BuiltinType::kAll:
        case sem::BuiltinType::kAny:
        case sem::BuiltinType::kAsin:
        case sem::BuiltinType::kAtan:
        case sem::BuiltinType::kAtan2:
        case sem::BuiltinType::kCeil:
        case sem::BuiltinType::kCos:
        case sem::BuiltinType::kCosh:
        case sem::BuiltinType::kCross:
        case sem::BuiltinType::kDeterminant:
        case sem::BuiltinType::kDistance:
        case sem::BuiltinType::kDot:
        case sem::BuiltinType::kExp:
        case sem::BuiltinType::kExp2:
        case sem::BuiltinType::kFloor:
        case sem::BuiltinType::kFma:
        case sem::BuiltinType::kFract:
        case sem::BuiltinType::kFrexp:
        case sem::BuiltinType::kLength:
        case sem::BuiltinType::kLdexp:
        case sem::BuiltinType::kLog:
        case sem::BuiltinType::kLog2:
        case sem::BuiltinType::kMix:
        case sem::BuiltinType::kModf:
        case sem::BuiltinType::kNormalize:
        case sem::BuiltinType::kPow:
        case sem::BuiltinType::kReflect:
        case sem::BuiltinType::kRefract:
        case sem::BuiltinType::kSelect:
        case sem::BuiltinType::kSin:
        case sem::BuiltinType::kSinh:
        case sem::BuiltinType::kSqrt:
        case sem::BuiltinType::kStep:
        case sem::BuiltinType::kTan:
        case sem::BuiltinType::kTanh:
        case sem::BuiltinType::kTranspose:
        case sem::BuiltinType::kTrunc:
        case sem::BuiltinType::kSign:
        case sem::BuiltinType::kClamp:
            out += builtin->str();
            break;
        case sem::BuiltinType::kAbs:
            if (builtin->ReturnType()->is_float_scalar_or_vector()) {
                out += "fabs";
            } else {
                out += "abs";
            }
            break;
        case sem::BuiltinType::kCountLeadingZeros:
            out += "clz";
            break;
        case sem::BuiltinType::kCountOneBits:
            out += "popcount";
            break;
        case sem::BuiltinType::kCountTrailingZeros:
            out += "ctz";
            break;
        case sem::BuiltinType::kDpdx:
        case sem::BuiltinType::kDpdxCoarse:
        case sem::BuiltinType::kDpdxFine:
            out += "dfdx";
            break;
        case sem::BuiltinType::kDpdy:
        case sem::BuiltinType::kDpdyCoarse:
        case sem::BuiltinType::kDpdyFine:
            out += "dfdy";
            break;
        case sem::BuiltinType::kExtractBits:
            out += "extract_bits";
            break;
        case sem::BuiltinType::kInsertBits:
            out += "insert_bits";
            break;
        case sem::BuiltinType::kFwidth:
        case sem::BuiltinType::kFwidthCoarse:
        case sem::BuiltinType::kFwidthFine:
            out += "fwidth";
            break;
        case sem::BuiltinType::kMax:
            if (builtin->ReturnType()->is_float_scalar_or_vector()) {
                out += "fmax";
            } else {
                out += "max";
            }
            break;
        case sem::BuiltinType::kMin:
            if (builtin->ReturnType()->is_float_scalar_or_vector()) {
                out += "fmin";
            } else {
                out += "min";
            }
            break;
        case sem::BuiltinType::kFaceForward:
            out += "faceforward";
            break;
        case sem::BuiltinType::kPack4x8snorm:
            out += "pack_float_to_snorm4x8";
            break;
        case sem::BuiltinType::kPack4x8unorm:
            out += "pack_float_to_unorm4x8";
            break;
        case sem::BuiltinType::kPack2x16snorm:
            out += "pack_float_to_snorm2x16";
            break;
        case sem::BuiltinType::kPack2x16unorm:
            out += "pack_float_to_unorm2x16";
            break;
        case sem::BuiltinType::kReverseBits:
            out += "reverse_bits";
            break;
        case sem::BuiltinType::kRound:
            out += "rint";
            break;
        case sem::BuiltinType::kSmoothstep:
        case sem::BuiltinType::kSmoothStep:
            out += "smoothstep";
            break;
        case sem::BuiltinType::kInverseSqrt:
            out += "rsqrt";
            break;
        case sem::BuiltinType::kUnpack4x8snorm:
            out += "unpack_snorm4x8_to_float";
            break;
        case sem::BuiltinType::kUnpack4x8unorm:
            out += "unpack_unorm4x8_to_float";
            break;
        case sem::BuiltinType::kUnpack2x16snorm:
            out += "unpack_snorm2x16_to_float";
            break;
        case sem::BuiltinType::kUnpack2x16unorm:
            out += "unpack_unorm2x16_to_float";
            break;
        case sem::BuiltinType::kArrayLength:
            diagnostics_.add_error(
                diag::System::Writer,
                "Unable to translate builtin: " + std::string(builtin->str()) +
                    "\nDid you forget to pass array_length_from_uniform generator "
                    "options?");
            return "";
        default:
            diagnostics_.add_error(diag::System::Writer,
                                   "Unknown import method: " + std::string(builtin->str()));
            return "";
    }
    return out;
}

bool GeneratorImpl::EmitCase(const ast::CaseStatement* stmt) {
    if (stmt->IsDefault()) {
        line() << "default: {";
    } else {
        for (auto* selector : stmt->selectors) {
            auto out = line();
            out << "case ";
            if (!EmitLiteral(out, selector)) {
                return false;
            }
            out << ":";
            if (selector == stmt->selectors.back()) {
                out << " {";
            }
        }
    }

    {
        ScopedIndent si(this);

        for (auto* s : stmt->body->statements) {
            if (!EmitStatement(s)) {
                return false;
            }
        }

        if (!last_is_break_or_fallthrough(stmt->body)) {
            line() << "break;";
        }
    }

    line() << "}";

    return true;
}

bool GeneratorImpl::EmitContinue(const ast::ContinueStatement*) {
    if (!emit_continuing_()) {
        return false;
    }

    line() << "continue;";
    return true;
}

bool GeneratorImpl::EmitZeroValue(std::ostream& out, const sem::Type* type) {
    return Switch(
        type,
        [&](const sem::Bool*) {
            out << "false";
            return true;
        },
        [&](const sem::F16*) {
            // Placeholder for emitting f16 zero value
            diagnostics_.add_error(diag::System::Writer,
                                   "Type f16 is not completely implemented yet");
            return false;
        },
        [&](const sem::F32*) {
            out << "0.0f";
            return true;
        },
        [&](const sem::I32*) {
            out << "0";
            return true;
        },
        [&](const sem::U32*) {
            out << "0u";
            return true;
        },
        [&](const sem::Vector* vec) {  //
            return EmitZeroValue(out, vec->type());
        },
        [&](const sem::Matrix* mat) {
            if (!EmitType(out, mat, "")) {
                return false;
            }
            ScopedParen sp(out);
            return EmitZeroValue(out, mat->type());
        },
        [&](const sem::Array* arr) {
            out << "{";
            TINT_DEFER(out << "}");
            return EmitZeroValue(out, arr->ElemType());
        },
        [&](const sem::Struct*) {
            out << "{}";
            return true;
        },
        [&](Default) {
            diagnostics_.add_error(
                diag::System::Writer,
                "Invalid type for zero emission: " + type->FriendlyName(builder_.Symbols()));
            return false;
        });
}

bool GeneratorImpl::EmitConstant(std::ostream& out, const sem::Constant& constant) {
    auto emit_bool = [&](size_t element_idx) {
        out << (constant.Element<AInt>(element_idx) ? "true" : "false");
        return true;
    };
    auto emit_f32 = [&](size_t element_idx) {
        PrintF32(out, static_cast<float>(constant.Element<AFloat>(element_idx)));
        return true;
    };
    auto emit_i32 = [&](size_t element_idx) {
        PrintI32(out, static_cast<int32_t>(constant.Element<AInt>(element_idx).value));
        return true;
    };
    auto emit_u32 = [&](size_t element_idx) {
        out << constant.Element<AInt>(element_idx).value << "u";
        return true;
    };
    auto emit_vector = [&](const sem::Vector* vec_ty, size_t start, size_t end) {
        if (!EmitType(out, vec_ty, "")) {
            return false;
        }

        ScopedParen sp(out);

        auto emit_els = [&](auto emit_el) {
            if (constant.AllEqual(start, end)) {
                return emit_el(start);
            }
            for (size_t i = start; i < end; i++) {
                if (i > start) {
                    out << ", ";
                }
                if (!emit_el(i)) {
                    return false;
                }
            }
            return true;
        };
        return Switch(
            vec_ty->type(),                                         //
            [&](const sem::Bool*) { return emit_els(emit_bool); },  //
            [&](const sem::F32*) { return emit_els(emit_f32); },    //
            [&](const sem::I32*) { return emit_els(emit_i32); },    //
            [&](const sem::U32*) { return emit_els(emit_u32); },    //
            [&](Default) {
                diagnostics_.add_error(diag::System::Writer,
                                       "unhandled constant vector element type: " +
                                           builder_.FriendlyName(vec_ty->type()));
                return false;
            });
    };
    auto emit_matrix = [&](const sem::Matrix* m) {
        if (!EmitType(out, constant.Type(), "")) {
            return false;
        }

        ScopedParen sp(out);

        for (size_t column_idx = 0; column_idx < m->columns(); column_idx++) {
            if (column_idx > 0) {
                out << ", ";
            }
            size_t start = m->rows() * column_idx;
            size_t end = m->rows() * (column_idx + 1);
            if (!emit_vector(m->ColumnType(), start, end)) {
                return false;
            }
        }
        return true;
    };
    return Switch(
        constant.Type(),                                                                   //
        [&](const sem::Bool*) { return emit_bool(0); },                                    //
        [&](const sem::F32*) { return emit_f32(0); },                                      //
        [&](const sem::I32*) { return emit_i32(0); },                                      //
        [&](const sem::U32*) { return emit_u32(0); },                                      //
        [&](const sem::Vector* v) { return emit_vector(v, 0, constant.ElementCount()); },  //
        [&](const sem::Matrix* m) { return emit_matrix(m); },                              //
        [&](Default) {
            diagnostics_.add_error(
                diag::System::Writer,
                "unhandled constant type: " + builder_.FriendlyName(constant.Type()));
            return false;
        });
}

bool GeneratorImpl::EmitLiteral(std::ostream& out, const ast::LiteralExpression* lit) {
    return Switch(
        lit,
        [&](const ast::BoolLiteralExpression* l) {
            out << (l->value ? "true" : "false");
            return true;
        },
        [&](const ast::FloatLiteralExpression* l) {
            PrintF32(out, static_cast<float>(l->value));
            return true;
        },
        [&](const ast::IntLiteralExpression* i) {
            switch (i->suffix) {
                case ast::IntLiteralExpression::Suffix::kNone:
                case ast::IntLiteralExpression::Suffix::kI: {
                    PrintI32(out, static_cast<int32_t>(i->value));
                    return true;
                }
                case ast::IntLiteralExpression::Suffix::kU: {
                    out << i->value << "u";
                    return true;
                }
            }
            diagnostics_.add_error(diag::System::Writer, "unknown integer literal suffix type");
            return false;
        },
        [&](Default) {
            diagnostics_.add_error(diag::System::Writer, "unknown literal type");
            return false;
        });
}

bool GeneratorImpl::EmitExpression(std::ostream& out, const ast::Expression* expr) {
    if (auto* sem = builder_.Sem().Get(expr)) {
        if (auto constant = sem->ConstantValue()) {
            return EmitConstant(out, constant);
        }
    }
    return Switch(
        expr,
        [&](const ast::IndexAccessorExpression* a) {  //
            return EmitIndexAccessor(out, a);
        },
        [&](const ast::BinaryExpression* b) {  //
            return EmitBinary(out, b);
        },
        [&](const ast::BitcastExpression* b) {  //
            return EmitBitcast(out, b);
        },
        [&](const ast::CallExpression* c) {  //
            return EmitCall(out, c);
        },
        [&](const ast::IdentifierExpression* i) {  //
            return EmitIdentifier(out, i);
        },
        [&](const ast::LiteralExpression* l) {  //
            return EmitLiteral(out, l);
        },
        [&](const ast::MemberAccessorExpression* m) {  //
            return EmitMemberAccessor(out, m);
        },
        [&](const ast::UnaryOpExpression* u) {  //
            return EmitUnaryOp(out, u);
        },
        [&](Default) {  //
            diagnostics_.add_error(diag::System::Writer, "unknown expression type: " +
                                                             std::string(expr->TypeInfo().name));
            return false;
        });
}

void GeneratorImpl::EmitStage(std::ostream& out, ast::PipelineStage stage) {
    switch (stage) {
        case ast::PipelineStage::kFragment:
            out << "fragment";
            break;
        case ast::PipelineStage::kVertex:
            out << "vertex";
            break;
        case ast::PipelineStage::kCompute:
            out << "kernel";
            break;
        case ast::PipelineStage::kNone:
            break;
    }
    return;
}

bool GeneratorImpl::EmitFunction(const ast::Function* func) {
    auto* func_sem = program_->Sem().Get(func);

    {
        auto out = line();
        if (!EmitType(out, func_sem->ReturnType(), "")) {
            return false;
        }
        out << " " << program_->Symbols().NameFor(func->symbol) << "(";

        bool first = true;
        for (auto* v : func->params) {
            if (!first) {
                out << ", ";
            }
            first = false;

            auto* type = program_->Sem().Get(v)->Type();

            std::string param_name = "const " + program_->Symbols().NameFor(v->symbol);
            if (!EmitType(out, type, param_name)) {
                return false;
            }
            // Parameter name is output as part of the type for arrays and pointers.
            if (!type->Is<sem::Array>() && !type->Is<sem::Pointer>()) {
                out << " " << program_->Symbols().NameFor(v->symbol);
            }
        }

        out << ") {";
    }

    if (!EmitStatementsWithIndent(func->body->statements)) {
        return false;
    }

    line() << "}";

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
        case ast::Builtin::kFragDepth:
            return "depth(any)";
        case ast::Builtin::kLocalInvocationId:
            return "thread_position_in_threadgroup";
        case ast::Builtin::kLocalInvocationIndex:
            return "thread_index_in_threadgroup";
        case ast::Builtin::kGlobalInvocationId:
            return "thread_position_in_grid";
        case ast::Builtin::kWorkgroupId:
            return "threadgroup_position_in_grid";
        case ast::Builtin::kNumWorkgroups:
            return "threadgroups_per_grid";
        case ast::Builtin::kSampleIndex:
            return "sample_id";
        case ast::Builtin::kSampleMask:
            return "sample_mask";
        case ast::Builtin::kPointSize:
            return "point_size";
        default:
            break;
    }
    return "";
}

std::string GeneratorImpl::interpolation_to_attribute(ast::InterpolationType type,
                                                      ast::InterpolationSampling sampling) const {
    std::string attr;
    switch (sampling) {
        case ast::InterpolationSampling::kCenter:
            attr = "center_";
            break;
        case ast::InterpolationSampling::kCentroid:
            attr = "centroid_";
            break;
        case ast::InterpolationSampling::kSample:
            attr = "sample_";
            break;
        case ast::InterpolationSampling::kNone:
            break;
    }
    switch (type) {
        case ast::InterpolationType::kPerspective:
            attr += "perspective";
            break;
        case ast::InterpolationType::kLinear:
            attr += "no_perspective";
            break;
        case ast::InterpolationType::kFlat:
            attr += "flat";
            break;
    }
    return attr;
}

bool GeneratorImpl::EmitEntryPointFunction(const ast::Function* func) {
    auto func_name = program_->Symbols().NameFor(func->symbol);

    // Returns the binding index of a variable, requiring that the group
    // attribute have a value of zero.
    const uint32_t kInvalidBindingIndex = std::numeric_limits<uint32_t>::max();
    auto get_binding_index = [&](const ast::Variable* var) -> uint32_t {
        auto bp = var->BindingPoint();
        if (bp.group == nullptr || bp.binding == nullptr) {
            TINT_ICE(Writer, diagnostics_)
                << "missing binding attributes for entry point parameter";
            return kInvalidBindingIndex;
        }
        if (bp.group->value != 0) {
            TINT_ICE(Writer, diagnostics_) << "encountered non-zero resource group index (use "
                                              "BindingRemapper to fix)";
            return kInvalidBindingIndex;
        }
        return bp.binding->value;
    };

    {
        auto out = line();

        EmitStage(out, func->PipelineStage());
        out << " " << func->return_type->FriendlyName(program_->Symbols());
        out << " " << func_name << "(";

        // Emit entry point parameters.
        bool first = true;
        for (auto* var : func->params) {
            if (!first) {
                out << ", ";
            }
            first = false;

            auto* type = program_->Sem().Get(var)->Type()->UnwrapRef();

            auto param_name = program_->Symbols().NameFor(var->symbol);
            if (!EmitType(out, type, param_name)) {
                return false;
            }
            // Parameter name is output as part of the type for arrays and pointers.
            if (!type->Is<sem::Array>() && !type->Is<sem::Pointer>()) {
                out << " " << param_name;
            }

            if (type->Is<sem::Struct>()) {
                out << " [[stage_in]]";
            } else if (type->is_handle()) {
                uint32_t binding = get_binding_index(var);
                if (binding == kInvalidBindingIndex) {
                    return false;
                }
                if (var->type->Is<ast::Sampler>()) {
                    out << " [[sampler(" << binding << ")]]";
                } else if (var->type->Is<ast::Texture>()) {
                    out << " [[texture(" << binding << ")]]";
                } else {
                    TINT_ICE(Writer, diagnostics_) << "invalid handle type entry point parameter";
                    return false;
                }
            } else if (auto* ptr = var->type->As<ast::Pointer>()) {
                auto sc = ptr->storage_class;
                if (sc == ast::StorageClass::kWorkgroup) {
                    auto& allocations = workgroup_allocations_[func_name];
                    out << " [[threadgroup(" << allocations.size() << ")]]";
                    allocations.push_back(program_->Sem().Get(ptr->type)->Size());
                } else if (sc == ast::StorageClass::kStorage || sc == ast::StorageClass::kUniform) {
                    uint32_t binding = get_binding_index(var);
                    if (binding == kInvalidBindingIndex) {
                        return false;
                    }
                    out << " [[buffer(" << binding << ")]]";
                } else {
                    TINT_ICE(Writer, diagnostics_)
                        << "invalid pointer storage class for entry point parameter";
                    return false;
                }
            } else {
                auto& attrs = var->attributes;
                bool builtin_found = false;
                for (auto* attr : attrs) {
                    auto* builtin = attr->As<ast::BuiltinAttribute>();
                    if (!builtin) {
                        continue;
                    }

                    builtin_found = true;

                    auto name = builtin_to_attribute(builtin->builtin);
                    if (name.empty()) {
                        diagnostics_.add_error(diag::System::Writer, "unknown builtin");
                        return false;
                    }
                    out << " [[" << name << "]]";
                }
                if (!builtin_found) {
                    TINT_ICE(Writer, diagnostics_) << "Unsupported entry point parameter";
                }
            }
        }
        out << ") {";
    }

    {
        ScopedIndent si(this);

        if (!EmitStatements(func->body->statements)) {
            return false;
        }

        if (!Is<ast::ReturnStatement>(func->body->Last())) {
            ast::ReturnStatement ret(ProgramID{}, Source{});
            if (!EmitStatement(&ret)) {
                return false;
            }
        }
    }

    line() << "}";
    return true;
}

bool GeneratorImpl::EmitIdentifier(std::ostream& out, const ast::IdentifierExpression* expr) {
    out << program_->Symbols().NameFor(expr->symbol);
    return true;
}

bool GeneratorImpl::EmitLoop(const ast::LoopStatement* stmt) {
    auto emit_continuing = [this, stmt]() {
        if (stmt->continuing && !stmt->continuing->Empty()) {
            if (!EmitBlock(stmt->continuing)) {
                return false;
            }
        }
        return true;
    };

    TINT_SCOPED_ASSIGNMENT(emit_continuing_, emit_continuing);
    line() << "while (true) {";
    {
        ScopedIndent si(this);
        if (!EmitStatements(stmt->body->statements)) {
            return false;
        }
        if (!emit_continuing_()) {
            return false;
        }
    }
    line() << "}";

    return true;
}

bool GeneratorImpl::EmitForLoop(const ast::ForLoopStatement* stmt) {
    TextBuffer init_buf;
    if (auto* init = stmt->initializer) {
        TINT_SCOPED_ASSIGNMENT(current_buffer_, &init_buf);
        if (!EmitStatement(init)) {
            return false;
        }
    }

    TextBuffer cond_pre;
    std::stringstream cond_buf;
    if (auto* cond = stmt->condition) {
        TINT_SCOPED_ASSIGNMENT(current_buffer_, &cond_pre);
        if (!EmitExpression(cond_buf, cond)) {
            return false;
        }
    }

    TextBuffer cont_buf;
    if (auto* cont = stmt->continuing) {
        TINT_SCOPED_ASSIGNMENT(current_buffer_, &cont_buf);
        if (!EmitStatement(cont)) {
            return false;
        }
    }

    // If the for-loop has a multi-statement conditional and / or continuing,
    // then we cannot emit this as a regular for-loop in MSL. Instead we need to
    // generate a `while(true)` loop.
    bool emit_as_loop = cond_pre.lines.size() > 0 || cont_buf.lines.size() > 1;

    // If the for-loop has multi-statement initializer, or is going to be
    // emitted as a `while(true)` loop, then declare the initializer
    // statement(s) before the loop in a new block.
    bool nest_in_block = init_buf.lines.size() > 1 || (stmt->initializer && emit_as_loop);
    if (nest_in_block) {
        line() << "{";
        increment_indent();
        current_buffer_->Append(init_buf);
        init_buf.lines.clear();  // Don't emit the initializer again in the 'for'
    }
    TINT_DEFER({
        if (nest_in_block) {
            decrement_indent();
            line() << "}";
        }
    });

    if (emit_as_loop) {
        auto emit_continuing = [&]() {
            current_buffer_->Append(cont_buf);
            return true;
        };

        TINT_SCOPED_ASSIGNMENT(emit_continuing_, emit_continuing);
        line() << "while (true) {";
        increment_indent();
        TINT_DEFER({
            decrement_indent();
            line() << "}";
        });

        if (stmt->condition) {
            current_buffer_->Append(cond_pre);
            line() << "if (!(" << cond_buf.str() << ")) { break; }";
        }

        if (!EmitStatements(stmt->body->statements)) {
            return false;
        }

        if (!emit_continuing_()) {
            return false;
        }
    } else {
        // For-loop can be generated.
        {
            auto out = line();
            out << "for";
            {
                ScopedParen sp(out);

                if (!init_buf.lines.empty()) {
                    out << init_buf.lines[0].content << " ";
                } else {
                    out << "; ";
                }

                out << cond_buf.str() << "; ";

                if (!cont_buf.lines.empty()) {
                    out << TrimSuffix(cont_buf.lines[0].content, ";");
                }
            }
            out << " {";
        }
        {
            auto emit_continuing = [] { return true; };
            TINT_SCOPED_ASSIGNMENT(emit_continuing_, emit_continuing);
            if (!EmitStatementsWithIndent(stmt->body->statements)) {
                return false;
            }
        }
        line() << "}";
    }

    return true;
}

bool GeneratorImpl::EmitDiscard(const ast::DiscardStatement*) {
    // TODO(dsinclair): Verify this is correct when the discard semantics are
    // defined for WGSL (https://github.com/gpuweb/gpuweb/issues/361)
    line() << "discard_fragment();";
    return true;
}

bool GeneratorImpl::EmitIf(const ast::IfStatement* stmt) {
    {
        auto out = line();
        out << "if (";
        if (!EmitExpression(out, stmt->condition)) {
            return false;
        }
        out << ") {";
    }

    if (!EmitStatementsWithIndent(stmt->body->statements)) {
        return false;
    }

    if (stmt->else_statement) {
        line() << "} else {";
        if (auto* block = stmt->else_statement->As<ast::BlockStatement>()) {
            if (!EmitStatementsWithIndent(block->statements)) {
                return false;
            }
        } else {
            if (!EmitStatementsWithIndent({stmt->else_statement})) {
                return false;
            }
        }
    }
    line() << "}";

    return true;
}

bool GeneratorImpl::EmitMemberAccessor(std::ostream& out,
                                       const ast::MemberAccessorExpression* expr) {
    auto write_lhs = [&] {
        bool paren_lhs =
            !expr->structure->IsAnyOf<ast::IndexAccessorExpression, ast::CallExpression,
                                      ast::IdentifierExpression, ast::MemberAccessorExpression>();
        if (paren_lhs) {
            out << "(";
        }
        if (!EmitExpression(out, expr->structure)) {
            return false;
        }
        if (paren_lhs) {
            out << ")";
        }
        return true;
    };

    auto& sem = program_->Sem();

    if (auto* swizzle = sem.Get(expr)->As<sem::Swizzle>()) {
        // Metal 1.x does not support swizzling of packed vector types.
        // For single element swizzles, we can use the index operator.
        // For multi-element swizzles, we need to cast to a regular vector type
        // first. Note that we do not currently allow assignments to swizzles, so
        // the casting which will convert the l-value to r-value is fine.
        if (swizzle->Indices().size() == 1) {
            if (!write_lhs()) {
                return false;
            }
            out << "[" << swizzle->Indices()[0] << "]";
        } else {
            if (!EmitType(out, sem.Get(expr->structure)->Type()->UnwrapRef(), "")) {
                return false;
            }
            out << "(";
            if (!write_lhs()) {
                return false;
            }
            out << ")." << program_->Symbols().NameFor(expr->member->symbol);
        }
    } else {
        if (!write_lhs()) {
            return false;
        }
        out << ".";
        if (!EmitExpression(out, expr->member)) {
            return false;
        }
    }

    return true;
}

bool GeneratorImpl::EmitReturn(const ast::ReturnStatement* stmt) {
    auto out = line();
    out << "return";
    if (stmt->value) {
        out << " ";
        if (!EmitExpression(out, stmt->value)) {
            return false;
        }
    }
    out << ";";
    return true;
}

bool GeneratorImpl::EmitBlock(const ast::BlockStatement* stmt) {
    line() << "{";

    if (!EmitStatementsWithIndent(stmt->statements)) {
        return false;
    }

    line() << "}";

    return true;
}

bool GeneratorImpl::EmitStatement(const ast::Statement* stmt) {
    return Switch(
        stmt,
        [&](const ast::AssignmentStatement* a) {  //
            return EmitAssign(a);
        },
        [&](const ast::BlockStatement* b) {  //
            return EmitBlock(b);
        },
        [&](const ast::BreakStatement* b) {  //
            return EmitBreak(b);
        },
        [&](const ast::CallStatement* c) {  //
            auto out = line();
            if (!EmitCall(out, c->expr)) {  //
                return false;
            }
            out << ";";
            return true;
        },
        [&](const ast::ContinueStatement* c) {  //
            return EmitContinue(c);
        },
        [&](const ast::DiscardStatement* d) {  //
            return EmitDiscard(d);
        },
        [&](const ast::FallthroughStatement*) {  //
            line() << "/* fallthrough */";
            return true;
        },
        [&](const ast::IfStatement* i) {  //
            return EmitIf(i);
        },
        [&](const ast::LoopStatement* l) {  //
            return EmitLoop(l);
        },
        [&](const ast::ForLoopStatement* l) {  //
            return EmitForLoop(l);
        },
        [&](const ast::ReturnStatement* r) {  //
            return EmitReturn(r);
        },
        [&](const ast::SwitchStatement* s) {  //
            return EmitSwitch(s);
        },
        [&](const ast::VariableDeclStatement* v) {  //
            auto* var = program_->Sem().Get(v->variable);
            return EmitVariable(var);
        },
        [&](Default) {
            diagnostics_.add_error(diag::System::Writer,
                                   "unknown statement type: " + std::string(stmt->TypeInfo().name));
            return false;
        });
}

bool GeneratorImpl::EmitStatements(const ast::StatementList& stmts) {
    for (auto* s : stmts) {
        if (!EmitStatement(s)) {
            return false;
        }
    }
    return true;
}

bool GeneratorImpl::EmitStatementsWithIndent(const ast::StatementList& stmts) {
    ScopedIndent si(this);
    return EmitStatements(stmts);
}

bool GeneratorImpl::EmitSwitch(const ast::SwitchStatement* stmt) {
    {
        auto out = line();
        out << "switch(";
        if (!EmitExpression(out, stmt->condition)) {
            return false;
        }
        out << ") {";
    }

    {
        ScopedIndent si(this);
        for (auto* s : stmt->body) {
            if (!EmitCase(s)) {
                return false;
            }
        }
    }

    line() << "}";

    return true;
}

bool GeneratorImpl::EmitType(std::ostream& out,
                             const sem::Type* type,
                             const std::string& name,
                             bool* name_printed /* = nullptr */) {
    if (name_printed) {
        *name_printed = false;
    }

    return Switch(
        type,
        [&](const sem::Atomic* atomic) {
            if (atomic->Type()->Is<sem::I32>()) {
                out << "atomic_int";
                return true;
            }
            if (atomic->Type()->Is<sem::U32>()) {
                out << "atomic_uint";
                return true;
            }
            TINT_ICE(Writer, diagnostics_)
                << "unhandled atomic type " << atomic->Type()->FriendlyName(builder_.Symbols());
            return false;
        },
        [&](const sem::Array* ary) {
            const sem::Type* base_type = ary;
            std::vector<uint32_t> sizes;
            while (auto* arr = base_type->As<sem::Array>()) {
                if (arr->IsRuntimeSized()) {
                    sizes.push_back(1);
                } else {
                    sizes.push_back(arr->Count());
                }
                base_type = arr->ElemType();
            }
            if (!EmitType(out, base_type, "")) {
                return false;
            }
            if (!name.empty()) {
                out << " " << name;
                if (name_printed) {
                    *name_printed = true;
                }
            }
            for (uint32_t size : sizes) {
                out << "[" << size << "]";
            }
            return true;
        },
        [&](const sem::Bool*) {
            out << "bool";
            return true;
        },
        [&](const sem::F16*) {
            diagnostics_.add_error(diag::System::Writer,
                                   "Type f16 is not completely implemented yet");
            return false;
        },
        [&](const sem::F32*) {
            out << "float";
            return true;
        },
        [&](const sem::I32*) {
            out << "int";
            return true;
        },
        [&](const sem::Matrix* mat) {
            if (!EmitType(out, mat->type(), "")) {
                return false;
            }
            out << mat->columns() << "x" << mat->rows();
            return true;
        },
        [&](const sem::Pointer* ptr) {
            if (ptr->Access() == ast::Access::kRead) {
                out << "const ";
            }
            if (!EmitStorageClass(out, ptr->StorageClass())) {
                return false;
            }
            out << " ";
            if (ptr->StoreType()->Is<sem::Array>()) {
                std::string inner = "(*" + name + ")";
                if (!EmitType(out, ptr->StoreType(), inner)) {
                    return false;
                }
                if (name_printed) {
                    *name_printed = true;
                }
            } else {
                if (!EmitType(out, ptr->StoreType(), "")) {
                    return false;
                }
                out << "* " << name;
                if (name_printed) {
                    *name_printed = true;
                }
            }
            return true;
        },
        [&](const sem::Sampler*) {
            out << "sampler";
            return true;
        },
        [&](const sem::Struct* str) {
            // The struct type emits as just the name. The declaration would be
            // emitted as part of emitting the declared types.
            out << StructName(str);
            return true;
        },
        [&](const sem::Texture* tex) {
            if (tex->Is<sem::ExternalTexture>()) {
                TINT_ICE(Writer, diagnostics_)
                    << "Multiplanar external texture transform was not run.";
                return false;
            }

            if (tex->IsAnyOf<sem::DepthTexture, sem::DepthMultisampledTexture>()) {
                out << "depth";
            } else {
                out << "texture";
            }

            switch (tex->dim()) {
                case ast::TextureDimension::k1d:
                    out << "1d";
                    break;
                case ast::TextureDimension::k2d:
                    out << "2d";
                    break;
                case ast::TextureDimension::k2dArray:
                    out << "2d_array";
                    break;
                case ast::TextureDimension::k3d:
                    out << "3d";
                    break;
                case ast::TextureDimension::kCube:
                    out << "cube";
                    break;
                case ast::TextureDimension::kCubeArray:
                    out << "cube_array";
                    break;
                default:
                    diagnostics_.add_error(diag::System::Writer, "Invalid texture dimensions");
                    return false;
            }
            if (tex->IsAnyOf<sem::MultisampledTexture, sem::DepthMultisampledTexture>()) {
                out << "_ms";
            }
            out << "<";
            TINT_DEFER(out << ">");

            return Switch(
                tex,
                [&](const sem::DepthTexture*) {
                    out << "float, access::sample";
                    return true;
                },
                [&](const sem::DepthMultisampledTexture*) {
                    out << "float, access::read";
                    return true;
                },
                [&](const sem::StorageTexture* storage) {
                    if (!EmitType(out, storage->type(), "")) {
                        return false;
                    }

                    std::string access_str;
                    if (storage->access() == ast::Access::kRead) {
                        out << ", access::read";
                    } else if (storage->access() == ast::Access::kWrite) {
                        out << ", access::write";
                    } else {
                        diagnostics_.add_error(diag::System::Writer,
                                               "Invalid access control for storage texture");
                        return false;
                    }
                    return true;
                },
                [&](const sem::MultisampledTexture* ms) {
                    if (!EmitType(out, ms->type(), "")) {
                        return false;
                    }
                    out << ", access::read";
                    return true;
                },
                [&](const sem::SampledTexture* sampled) {
                    if (!EmitType(out, sampled->type(), "")) {
                        return false;
                    }
                    out << ", access::sample";
                    return true;
                },
                [&](Default) {
                    diagnostics_.add_error(diag::System::Writer, "invalid texture type");
                    return false;
                });
        },
        [&](const sem::U32*) {
            out << "uint";
            return true;
        },
        [&](const sem::Vector* vec) {
            if (!EmitType(out, vec->type(), "")) {
                return false;
            }
            out << vec->Width();
            return true;
        },
        [&](const sem::Void*) {
            out << "void";
            return true;
        },
        [&](Default) {
            diagnostics_.add_error(
                diag::System::Writer,
                "unknown type in EmitType: " + type->FriendlyName(builder_.Symbols()));
            return false;
        });
}

bool GeneratorImpl::EmitTypeAndName(std::ostream& out,
                                    const sem::Type* type,
                                    const std::string& name) {
    bool name_printed = false;
    if (!EmitType(out, type, name, &name_printed)) {
        return false;
    }
    if (!name_printed) {
        out << " " << name;
    }
    return true;
}

bool GeneratorImpl::EmitStorageClass(std::ostream& out, ast::StorageClass sc) {
    switch (sc) {
        case ast::StorageClass::kFunction:
        case ast::StorageClass::kPrivate:
        case ast::StorageClass::kHandle:
            out << "thread";
            return true;
        case ast::StorageClass::kWorkgroup:
            out << "threadgroup";
            return true;
        case ast::StorageClass::kStorage:
            out << "device";
            return true;
        case ast::StorageClass::kUniform:
            out << "constant";
            return true;
        default:
            break;
    }
    TINT_ICE(Writer, diagnostics_) << "unhandled storage class: " << sc;
    return false;
}

bool GeneratorImpl::EmitPackedType(std::ostream& out,
                                   const sem::Type* type,
                                   const std::string& name) {
    auto* vec = type->As<sem::Vector>();
    if (vec && vec->Width() == 3) {
        out << "packed_";
        if (!EmitType(out, vec, "")) {
            return false;
        }

        if (vec->is_float_vector() && !matrix_packed_vector_overloads_) {
            // Overload operators for matrix-vector arithmetic where the vector
            // operand is packed, as these overloads to not exist in the metal
            // namespace.
            TextBuffer b;
            TINT_DEFER(helpers_.Append(b));
            line(&b) << R"(template<typename T, int N, int M>
inline vec<T, M> operator*(matrix<T, N, M> lhs, packed_vec<T, N> rhs) {
  return lhs * vec<T, N>(rhs);
}

template<typename T, int N, int M>
inline vec<T, N> operator*(packed_vec<T, M> lhs, matrix<T, N, M> rhs) {
  return vec<T, M>(lhs) * rhs;
}
)";
            matrix_packed_vector_overloads_ = true;
        }

        return true;
    }

    return EmitType(out, type, name);
}

bool GeneratorImpl::EmitStructType(TextBuffer* b, const sem::Struct* str) {
    line(b) << "struct " << StructName(str) << " {";

    bool is_host_shareable = str->IsHostShareable();

    // Emits a `/* 0xnnnn */` byte offset comment for a struct member.
    auto add_byte_offset_comment = [&](std::ostream& out, uint32_t offset) {
        std::ios_base::fmtflags saved_flag_state(out.flags());
        out << "/* 0x" << std::hex << std::setfill('0') << std::setw(4) << offset << " */ ";
        out.flags(saved_flag_state);
    };

    auto add_padding = [&](uint32_t size, uint32_t msl_offset) {
        std::string name;
        do {
            name = UniqueIdentifier("tint_pad");
        } while (str->FindMember(program_->Symbols().Get(name)));

        auto out = line(b);
        add_byte_offset_comment(out, msl_offset);
        out << "int8_t " << name << "[" << size << "];";
    };

    b->IncrementIndent();

    uint32_t msl_offset = 0;
    for (auto* mem : str->Members()) {
        auto out = line(b);
        auto mem_name = program_->Symbols().NameFor(mem->Name());
        auto wgsl_offset = mem->Offset();

        if (is_host_shareable) {
            if (wgsl_offset < msl_offset) {
                // Unimplementable layout
                TINT_ICE(Writer, diagnostics_) << "Structure member WGSL offset (" << wgsl_offset
                                               << ") is behind MSL offset (" << msl_offset << ")";
                return false;
            }

            // Generate padding if required
            if (auto padding = wgsl_offset - msl_offset) {
                add_padding(padding, msl_offset);
                msl_offset += padding;
            }

            add_byte_offset_comment(out, msl_offset);

            if (!EmitPackedType(out, mem->Type(), mem_name)) {
                return false;
            }
        } else {
            if (!EmitType(out, mem->Type(), mem_name)) {
                return false;
            }
        }

        auto* ty = mem->Type();

        // Array member name will be output with the type
        if (!ty->Is<sem::Array>()) {
            out << " " << mem_name;
        }

        // Emit attributes
        if (auto* decl = mem->Declaration()) {
            for (auto* attr : decl->attributes) {
                bool ok = Switch(
                    attr,
                    [&](const ast::BuiltinAttribute* builtin) {
                        auto name = builtin_to_attribute(builtin->builtin);
                        if (name.empty()) {
                            diagnostics_.add_error(diag::System::Writer, "unknown builtin");
                            return false;
                        }
                        out << " [[" << name << "]]";
                        return true;
                    },
                    [&](const ast::LocationAttribute* loc) {
                        auto& pipeline_stage_uses = str->PipelineStageUses();
                        if (pipeline_stage_uses.size() != 1) {
                            TINT_ICE(Writer, diagnostics_) << "invalid entry point IO struct uses";
                            return false;
                        }

                        if (pipeline_stage_uses.count(sem::PipelineStageUsage::kVertexInput)) {
                            out << " [[attribute(" + std::to_string(loc->value) + ")]]";
                        } else if (pipeline_stage_uses.count(
                                       sem::PipelineStageUsage::kVertexOutput)) {
                            out << " [[user(locn" + std::to_string(loc->value) + ")]]";
                        } else if (pipeline_stage_uses.count(
                                       sem::PipelineStageUsage::kFragmentInput)) {
                            out << " [[user(locn" + std::to_string(loc->value) + ")]]";
                        } else if (pipeline_stage_uses.count(
                                       sem::PipelineStageUsage::kFragmentOutput)) {
                            out << " [[color(" + std::to_string(loc->value) + ")]]";
                        } else {
                            TINT_ICE(Writer, diagnostics_) << "invalid use of location decoration";
                            return false;
                        }
                        return true;
                    },
                    [&](const ast::InterpolateAttribute* interpolate) {
                        auto name =
                            interpolation_to_attribute(interpolate->type, interpolate->sampling);
                        if (name.empty()) {
                            diagnostics_.add_error(diag::System::Writer,
                                                   "unknown interpolation attribute");
                            return false;
                        }
                        out << " [[" << name << "]]";
                        return true;
                    },
                    [&](const ast::InvariantAttribute*) {
                        if (invariant_define_name_.empty()) {
                            invariant_define_name_ = UniqueIdentifier("TINT_INVARIANT");
                        }
                        out << " " << invariant_define_name_;
                        return true;
                    },
                    [&](const ast::StructMemberOffsetAttribute*) { return true; },
                    [&](const ast::StructMemberAlignAttribute*) { return true; },
                    [&](const ast::StructMemberSizeAttribute*) { return true; },
                    [&](Default) {
                        TINT_ICE(Writer, diagnostics_)
                            << "unhandled struct member attribute: " << attr->Name();
                        return false;
                    });
                if (!ok) {
                    return false;
                }
            }
        }

        out << ";";

        if (is_host_shareable) {
            // Calculate new MSL offset
            auto size_align = MslPackedTypeSizeAndAlign(ty);
            if (msl_offset % size_align.align) {
                TINT_ICE(Writer, diagnostics_)
                    << "Misaligned MSL structure member " << ty->FriendlyName(program_->Symbols())
                    << " " << mem_name;
                return false;
            }
            msl_offset += size_align.size;
        }
    }

    if (is_host_shareable && str->Size() != msl_offset) {
        add_padding(str->Size() - msl_offset, msl_offset);
    }

    b->DecrementIndent();

    line(b) << "};";
    return true;
}

bool GeneratorImpl::EmitStructTypeOnce(TextBuffer* buffer, const sem::Struct* str) {
    auto it = emitted_structs_.emplace(str);
    if (!it.second) {
        return true;
    }
    return EmitStructType(buffer, str);
}

bool GeneratorImpl::EmitUnaryOp(std::ostream& out, const ast::UnaryOpExpression* expr) {
    // Handle `-e` when `e` is signed, so that we ensure that if `e` is the
    // largest negative value, it returns `e`.
    auto* expr_type = TypeOf(expr->expr)->UnwrapRef();
    if (expr->op == ast::UnaryOp::kNegation && expr_type->is_signed_scalar_or_vector()) {
        auto fn = utils::GetOrCreate(unary_minus_funcs_, expr_type, [&]() -> std::string {
            // e.g.:
            // int tint_unary_minus(const int v) {
            //     return (v == -2147483648) ? v : -v;
            // }
            TextBuffer b;
            TINT_DEFER(helpers_.Append(b));

            auto fn_name = UniqueIdentifier("tint_unary_minus");
            {
                auto decl = line(&b);
                if (!EmitTypeAndName(decl, expr_type, fn_name)) {
                    return "";
                }
                decl << "(const ";
                if (!EmitType(decl, expr_type, "")) {
                    return "";
                }
                decl << " v) {";
            }

            {
                ScopedIndent si(&b);
                const auto largest_negative_value =
                    std::to_string(std::numeric_limits<int32_t>::min());
                line(&b) << "return select(-v, v, v == " << largest_negative_value << ");";
            }
            line(&b) << "}";
            line(&b);
            return fn_name;
        });

        out << fn << "(";
        if (!EmitExpression(out, expr->expr)) {
            return false;
        }
        out << ")";
        return true;
    }

    switch (expr->op) {
        case ast::UnaryOp::kAddressOf:
            out << "&";
            break;
        case ast::UnaryOp::kComplement:
            out << "~";
            break;
        case ast::UnaryOp::kIndirection:
            out << "*";
            break;
        case ast::UnaryOp::kNot:
            out << "!";
            break;
        case ast::UnaryOp::kNegation:
            out << "-";
            break;
    }
    out << "(";

    if (!EmitExpression(out, expr->expr)) {
        return false;
    }

    out << ")";

    return true;
}

bool GeneratorImpl::EmitVariable(const sem::Variable* var) {
    auto* decl = var->Declaration();

    for (auto* attr : decl->attributes) {
        if (!attr->Is<ast::InternalAttribute>()) {
            TINT_ICE(Writer, diagnostics_) << "unexpected variable attribute";
            return false;
        }
    }

    auto out = line();

    switch (var->StorageClass()) {
        case ast::StorageClass::kFunction:
        case ast::StorageClass::kHandle:
        case ast::StorageClass::kNone:
            break;
        case ast::StorageClass::kPrivate:
            out << "thread ";
            break;
        case ast::StorageClass::kWorkgroup:
            out << "threadgroup ";
            break;
        default:
            TINT_ICE(Writer, diagnostics_) << "unhandled variable storage class";
            return false;
    }

    auto* type = var->Type()->UnwrapRef();

    std::string name = program_->Symbols().NameFor(decl->symbol);
    if (decl->is_const) {
        name = "const " + name;
    }
    if (!EmitType(out, type, name)) {
        return false;
    }
    // Variable name is output as part of the type for arrays and pointers.
    if (!type->Is<sem::Array>() && !type->Is<sem::Pointer>()) {
        out << " " << name;
    }

    if (decl->constructor != nullptr) {
        out << " = ";
        if (!EmitExpression(out, decl->constructor)) {
            return false;
        }
    } else if (var->StorageClass() == ast::StorageClass::kPrivate ||
               var->StorageClass() == ast::StorageClass::kFunction ||
               var->StorageClass() == ast::StorageClass::kNone) {
        out << " = ";
        if (!EmitZeroValue(out, type)) {
            return false;
        }
    }
    out << ";";

    return true;
}

bool GeneratorImpl::EmitProgramConstVariable(const ast::Variable* var) {
    for (auto* d : var->attributes) {
        if (!d->Is<ast::IdAttribute>()) {
            diagnostics_.add_error(diag::System::Writer, "Decorated const values not valid");
            return false;
        }
    }
    if (!var->is_const) {
        diagnostics_.add_error(diag::System::Writer, "Expected a const value");
        return false;
    }

    auto out = line();
    out << "constant ";
    auto* type = program_->Sem().Get(var)->Type()->UnwrapRef();
    if (!EmitType(out, type, program_->Symbols().NameFor(var->symbol))) {
        return false;
    }
    if (!type->Is<sem::Array>()) {
        out << " " << program_->Symbols().NameFor(var->symbol);
    }

    auto* global = program_->Sem().Get<sem::GlobalVariable>(var);
    if (global && global->IsOverridable()) {
        out << " [[function_constant(" << global->ConstantId() << ")]]";
    } else if (var->constructor != nullptr) {
        out << " = ";
        if (!EmitExpression(out, var->constructor)) {
            return false;
        }
    }
    out << ";";

    return true;
}

GeneratorImpl::SizeAndAlign GeneratorImpl::MslPackedTypeSizeAndAlign(const sem::Type* ty) {
    return Switch(
        ty,

        // https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
        // 2.1 Scalar Data Types
        [&](const sem::U32*) {
            return SizeAndAlign{4, 4};
        },
        [&](const sem::I32*) {
            return SizeAndAlign{4, 4};
        },
        [&](const sem::F32*) {
            return SizeAndAlign{4, 4};
        },

        [&](const sem::Vector* vec) {
            auto num_els = vec->Width();
            auto* el_ty = vec->type();
            if (el_ty->IsAnyOf<sem::U32, sem::I32, sem::F32>()) {
                // Use a packed_vec type for 3-element vectors only.
                if (num_els == 3) {
                    // https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
                    // 2.2.3 Packed Vector Types
                    return SizeAndAlign{num_els * 4, 4};
                } else {
                    // https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
                    // 2.2 Vector Data Types
                    return SizeAndAlign{num_els * 4, num_els * 4};
                }
            }
            TINT_UNREACHABLE(Writer, diagnostics_)
                << "Unhandled vector element type " << el_ty->TypeInfo().name;
            return SizeAndAlign{};
        },

        [&](const sem::Matrix* mat) {
            // https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
            // 2.3 Matrix Data Types
            auto cols = mat->columns();
            auto rows = mat->rows();
            auto* el_ty = mat->type();
            if (el_ty->IsAnyOf<sem::U32, sem::I32, sem::F32>()) {
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

            TINT_UNREACHABLE(Writer, diagnostics_)
                << "Unhandled matrix element type " << el_ty->TypeInfo().name;
            return SizeAndAlign{};
        },

        [&](const sem::Array* arr) {
            if (!arr->IsStrideImplicit()) {
                TINT_ICE(Writer, diagnostics_) << "arrays with explicit strides not "
                                                  "exist past the SPIR-V reader";
                return SizeAndAlign{};
            }
            auto num_els = std::max<uint32_t>(arr->Count(), 1);
            return SizeAndAlign{arr->Stride() * num_els, arr->Align()};
        },

        [&](const sem::Struct* str) {
            // TODO(crbug.com/tint/650): There's an assumption here that MSL's
            // default structure size and alignment matches WGSL's. We need to
            // confirm this.
            return SizeAndAlign{str->Size(), str->Align()};
        },

        [&](const sem::Atomic* atomic) { return MslPackedTypeSizeAndAlign(atomic->Type()); },

        [&](Default) {
            TINT_UNREACHABLE(Writer, diagnostics_) << "Unhandled type " << ty->TypeInfo().name;
            return SizeAndAlign{};
        });
}

template <typename F>
bool GeneratorImpl::CallBuiltinHelper(std::ostream& out,
                                      const ast::CallExpression* call,
                                      const sem::Builtin* builtin,
                                      F&& build) {
    // Generate the helper function if it hasn't been created already
    auto fn = utils::GetOrCreate(builtins_, builtin, [&]() -> std::string {
        TextBuffer b;
        TINT_DEFER(helpers_.Append(b));

        auto fn_name = UniqueIdentifier(std::string("tint_") + sem::str(builtin->Type()));
        std::vector<std::string> parameter_names;
        {
            auto decl = line(&b);
            if (!EmitTypeAndName(decl, builtin->ReturnType(), fn_name)) {
                return "";
            }
            {
                ScopedParen sp(decl);
                for (auto* param : builtin->Parameters()) {
                    if (!parameter_names.empty()) {
                        decl << ", ";
                    }
                    auto param_name = "param_" + std::to_string(parameter_names.size());
                    if (!EmitTypeAndName(decl, param->Type(), param_name)) {
                        return "";
                    }
                    parameter_names.emplace_back(std::move(param_name));
                }
            }
            decl << " {";
        }
        {
            ScopedIndent si(&b);
            if (!build(&b, parameter_names)) {
                return "";
            }
        }
        line(&b) << "}";
        line(&b);
        return fn_name;
    });

    if (fn.empty()) {
        return false;
    }

    // Call the helper
    out << fn;
    {
        ScopedParen sp(out);
        bool first = true;
        for (auto* arg : call->args) {
            if (!first) {
                out << ", ";
            }
            first = false;
            if (!EmitExpression(out, arg)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace tint::writer::msl
