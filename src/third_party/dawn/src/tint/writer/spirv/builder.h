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

#ifndef SRC_TINT_WRITER_SPIRV_BUILDER_H_
#define SRC_TINT_WRITER_SPIRV_BUILDER_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "spirv/unified1/spirv.h"
#include "src/tint/ast/assignment_statement.h"
#include "src/tint/ast/bitcast_expression.h"
#include "src/tint/ast/break_statement.h"
#include "src/tint/ast/continue_statement.h"
#include "src/tint/ast/discard_statement.h"
#include "src/tint/ast/if_statement.h"
#include "src/tint/ast/interpolate_attribute.h"
#include "src/tint/ast/loop_statement.h"
#include "src/tint/ast/return_statement.h"
#include "src/tint/ast/switch_statement.h"
#include "src/tint/ast/unary_op_expression.h"
#include "src/tint/ast/variable_decl_statement.h"
#include "src/tint/program_builder.h"
#include "src/tint/scope_stack.h"
#include "src/tint/sem/builtin.h"
#include "src/tint/sem/storage_texture_type.h"
#include "src/tint/writer/spirv/function.h"
#include "src/tint/writer/spirv/scalar_constant.h"

// Forward declarations
namespace tint::sem {
class Call;
class Reference;
class TypeConstructor;
class TypeConversion;
}  // namespace tint::sem

namespace tint::writer::spirv {

/// Builder class to create SPIR-V instructions from a module.
class Builder {
 public:
  /// Contains information for generating accessor chains
  struct AccessorInfo {
    AccessorInfo();
    ~AccessorInfo();

    /// The ID of the current chain source. The chain source may change as we
    /// evaluate the access chain. The chain source always points to the ID
    /// which we will use to evaluate the current set of accessors. This maybe
    /// the original variable, or maybe an intermediary if we had to evaulate
    /// the access chain early (in the case of a swizzle of an access chain).
    uint32_t source_id;
    /// The type of the current chain source. This type matches the deduced
    /// result_type of the current source defined above.
    const sem::Type* source_type;
    /// A list of access chain indices to emit. Note, we _only_ have access
    /// chain indices if the source is reference.
    std::vector<uint32_t> access_chain_indices;
  };

  /// Constructor
  /// @param program the program
  /// @param zero_initialize_workgroup_memory `true` to initialize all the
  /// variables in the Workgroup storage class with OpConstantNull
  Builder(const Program* program,
          bool zero_initialize_workgroup_memory = false);
  ~Builder();

  /// Generates the SPIR-V instructions for the given program
  /// @returns true if the SPIR-V was successfully built
  bool Build();

  /// @returns the error string or blank if no error was reported.
  const std::string& error() const { return error_; }
  /// @returns true if the builder encountered an error
  bool has_error() const { return !error_.empty(); }

  /// @returns the number of uint32_t's needed to make up the results
  uint32_t total_size() const;

  /// @returns the id bound for this program
  uint32_t id_bound() const { return next_id_; }

  /// @returns the next id to be used
  uint32_t next_id() {
    auto id = next_id_;
    next_id_ += 1;
    return id;
  }

  /// Iterates over all the instructions in the correct order and calls the
  /// given callback
  /// @param cb the callback to execute
  void iterate(std::function<void(const Instruction&)> cb) const;

