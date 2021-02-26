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

#include "src/inspector/inspector.h"

#include <utility>

#include "gtest/gtest.h"
#include "src/ast/assignment_statement.h"
#include "src/ast/bool_literal.h"
#include "src/ast/call_expression.h"
#include "src/ast/call_statement.h"
#include "src/ast/constant_id_decoration.h"
#include "src/ast/decorated_variable.h"
#include "src/ast/float_literal.h"
#include "src/ast/function.h"
#include "src/ast/identifier_expression.h"
#include "src/ast/member_accessor_expression.h"
#include "src/ast/null_literal.h"
#include "src/ast/pipeline_stage.h"
#include "src/ast/return_statement.h"
#include "src/ast/scalar_constructor_expression.h"
#include "src/ast/sint_literal.h"
#include "src/ast/stage_decoration.h"
#include "src/ast/stride_decoration.h"
#include "src/ast/struct_block_decoration.h"
#include "src/ast/struct_decoration.h"
#include "src/ast/struct_member.h"
#include "src/ast/struct_member_decoration.h"
#include "src/ast/struct_member_offset_decoration.h"
#include "src/ast/type/access_control_type.h"
#include "src/ast/type/array_type.h"
#include "src/ast/type/bool_type.h"
#include "src/ast/type/depth_texture_type.h"
#include "src/ast/type/f32_type.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type/matrix_type.h"
#include "src/ast/type/pointer_type.h"
#include "src/ast/type/sampled_texture_type.h"
#include "src/ast/type/sampler_type.h"
#include "src/ast/type/struct_type.h"
#include "src/ast/type/type.h"
#include "src/ast/type/u32_type.h"
#include "src/ast/type/vector_type.h"
#include "src/ast/type/void_type.h"
#include "src/ast/uint_literal.h"
#include "src/ast/variable_decl_statement.h"
#include "src/ast/variable_decoration.h"
#include "src/ast/workgroup_decoration.h"
#include "src/context.h"
#include "src/type_determiner.h"
#include "tint/tint.h"

namespace tint {
namespace inspector {
namespace {

class InspectorHelper {
 public:
  InspectorHelper()
      : td_(std::make_unique<TypeDeterminer>(&ctx_, &mod_)),
        inspector_(std::make_unique<Inspector>(mod_)),
        sampler_type_(ast::type::SamplerKind::kSampler),
        comparison_sampler_type_(ast::type::SamplerKind::kComparisonSampler) {}

  /// Generates an empty function
  /// @param name name of the function created
  /// @returns a function object
  std::unique_ptr<ast::Function> MakeEmptyBodyFunction(std::string name) {
    auto body = std::make_unique<ast::BlockStatement>();
    body->append(std::make_unique<ast::ReturnStatement>());
    std::unique_ptr<ast::Function> func =
        std::make_unique<ast::Function>(name, ast::VariableList(), void_type());
    func->set_body(std::move(body));
    return func;
  }

  /// Generates a function that calls another
  /// @param caller name of the function created
  /// @param callee name of the function to be called
  /// @returns a function object
  std::unique_ptr<ast::Function> MakeCallerBodyFunction(std::string caller,
                                                        std::string callee) {
    auto body = std::make_unique<ast::BlockStatement>();
    auto ident_expr = std::make_unique<ast::IdentifierExpression>(callee);
    auto call_expr = std::make_unique<ast::CallExpression>(
        std::move(ident_expr), ast::ExpressionList());
    body->append(std::make_unique<ast::CallStatement>(std::move(call_expr)));
    body->append(std::make_unique<ast::ReturnStatement>());
    std::unique_ptr<ast::Function> func = std::make_unique<ast::Function>(
        caller, ast::VariableList(), void_type());
    func->set_body(std::move(body));
    return func;
  }

  /// Add In/Out variables to the global variables
  /// @param inout_vars tuples of {in, out} that will be added as entries to the
  ///                   global variables
  void AddInOutVariables(
      std::vector<std::tuple<std::string, std::string>> inout_vars) {
    for (auto inout : inout_vars) {
      std::string in, out;
      std::tie(in, out) = inout;
      auto in_var = std::make_unique<ast::Variable>(
          in, ast::StorageClass::kInput, u32_type());
      auto out_var = std::make_unique<ast::Variable>(
          out, ast::StorageClass::kOutput, u32_type());
      mod()->AddGlobalVariable(std::move(in_var));
      mod()->AddGlobalVariable(std::move(out_var));
    }
  }

  /// Generates a function that references in/out variables
  /// @param name name of the function created
  /// @param inout_vars tuples of {in, out} that will be converted into out = in
  ///                   calls in the function body
  /// @returns a function object
  std::unique_ptr<ast::Function> MakeInOutVariableBodyFunction(
      std::string name,
      std::vector<std::tuple<std::string, std::string>> inout_vars) {
    auto body = std::make_unique<ast::BlockStatement>();
    for (auto inout : inout_vars) {
      std::string in, out;
      std::tie(in, out) = inout;
      body->append(std::make_unique<ast::AssignmentStatement>(
          std::make_unique<ast::IdentifierExpression>(out),
          std::make_unique<ast::IdentifierExpression>(in)));
    }
    body->append(std::make_unique<ast::ReturnStatement>());
    auto func =
        std::make_unique<ast::Function>(name, ast::VariableList(), void_type());
    func->set_body(std::move(body));
    return func;
  }

  /// Generates a function that references in/out variables and calls another
  /// function.
  /// @param caller name of the function created
  /// @param callee name of the function to be called
  /// @param inout_vars tuples of {in, out} that will be converted into out = in
  ///                   calls in the function body
  /// @returns a function object
  std::unique_ptr<ast::Function> MakeInOutVariableCallerBodyFunction(
      std::string caller,
      std::string callee,
      std::vector<std::tuple<std::string, std::string>> inout_vars) {
    auto body = std::make_unique<ast::BlockStatement>();
    for (auto inout : inout_vars) {
      std::string in, out;
      std::tie(in, out) = inout;
      body->append(std::make_unique<ast::AssignmentStatement>(
          std::make_unique<ast::IdentifierExpression>(out),
          std::make_unique<ast::IdentifierExpression>(in)));
    }
    auto ident_expr = std::make_unique<ast::IdentifierExpression>(callee);
    auto call_expr = std::make_unique<ast::CallExpression>(
        std::move(ident_expr), ast::ExpressionList());
    body->append(std::make_unique<ast::CallStatement>(std::move(call_expr)));
    body->append(std::make_unique<ast::ReturnStatement>());
    auto func = std::make_unique<ast::Function>(caller, ast::VariableList(),
                                                void_type());
    func->set_body(std::move(body));
    return func;
  }

  /// Add a Constant ID to the global variables.
  /// @param name name of the variable to add
  /// @param id id number for the constant id
  /// @param type type of the variable
  /// @param val value to initialize the variable with, if NULL no initializer
  ///            will be added.
  template <class T>
  void AddConstantID(std::string name,
                     uint32_t id,
                     ast::type::Type* type,
                     T* val) {
    auto dvar = std::make_unique<ast::DecoratedVariable>(
        std::make_unique<ast::Variable>(name, ast::StorageClass::kNone, type));
    dvar->set_is_const(true);
    ast::VariableDecorationList decos;
    decos.push_back(std::make_unique<ast::ConstantIdDecoration>(id, Source{}));
    dvar->set_decorations(std::move(decos));
    if (val) {
      dvar->set_constructor(std::make_unique<ast::ScalarConstructorExpression>(
          MakeLiteral(type, val)));
    }
    mod()->AddGlobalVariable(std::move(dvar));
  }

  /// @param type AST type of the literal, must resolve to BoolLiteral
  /// @param val scalar value for the literal to contain
  /// @returns a Literal of the expected type and value
  std::unique_ptr<ast::Literal> MakeLiteral(ast::type::Type* type, bool* val) {
    return std::make_unique<ast::BoolLiteral>(type, *val);
  }

