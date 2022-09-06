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

#ifndef SRC_TINT_READER_SPIRV_PARSER_IMPL_H_
#define SRC_TINT_READER_SPIRV_PARSER_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if TINT_BUILD_SPV_READER
#include "source/opt/ir_context.h"
#endif

#include "src/tint/program_builder.h"
#include "src/tint/reader/reader.h"
#include "src/tint/reader/spirv/entry_point_info.h"
#include "src/tint/reader/spirv/enum_converter.h"
#include "src/tint/reader/spirv/namer.h"
#include "src/tint/reader/spirv/parser_type.h"
#include "src/tint/reader/spirv/usage.h"

/// This is the implementation of the SPIR-V parser for Tint.

/// Notes on terminology:
///
/// A WGSL "handle" is an opaque object used for accessing a resource via
/// special builtins.  In SPIR-V, a handle is stored a variable in the
/// UniformConstant storage class.  The handles supported by SPIR-V are:
///   - images, both sampled texture and storage image
///   - samplers
///   - combined image+sampler
///   - acceleration structures for raytracing.
///
/// WGSL only supports samplers and images, but calls images "textures".
/// When emitting errors, we aim to use terminology most likely to be
/// familiar to Vulkan SPIR-V developers.  We will tend to use "image"
/// and "sampler" instead of "handle".

namespace tint::reader::spirv {

/// The binary representation of a SPIR-V decoration enum followed by its
/// operands, if any.
/// Example:   { SpvDecorationBlock }
/// Example:   { SpvDecorationArrayStride, 16 }
using Decoration = std::vector<uint32_t>;

/// DecorationList is a list of decorations
using DecorationList = std::vector<Decoration>;

/// An AST expression with its type.
struct TypedExpression {
    /// Constructor
    TypedExpression();

    /// Copy constructor
    TypedExpression(const TypedExpression&);

    /// Constructor
    /// @param type_in the type of the expression
    /// @param expr_in the expression
    TypedExpression(const Type* type_in, const ast::Expression* expr_in);

    /// Assignment operator
    /// @returns this TypedExpression
    TypedExpression& operator=(const TypedExpression&);

    /// @returns true if both type and expr are not nullptr
    operator bool() const { return type && expr; }

    /// The type
    const Type* type = nullptr;
    /// The expression
    const ast::Expression* expr = nullptr;
};

/// Info about the WorkgroupSize builtin.
struct WorkgroupSizeInfo {
    /// Constructor
    WorkgroupSizeInfo();
    /// Destructor
    ~WorkgroupSizeInfo();
    /// The SPIR-V ID of the WorkgroupSize builtin, if any.
    uint32_t id = 0u;
    /// The SPIR-V type ID of the WorkgroupSize builtin, if any.
    uint32_t type_id = 0u;
    /// The SPIR-V type IDs of the x, y, and z components.
    uint32_t component_type_id = 0u;
    /// The SPIR-V IDs of the X, Y, and Z components of the workgroup size
    /// builtin.
    uint32_t x_id = 0u;  /// X component ID
    uint32_t y_id = 0u;  /// Y component ID
    uint32_t z_id = 0u;  /// Z component ID
    /// The effective workgroup size, if this is a compute shader.
    uint32_t x_value = 0u;  /// X workgroup size
    uint32_t y_value = 0u;  /// Y workgroup size
    uint32_t z_value = 0u;  /// Z workgroup size
};

/// Parser implementation for SPIR-V.
class ParserImpl : Reader {
  public:
    /// Creates a new parser
    /// @param input the input data to parse
    explicit ParserImpl(const std::vector<uint32_t>& input);
    /// Destructor
    ~ParserImpl() override;

    /// Run the parser
    /// @returns true if the parse was successful, false otherwise.
    bool Parse() override;

    /// @returns the program. The program builder in the parser will be reset
    /// after this.
    Program program() override;

    /// @returns a reference to the internal builder, without building the
    /// program. To be used only for testing.
    ProgramBuilder& builder() { return builder_; }

    /// @returns the type manager
    TypeManager& type_manager() { return ty_; }

    /// Logs failure, ands return a failure stream to accumulate diagnostic
    /// messages. By convention, a failure should only be logged along with
    /// a non-empty string diagnostic.
    /// @returns the failure stream
    FailStream& Fail() {
        success_ = false;
        return fail_stream_;
    }

    /// @return true if failure has not yet occurred
    bool success() const { return success_; }

    /// @returns the accumulated error string
    const std::string error() { return errors_.str(); }