  /// Adds an instruction to the list of capabilities, if the capability
  /// hasn't already been added.
  /// @param cap the capability to set
  void push_capability(uint32_t cap);
  /// @returns the capabilities
  const InstructionList& capabilities() const { return capabilities_; }
  /// Adds an instruction to the extensions
  /// @param op the op to set
  /// @param operands the operands for the instruction
  void push_extension(spv::Op op, const OperandList& operands) {
    extensions_.push_back(Instruction{op, operands});
  }
  /// @returns the extensions
  const InstructionList& extensions() const { return extensions_; }
  /// Adds an instruction to the ext import
  /// @param op the op to set
  /// @param operands the operands for the instruction
  void push_ext_import(spv::Op op, const OperandList& operands) {
    ext_imports_.push_back(Instruction{op, operands});
  }
  /// @returns the ext imports
  const InstructionList& ext_imports() const { return ext_imports_; }
  /// Adds an instruction to the memory model
  /// @param op the op to set
  /// @param operands the operands for the instruction
  void push_memory_model(spv::Op op, const OperandList& operands) {
    memory_model_.push_back(Instruction{op, operands});
  }
  /// @returns the memory model
  const InstructionList& memory_model() const { return memory_model_; }
  /// Adds an instruction to the entry points
  /// @param op the op to set
  /// @param operands the operands for the instruction
  void push_entry_point(spv::Op op, const OperandList& operands) {
    entry_points_.push_back(Instruction{op, operands});
  }
  /// @returns the entry points
  const InstructionList& entry_points() const { return entry_points_; }
  /// Adds an instruction to the execution modes
  /// @param op the op to set
  /// @param operands the operands for the instruction
  void push_execution_mode(spv::Op op, const OperandList& operands) {
    execution_modes_.push_back(Instruction{op, operands});
  }
  /// @returns the execution modes
  const InstructionList& execution_modes() const { return execution_modes_; }
  /// Adds an instruction to the debug
  /// @param op the op to set
  /// @param operands the operands for the instruction
  void push_debug(spv::Op op, const OperandList& operands) {
    debug_.push_back(Instruction{op, operands});
  }
  /// @returns the debug instructions
  const InstructionList& debug() const { return debug_; }
  /// Adds an instruction to the types
  /// @param op the op to set
  /// @param operands the operands for the instruction
  void push_type(spv::Op op, const OperandList& operands) {
    types_.push_back(Instruction{op, operands});
  }
  /// @returns the type instructions
  const InstructionList& types() const { return types_; }
  /// Adds an instruction to the annotations
  /// @param op the op to set
  /// @param operands the operands for the instruction
  void push_annot(spv::Op op, const OperandList& operands) {
    annotations_.push_back(Instruction{op, operands});
  }
  /// @returns the annotations
  const InstructionList& annots() const { return annotations_; }

  /// Adds a function to the builder
  /// @param func the function to add
  void push_function(const Function& func) {
    functions_.push_back(func);
    current_label_id_ = func.label_id();
  }
  /// @returns the functions
  const std::vector<Function>& functions() const { return functions_; }
  /// Pushes an instruction to the current function. If we're outside
  /// a function then issue an internal error and return false.
  /// @param op the operation
  /// @param operands the operands
  /// @returns true if we succeeded
  bool push_function_inst(spv::Op op, const OperandList& operands);
  /// Pushes a variable to the current function
  /// @param operands the variable operands
  void push_function_var(const OperandList& operands) {
    if (functions_.empty()) {
      TINT_ICE(Writer, builder_.Diagnostics())
          << "push_function_var() called without a function";
    }
    functions_.back().push_var(operands);
  }

  /// @returns true if the current instruction insertion point is
  /// inside a basic block.
  bool InsideBasicBlock() const;

  /// Converts a storage class to a SPIR-V storage class.
  /// @param klass the storage class to convert
  /// @returns the SPIR-V storage class or SpvStorageClassMax on error.
  SpvStorageClass ConvertStorageClass(ast::StorageClass klass) const;
  /// Converts a builtin to a SPIR-V builtin and pushes a capability if needed.
  /// @param builtin the builtin to convert
  /// @param storage the storage class that this builtin is being used with
  /// @returns the SPIR-V builtin or SpvBuiltInMax on error.
  SpvBuiltIn ConvertBuiltin(ast::Builtin builtin, ast::StorageClass storage);

  /// Converts an interpolate attribute to SPIR-V decorations and pushes a
  /// capability if needed.
  /// @param id the id to decorate
  /// @param type the interpolation type
  /// @param sampling the interpolation sampling
  void AddInterpolationDecorations(uint32_t id,
                                   ast::InterpolationType type,
                                   ast::InterpolationSampling sampling);

