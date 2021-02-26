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

#include "src/ast/function.h"

#include <sstream>

#include "src/ast/decorated_variable.h"
#include "src/ast/stage_decoration.h"
#include "src/ast/type/texture_type.h"
#include "src/ast/workgroup_decoration.h"

namespace tint {
namespace ast {

Function::Function() = default;

Function::Function(const std::string& name,
                   VariableList params,
                   type::Type* return_type)
    : Node(),
      name_(name),
      params_(std::move(params)),
      return_type_(return_type),
      body_(std::make_unique<BlockStatement>()) {}

Function::Function(const Source& source,
                   const std::string& name,
                   VariableList params,
                   type::Type* return_type)
    : Node(source),
      name_(name),
      params_(std::move(params)),
      return_type_(return_type),
      body_(std::make_unique<BlockStatement>()) {}

Function::Function(Function&&) = default;

Function::~Function() = default;

std::tuple<uint32_t, uint32_t, uint32_t> Function::workgroup_size() const {
  for (const auto& deco : decorations_) {
    if (deco->IsWorkgroup()) {
      return deco->AsWorkgroup()->values();
    }
  }
  return {1, 1, 1};
}

ast::PipelineStage Function::pipeline_stage() const {
  for (const auto& deco : decorations_) {
    if (deco->IsStage()) {
      return deco->AsStage()->value();
    }
  }
  return ast::PipelineStage::kNone;
}

void Function::add_referenced_module_variable(Variable* var) {
  for (const auto* v : referenced_module_vars_) {
    if (v->name() == var->name()) {
      return;
    }
  }
  referenced_module_vars_.push_back(var);
}

const std::vector<std::pair<Variable*, LocationDecoration*>>
Function::referenced_location_variables() const {
  std::vector<std::pair<Variable*, LocationDecoration*>> ret;

  for (auto* var : referenced_module_variables()) {
    if (!var->IsDecorated()) {
      continue;
    }
    for (auto& deco : var->AsDecorated()->decorations()) {
      if (deco->IsLocation()) {
        ret.push_back({var, deco.get()->AsLocation()});
        break;
      }
    }
  }
  return ret;
}

const std::vector<std::pair<Variable*, Function::BindingInfo>>
Function::referenced_uniform_variables() const {
  std::vector<std::pair<Variable*, Function::BindingInfo>> ret;

  for (auto* var : referenced_module_variables()) {
    if (!var->IsDecorated() ||
        var->storage_class() != ast::StorageClass::kUniform) {
      continue;
    }

    BindingDecoration* binding = nullptr;
    SetDecoration* set = nullptr;
    for (const auto& deco : var->AsDecorated()->decorations()) {
      if (deco->IsBinding()) {
        binding = deco->AsBinding();
      } else if (deco->IsSet()) {
        set = deco->AsSet();
      }
    }
    if (binding == nullptr || set == nullptr) {
      continue;
    }

    ret.push_back({var, BindingInfo{binding, set}});
  }
  return ret;
}

const std::vector<std::pair<Variable*, Function::BindingInfo>>
Function::referenced_storagebuffer_variables() const {
  std::vector<std::pair<Variable*, Function::BindingInfo>> ret;

  for (auto* var : referenced_module_variables()) {
    if (!var->IsDecorated() ||
        var->storage_class() != ast::StorageClass::kStorageBuffer) {
      continue;
    }

    BindingDecoration* binding = nullptr;
    SetDecoration* set = nullptr;
    for (const auto& deco : var->AsDecorated()->decorations()) {
      if (deco->IsBinding()) {
        binding = deco->AsBinding();
      } else if (deco->IsSet()) {
        set = deco->AsSet();
      }
    }
    if (binding == nullptr || set == nullptr) {
      continue;
    }

    ret.push_back({var, BindingInfo{binding, set}});
  }
  return ret;
}

const std::vector<std::pair<Variable*, BuiltinDecoration*>>
Function::referenced_builtin_variables() const {
  std::vector<std::pair<Variable*, BuiltinDecoration*>> ret;

  for (auto* var : referenced_module_variables()) {
    if (!var->IsDecorated()) {
      continue;
    }
    for (auto& deco : var->AsDecorated()->decorations()) {
      if (deco->IsBuiltin()) {
        ret.push_back({var, deco.get()->AsBuiltin()});
        break;
      }
    }
  }
  return ret;
}

const std::vector<std::pair<Variable*, Function::BindingInfo>>
Function::referenced_sampler_variables() const {
  return ReferencedSamplerVariablesImpl(type::SamplerKind::kSampler);
}

const std::vector<std::pair<Variable*, Function::BindingInfo>>
Function::referenced_comparison_sampler_variables() const {
  return ReferencedSamplerVariablesImpl(type::SamplerKind::kComparisonSampler);
}

const std::vector<std::pair<Variable*, Function::BindingInfo>>
Function::referenced_sampled_texture_variables() const {
  std::vector<std::pair<Variable*, Function::BindingInfo>> ret;

  for (auto* var : referenced_module_variables()) {
    auto* unwrapped_type = var->type()->UnwrapIfNeeded();
    if (!var->IsDecorated() || !unwrapped_type->IsTexture() ||
        !unwrapped_type->AsTexture()->IsSampled()) {
      continue;
    }

    BindingDecoration* binding = nullptr;
    SetDecoration* set = nullptr;
    for (const auto& deco : var->AsDecorated()->decorations()) {
      if (deco->IsBinding()) {
        binding = deco->AsBinding();
      } else if (deco->IsSet()) {
        set = deco->AsSet();
      }
    }
    if (binding == nullptr || set == nullptr) {
      continue;
    }

    ret.push_back({var, BindingInfo{binding, set}});
  }

  return ret;
}

void Function::add_ancestor_entry_point(const std::string& ep) {
  for (const auto& point : ancestor_entry_points_) {
    if (point == ep) {
      return;
    }
  }
  ancestor_entry_points_.push_back(ep);
}

bool Function::HasAncestorEntryPoint(const std::string& name) const {
  for (const auto& point : ancestor_entry_points_) {
    if (point == name) {
      return true;
    }
  }
  return false;
}

const Statement* Function::get_last_statement() const {
  return body_->last();
}

bool Function::IsValid() const {
  for (const auto& param : params_) {
    if (param == nullptr || !param->IsValid())
      return false;
  }
  if (body_ == nullptr || !body_->IsValid()) {
    return false;
  }
  if (name_.length() == 0) {
    return false;
  }
  if (return_type_ == nullptr) {
    return false;
  }
  return true;
}

void Function::to_str(std::ostream& out, size_t indent) const {
  make_indent(out, indent);
  out << "Function " << name_ << " -> " << return_type_->type_name()
      << std::endl;

  for (const auto& deco : decorations()) {
    make_indent(out, indent);
    deco->to_str(out);
  }

  make_indent(out, indent);
  out << "(";

  if (params_.size() > 0) {
    out << std::endl;

    for (const auto& param : params_)
      param->to_str(out, indent + 2);

    make_indent(out, indent);
  }
  out << ")" << std::endl;

  make_indent(out, indent);
  out << "{" << std::endl;

  if (body_ != nullptr) {
    for (const auto& stmt : *body_) {
      stmt->to_str(out, indent + 2);
    }
  }

  make_indent(out, indent);
  out << "}" << std::endl;
}

std::string Function::type_name() const {
  std::ostringstream out;

  out << "__func" + return_type_->type_name();
  for (const auto& param : params_) {
    out << param->type()->type_name();
  }

  return out.str();
}

const std::vector<std::pair<Variable*, Function::BindingInfo>>
Function::ReferencedSamplerVariablesImpl(type::SamplerKind kind) const {
  std::vector<std::pair<Variable*, Function::BindingInfo>> ret;

  for (auto* var : referenced_module_variables()) {
    auto* unwrapped_type = var->type()->UnwrapIfNeeded();
    if (!var->IsDecorated() || !unwrapped_type->IsSampler() ||
        unwrapped_type->AsSampler()->kind() != kind) {
      continue;
    }

    BindingDecoration* binding = nullptr;
    SetDecoration* set = nullptr;
    for (const auto& deco : var->AsDecorated()->decorations()) {
      if (deco->IsBinding()) {
        binding = deco->AsBinding();
      } else if (deco->IsSet()) {
        set = deco->AsSet();
      }
    }
    if (binding == nullptr || set == nullptr) {
      continue;
    }

    ret.push_back({var, BindingInfo{binding, set}});
  }
  return ret;
}

}  // namespace ast
}  // namespace tint