    /// Builds an internal representation of the SPIR-V binary,
    /// and parses it into a Tint AST module.  Diagnostics are emitted
    /// to the error stream.
    /// @returns true if it was successful.
    bool BuildAndParseInternalModule() { return BuildInternalModule() && ParseInternalModule(); }
    /// Builds an internal representation of the SPIR-V binary,
    /// and parses the module, except functions, into a Tint AST module.
    /// Diagnostics are emitted to the error stream.
    /// @returns true if it was successful.
    bool BuildAndParseInternalModuleExceptFunctions() {
        return BuildInternalModule() && ParseInternalModuleExceptFunctions();
    }

    /// @returns the set of SPIR-V IDs for imports of the "GLSL.std.450"
    /// extended instruction set.
    const std::unordered_set<uint32_t>& glsl_std_450_imports() const {
        return glsl_std_450_imports_;
    }

    /// Desired handling of SPIR-V pointers by ConvertType()
    enum class PtrAs {
        // SPIR-V pointer is converted to a spirv::Pointer
        Ptr,
        // SPIR-V pointer is converted to a spirv::Reference
        Ref
    };

    /// Converts a SPIR-V type to a Tint type, and saves it for fast lookup.
    /// If the type is only used for builtins, then register that specially,
    /// and return null.  If the type is a sampler, image, or sampled image, then
    /// return the Void type, because those opaque types are handled in a
    /// different way.
    /// On failure, logs an error and returns null.  This should only be called
    /// after the internal representation of the module has been built.
    /// @param type_id the SPIR-V ID of a type.
    /// @param ptr_as if the SPIR-V type is a pointer and ptr_as is equal to
    /// PtrAs::Ref then a Reference will be returned, otherwise a Pointer will be
    /// returned for a SPIR-V pointer
    /// @returns a Tint type, or nullptr
    const Type* ConvertType(uint32_t type_id, PtrAs ptr_as = PtrAs::Ptr);

    /// Emits an alias type declaration for array or runtime-sized array type,
    /// when needed to distinguish between differently-decorated underlying types.
    /// Updates the mapping of the SPIR-V type ID to the alias type.
    /// This is a no-op if the parser has already failed.
    /// @param type_id the SPIR-V ID for the type
    /// @param type the type that might get an alias
    /// @param ast_type the ast type that might get an alias
    /// @returns an alias type or `ast_type` if no alias was created
    const Type* MaybeGenerateAlias(uint32_t type_id,
                                   const spvtools::opt::analysis::Type* type,
                                   const Type* ast_type);

    /// Adds `decl` as a declared type if it hasn't been added yet.
    /// @param name the type's unique name
    /// @param decl the type declaration to add
    void AddTypeDecl(Symbol name, const ast::TypeDecl* decl);

    /// @returns the fail stream object
    FailStream& fail_stream() { return fail_stream_; }
    /// @returns the namer object
    Namer& namer() { return namer_; }
    /// @returns a borrowed pointer to the internal representation of the module.
    /// This is null until BuildInternalModule has been called.
    spvtools::opt::IRContext* ir_context() { return ir_context_.get(); }

    /// Gets the list of unique decorations for a SPIR-V result ID.  Returns an
    /// empty vector if the ID is not a result ID, or if no decorations target
    /// that ID. The internal representation must have already been built.
    /// Ignores decorations that have no effect in graphics APIs, e.g. Restrict
    /// and RestrictPointer.
    /// @param id SPIR-V ID
    /// @returns the list of decorations on the given ID
    DecorationList GetDecorationsFor(uint32_t id) const;
    /// Gets the list of unique decorations for the member of a struct.  Returns
    /// an empty list if the `id` is not the ID of a struct, or if the member
    /// index is out of range, or if the target member has no decorations. The
    /// internal representation must have already been built.
    /// Ignores decorations that have no effect in graphics APIs, e.g. Restrict
    /// and RestrictPointer.
    /// @param id SPIR-V ID of a struct
    /// @param member_index the member within the struct
    /// @returns the list of decorations on the member
    DecorationList GetDecorationsForMember(uint32_t id, uint32_t member_index) const;

    /// Converts SPIR-V decorations for the variable with the given ID.
    /// Registers the IDs of variables that require special handling by code
    /// generation.  If the WGSL type differs from the store type for SPIR-V,
    /// then the `type` parameter is updated.  Returns false on failure (with
    /// a diagnostic), or when the variable should not be emitted, e.g. for a
    /// PointSize builtin.
    /// @param id the ID of the SPIR-V variable
    /// @param store_type the WGSL store type for the variable, which should be
    /// prepopulatd
    /// @param attributes the attribute list to populate
    /// @param transfer_pipeline_io true if pipeline IO decorations (builtins,
    /// or locations) will update the store type and the decorations list
    /// @returns false when the variable should not be emitted as a variable
    bool ConvertDecorationsForVariable(uint32_t id,
                                       const Type** store_type,
                                       ast::AttributeList* attributes,
                                       bool transfer_pipeline_io);