  /// Generates a label for the given id. Emits an error and returns false if
  /// we're currently outside a function.
  /// @param id the id to use for the label
  /// @returns true on success.
  bool GenerateLabel(uint32_t id);
  /// Generates an assignment statement
  /// @param assign the statement to generate
  /// @returns true if the statement was successfully generated
  bool GenerateAssignStatement(const ast::AssignmentStatement* assign);
  /// Generates a block statement, wrapped in a push/pop scope
  /// @param stmt the statement to generate
  /// @returns true if the statement was successfully generated
  bool GenerateBlockStatement(const ast::BlockStatement* stmt);
  /// Generates a block statement
  /// @param stmt the statement to generate
  /// @returns true if the statement was successfully generated
  bool GenerateBlockStatementWithoutScoping(const ast::BlockStatement* stmt);
  /// Generates a break statement
  /// @param stmt the statement to generate
  /// @returns true if the statement was successfully generated
  bool GenerateBreakStatement(const ast::BreakStatement* stmt);
  /// Generates a continue statement
  /// @param stmt the statement to generate
  /// @returns true if the statement was successfully generated
  bool GenerateContinueStatement(const ast::ContinueStatement* stmt);
  /// Generates a discard statement
  /// @param stmt the statement to generate
  /// @returns true if the statement was successfully generated
  bool GenerateDiscardStatement(const ast::DiscardStatement* stmt);
  /// Generates an entry point instruction
  /// @param func the function
  /// @param id the id of the function
  /// @returns true if the instruction was generated, false otherwise
  bool GenerateEntryPoint(const ast::Function* func, uint32_t id);
  /// Generates execution modes for an entry point
  /// @param func the function
  /// @param id the id of the function
  /// @returns false on failure
  bool GenerateExecutionModes(const ast::Function* func, uint32_t id);
  /// Generates an expression
  /// @param expr the expression to generate
  /// @returns the resulting ID of the expression or 0 on error
  uint32_t GenerateExpression(const ast::Expression* expr);
  /// Generates the instructions for a function
  /// @param func the function to generate
  /// @returns true if the instructions were generated
  bool GenerateFunction(const ast::Function* func);
  /// Generates a function type if not already created
  /// @param func the function to generate for
  /// @returns the ID to use for the function type. Returns 0 on failure.
  uint32_t GenerateFunctionTypeIfNeeded(const sem::Function* func);
  /// Generates access control annotations if needed
  /// @param type the type to generate for
  /// @param struct_id the struct id
  /// @param member_idx the member index
  void GenerateMemberAccessIfNeeded(const sem::Type* type,
                                    uint32_t struct_id,
                                    uint32_t member_idx);
  /// Generates a function variable
  /// @param var the variable
  /// @returns true if the variable was generated
  bool GenerateFunctionVariable(const ast::Variable* var);
  /// Generates a global variable
  /// @param var the variable to generate
  /// @returns true if the variable is emited.
  bool GenerateGlobalVariable(const ast::Variable* var);
  /// Generates an index accessor expression.
  ///
  /// For more information on accessors see the "Pointer evaluation" section of
  /// the WGSL specification.
  ///
  /// @param expr the expresssion to generate
  /// @returns the id of the expression or 0 on failure
  uint32_t GenerateAccessorExpression(const ast::Expression* expr);
  /// Generates an index accessor
  /// @param expr the accessor to generate
  /// @param info the current accessor information
  /// @returns true if the accessor was generated successfully
  bool GenerateIndexAccessor(const ast::IndexAccessorExpression* expr,
                             AccessorInfo* info);
  /// Generates a member accessor
  /// @param expr the accessor to generate
  /// @param info the current accessor information
  /// @returns true if the accessor was generated successfully
  bool GenerateMemberAccessor(const ast::MemberAccessorExpression* expr,
                              AccessorInfo* info);
  /// Generates an identifier expression
  /// @param expr the expresssion to generate
  /// @returns the id of the expression or 0 on failure
  uint32_t GenerateIdentifierExpression(const ast::IdentifierExpression* expr);
  /// Generates a unary op expression
  /// @param expr the expression to generate
  /// @returns the id of the expression or 0 on failure
  uint32_t GenerateUnaryOpExpression(const ast::UnaryOpExpression* expr);
  /// Generates an if statement
  /// @param stmt the statement to generate
  /// @returns true on success
  bool GenerateIfStatement(const ast::IfStatement* stmt);
  /// Generates an import instruction for the "GLSL.std.450" extended
  /// instruction set, if one doesn't exist yet, and returns the import ID.
  /// @returns the import ID, or 0 on error.
  uint32_t GetGLSLstd450Import();
  /// Generates a constructor expression
  /// @param var the variable generated for, nullptr if no variable associated.
  /// @param expr the expression to generate
  /// @returns the ID of the expression or 0 on failure.
  uint32_t GenerateConstructorExpression(const ast::Variable* var,
                                         const ast::Expression* expr);
  /// Generates a literal constant if needed
  /// @param var the variable generated for, nullptr if no variable associated.
  /// @param lit the literal to generate
  /// @returns the ID on success or 0 on failure
  uint32_t GenerateLiteralIfNeeded(const ast::Variable* var,
                                   const ast::LiteralExpression* lit);
  /// Generates a binary expression
  /// @param expr the expression to generate
  /// @returns the expression ID on success or 0 otherwise
  uint32_t GenerateBinaryExpression(const ast::BinaryExpression* expr);
  /// Generates a bitcast expression
  /// @param expr the expression to generate
  /// @returns the expression ID on success or 0 otherwise
  uint32_t GenerateBitcastExpression(const ast::BitcastExpression* expr);
  /// Generates a short circuting binary expression
  /// @param expr the expression to generate
  /// @returns teh expression ID on success or 0 otherwise
  uint32_t GenerateShortCircuitBinaryExpression(
      const ast::BinaryExpression* expr);
  /// Generates a call expression
  /// @param expr the expression to generate
  /// @returns the expression ID on success or 0 otherwise
  uint32_t GenerateCallExpression(const ast::CallExpression* expr);
  /// Handles generating a function call expression
  /// @param call the call expression
  /// @param function the function being called
  /// @returns the expression ID on success or 0 otherwise
  uint32_t GenerateFunctionCall(const sem::Call* call,
                                const sem::Function* function);
  /// Handles generating a builtin call expression
  /// @param call the call expression
  /// @param builtin the builtin being called
  /// @returns the expression ID on success or 0 otherwise
  uint32_t GenerateBuiltinCall(const sem::Call* call,
                               const sem::Builtin* builtin);
  /// Handles generating a type constructor or type conversion expression
  /// @param call the call expression
  /// @param var the variable that is being initialized. May be null.
  /// @returns the expression ID on success or 0 otherwise
  uint32_t GenerateTypeConstructorOrConversion(const sem::Call* call,
                                               const ast::Variable* var);
  /// Generates a texture builtin call. Emits an error and returns false if
  /// we're currently outside a function.
  /// @param call the call expression
  /// @param builtin the semantic information for the texture builtin
  /// @param result_type result type operand of the texture instruction
  /// @param result_id result identifier operand of the texture instruction
  /// parameters
  /// @returns true on success
  bool GenerateTextureBuiltin(const sem::Call* call,
                              const sem::Builtin* builtin,
                              spirv::Operand result_type,
                              spirv::Operand result_id);
  /// Generates a control barrier statement.
  /// @param builtin the semantic information for the barrier builtin call
  /// @returns true on success
  bool GenerateControlBarrierBuiltin(const sem::Builtin* builtin);
  /// Generates an atomic builtin call.
  /// @param call the call expression
  /// @param builtin the semantic information for the atomic builtin call
  /// @param result_type result type operand of the texture instruction
  /// @param result_id result identifier operand of the texture instruction
  /// @returns true on success
  bool GenerateAtomicBuiltin(const sem::Call* call,
                             const sem::Builtin* builtin,
                             Operand result_type,
                             Operand result_id);
  /// Generates a sampled image
  /// @param texture_type the texture type
  /// @param texture_operand the texture operand
  /// @param sampler_operand the sampler operand
  /// @returns the expression ID
  uint32_t GenerateSampledImage(const sem::Type* texture_type,
                                Operand texture_operand,
                                Operand sampler_operand);
  /// Generates a cast or object copy for the expression result,
  /// or return the ID generated the expression if it is already
  /// of the right type.
  /// @param to_type the type we're casting too
  /// @param from_expr the expression to cast
  /// @param is_global_init if this is a global initializer
  /// @returns the expression ID on success or 0 otherwise
  uint32_t GenerateCastOrCopyOrPassthrough(const sem::Type* to_type,
                                           const ast::Expression* from_expr,
                                           bool is_global_init);
  /// Generates a loop statement
  /// @param stmt the statement to generate
  /// @returns true on successful generation
  bool GenerateLoopStatement(const ast::LoopStatement* stmt);
  /// Generates a return statement
  /// @param stmt the statement to generate
  /// @returns true on success, false otherwise
  bool GenerateReturnStatement(const ast::ReturnStatement* stmt);
  /// Generates a switch statement
  /// @param stmt the statement to generate
  /// @returns ture on success, false otherwise
  bool GenerateSwitchStatement(const ast::SwitchStatement* stmt);
  /// Generates a conditional section merge block
  /// @param cond the condition
  /// @param true_body the statements making up the true block
  /// @param cur_else_idx the index of the current else statement to process
  /// @param else_stmts the list of all else statements
  /// @returns true on success, false on failure
  bool GenerateConditionalBlock(const ast::Expression* cond,
                                const ast::BlockStatement* true_body,
                                size_t cur_else_idx,
                                const ast::ElseStatementList& else_stmts);
  /// Generates a statement
  /// @param stmt the statement to generate
  /// @returns true if the statement was generated
  bool GenerateStatement(const ast::Statement* stmt);
  /// Generates an expression. If the WGSL expression does not have reference
  /// type, then return the SPIR-V ID for the expression. Otherwise implement
  /// the WGSL Load Rule: generate an OpLoad and return the ID of the result.
  /// Returns 0 if the expression could not be generated.
  /// @param expr the semantic expression node to be generated
  /// @returns the the ID of the expression, or loaded expression
  uint32_t GenerateExpressionWithLoadIfNeeded(const sem::Expression* expr);
  /// Generates an expression. If the WGSL expression does not have reference
  /// type, then return the SPIR-V ID for the expression. Otherwise implement
  /// the WGSL Load Rule: generate an OpLoad and return the ID of the result.
  /// Returns 0 if the expression could not be generated.
  /// @param expr the AST expression to be generated
  /// @returns the the ID of the expression, or loaded expression
  uint32_t GenerateExpressionWithLoadIfNeeded(const ast::Expression* expr);
  /// Generates an OpLoad on the given ID if it has reference type in WGSL,
  /// othewrise return the ID itself.
  /// @param type the type of the expression
  /// @param id the SPIR-V id of the experssion
  /// @returns the ID of the loaded value or `id` if type is not a reference
  uint32_t GenerateLoadIfNeeded(const sem::Type* type, uint32_t id);
  /// Generates an OpStore. Emits an error and returns false if we're
  /// currently outside a function.
  /// @param to the ID to store too
  /// @param from the ID to store from
  /// @returns true on success
  bool GenerateStore(uint32_t to, uint32_t from);
  /// Generates a type if not already created
  /// @param type the type to create
  /// @returns the ID to use for the given type. Returns 0 on unknown type.
  uint32_t GenerateTypeIfNeeded(const sem::Type* type);
  /// Generates a texture type declaration
  /// @param texture the texture to generate
  /// @param result the result operand
  /// @returns true if the texture was successfully generated
  bool GenerateTextureType(const sem::Texture* texture, const Operand& result);
  /// Generates an array type declaration
  /// @param ary the array to generate
  /// @param result the result operand
  /// @returns true if the array was successfully generated
  bool GenerateArrayType(const sem::Array* ary, const Operand& result);
  /// Generates a matrix type declaration
  /// @param mat the matrix to generate
  /// @param result the result operand
  /// @returns true if the matrix was successfully generated
  bool GenerateMatrixType(const sem::Matrix* mat, const Operand& result);
  /// Generates a pointer type declaration
  /// @param ptr the pointer type to generate
  /// @param result the result operand
  /// @returns true if the pointer was successfully generated
  bool GeneratePointerType(const sem::Pointer* ptr, const Operand& result);
  /// Generates a reference type declaration
  /// @param ref the reference type to generate
  /// @param result the result operand
  /// @returns true if the reference was successfully generated
  bool GenerateReferenceType(const sem::Reference* ref, const Operand& result);
  /// Generates a vector type declaration
  /// @param struct_type the vector to generate
  /// @param result the result operand
  /// @returns true if the vector was successfully generated
  bool GenerateStructType(const sem::Struct* struct_type,
                          const Operand& result);
  /// Generates a struct member
  /// @param struct_id the id of the parent structure
  /// @param idx the index of the member
  /// @param member the member to generate
  /// @returns the id of the struct member or 0 on error.
  uint32_t GenerateStructMember(uint32_t struct_id,
                                uint32_t idx,
                                const sem::StructMember* member);
  /// Generates a variable declaration statement
  /// @param stmt the statement to generate
  /// @returns true on successfull generation
  bool GenerateVariableDeclStatement(const ast::VariableDeclStatement* stmt);
  /// Generates a vector type declaration
  /// @param vec the vector to generate
  /// @param result the result operand
  /// @returns true if the vector was successfully generated
  bool GenerateVectorType(const sem::Vector* vec, const Operand& result);