  /// @param type AST type of the literal, must resolve to UIntLiteral
  /// @param val scalar value for the literal to contain
  /// @returns a Literal of the expected type and value
  std::unique_ptr<ast::Literal> MakeLiteral(ast::type::Type* type,
                                            uint32_t* val) {
    return std::make_unique<ast::UintLiteral>(type, *val);
  }

  /// @param type AST type of the literal, must resolve to IntLiteral
  /// @param val scalar value for the literal to contain
  /// @returns a Literal of the expected type and value
  std::unique_ptr<ast::Literal> MakeLiteral(ast::type::Type* type,
                                            int32_t* val) {
    return std::make_unique<ast::SintLiteral>(type, *val);
  }

  /// @param type AST type of the literal, must resolve to FloattLiteral
  /// @param val scalar value for the literal to contain
  /// @returns a Literal of the expected type and value
  std::unique_ptr<ast::Literal> MakeLiteral(ast::type::Type* type, float* val) {
    return std::make_unique<ast::FloatLiteral>(type, *val);
  }

  /// @param vec Vector of strings to be searched
  /// @param str String to be searching for
  /// @returns true if str is in vec, otherwise false
  bool ContainsString(const std::vector<std::string>& vec,
                      const std::string& str) {
    for (auto& s : vec) {
      if (s == str) {
        return true;
      }
    }
    return false;
  }

  /// Builds a string for accessing a member in a generated struct
  /// @param idx index of member
  /// @param type type of member
  /// @returns a string for the member
  std::string StructMemberName(size_t idx, ast::type::Type* type) {
    return std::to_string(idx) + type->type_name();
  }

  /// Generates a struct type
  /// @param name name for the type
  /// @param members_info a vector of {type, offset} where each entry is the
  ///                     type and offset of a member of the struct
  /// @param is_block whether or not to decorate as a Block
  /// @returns a struct type
  std::unique_ptr<ast::type::StructType> MakeStructType(
      const std::string& name,
      std::vector<std::tuple<ast::type::Type*, uint32_t>> members_info,
      bool is_block) {
    ast::StructMemberList members;
    for (auto& member_info : members_info) {
      ast::type::Type* type;
      uint32_t offset;
      std::tie(type, offset) = member_info;

      ast::StructMemberDecorationList deco;
      deco.push_back(std::make_unique<ast::StructMemberOffsetDecoration>(
          offset, Source{}));

      members.push_back(std::make_unique<ast::StructMember>(
          StructMemberName(members.size(), type), type, std::move(deco)));
    }

    ast::StructDecorationList decos;
    if (is_block) {
      decos.push_back(std::make_unique<ast::StructBlockDecoration>(Source{}));
    }

    auto str =
        std::make_unique<ast::Struct>(std::move(decos), std::move(members));

    return std::make_unique<ast::type::StructType>(name, std::move(str));
  }

  /// Generates types appropriate for using in an uniform buffer
  /// @param name name for the type
  /// @param members_info a vector of {type, offset} where each entry is the
  ///                     type and offset of a member of the struct
  /// @returns a tuple {struct type, access control type}, where the struct has
  ///          the layout for an uniform buffer, and the control type wraps the
  ///          struct.
  std::tuple<std::unique_ptr<ast::type::StructType>,
             std::unique_ptr<ast::type::AccessControlType>>
  MakeUniformBufferTypes(
      const std::string& name,
      std::vector<std::tuple<ast::type::Type*, uint32_t>> members_info) {
    auto struct_type = MakeStructType(name, members_info, true);
    auto access_type = std::make_unique<ast::type::AccessControlType>(
        ast::AccessControl::kReadOnly, struct_type.get());
    return {std::move(struct_type), std::move(access_type)};
  }

  /// Generates types appropriate for using in a storage buffer
  /// @param name name for the type
  /// @param members_info a vector of {type, offset} where each entry is the
  ///                     type and offset of a member of the struct
  /// @returns a tuple {struct type, access control type}, where the struct has
  ///          the layout for a storage buffer, and the control type wraps the
  ///          struct.
  std::tuple<std::unique_ptr<ast::type::StructType>,
             std::unique_ptr<ast::type::AccessControlType>>
  MakeStorageBufferTypes(
      const std::string& name,
      std::vector<std::tuple<ast::type::Type*, uint32_t>> members_info) {
    auto struct_type = MakeStructType(name, members_info, false);
    auto access_type = std::make_unique<ast::type::AccessControlType>(
        ast::AccessControl::kReadWrite, struct_type.get());
    return {std::move(struct_type), std::move(access_type)};
  }

  /// Generates types appropriate for using in a read-only storage buffer
  /// @param name name for the type
  /// @param members_info a vector of {type, offset} where each entry is the
  ///                     type and offset of a member of the struct
  /// @returns a tuple {struct type, access control type}, where the struct has
  ///          the layout for a read-only storage buffer, and the control type
  ///          wraps the struct.
  std::tuple<std::unique_ptr<ast::type::StructType>,
             std::unique_ptr<ast::type::AccessControlType>>
  MakeReadOnlyStorageBufferTypes(
      const std::string& name,
      std::vector<std::tuple<ast::type::Type*, uint32_t>> members_info) {
    auto struct_type = MakeStructType(name, members_info, false);
    auto access_type = std::make_unique<ast::type::AccessControlType>(
        ast::AccessControl::kReadOnly, struct_type.get());
    return {std::move(struct_type), std::move(access_type)};
  }

  /// Adds a binding variable with a struct type to the module
  /// @param name the name of the variable
  /// @param type the type to use
  /// @param storage_class the storage class to use
  /// @param set the binding group/set to use for the uniform buffer
  /// @param binding the binding number to use for the uniform buffer
  void AddBinding(const std::string& name,
                  ast::type::Type* type,
                  ast::StorageClass storage_class,
                  uint32_t set,
                  uint32_t binding) {
    auto var = std::make_unique<ast::DecoratedVariable>(
        std::make_unique<ast::Variable>(name, storage_class, type));
    ast::VariableDecorationList decorations;

    decorations.push_back(
        std::make_unique<ast::BindingDecoration>(binding, Source{}));
    decorations.push_back(std::make_unique<ast::SetDecoration>(set, Source{}));
    var->set_decorations(std::move(decorations));

    mod()->AddGlobalVariable(std::move(var));
  }

  /// Adds an uniform buffer variable to the module
  /// @param name the name of the variable
  /// @param type the type to use
  /// @param set the binding group/set to use for the uniform buffer
  /// @param binding the binding number to use for the uniform buffer
  void AddUniformBuffer(const std::string& name,
                        ast::type::Type* type,
                        uint32_t set,
                        uint32_t binding) {
    AddBinding(name, type, ast::StorageClass::kUniform, set, binding);
  }

  /// Adds a storage buffer variable to the module
  /// @param name the name of the variable
  /// @param type the type to use
  /// @param set the binding group/set to use for the storage buffer
  /// @param binding the binding number to use for the storage buffer
  void AddStorageBuffer(const std::string& name,
                        ast::type::Type* type,
                        uint32_t set,
                        uint32_t binding) {
    AddBinding(name, type, ast::StorageClass::kStorageBuffer, set, binding);
  }

  /// Generates a function that references a specific struct variable
  /// @param func_name name of the function created
  /// @param struct_name name of the struct variabler to be accessed
  /// @param members list of members to access, by index and type
  /// @returns a function that references all of the members specified
  std::unique_ptr<ast::Function> MakeStructVariableReferenceBodyFunction(
      std::string func_name,
      std::string struct_name,
      std::vector<std::tuple<size_t, ast::type::Type*>> members) {
    auto body = std::make_unique<ast::BlockStatement>();

    for (auto member : members) {
      size_t member_idx;
      ast::type::Type* member_type;
      std::tie(member_idx, member_type) = member;
      std::string member_name = StructMemberName(member_idx, member_type);
      body->append(std::make_unique<ast::VariableDeclStatement>(
          std::make_unique<ast::Variable>(
              "local" + member_name, ast::StorageClass::kNone, member_type)));
    }

    for (auto member : members) {
      size_t member_idx;
      ast::type::Type* member_type;
      std::tie(member_idx, member_type) = member;
      std::string member_name = StructMemberName(member_idx, member_type);
      body->append(std::make_unique<ast::AssignmentStatement>(
          std::make_unique<ast::IdentifierExpression>("local" + member_name),
          std::make_unique<ast::MemberAccessorExpression>(
              std::make_unique<ast::IdentifierExpression>(struct_name),
              std::make_unique<ast::IdentifierExpression>(member_name))));
    }

    body->append(std::make_unique<ast::ReturnStatement>());
    auto func = std::make_unique<ast::Function>(func_name, ast::VariableList(),
                                                void_type());
    func->set_body(std::move(body));
    return func;
  }