    /// Converts SPIR-V decorations for pipeline IO into AST decorations.
    /// @param store_type the store type for the variable or member
    /// @param decorations the SPIR-V interpolation decorations
    /// @param attributes the attribute list to populate.
    /// @returns false if conversion fails
    bool ConvertPipelineDecorations(const Type* store_type,
                                    const DecorationList& decorations,
                                    ast::AttributeList* attributes);

    /// Updates the attribute list, placing a non-null location decoration into
    /// the list, replacing an existing one if it exists. Does nothing if the
    /// replacement is nullptr.
    /// Assumes the list contains at most one Location decoration.
    /// @param decos the attribute list to modify
    /// @param replacement the location decoration to place into the list
    /// @returns the location decoration that was replaced, if one was replaced,
    /// or null otherwise.
    const ast::Attribute* SetLocation(ast::AttributeList* decos, const ast::Attribute* replacement);

    /// Converts a SPIR-V struct member decoration into a number of AST
    /// decorations. If the decoration is recognized but deliberately dropped,
    /// then returns an empty list without a diagnostic. On failure, emits a
    /// diagnostic and returns an empty list.
    /// @param struct_type_id the ID of the struct type
    /// @param member_index the index of the member
    /// @param member_ty the type of the member
    /// @param decoration an encoded SPIR-V Decoration
    /// @returns the AST decorations
    ast::AttributeList ConvertMemberDecoration(uint32_t struct_type_id,
                                               uint32_t member_index,
                                               const Type* member_ty,
                                               const Decoration& decoration);

    /// Returns a string for the given type.  If the type ID is invalid,
    /// then the resulting string only names the type ID.
    /// @param type_id the SPIR-V ID for the type
    /// @returns a string description of the type.
    std::string ShowType(uint32_t type_id);

    /// Builds the internal representation of the SPIR-V module.
    /// Assumes the module is somewhat well-formed.  Normally you
    /// would want to validate the SPIR-V module before attempting
    /// to build this internal representation. Also computes a topological
    /// ordering of the functions.
    /// This is a no-op if the parser has already failed.
    /// @returns true if the parser is still successful.
    bool BuildInternalModule();

    /// Walks the internal representation of the module to populate
    /// the AST form of the module.
    /// This is a no-op if the parser has already failed.
    /// @returns true if the parser is still successful.
    bool ParseInternalModule();

    /// Records line numbers for each instruction.
    void RegisterLineNumbers();

    /// Walks the internal representation of the module, except for function
    /// definitions, to populate the AST form of the module.
    /// This is a no-op if the parser has already failed.
    /// @returns true if the parser is still successful.
    bool ParseInternalModuleExceptFunctions();

    /// Destroys the internal representation of the SPIR-V module.
    void ResetInternalModule();

    /// Registers extended instruction imports.  Only "GLSL.std.450" is supported.
    /// This is a no-op if the parser has already failed.
    /// @returns true if parser is still successful.
    bool RegisterExtendedInstructionImports();

    /// Returns true when the given instruction is an extended instruction
    /// for GLSL.std.450.
    /// @param inst a SPIR-V instruction
    /// @returns true if its an SpvOpExtInst for GLSL.std.450
    bool IsGlslExtendedInstruction(const spvtools::opt::Instruction& inst) const;

    /// Returns true when the given instruction is an extended instruction
    /// from an ignored extended instruction set.
    /// @param inst a SPIR-V instruction
    /// @returns true if its an SpvOpExtInst for an ignored extended instruction
    bool IsIgnoredExtendedInstruction(const spvtools::opt::Instruction& inst) const;

    /// Registers user names for SPIR-V objects, from OpName, and OpMemberName.
    /// Also synthesizes struct field names.  Ensures uniqueness for names for
    /// SPIR-V IDs, and uniqueness of names of fields within any single struct.
    /// This is a no-op if the parser has already failed.
    /// @returns true if parser is still successful.
    bool RegisterUserAndStructMemberNames();

    /// Register the WorkgroupSize builtin and its associated constant value.
    /// @returns true if parser is still successful.
    bool RegisterWorkgroupSizeBuiltin();

    /// @returns the workgroup size builtin
    const WorkgroupSizeInfo& workgroup_size_builtin() { return workgroup_size_builtin_; }

    /// Register entry point information.
    /// This is a no-op if the parser has already failed.
    /// @returns true if parser is still successful.
    bool RegisterEntryPoints();

    /// Register Tint AST types for SPIR-V types, including type aliases as
    /// needed.  This is a no-op if the parser has already failed.
    /// @returns true if parser is still successful.
    bool RegisterTypes();