  /// Generates instructions to splat `scalar_id` into a vector of type
  /// `vec_type`
  /// @param scalar_id scalar to splat
  /// @param vec_type type of vector
  /// @returns id of the new vector
  uint32_t GenerateSplat(uint32_t scalar_id, const sem::Type* vec_type);

  /// Generates instructions to add or subtract two matrices
  /// @param lhs_id id of multiplicand
  /// @param rhs_id id of multiplier
  /// @param type type of both matrices and of result
  /// @param op one of `spv::Op::OpFAdd` or `spv::Op::OpFSub`
  /// @returns id of the result matrix
  uint32_t GenerateMatrixAddOrSub(uint32_t lhs_id,
                                  uint32_t rhs_id,
                                  const sem::Matrix* type,
                                  spv::Op op);

  /// Converts TexelFormat to SPIR-V and pushes an appropriate capability.
  /// @param format AST image format type
  /// @returns SPIR-V image format type
  SpvImageFormat convert_texel_format_to_spv(const ast::TexelFormat format);

  /// Determines if the given type constructor is created from constant values
  /// @param expr the expression to check
  /// @returns true if the constructor is constant
  bool IsConstructorConst(const ast::Expression* expr);

 private:
  /// @returns an Operand with a new result ID in it. Increments the next_id_
  /// automatically.
  Operand result_op();