  /// Adds a regular sampler variable to the module
  /// @param name the name of the variable
  /// @param set the binding group/set to use for the storage buffer
  /// @param binding the binding number to use for the storage buffer
  void AddSampler(const std::string& name, uint32_t set, uint32_t binding) {
    AddBinding(name, sampler_type(), ast::StorageClass::kUniformConstant, set,
               binding);
  }

  /// Adds a comparison sampler variable to the module
  /// @param name the name of the variable
  /// @param set the binding group/set to use for the storage buffer
  /// @param binding the binding number to use for the storage buffer
  void AddComparisonSampler(const std::string& name,
                            uint32_t set,
                            uint32_t binding) {
    AddBinding(name, comparison_sampler_type(),
               ast::StorageClass::kUniformConstant, set, binding);
  }

  /// Generates a SampledTextureType appropriate for the params
  /// @param dim the dimensions of the texture
  /// @param type the data type of the sampled texture
  /// @returns the generated SampleTextureType
  std::unique_ptr<ast::type::SampledTextureType> MakeSampledTextureType(
      ast::type::TextureDimension dim,
      ast::type::Type* type) {
    return std::make_unique<ast::type::SampledTextureType>(dim, type);
  }

  /// Generates a DepthTextureType appropriate for the params
  /// @param dim the dimensions of the texture
  /// @returns the generated DepthTextureType
  std::unique_ptr<ast::type::DepthTextureType> MakeDepthTextureType(
      ast::type::TextureDimension dim) {
    return std::make_unique<ast::type::DepthTextureType>(dim);
  }

  /// Adds a sampled texture variable to the module
  /// @param name the name of the variable
  /// @param type the type to use
  /// @param set the binding group/set to use for the sampled texture
  /// @param binding the binding number to use for the sampled texture
  void AddSampledTexture(const std::string& name,
                         ast::type::Type* type,
                         uint32_t set,
                         uint32_t binding) {
    AddBinding(name, type, ast::StorageClass::kUniformConstant, set, binding);
  }

  void AddF32(const std::string& name) {
    mod()->AddGlobalVariable(std::make_unique<ast::Variable>(
        name, ast::StorageClass::kUniformConstant, f32_type()));
  }

  /// Adds a depth texture variable to the module
  /// @param name the name of the variable
  /// @param type the type to use
  void AddDepthTexture(const std::string& name, ast::type::Type* type) {
    mod()->AddGlobalVariable(std::make_unique<ast::Variable>(
        name, ast::StorageClass::kUniformConstant, type));
  }

  /// Generates a function that references a specific sampler variable
  /// @param func_name name of the function created
  /// @param texture_name name of the texture to be sampled
  /// @param sampler_name name of the sampler to use
  /// @param coords_name name of the coords variable to use
  /// @returns a function that references all of the values specified
  std::unique_ptr<ast::Function> MakeSamplerReferenceBodyFunction(
      const std::string& func_name,
      const std::string& texture_name,
      const std::string& sampler_name,
      const std::string& coords_name) {
    std::string result_name = "sampler_result";

    auto body = std::make_unique<ast::BlockStatement>();

    auto call_result = std::make_unique<ast::Variable>(
        "sampler_result", ast::StorageClass::kFunction, f32_vec_type(4));
    body->append(
        std::make_unique<ast::VariableDeclStatement>(std::move(call_result)));

    ast::ExpressionList call_params;
    call_params.push_back(
        std::make_unique<ast::IdentifierExpression>(texture_name));
    call_params.push_back(
        std::make_unique<ast::IdentifierExpression>(sampler_name));
    call_params.push_back(
        std::make_unique<ast::IdentifierExpression>(coords_name));
    auto call_expr = std::make_unique<ast::CallExpression>(
        std::make_unique<ast::IdentifierExpression>("textureSample"),
        std::move(call_params));

    body->append(std::make_unique<ast::AssignmentStatement>(
        std::make_unique<ast::IdentifierExpression>("sampler_result"),
        std::move(call_expr)));
    body->append(std::make_unique<ast::ReturnStatement>());

    auto func = std::make_unique<ast::Function>(func_name, ast::VariableList(),
                                                void_type());
    func->set_body(std::move(body));
    return func;
  }

  /// Generates a function that references a specific comparison sampler
  /// variable.
  /// @param func_name name of the function created
  /// @param texture_name name of the depth texture to  use
  /// @param sampler_name name of the sampler to use
  /// @param coords_name name of the coords variable to use
  /// @param depth_name name of the depth reference to use
  /// @returns a function that references all of the values specified
  std::unique_ptr<ast::Function> MakeComparisonSamplerReferenceBodyFunction(
      const std::string& func_name,
      const std::string& texture_name,
      const std::string& sampler_name,
      const std::string& coords_name,
      const std::string& depth_name) {
    std::string result_name = "sampler_result";

    auto body = std::make_unique<ast::BlockStatement>();

    auto call_result = std::make_unique<ast::Variable>(
        "sampler_result", ast::StorageClass::kFunction, f32_type());
    body->append(
        std::make_unique<ast::VariableDeclStatement>(std::move(call_result)));

    ast::ExpressionList call_params;
    call_params.push_back(
        std::make_unique<ast::IdentifierExpression>(texture_name));
    call_params.push_back(
        std::make_unique<ast::IdentifierExpression>(sampler_name));
    call_params.push_back(
        std::make_unique<ast::IdentifierExpression>(coords_name));
    call_params.push_back(
        std::make_unique<ast::IdentifierExpression>(depth_name));
    auto call_expr = std::make_unique<ast::CallExpression>(
        std::make_unique<ast::IdentifierExpression>("textureSampleCompare"),
        std::move(call_params));

    body->append(std::make_unique<ast::AssignmentStatement>(
        std::make_unique<ast::IdentifierExpression>("sampler_result"),
        std::move(call_expr)));
    body->append(std::make_unique<ast::ReturnStatement>());

    auto func = std::make_unique<ast::Function>(func_name, ast::VariableList(),
                                                void_type());
    func->set_body(std::move(body));
    return func;
  }

  /// Gets an appropriate type for the coords parameter depending the the
  /// dimensionality of the texture being sampled.
  /// @param dim dimensionality of the texture being sampled.
  /// @returns a pointer to a type appropriate for the coord param
  ast::type::Type* GetCoordsType(ast::type::TextureDimension dim) {
    if (dim == ast::type::TextureDimension::k1d) {
      f32_type();
    } else if (dim == ast::type::TextureDimension::k1dArray ||
               dim == ast::type::TextureDimension::k2d) {
      return f32_vec_type(2);
    } else if (dim == ast::type::TextureDimension::kCubeArray) {
      return f32_vec_type(4);
    }
    return f32_vec_type(3);
  }

  ast::Module* mod() { return &mod_; }
  TypeDeterminer* td() { return td_.get(); }
  Inspector* inspector() { return inspector_.get(); }