    /// Fail if there are any module-scope pointer values other than those
    /// declared by OpVariable.
    /// @returns true if parser is still successful.
    bool RejectInvalidPointerRoots();

    /// Register sampler and texture usage for memory object declarations.
    /// This must be called after we've registered line numbers for all
    /// instructions. This is a no-op if the parser has already failed.
    /// @returns true if parser is still successful.
    bool RegisterHandleUsage();

    /// Emit const definitions for scalar specialization constants generated
    /// by one of OpConstantTrue, OpConstantFalse, or OpSpecConstant.
    /// This is a no-op if the parser has already failed.
    /// @returns true if parser is still successful.
    bool EmitScalarSpecConstants();

    /// Emits module-scope variables.
    /// This is a no-op if the parser has already failed.
    /// @returns true if parser is still successful.
    bool EmitModuleScopeVariables();

    /// Emits functions, with callees preceding their callers.
    /// This is a no-op if the parser has already failed.
    /// @returns true if parser is still successful.
    bool EmitFunctions();

    /// Emits a single function, if it has a body.
    /// This is a no-op if the parser has already failed.
    /// @param f the function to emit
    /// @returns true if parser is still successful.
    bool EmitFunction(const spvtools::opt::Function& f);

    /// Returns the integer constant for the array size of the given variable.
    /// @param var_id SPIR-V ID for an array variable
    /// @returns the integer constant for its array size, or nullptr.
    const spvtools::opt::analysis::IntConstant* GetArraySize(uint32_t var_id);

    /// Returns the member name for the struct member.
    /// @param struct_type the parser's structure type.
    /// @param member_index the member index
    /// @returns the field name
    std::string GetMemberName(const Struct& struct_type, int member_index);

    /// Returns the SPIR-V decorations for pipeline IO, if any, on a struct
    /// member.
    /// @param struct_type the parser's structure type.
    /// @param member_index the member index
    /// @returns a list of SPIR-V decorations.
    DecorationList GetMemberPipelineDecorations(const Struct& struct_type, int member_index);

    /// Creates an AST Variable node for a SPIR-V ID, including any attached
    /// decorations, unless it's an ignorable builtin variable.
    /// @param id the SPIR-V result ID
    /// @param sc the storage class, which cannot be ast::StorageClass::kNone
    /// @param storage_type the storage type of the variable
    /// @param is_const if true, the variable is const
    /// @param is_overridable if true, the variable is pipeline-overridable
    /// @param constructor the variable constructor
    /// @param decorations the variable decorations
    /// @returns a new Variable node, or null in the ignorable variable case and
    /// in the error case
    ast::Variable* MakeVariable(uint32_t id,
                                ast::StorageClass sc,
                                const Type* storage_type,
                                bool is_const,
                                bool is_overridable,
                                const ast::Expression* constructor,
                                ast::AttributeList decorations);

    /// Returns true if a constant expression can be generated.
    /// @param id the SPIR-V ID of the value
    /// @returns true if a constant expression can be generated
    bool CanMakeConstantExpression(uint32_t id);

    /// Creates an AST expression node for a SPIR-V ID.  This is valid to call
    /// when `CanMakeConstantExpression` returns true.
    /// @param id the SPIR-V ID of the constant
    /// @returns a new expression
    TypedExpression MakeConstantExpression(uint32_t id);

    /// Creates an AST expression node for a scalar SPIR-V constant.
    /// @param source the source location
    /// @param ast_type the AST type for the value
    /// @param spirv_const the internal representation of the SPIR-V constant.
    /// @returns a new expression
    TypedExpression MakeConstantExpressionForScalarSpirvConstant(
        Source source,
        const Type* ast_type,
        const spvtools::opt::analysis::Constant* spirv_const);

    /// Creates an AST expression node for the null value for the given type.
    /// @param type the AST type
    /// @returns a new expression
    const ast::Expression* MakeNullValue(const Type* type);

    /// Make a typed expression for the null value for the given type.
    /// @param type the AST type
    /// @returns a new typed expression
    TypedExpression MakeNullExpression(const Type* type);

    /// Converts a given expression to the signedness demanded for an operand
    /// of the given SPIR-V instruction, if required.  If the instruction assumes
    /// signed integer operands, and `expr` is unsigned, then return an
    /// as-cast expression converting it to signed. Otherwise, return
    /// `expr` itself.  Similarly, convert as required from unsigned
    /// to signed. Assumes all SPIR-V types have been mapped to AST types.
    /// @param inst the SPIR-V instruction
    /// @param expr an expression
    /// @returns expr, or a cast of expr
    TypedExpression RectifyOperandSignedness(const spvtools::opt::Instruction& inst,
                                             TypedExpression&& expr);