  /// @returns the resolved type of the ast::Expression `expr`
  /// @param expr the expression
  const sem::Type* TypeOf(const ast::Expression* expr) const {
    return builder_.TypeOf(expr);
  }

  /// Generates a scalar constant if needed
  /// @param constant the constant to generate.
  /// @returns the ID on success or 0 on failure
  uint32_t GenerateConstantIfNeeded(const ScalarConstant& constant);

  /// Generates a constant-null of the given type, if needed
  /// @param type the type of the constant null to generate.
  /// @returns the ID on success or 0 on failure
  uint32_t GenerateConstantNullIfNeeded(const sem::Type* type);

  /// Generates a vector constant splat if needed
  /// @param type the type of the vector to generate
  /// @param value_id the ID of the scalar value to splat
  /// @returns the ID on success or 0 on failure
  uint32_t GenerateConstantVectorSplatIfNeeded(const sem::Vector* type,
                                               uint32_t value_id);

  ProgramBuilder builder_;
  std::string error_;
  uint32_t next_id_ = 1;
  uint32_t current_label_id_ = 0;
  InstructionList capabilities_;
  InstructionList extensions_;
  InstructionList ext_imports_;
  InstructionList memory_model_;
  InstructionList entry_points_;
  InstructionList execution_modes_;
  InstructionList debug_;
  InstructionList types_;
  InstructionList annotations_;
  std::vector<Function> functions_;