  ast::type::BoolType* bool_type() { return &bool_type_; }
  ast::type::F32Type* f32_type() { return &f32_type_; }
  ast::type::I32Type* i32_type() { return &i32_type_; }
  ast::type::U32Type* u32_type() { return &u32_type_; }
  ast::type::ArrayType* u32_array_type(uint32_t count) {
    if (array_type_memo_.find(count) == array_type_memo_.end()) {
      array_type_memo_[count] =
          std::make_unique<ast::type::ArrayType>(u32_type(), count);
      ast::ArrayDecorationList decos;
      decos.push_back(std::make_unique<ast::StrideDecoration>(4, Source{}));
      array_type_memo_[count]->set_decorations(std::move(decos));
    }
    return array_type_memo_[count].get();
  }
  ast::type::VectorType* f32_vec_type(uint32_t count) {
    if (vector_type_memo_.find(count) == vector_type_memo_.end()) {
      vector_type_memo_[count] =
          std::make_unique<ast::type::VectorType>(u32_type(), count);
    }
    return vector_type_memo_[count].get();
  }
  ast::type::VoidType* void_type() { return &void_type_; }
  ast::type::SamplerType* sampler_type() { return &sampler_type_; }
  ast::type::SamplerType* comparison_sampler_type() {
    return &comparison_sampler_type_;
  }

 private:
  Context ctx_;
  ast::Module mod_;
  std::unique_ptr<TypeDeterminer> td_;
  std::unique_ptr<Inspector> inspector_;

  ast::type::BoolType bool_type_;
  ast::type::F32Type f32_type_;
  ast::type::I32Type i32_type_;
  ast::type::U32Type u32_type_;
  ast::type::VoidType void_type_;
  ast::type::SamplerType sampler_type_;
  ast::type::SamplerType comparison_sampler_type_;
  std::map<uint32_t, std::unique_ptr<ast::type::ArrayType>> array_type_memo_;
  std::map<uint32_t, std::unique_ptr<ast::type::VectorType>> vector_type_memo_;
};

class InspectorGetEntryPointTest : public InspectorHelper,
                                   public testing::Test {};
class InspectorGetRemappedNameForEntryPointTest : public InspectorHelper,
                                                  public testing::Test {};
class InspectorGetConstantIDsTest : public InspectorHelper,
                                    public testing::Test {};
class InspectorGetUniformBufferResourceBindingsTest : public InspectorHelper,
                                                      public testing::Test {};
class InspectorGetStorageBufferResourceBindingsTest : public InspectorHelper,
                                                      public testing::Test {};
class InspectorGetReadOnlyStorageBufferResourceBindingsTest
    : public InspectorHelper,
      public testing::Test {};
class InspectorGetSamplerResourceBindingsTest : public InspectorHelper,
                                                public testing::Test {};
class InspectorGetComparisonSamplerResourceBindingsTest
    : public InspectorHelper,
      public testing::Test {};

class InspectorGetSampledTextureResourceBindingsTest : public InspectorHelper,
                                                       public testing::Test {};
struct GetSampledTextureTestParams {
  ast::type::TextureDimension type_dim;
  inspector::ResourceBinding::TextureDimension inspector_dim;
};
class InspectorGetSampledTextureResourceBindingsTestWithParam
    : public InspectorHelper,
      public testing::TestWithParam<GetSampledTextureTestParams> {};

TEST_F(InspectorGetEntryPointTest, NoFunctions) {
  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  EXPECT_EQ(0u, result.size());
}

TEST_F(InspectorGetEntryPointTest, NoEntryPoints) {
  mod()->AddFunction(MakeEmptyBodyFunction("foo"));

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  EXPECT_EQ(0u, result.size());
}

TEST_F(InspectorGetEntryPointTest, OneEntryPoint) {
  auto foo = MakeEmptyBodyFunction("foo");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("foo", result[0].name);
  EXPECT_EQ("tint_666f6f", result[0].remapped_name);
  EXPECT_EQ(ast::PipelineStage::kVertex, result[0].stage);
}

TEST_F(InspectorGetEntryPointTest, MultipleEntryPoints) {
  auto foo = MakeEmptyBodyFunction("foo");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto bar = MakeEmptyBodyFunction("bar");
  bar->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kCompute, Source{}));
  mod()->AddFunction(std::move(bar));

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("foo", result[0].name);
  EXPECT_EQ("tint_666f6f", result[0].remapped_name);
  EXPECT_EQ(ast::PipelineStage::kVertex, result[0].stage);
  EXPECT_EQ("bar", result[1].name);
  EXPECT_EQ("tint_626172", result[1].remapped_name);
  EXPECT_EQ(ast::PipelineStage::kCompute, result[1].stage);
}

TEST_F(InspectorGetEntryPointTest, MixFunctionsAndEntryPoints) {
  auto func = MakeEmptyBodyFunction("func");
  mod()->AddFunction(std::move(func));

  auto foo = MakeCallerBodyFunction("foo", "func");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto bar = MakeCallerBodyFunction("bar", "func");
  bar->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kFragment, Source{}));
  mod()->AddFunction(std::move(bar));

  auto result = inspector()->GetEntryPoints();
  EXPECT_FALSE(inspector()->has_error());

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("foo", result[0].name);
  EXPECT_EQ("tint_666f6f", result[0].remapped_name);
  EXPECT_EQ(ast::PipelineStage::kVertex, result[0].stage);
  EXPECT_EQ("bar", result[1].name);
  EXPECT_EQ("tint_626172", result[1].remapped_name);
  EXPECT_EQ(ast::PipelineStage::kFragment, result[1].stage);
}

TEST_F(InspectorGetEntryPointTest, DefaultWorkgroupSize) {
  auto foo = MakeCallerBodyFunction("foo", "func");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  uint32_t x, y, z;
  std::tie(x, y, z) = result[0].workgroup_size();
  EXPECT_EQ(1u, x);
  EXPECT_EQ(1u, y);
  EXPECT_EQ(1u, z);
}

TEST_F(InspectorGetEntryPointTest, NonDefaultWorkgroupSize) {
  auto foo = MakeEmptyBodyFunction("foo");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kCompute, Source{}));
  foo->add_decoration(
      std::make_unique<ast::WorkgroupDecoration>(8u, 2u, 1u, Source{}));
  mod()->AddFunction(std::move(foo));

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  uint32_t x, y, z;
  std::tie(x, y, z) = result[0].workgroup_size();
  EXPECT_EQ(8u, x);
  EXPECT_EQ(2u, y);
  EXPECT_EQ(1u, z);
}

TEST_F(InspectorGetEntryPointTest, NoInOutVariables) {
  auto func = MakeEmptyBodyFunction("func");
  mod()->AddFunction(std::move(func));

  auto foo = MakeCallerBodyFunction("foo", "func");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(0u, result[0].input_variables.size());
  EXPECT_EQ(0u, result[0].output_variables.size());
}

TEST_F(InspectorGetEntryPointTest, EntryPointInOutVariables) {
  AddInOutVariables({{"in_var", "out_var"}});

  auto foo = MakeInOutVariableBodyFunction("foo", {{"in_var", "out_var"}});
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());

  ASSERT_EQ(1u, result[0].input_variables.size());
  EXPECT_EQ("in_var", result[0].input_variables[0]);
  ASSERT_EQ(1u, result[0].output_variables.size());
  EXPECT_EQ("out_var", result[0].output_variables[0]);
}

TEST_F(InspectorGetEntryPointTest, FunctionInOutVariables) {
  AddInOutVariables({{"in_var", "out_var"}});

  auto func = MakeInOutVariableBodyFunction("func", {{"in_var", "out_var"}});
  mod()->AddFunction(std::move(func));

  auto foo = MakeCallerBodyFunction("foo", "func");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());

  ASSERT_EQ(1u, result[0].input_variables.size());
  EXPECT_EQ("in_var", result[0].input_variables[0]);
  ASSERT_EQ(1u, result[0].output_variables.size());
  EXPECT_EQ("out_var", result[0].output_variables[0]);
}