    /// Converts a second operand to the signedness of the first operand
    /// of a binary operator, if the WGSL operator requires they be the same.
    /// Returns the converted expression, or the original expression if the
    /// conversion is not needed.
    /// @param inst the SPIR-V instruction
    /// @param first_operand_type the type of the first operand to the instruction
    /// @param second_operand_expr the second operand of the instruction
    /// @returns second_operand_expr, or a cast of it
    TypedExpression RectifySecondOperandSignedness(const spvtools::opt::Instruction& inst,
                                                   const Type* first_operand_type,
                                                   TypedExpression&& second_operand_expr);

    /// Returns the "forced" result type for the given SPIR-V instruction.
    /// If the WGSL result type for an operation has a more strict rule than
    /// requried by SPIR-V, then we say the result type is "forced".  This occurs
    /// for signed integer division (OpSDiv), for example, where the result type
    /// in WGSL must match the operand types.
    /// @param inst the SPIR-V instruction
    /// @param first_operand_type the AST type for the first operand.
    /// @returns the forced AST result type, or nullptr if no forcing is required.
    const Type* ForcedResultType(const spvtools::opt::Instruction& inst,
                                 const Type* first_operand_type);

    /// Returns a signed integer scalar or vector type matching the shape (scalar,
    /// vector, and component bit width) of another type, which itself is a
    /// numeric scalar or vector. Returns null if the other type does not meet the
    /// requirement.
    /// @param other the type whose shape must be matched
    /// @returns the signed scalar or vector type
    const Type* GetSignedIntMatchingShape(const Type* other);

    /// Returns a signed integer scalar or vector type matching the shape (scalar,
    /// vector, and component bit width) of another type, which itself is a
    /// numeric scalar or vector. Returns null if the other type does not meet the
    /// requirement.
    /// @param other the type whose shape must be matched
    /// @returns the unsigned scalar or vector type
    const Type* GetUnsignedIntMatchingShape(const Type* other);

    /// Wraps the given expression in an as-cast to the given expression's type,
    /// when the underlying operation produces a forced result type different
    /// from the expression's result type. Otherwise, returns the given expression
    /// unchanged.
    /// @param expr the expression to pass through or to wrap
    /// @param inst the SPIR-V instruction
    /// @param first_operand_type the AST type for the first operand.
    /// @returns the forced AST result type, or nullptr if no forcing is required.
    TypedExpression RectifyForcedResultType(TypedExpression expr,
                                            const spvtools::opt::Instruction& inst,
                                            const Type* first_operand_type);

    /// Returns the given expression, but ensuring it's an unsigned type of the
    /// same shape as the operand. Wraps the expression with a bitcast if needed.
    /// Assumes the given expresion is a integer scalar or vector.
    /// @param expr an integer scalar or integer vector expression.
    /// @return the potentially cast TypedExpression
    TypedExpression AsUnsigned(TypedExpression expr);

    /// Returns the given expression, but ensuring it's a signed type of the
    /// same shape as the operand. Wraps the expression with a bitcast if needed.
    /// Assumes the given expresion is a integer scalar or vector.
    /// @param expr an integer scalar or integer vector expression.
    /// @return the potentially cast TypedExpression
    TypedExpression AsSigned(TypedExpression expr);

    /// Bookkeeping used for tracking the "position" builtin variable.
    struct BuiltInPositionInfo {
        /// The ID for the gl_PerVertex struct containing the Position builtin.
        uint32_t struct_type_id = 0;
        /// The member index for the Position builtin within the struct.
        uint32_t position_member_index = 0;
        /// The member index for the PointSize builtin within the struct.
        uint32_t pointsize_member_index = 0;
        /// The ID for the member type, which should map to vec4<f32>.
        uint32_t position_member_type_id = 0;
        /// The ID of the type of a pointer to the struct in the Output storage
        /// class class.
        uint32_t pointer_type_id = 0;
        /// The SPIR-V storage class.
        SpvStorageClass storage_class = SpvStorageClassOutput;
        /// The ID of the type of a pointer to the Position member.
        uint32_t position_member_pointer_type_id = 0;
        /// The ID of the gl_PerVertex variable, if it was declared.
        /// We'll use this for the gl_Position variable instead.
        uint32_t per_vertex_var_id = 0;
        /// The ID of the initializer to gl_PerVertex, if any.
        uint32_t per_vertex_var_init_id = 0;
    };
    /// @returns info about the gl_Position builtin variable.
    const BuiltInPositionInfo& GetBuiltInPositionInfo() { return builtin_position_; }