  std::unordered_map<std::string, uint32_t> import_name_to_id_;
  std::unordered_map<Symbol, uint32_t> func_symbol_to_id_;
  std::unordered_map<sem::CallTargetSignature, uint32_t> func_sig_to_id_;
  std::unordered_map<const sem::Type*, uint32_t> type_to_id_;
  std::unordered_map<ScalarConstant, uint32_t> const_to_id_;
  std::unordered_map<std::string, uint32_t> type_constructor_to_id_;
  std::unordered_map<const sem::Type*, uint32_t> const_null_to_id_;
  std::unordered_map<uint64_t, uint32_t> const_splat_to_id_;
  std::unordered_map<const sem::Type*, uint32_t>
      texture_type_to_sampled_image_type_id_;
  ScopeStack<uint32_t> scope_stack_;
  std::unordered_map<uint32_t, const ast::Variable*> spirv_id_to_variable_;
  std::vector<uint32_t> merge_stack_;
  std::vector<uint32_t> continue_stack_;
  std::unordered_set<uint32_t> capability_set_;
  bool has_overridable_workgroup_size_ = false;
  bool zero_initialize_workgroup_memory_ = false;

  struct ContinuingInfo {
    ContinuingInfo(const ast::Statement* last_statement,
                   uint32_t loop_header_id,
                   uint32_t break_target_id);
    // The last statement in the continiung block.
    const ast::Statement* const last_statement = nullptr;
    // The ID of the loop header
    const uint32_t loop_header_id = 0u;
    // The ID of the merge block for the loop.
    const uint32_t break_target_id = 0u;
  };
  // Stack of nodes, where each is the last statement in a surrounding
  // continuing block.
  std::vector<ContinuingInfo> continuing_stack_;

  // The instruction to emit as the backedge of a loop.
  struct Backedge {
    Backedge(spv::Op, OperandList);
    Backedge(const Backedge&);
    Backedge& operator=(const Backedge&);
    ~Backedge();

    spv::Op opcode;
    OperandList operands;
  };
  std::vector<Backedge> backedge_stack_;
};

}  // namespace tint::writer::spirv

#endif  // SRC_TINT_WRITER_SPIRV_BUILDER_H_