TEST_F(InspectorGetEntryPointTest, RepeatedInOutVariables) {
  AddInOutVariables({{"in_var", "out_var"}});

  auto func = MakeInOutVariableBodyFunction("func", {{"in_var", "out_var"}});
  mod()->AddFunction(std::move(func));

  auto foo = MakeInOutVariableCallerBodyFunction("foo", "func",
                                                 {{"in_var", "out_var"}});
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());

  ASSERT_EQ(1u, result[0].input_variables.size());
  EXPECT_EQ("in_var", result[0].input_variables[0]);
  ASSERT_EQ(1u, result[0].output_variables.size());
  EXPECT_EQ("out_var", result[0].output_variables[0]);
}

TEST_F(InspectorGetEntryPointTest, EntryPointMultipleInOutVariables) {
  AddInOutVariables({{"in_var", "out_var"}, {"in2_var", "out2_var"}});

  auto foo = MakeInOutVariableBodyFunction(
      "foo", {{"in_var", "out_var"}, {"in2_var", "out2_var"}});
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());

  ASSERT_EQ(2u, result[0].input_variables.size());
  EXPECT_TRUE(ContainsString(result[0].input_variables, "in_var"));
  EXPECT_TRUE(ContainsString(result[0].input_variables, "in2_var"));
  ASSERT_EQ(2u, result[0].output_variables.size());
  EXPECT_TRUE(ContainsString(result[0].output_variables, "out_var"));
  EXPECT_TRUE(ContainsString(result[0].output_variables, "out2_var"));
}

TEST_F(InspectorGetEntryPointTest, FunctionMultipleInOutVariables) {
  AddInOutVariables({{"in_var", "out_var"}, {"in2_var", "out2_var"}});

  auto func = MakeInOutVariableBodyFunction(
      "func", {{"in_var", "out_var"}, {"in2_var", "out2_var"}});
  mod()->AddFunction(std::move(func));

  auto foo = MakeCallerBodyFunction("foo", "func");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());

  ASSERT_EQ(2u, result[0].input_variables.size());
  EXPECT_TRUE(ContainsString(result[0].input_variables, "in_var"));
  EXPECT_TRUE(ContainsString(result[0].input_variables, "in2_var"));
  ASSERT_EQ(2u, result[0].output_variables.size());
  EXPECT_TRUE(ContainsString(result[0].output_variables, "out_var"));
  EXPECT_TRUE(ContainsString(result[0].output_variables, "out2_var"));
}

TEST_F(InspectorGetEntryPointTest, MultipleEntryPointsInOutVariables) {
  AddInOutVariables({{"in_var", "out_var"}, {"in2_var", "out2_var"}});

  auto foo = MakeInOutVariableBodyFunction("foo", {{"in_var", "out2_var"}});
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto bar = MakeInOutVariableBodyFunction("bar", {{"in2_var", "out_var"}});
  bar->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kCompute, Source{}));
  mod()->AddFunction(std::move(bar));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(2u, result.size());

  ASSERT_EQ("foo", result[0].name);
  ASSERT_EQ("tint_666f6f", result[0].remapped_name);
  ASSERT_EQ(1u, result[0].input_variables.size());
  EXPECT_EQ("in_var", result[0].input_variables[0]);
  ASSERT_EQ(1u, result[0].output_variables.size());
  EXPECT_EQ("out2_var", result[0].output_variables[0]);

  ASSERT_EQ("bar", result[1].name);
  ASSERT_EQ("tint_626172", result[1].remapped_name);
  ASSERT_EQ(1u, result[1].input_variables.size());
  EXPECT_EQ("in2_var", result[1].input_variables[0]);
  ASSERT_EQ(1u, result[1].output_variables.size());
  EXPECT_EQ("out_var", result[1].output_variables[0]);
}

TEST_F(InspectorGetEntryPointTest, MultipleEntryPointsSharedInOutVariables) {
  AddInOutVariables({{"in_var", "out_var"}, {"in2_var", "out2_var"}});

  auto func = MakeInOutVariableBodyFunction("func", {{"in2_var", "out2_var"}});
  mod()->AddFunction(std::move(func));

  auto foo = MakeInOutVariableCallerBodyFunction("foo", "func",
                                                 {{"in_var", "out_var"}});
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto bar = MakeCallerBodyFunction("bar", "func");
  bar->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kCompute, Source{}));
  mod()->AddFunction(std::move(bar));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetEntryPoints();
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(2u, result.size());

  ASSERT_EQ("foo", result[0].name);
  ASSERT_EQ("tint_666f6f", result[0].remapped_name);
  EXPECT_EQ(2u, result[0].input_variables.size());
  EXPECT_TRUE(ContainsString(result[0].input_variables, "in_var"));
  EXPECT_TRUE(ContainsString(result[0].input_variables, "in2_var"));
  EXPECT_EQ(2u, result[0].output_variables.size());
  EXPECT_TRUE(ContainsString(result[0].output_variables, "out_var"));
  EXPECT_TRUE(ContainsString(result[0].output_variables, "out2_var"));

  ASSERT_EQ("bar", result[1].name);
  ASSERT_EQ("tint_626172", result[1].remapped_name);
  EXPECT_EQ(1u, result[1].input_variables.size());
  EXPECT_EQ("in2_var", result[1].input_variables[0]);
  EXPECT_EQ(1u, result[1].output_variables.size());
  EXPECT_EQ("out2_var", result[1].output_variables[0]);
}

// TODO(rharrison): Reenable once GetRemappedNameForEntryPoint isn't a pass
// through
TEST_F(InspectorGetRemappedNameForEntryPointTest, DISABLED_NoFunctions) {
  auto result = inspector()->GetRemappedNameForEntryPoint("foo");
  ASSERT_TRUE(inspector()->has_error());

  EXPECT_EQ("", result);
}

// TODO(rharrison): Reenable once GetRemappedNameForEntryPoint isn't a pass
// through
TEST_F(InspectorGetRemappedNameForEntryPointTest, DISABLED_NoEntryPoints) {
  mod()->AddFunction(MakeEmptyBodyFunction("foo"));

  auto result = inspector()->GetRemappedNameForEntryPoint("foo");
  ASSERT_TRUE(inspector()->has_error());

  EXPECT_EQ("", result);
}

// TODO(rharrison): Reenable once GetRemappedNameForEntryPoint isn't a pass
// through
TEST_F(InspectorGetRemappedNameForEntryPointTest, DISABLED_OneEntryPoint) {
  auto foo = MakeEmptyBodyFunction("foo");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto result = inspector()->GetRemappedNameForEntryPoint("foo");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  EXPECT_EQ("tint_666f6f", result);
}

// TODO(rharrison): Reenable once GetRemappedNameForEntryPoint isn't a pass
// through
TEST_F(InspectorGetRemappedNameForEntryPointTest,
       DISABLED_MultipleEntryPoints) {
  auto foo = MakeEmptyBodyFunction("foo");
  foo->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(foo));

  auto bar = MakeEmptyBodyFunction("bar");
  bar->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kCompute, Source{}));
  mod()->AddFunction(std::move(bar));

  {
    auto result = inspector()->GetRemappedNameForEntryPoint("foo");
    ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
    EXPECT_EQ("tint_666f6f", result);
  }
  {
    auto result = inspector()->GetRemappedNameForEntryPoint("bar");
    ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
    EXPECT_EQ("tint_626172", result);
  }
}

TEST_F(InspectorGetConstantIDsTest, Bool) {
  bool val_true = true;
  bool val_false = false;
  AddConstantID<bool>("foo", 1, bool_type(), nullptr);
  AddConstantID<bool>("bar", 20, bool_type(), &val_true);
  AddConstantID<bool>("baz", 300, bool_type(), &val_false);

  auto result = inspector()->GetConstantIDs();
  ASSERT_EQ(3u, result.size());

  ASSERT_TRUE(result.find(1) != result.end());
  EXPECT_TRUE(result[1].IsNull());

  ASSERT_TRUE(result.find(20) != result.end());
  EXPECT_TRUE(result[20].IsBool());
  EXPECT_TRUE(result[20].AsBool());

  ASSERT_TRUE(result.find(300) != result.end());
  EXPECT_TRUE(result[300].IsBool());
  EXPECT_FALSE(result[300].AsBool());
}