    /// Returns the source record for the SPIR-V instruction with the given
    /// result ID.
    /// @param id the SPIR-V result id.
    /// @return the Source record, or a default one
    Source GetSourceForResultIdForTest(uint32_t id) const;
    /// Returns the source record for the given instruction.
    /// @param inst the SPIR-V instruction
    /// @return the Source record, or a default one
    Source GetSourceForInst(const spvtools::opt::Instruction* inst) const;

    /// @param str a candidate identifier
    /// @returns true if the given string is a valid WGSL identifier.
    static bool IsValidIdentifier(const std::string& str);

    /// Returns true if the given SPIR-V ID is a declared specialization constant,
    /// generated by one of OpConstantTrue, OpConstantFalse, or OpSpecConstant
    /// @param id a SPIR-V result ID
    /// @returns true if the ID is a scalar spec constant.
    bool IsScalarSpecConstant(uint32_t id) {
        return scalar_spec_constants_.find(id) != scalar_spec_constants_.end();
    }

    /// For a SPIR-V ID that might define a sampler, image, or sampled image
    /// value, return the SPIR-V instruction that represents the memory object
    /// declaration for the object.  If we encounter an OpSampledImage along the
    /// way, follow the image operand when follow_image is true; otherwise follow
    /// the sampler operand. Returns nullptr if we can't trace back to a memory
    /// object declaration.  Emits an error and returns nullptr when the scan
    /// fails due to a malformed module. This method can be used any time after
    /// BuildInternalModule has been invoked.
    /// @param id the SPIR-V ID of the sampler, image, or sampled image
    /// @param follow_image indicates whether to follow the image operand of
    /// OpSampledImage
    /// @returns the memory object declaration for the handle, or nullptr
    const spvtools::opt::Instruction* GetMemoryObjectDeclarationForHandle(uint32_t id,
                                                                          bool follow_image);

    /// Returns the handle usage for a memory object declaration.
    /// @param id SPIR-V ID of a sampler or image OpVariable or
    /// OpFunctionParameter
    /// @returns the handle usage, or an empty usage object.
    Usage GetHandleUsage(uint32_t id) const;

    /// Returns the SPIR-V type for the sampler or image type for the given
    /// variable in UniformConstant storage class, or function parameter pointing
    /// into the UniformConstant storage class .  Returns null and emits an
    /// error on failure.
    /// @param var the OpVariable instruction or OpFunctionParameter
    /// @returns the Tint AST type for the sampler or texture, or null on error
    const spvtools::opt::Instruction* GetSpirvTypeForHandleMemoryObjectDeclaration(
        const spvtools::opt::Instruction& var);

    /// Returns the AST type for the pointer-to-sampler or pointer-to-texture type
    /// for the given variable in UniformConstant storage class.  Returns null and
    /// emits an error on failure.
    /// @param var the OpVariable instruction
    /// @returns the Tint AST type for the poiner-to-{sampler|texture} or null on
    /// error
    const Pointer* GetTypeForHandleVar(const spvtools::opt::Instruction& var);

    /// Returns the channel component type corresponding to the given image
    /// format.
    /// @param format image texel format
    /// @returns the component type, one of f32, i32, u32
    const Type* GetComponentTypeForFormat(ast::TexelFormat format);

    /// Returns the number of channels in the given image format.
    /// @param format image texel format
    /// @returns the number of channels in the format
    unsigned GetChannelCountForFormat(ast::TexelFormat format);

    /// Returns the texel type corresponding to the given image format.
    /// This the WGSL type used for the texel parameter to textureStore.
    /// It's always a 4-element vector.
    /// @param format image texel format
    /// @returns the texel format
    const Type* GetTexelTypeForFormat(ast::TexelFormat format);

    /// Returns the SPIR-V instruction with the given ID, or nullptr.
    /// @param id the SPIR-V result ID
    /// @returns the instruction, or nullptr on error
    const spvtools::opt::Instruction* GetInstructionForTest(uint32_t id) const;

    /// A map of SPIR-V identifiers to builtins
    using BuiltInsMap = std::unordered_map<uint32_t, SpvBuiltIn>;

    /// @returns a map of builtins that should be handled specially by code
    /// generation. Either the builtin does not exist in WGSL, or a type
    /// conversion must be implemented on load and store.
    const BuiltInsMap& special_builtins() const { return special_builtins_; }

    /// @param builtin the SPIR-V builtin variable kind
    /// @returns the SPIR-V ID for the variable defining the given builtin, or 0
    uint32_t IdForSpecialBuiltIn(SpvBuiltIn builtin) const {
        // Do a linear search.
        for (const auto& entry : special_builtins_) {
            if (entry.second == builtin) {
                return entry.first;
            }
        }
        return 0;
    }

    /// @param entry_point the SPIR-V ID of an entry point.
    /// @returns the entry point info for the given ID
    const std::vector<EntryPointInfo>& GetEntryPointInfo(uint32_t entry_point) {
        return function_to_ep_info_[entry_point];
    }

    /// @returns the SPIR-V binary.
    const std::vector<uint32_t>& spv_binary() { return spv_binary_; }

  private:
    /// Converts a specific SPIR-V type to a Tint type. Integer case
    const Type* ConvertType(const spvtools::opt::analysis::Integer* int_ty);
    /// Converts a specific SPIR-V type to a Tint type. Float case
    const Type* ConvertType(const spvtools::opt::analysis::Float* float_ty);
    /// Converts a specific SPIR-V type to a Tint type. Vector case
    const Type* ConvertType(const spvtools::opt::analysis::Vector* vec_ty);
    /// Converts a specific SPIR-V type to a Tint type. Matrix case
    const Type* ConvertType(const spvtools::opt::analysis::Matrix* mat_ty);
    /// Converts a specific SPIR-V type to a Tint type. RuntimeArray case
    /// Distinct SPIR-V array types map to distinct Tint array types.
    /// @param rtarr_ty the Tint type
    const Type* ConvertType(uint32_t type_id,
                            const spvtools::opt::analysis::RuntimeArray* rtarr_ty);
    /// Converts a specific SPIR-V type to a Tint type. Array case
    /// Distinct SPIR-V array types map to distinct Tint array types.
    /// @param arr_ty the Tint type
    const Type* ConvertType(uint32_t type_id, const spvtools::opt::analysis::Array* arr_ty);
    /// Converts a specific SPIR-V type to a Tint type. Struct case.
    /// SPIR-V allows distinct struct type definitions for two OpTypeStruct
    /// that otherwise have the same set of members (and struct and member
    /// decorations).  However, the SPIRV-Tools always produces a unique
    /// `spvtools::opt::analysis::Struct` object in these cases. For this type
    /// conversion, we need to have the original SPIR-V ID because we can't always
    /// recover it from the optimizer's struct type object. This also lets us
    /// preserve member names, which are given by OpMemberName which is normally
    /// not significant to the optimizer's module representation.
    /// @param type_id the SPIR-V ID for the type.
    /// @param struct_ty the Tint type
    const Type* ConvertType(uint32_t type_id, const spvtools::opt::analysis::Struct* struct_ty);
    /// Converts a specific SPIR-V type to a Tint type. Pointer / Reference case
    /// The pointer to gl_PerVertex maps to nullptr, and instead is recorded
    /// in member #builtin_position_.
    /// @param type_id the SPIR-V ID for the type.
    /// @param ptr_as if PtrAs::Ref then a Reference will be returned, otherwise
    /// Pointer
    /// @param ptr_ty the Tint type
    const Type* ConvertType(uint32_t type_id,
                            PtrAs ptr_as,
                            const spvtools::opt::analysis::Pointer* ptr_ty);

    /// If `type` is a signed integral, or vector of signed integral,
    /// returns the unsigned type, otherwise returns `type`.
    /// @param type the possibly signed type
    /// @returns the unsigned type
    const Type* UnsignedTypeFor(const Type* type);

    /// If `type` is a unsigned integral, or vector of unsigned integral,
    /// returns the signed type, otherwise returns `type`.
    /// @param type the possibly unsigned type
    /// @returns the signed type
    const Type* SignedTypeFor(const Type* type);

    /// Parses the array or runtime-array decorations. Sets 0 if no explicit
    /// stride was found, and therefore the implicit stride should be used.
    /// @param spv_type the SPIR-V array or runtime-array type.
    /// @param array_stride pointer to the array stride
    /// @returns true on success.
    bool ParseArrayDecorations(const spvtools::opt::analysis::Type* spv_type,
                               uint32_t* array_stride);

    /// Creates a new `ast::Node` owned by the ProgramBuilder.
    /// @param args the arguments to pass to the type constructor
    /// @returns the node pointer
    template <typename T, typename... ARGS>
    T* create(ARGS&&... args) {
        return builder_.create<T>(std::forward<ARGS>(args)...);
    }

    // The SPIR-V binary we're parsing
    std::vector<uint32_t> spv_binary_;

    // The program builder.
    ProgramBuilder builder_;

    // The type manager.
    TypeManager ty_;

    // Is the parse successful?
    bool success_ = true;
    // Collector for diagnostic messages.
    std::stringstream errors_;
    FailStream fail_stream_;
    spvtools::MessageConsumer message_consumer_;

    // An object used to store and generate names for SPIR-V objects.
    Namer namer_;
    // An object used to convert SPIR-V enums to Tint enums
    EnumConverter enum_converter_;