TEST_F(InspectorGetConstantIDsTest, U32) {
  uint32_t val = 42;
  AddConstantID<uint32_t>("foo", 1, u32_type(), nullptr);
  AddConstantID<uint32_t>("bar", 20, u32_type(), &val);

  auto result = inspector()->GetConstantIDs();
  ASSERT_EQ(2u, result.size());

  ASSERT_TRUE(result.find(1) != result.end());
  EXPECT_TRUE(result[1].IsNull());

  ASSERT_TRUE(result.find(20) != result.end());
  EXPECT_TRUE(result[20].IsU32());
  EXPECT_EQ(42u, result[20].AsU32());
}

TEST_F(InspectorGetConstantIDsTest, I32) {
  int32_t val_neg = -42;
  int32_t val_pos = 42;
  AddConstantID<int32_t>("foo", 1, i32_type(), nullptr);
  AddConstantID<int32_t>("bar", 20, i32_type(), &val_neg);
  AddConstantID<int32_t>("baz", 300, i32_type(), &val_pos);

  auto result = inspector()->GetConstantIDs();
  ASSERT_EQ(3u, result.size());

  ASSERT_TRUE(result.find(1) != result.end());
  EXPECT_TRUE(result[1].IsNull());

  ASSERT_TRUE(result.find(20) != result.end());
  EXPECT_TRUE(result[20].IsI32());
  EXPECT_EQ(-42, result[20].AsI32());

  ASSERT_TRUE(result.find(300) != result.end());
  EXPECT_TRUE(result[300].IsI32());
  EXPECT_EQ(42, result[300].AsI32());
}

TEST_F(InspectorGetConstantIDsTest, Float) {
  float val_zero = 0.0f;
  float val_neg = -10.0f;
  float val_pos = 15.0f;
  AddConstantID<float>("foo", 1, f32_type(), nullptr);
  AddConstantID<float>("bar", 20, f32_type(), &val_zero);
  AddConstantID<float>("baz", 300, f32_type(), &val_neg);
  AddConstantID<float>("x", 4000, f32_type(), &val_pos);

  auto result = inspector()->GetConstantIDs();
  ASSERT_EQ(4u, result.size());

  ASSERT_TRUE(result.find(1) != result.end());
  EXPECT_TRUE(result[1].IsNull());

  ASSERT_TRUE(result.find(20) != result.end());
  EXPECT_TRUE(result[20].IsFloat());
  EXPECT_FLOAT_EQ(0.0, result[20].AsFloat());

  ASSERT_TRUE(result.find(300) != result.end());
  EXPECT_TRUE(result[300].IsFloat());
  EXPECT_FLOAT_EQ(-10.0, result[300].AsFloat());

  ASSERT_TRUE(result.find(4000) != result.end());
  EXPECT_TRUE(result[4000].IsFloat());
  EXPECT_FLOAT_EQ(15.0, result[4000].AsFloat());
}

TEST_F(InspectorGetUniformBufferResourceBindingsTest, MissingEntryPoint) {
  auto result = inspector()->GetUniformBufferResourceBindings("ep_func");
  ASSERT_TRUE(inspector()->has_error());
  std::string error = inspector()->error();
  EXPECT_TRUE(error.find("not found") != std::string::npos);
}