    // The internal representation of the SPIR-V module and its context.
    spvtools::Context tools_context_;
    // All the state is owned by ir_context_.
    std::unique_ptr<spvtools::opt::IRContext> ir_context_;
    // The following are borrowed pointers to the internal state of ir_context_.
    spvtools::opt::Module* module_ = nullptr;
    spvtools::opt::analysis::DefUseManager* def_use_mgr_ = nullptr;
    spvtools::opt::analysis::ConstantManager* constant_mgr_ = nullptr;
    spvtools::opt::analysis::TypeManager* type_mgr_ = nullptr;
    spvtools::opt::analysis::DecorationManager* deco_mgr_ = nullptr;

    // The functions ordered so that callees precede their callers.
    std::vector<const spvtools::opt::Function*> topologically_ordered_functions_;

    // Maps an instruction to its source location. If no OpLine information
    // is in effect for the instruction, map the instruction to its position
    // in the SPIR-V module, counting by instructions, where the first
    // instruction is line 1.
    std::unordered_map<const spvtools::opt::Instruction*, Source::Location> inst_source_;

    // The set of IDs that are imports of the GLSL.std.450 extended instruction
    // sets.
    std::unordered_set<uint32_t> glsl_std_450_imports_;
    // The set of IDs of imports that are ignored. For example, any
    // "NonSemanticInfo." import is ignored.
    std::unordered_set<uint32_t> ignored_imports_;

    // The SPIR-V IDs of structure types that are the store type for buffer
    // variables, either UBO or SSBO.
    std::unordered_set<uint32_t> struct_types_for_buffers_;

    // Bookkeeping for the gl_Position builtin.
    // In Vulkan SPIR-V, it's the 0 member of the gl_PerVertex structure.
    // But in WGSL we make a module-scope variable:
    //    [[position]] var<in> gl_Position : vec4<f32>;
    // The builtin variable was detected if and only if the struct_id is non-zero.
    BuiltInPositionInfo builtin_position_;

    // SPIR-V type IDs that are either:
    // - a struct type decorated by BufferBlock
    // - an array, runtime array containing one of these
    // - a pointer type to one of these
    // These are the types "enclosing" a buffer block with the old style
    // representation: using Uniform storage class and BufferBlock decoration
    // on the struct.  The new style is to use the StorageBuffer storage class
    // and Block decoration.
    std::unordered_set<uint32_t> remap_buffer_block_type_;

    // The ast::Struct type names with only read-only members.
    std::unordered_set<Symbol> read_only_struct_types_;

    // The IDs of scalar spec constants
    std::unordered_set<uint32_t> scalar_spec_constants_;

    // Maps function_id to a list of entrypoint information
    std::unordered_map<uint32_t, std::vector<EntryPointInfo>> function_to_ep_info_;

    // Maps from a SPIR-V ID to its underlying memory object declaration,
    // following image paths. This a memoization table for
    // GetMemoryObjectDeclarationForHandle. (A SPIR-V memory object declaration is
    // an OpVariable or an OpFunctinParameter with pointer type).
    std::unordered_map<uint32_t, const spvtools::opt::Instruction*> mem_obj_decl_image_;
    // Maps from a SPIR-V ID to its underlying memory object declaration,
    // following sampler paths. This a memoization table for
    // GetMemoryObjectDeclarationForHandle.
    std::unordered_map<uint32_t, const spvtools::opt::Instruction*> mem_obj_decl_sampler_;

    // Maps a memory-object-declaration instruction to any sampler or texture
    // usages implied by usages of the memory-object-declaration.
    std::unordered_map<const spvtools::opt::Instruction*, Usage> handle_usage_;
    // The inferred pointer type for the given handle variable.
    std::unordered_map<const spvtools::opt::Instruction*, const Pointer*> handle_type_;

    // Set of symbols of declared type that have been added, used to avoid
    // adding duplicates.
    std::unordered_set<Symbol> declared_types_;

    // Maps a struct type name to the SPIR-V ID for the structure type.
    std::unordered_map<Symbol, uint32_t> struct_id_for_symbol_;

    /// Maps the SPIR-V ID of a module-scope builtin variable that should be
    /// ignored or type-converted, to its builtin kind.
    /// See also BuiltInPositionInfo which is a separate mechanism for a more
    /// complex case of replacing an entire structure.
    BuiltInsMap special_builtins_;

    /// Info about the WorkgroupSize builtin. If it's not present, then the 'id'
    /// field will be 0. Sadly, in SPIR-V right now, there's only one workgroup
    /// size object in the module.
    WorkgroupSizeInfo workgroup_size_builtin_;
};

}  // namespace tint::reader::spirv

#endif  // SRC_TINT_READER_SPIRV_PARSER_IMPL_H_