TEST_F(InspectorGetUniformBufferResourceBindingsTest, NonEntryPointFunc) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) =
      MakeUniformBufferTypes("foo_type", {{i32_type(), 0}});
  AddUniformBuffer("foo_ub", foo_control_type.get(), 0, 0);

  auto ub_func = MakeStructVariableReferenceBodyFunction("ub_func", "foo_ub",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(ub_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "ub_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetUniformBufferResourceBindings("ub_func");
  std::string error = inspector()->error();
  EXPECT_TRUE(error.find("not an entry point") != std::string::npos);
}

TEST_F(InspectorGetUniformBufferResourceBindingsTest, MissingBlockDeco) {
  ast::StructMemberList members;
  ast::StructMemberDecorationList deco;
  deco.push_back(
      std::make_unique<ast::StructMemberOffsetDecoration>(0, Source{}));

  members.push_back(std::make_unique<ast::StructMember>(
      StructMemberName(members.size(), i32_type()), i32_type(),
      std::move(deco)));

  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  auto foo_type =
      std::make_unique<ast::type::StructType>("foo_type", std::move(str));

  AddUniformBuffer("foo_ub", foo_type.get(), 0, 0);

  auto ub_func = MakeStructVariableReferenceBodyFunction("ub_func", "foo_ub",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(ub_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "ub_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetUniformBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  EXPECT_EQ(0u, result.size());
}

TEST_F(InspectorGetUniformBufferResourceBindingsTest, Simple) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) =
      MakeUniformBufferTypes("foo_type", {{i32_type(), 0}});
  AddUniformBuffer("foo_ub", foo_control_type.get(), 0, 0);

  auto ub_func = MakeStructVariableReferenceBodyFunction("ub_func", "foo_ub",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(ub_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "ub_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetUniformBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(16u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetUniformBufferResourceBindingsTest, MultipleMembers) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) = MakeUniformBufferTypes(
      "foo_type", {{i32_type(), 0}, {u32_type(), 4}, {f32_type(), 8}});
  AddUniformBuffer("foo_ub", foo_control_type.get(), 0, 0);

  auto ub_func = MakeStructVariableReferenceBodyFunction(
      "ub_func", "foo_ub", {{0, i32_type()}, {1, u32_type()}, {2, f32_type()}});
  mod()->AddFunction(std::move(ub_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "ub_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetUniformBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(16u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetUniformBufferResourceBindingsTest, MultipleUniformBuffers) {
  std::unique_ptr<ast::type::StructType> ub_struct_type;
  std::unique_ptr<ast::type::AccessControlType> ub_control_type;
  std::tie(ub_struct_type, ub_control_type) = MakeUniformBufferTypes(
      "ub_type", {{i32_type(), 0}, {u32_type(), 4}, {f32_type(), 8}});
  AddUniformBuffer("ub_foo", ub_control_type.get(), 0, 0);
  AddUniformBuffer("ub_bar", ub_control_type.get(), 0, 1);
  AddUniformBuffer("ub_baz", ub_control_type.get(), 2, 0);

  auto AddReferenceFunc = [this](const std::string& func_name,
                                 const std::string& var_name) {
    auto ub_func = MakeStructVariableReferenceBodyFunction(
        func_name, var_name,
        {{0, i32_type()}, {1, u32_type()}, {2, f32_type()}});
    mod()->AddFunction(std::move(ub_func));
  };
  AddReferenceFunc("ub_foo_func", "ub_foo");
  AddReferenceFunc("ub_bar_func", "ub_bar");
  AddReferenceFunc("ub_baz_func", "ub_baz");

  auto AddFuncCall = [](ast::BlockStatement* body, const std::string& callee) {
    auto ident_expr = std::make_unique<ast::IdentifierExpression>(callee);
    auto call_expr = std::make_unique<ast::CallExpression>(
        std::move(ident_expr), ast::ExpressionList());
    body->append(std::make_unique<ast::CallStatement>(std::move(call_expr)));
  };
  auto body = std::make_unique<ast::BlockStatement>();

  AddFuncCall(body.get(), "ub_foo_func");
  AddFuncCall(body.get(), "ub_bar_func");
  AddFuncCall(body.get(), "ub_baz_func");

  body->append(std::make_unique<ast::ReturnStatement>());
  std::unique_ptr<ast::Function> func = std::make_unique<ast::Function>(
      "ep_func", ast::VariableList(), void_type());
  func->set_body(std::move(body));

  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetUniformBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(3u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(16u, result[0].min_buffer_binding_size);

  EXPECT_EQ(0u, result[1].bind_group);
  EXPECT_EQ(1u, result[1].binding);
  EXPECT_EQ(16u, result[1].min_buffer_binding_size);

  EXPECT_EQ(2u, result[2].bind_group);
  EXPECT_EQ(0u, result[2].binding);
  EXPECT_EQ(16u, result[2].min_buffer_binding_size);
}

TEST_F(InspectorGetUniformBufferResourceBindingsTest, ContainingArray) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) = MakeUniformBufferTypes(
      "foo_type", {{i32_type(), 0}, {u32_array_type(4), 4}});
  AddUniformBuffer("foo_ub", foo_control_type.get(), 0, 0);

  auto ub_func = MakeStructVariableReferenceBodyFunction("ub_func", "foo_ub",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(ub_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "ub_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetUniformBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(32u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetStorageBufferResourceBindingsTest, Simple) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) =
      MakeStorageBufferTypes("foo_type", {{i32_type(), 0}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction("sb_func", "foo_sb",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(4u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetStorageBufferResourceBindingsTest, MultipleMembers) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) = MakeStorageBufferTypes(
      "foo_type", {{i32_type(), 0}, {u32_type(), 4}, {f32_type(), 8}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction(
      "sb_func", "foo_sb", {{0, i32_type()}, {1, u32_type()}, {2, f32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(12u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetStorageBufferResourceBindingsTest, MultipleStorageBuffers) {
  std::unique_ptr<ast::type::StructType> sb_struct_type;
  std::unique_ptr<ast::type::AccessControlType> sb_control_type;
  std::tie(sb_struct_type, sb_control_type) = MakeStorageBufferTypes(
      "sb_type", {{i32_type(), 0}, {u32_type(), 4}, {f32_type(), 8}});
  AddStorageBuffer("sb_foo", sb_control_type.get(), 0, 0);
  AddStorageBuffer("sb_bar", sb_control_type.get(), 0, 1);
  AddStorageBuffer("sb_baz", sb_control_type.get(), 2, 0);

  auto AddReferenceFunc = [this](const std::string& func_name,
                                 const std::string& var_name) {
    auto sb_func = MakeStructVariableReferenceBodyFunction(
        func_name, var_name,
        {{0, i32_type()}, {1, u32_type()}, {2, f32_type()}});
    mod()->AddFunction(std::move(sb_func));
  };
  AddReferenceFunc("sb_foo_func", "sb_foo");
  AddReferenceFunc("sb_bar_func", "sb_bar");
  AddReferenceFunc("sb_baz_func", "sb_baz");

  auto AddFuncCall = [](ast::BlockStatement* body, const std::string& callee) {
    auto ident_expr = std::make_unique<ast::IdentifierExpression>(callee);
    auto call_expr = std::make_unique<ast::CallExpression>(
        std::move(ident_expr), ast::ExpressionList());
    body->append(std::make_unique<ast::CallStatement>(std::move(call_expr)));
  };
  auto body = std::make_unique<ast::BlockStatement>();

  AddFuncCall(body.get(), "sb_foo_func");
  AddFuncCall(body.get(), "sb_bar_func");
  AddFuncCall(body.get(), "sb_baz_func");

  body->append(std::make_unique<ast::ReturnStatement>());
  std::unique_ptr<ast::Function> func = std::make_unique<ast::Function>(
      "ep_func", ast::VariableList(), void_type());
  func->set_body(std::move(body));

  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(3u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(12u, result[0].min_buffer_binding_size);

  EXPECT_EQ(0u, result[1].bind_group);
  EXPECT_EQ(1u, result[1].binding);
  EXPECT_EQ(12u, result[1].min_buffer_binding_size);

  EXPECT_EQ(2u, result[2].bind_group);
  EXPECT_EQ(0u, result[2].binding);
  EXPECT_EQ(12u, result[2].min_buffer_binding_size);
}

TEST_F(InspectorGetStorageBufferResourceBindingsTest, ContainingArray) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) = MakeStorageBufferTypes(
      "foo_type", {{i32_type(), 0}, {u32_array_type(4), 4}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction("sb_func", "foo_sb",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(20u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetStorageBufferResourceBindingsTest, ContainingRuntimeArray) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) = MakeStorageBufferTypes(
      "foo_type", {{i32_type(), 0}, {u32_array_type(0), 4}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction("sb_func", "foo_sb",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(8u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetStorageBufferResourceBindingsTest, SkipReadOnly) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) =
      MakeReadOnlyStorageBufferTypes("foo_type", {{i32_type(), 0}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction("sb_func", "foo_sb",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(0u, result.size());
}

TEST_F(InspectorGetReadOnlyStorageBufferResourceBindingsTest, Simple) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) =
      MakeReadOnlyStorageBufferTypes("foo_type", {{i32_type(), 0}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction("sb_func", "foo_sb",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result =
      inspector()->GetReadOnlyStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(4u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetReadOnlyStorageBufferResourceBindingsTest,
       MultipleStorageBuffers) {
  std::unique_ptr<ast::type::StructType> sb_struct_type;
  std::unique_ptr<ast::type::AccessControlType> sb_control_type;
  std::tie(sb_struct_type, sb_control_type) = MakeReadOnlyStorageBufferTypes(
      "sb_type", {{i32_type(), 0}, {u32_type(), 4}, {f32_type(), 8}});
  AddStorageBuffer("sb_foo", sb_control_type.get(), 0, 0);
  AddStorageBuffer("sb_bar", sb_control_type.get(), 0, 1);
  AddStorageBuffer("sb_baz", sb_control_type.get(), 2, 0);

  auto AddReferenceFunc = [this](const std::string& func_name,
                                 const std::string& var_name) {
    auto sb_func = MakeStructVariableReferenceBodyFunction(
        func_name, var_name,
        {{0, i32_type()}, {1, u32_type()}, {2, f32_type()}});
    mod()->AddFunction(std::move(sb_func));
  };
  AddReferenceFunc("sb_foo_func", "sb_foo");
  AddReferenceFunc("sb_bar_func", "sb_bar");
  AddReferenceFunc("sb_baz_func", "sb_baz");

  auto AddFuncCall = [](ast::BlockStatement* body, const std::string& callee) {
    auto ident_expr = std::make_unique<ast::IdentifierExpression>(callee);
    auto call_expr = std::make_unique<ast::CallExpression>(
        std::move(ident_expr), ast::ExpressionList());
    body->append(std::make_unique<ast::CallStatement>(std::move(call_expr)));
  };
  auto body = std::make_unique<ast::BlockStatement>();

  AddFuncCall(body.get(), "sb_foo_func");
  AddFuncCall(body.get(), "sb_bar_func");
  AddFuncCall(body.get(), "sb_baz_func");

  body->append(std::make_unique<ast::ReturnStatement>());
  std::unique_ptr<ast::Function> func = std::make_unique<ast::Function>(
      "ep_func", ast::VariableList(), void_type());
  func->set_body(std::move(body));

  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result =
      inspector()->GetReadOnlyStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(3u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(12u, result[0].min_buffer_binding_size);

  EXPECT_EQ(0u, result[1].bind_group);
  EXPECT_EQ(1u, result[1].binding);
  EXPECT_EQ(12u, result[1].min_buffer_binding_size);

  EXPECT_EQ(2u, result[2].bind_group);
  EXPECT_EQ(0u, result[2].binding);
  EXPECT_EQ(12u, result[2].min_buffer_binding_size);
}

TEST_F(InspectorGetReadOnlyStorageBufferResourceBindingsTest, ContainingArray) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) = MakeReadOnlyStorageBufferTypes(
      "foo_type", {{i32_type(), 0}, {u32_array_type(4), 4}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction("sb_func", "foo_sb",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result =
      inspector()->GetReadOnlyStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(20u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetReadOnlyStorageBufferResourceBindingsTest,
       ContainingRuntimeArray) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) = MakeReadOnlyStorageBufferTypes(
      "foo_type", {{i32_type(), 0}, {u32_array_type(0), 4}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction("sb_func", "foo_sb",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result =
      inspector()->GetReadOnlyStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(8u, result[0].min_buffer_binding_size);
}

TEST_F(InspectorGetReadOnlyStorageBufferResourceBindingsTest, SkipNonReadOnly) {
  std::unique_ptr<ast::type::StructType> foo_struct_type;
  std::unique_ptr<ast::type::AccessControlType> foo_control_type;
  std::tie(foo_struct_type, foo_control_type) =
      MakeStorageBufferTypes("foo_type", {{i32_type(), 0}});
  AddStorageBuffer("foo_sb", foo_control_type.get(), 0, 0);

  auto sb_func = MakeStructVariableReferenceBodyFunction("sb_func", "foo_sb",
                                                         {{0, i32_type()}});
  mod()->AddFunction(std::move(sb_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "sb_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result =
      inspector()->GetReadOnlyStorageBufferResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();
  ASSERT_EQ(0u, result.size());
}

TEST_F(InspectorGetSamplerResourceBindingsTest, Simple) {
  auto sampled_texture_type =
      MakeSampledTextureType(ast::type::TextureDimension::k1d, f32_type());
  AddSampledTexture("foo_texture", sampled_texture_type.get(), 0, 0);
  AddSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");

  auto func = MakeSamplerReferenceBodyFunction("ep", "foo_texture",
                                               "foo_sampler", "foo_coords");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetSamplerResourceBindings("ep");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(1u, result[0].binding);
}

TEST_F(InspectorGetSamplerResourceBindingsTest, NoSampler) {
  auto func = MakeEmptyBodyFunction("ep_func");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetSamplerResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(0u, result.size());
}

TEST_F(InspectorGetSamplerResourceBindingsTest, InFunction) {
  auto sampled_texture_type =
      MakeSampledTextureType(ast::type::TextureDimension::k1d, f32_type());
  AddSampledTexture("foo_texture", sampled_texture_type.get(), 0, 0);
  AddSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");

  auto foo_func = MakeSamplerReferenceBodyFunction("foo_func", "foo_texture",
                                                   "foo_sampler", "foo_coords");
  mod()->AddFunction(std::move(foo_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "foo_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetSamplerResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(1u, result[0].binding);
}

TEST_F(InspectorGetSamplerResourceBindingsTest, UnknownEntryPoint) {
  auto sampled_texture_type =
      MakeSampledTextureType(ast::type::TextureDimension::k1d, f32_type());
  AddSampledTexture("foo_texture", sampled_texture_type.get(), 0, 0);
  AddSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");

  auto func = MakeSamplerReferenceBodyFunction("ep", "foo_texture",
                                               "foo_sampler", "foo_coords");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetSamplerResourceBindings("foo");
  ASSERT_TRUE(inspector()->has_error()) << inspector()->error();
}

TEST_F(InspectorGetSamplerResourceBindingsTest, SkipsComparisonSamplers) {
  auto depth_texture_type =
      MakeDepthTextureType(ast::type::TextureDimension::k2d);
  AddDepthTexture("foo_texture", depth_texture_type.get());
  AddComparisonSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");
  AddF32("foo_depth");

  auto func = MakeComparisonSamplerReferenceBodyFunction(
      "ep", "foo_texture", "foo_sampler", "foo_coords", "foo_depth");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetSamplerResourceBindings("ep");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(0u, result.size());
}

TEST_F(InspectorGetComparisonSamplerResourceBindingsTest, Simple) {
  auto depth_texture_type =
      MakeDepthTextureType(ast::type::TextureDimension::k2d);
  AddDepthTexture("foo_texture", depth_texture_type.get());
  AddComparisonSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");
  AddF32("foo_depth");

  auto func = MakeComparisonSamplerReferenceBodyFunction(
      "ep", "foo_texture", "foo_sampler", "foo_coords", "foo_depth");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetComparisonSamplerResourceBindings("ep");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(1u, result[0].binding);
}

TEST_F(InspectorGetComparisonSamplerResourceBindingsTest, NoSampler) {
  auto func = MakeEmptyBodyFunction("ep_func");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetComparisonSamplerResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(0u, result.size());
}

TEST_F(InspectorGetComparisonSamplerResourceBindingsTest, InFunction) {
  auto depth_texture_type =
      MakeDepthTextureType(ast::type::TextureDimension::k2d);
  AddDepthTexture("foo_texture", depth_texture_type.get());
  AddComparisonSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");
  AddF32("foo_depth");

  auto foo_func = MakeComparisonSamplerReferenceBodyFunction(
      "foo_func", "foo_texture", "foo_sampler", "foo_coords", "foo_depth");
  mod()->AddFunction(std::move(foo_func));

  auto ep_func = MakeCallerBodyFunction("ep_func", "foo_func");
  ep_func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(ep_func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetComparisonSamplerResourceBindings("ep_func");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(1u, result[0].binding);
}

TEST_F(InspectorGetComparisonSamplerResourceBindingsTest, UnknownEntryPoint) {
  auto depth_texture_type =
      MakeDepthTextureType(ast::type::TextureDimension::k2d);
  AddDepthTexture("foo_texture", depth_texture_type.get());
  AddComparisonSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");
  AddF32("foo_depth");

  auto func = MakeComparisonSamplerReferenceBodyFunction(
      "ep", "foo_texture", "foo_sampler", "foo_coords", "foo_depth");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetSamplerResourceBindings("foo");
  ASSERT_TRUE(inspector()->has_error()) << inspector()->error();
}

TEST_F(InspectorGetComparisonSamplerResourceBindingsTest, SkipsSamplers) {
  auto sampled_texture_type =
      MakeSampledTextureType(ast::type::TextureDimension::k1d, f32_type());
  AddSampledTexture("foo_texture", sampled_texture_type.get(), 0, 0);
  AddSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");

  auto func = MakeSamplerReferenceBodyFunction("ep", "foo_texture",
                                               "foo_sampler", "foo_coords");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetComparisonSamplerResourceBindings("ep");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(0u, result.size());
}

TEST_P(InspectorGetSampledTextureResourceBindingsTestWithParam, Simple) {
  auto* coord_type = GetCoordsType(GetParam().type_dim);
  auto sampled_texture_type =
      MakeSampledTextureType(GetParam().type_dim, coord_type);
  AddSampledTexture("foo_texture", sampled_texture_type.get(), 0, 0);
  AddSampler("foo_sampler", 0, 1);
  AddF32("foo_coords");

  auto func = MakeSamplerReferenceBodyFunction("ep", "foo_texture",
                                               "foo_sampler", "foo_coords");
  func->add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kVertex, Source{}));
  mod()->AddFunction(std::move(func));

  ASSERT_TRUE(td()->Determine()) << td()->error();

  auto result = inspector()->GetSampledTextureResourceBindings("ep");
  ASSERT_FALSE(inspector()->has_error()) << inspector()->error();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(0u, result[0].bind_group);
  EXPECT_EQ(0u, result[0].binding);
  EXPECT_EQ(GetParam().inspector_dim, result[0].dim);
}

INSTANTIATE_TEST_SUITE_P(
    InspectorGetSampledTextureResourceBindingsTest,
    InspectorGetSampledTextureResourceBindingsTestWithParam,
    testing::Values(
        GetSampledTextureTestParams{
            ast::type::TextureDimension::k1d,
            inspector::ResourceBinding::TextureDimension::k1d},
        GetSampledTextureTestParams{
            ast::type::TextureDimension::k1dArray,
            inspector::ResourceBinding::TextureDimension::k1dArray},
        GetSampledTextureTestParams{
            ast::type::TextureDimension::k2d,
            inspector::ResourceBinding::TextureDimension::k2d},
        GetSampledTextureTestParams{
            ast::type::TextureDimension::k2dArray,
            inspector::ResourceBinding::TextureDimension::k2dArray},
        GetSampledTextureTestParams{
            ast::type::TextureDimension::k3d,
            inspector::ResourceBinding::TextureDimension::k3d},
        GetSampledTextureTestParams{
            ast::type::TextureDimension::kCube,
            inspector::ResourceBinding::TextureDimension::kCube},
        GetSampledTextureTestParams{
            ast::type::TextureDimension::kCubeArray,
            inspector::ResourceBinding::TextureDimension::kCubeArray}));

}  // namespace
}  // namespace inspector
}  // namespace tint
