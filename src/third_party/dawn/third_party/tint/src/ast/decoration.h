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

#ifndef SRC_AST_DECORATION_H_
#define SRC_AST_DECORATION_H_

#include <string>
#include <vector>

#include "src/ast/node.h"

namespace tint {
namespace ast {

/// The base class for all decorations
class Decoration : public Castable<Decoration, Node> {
 public:
  ~Decoration() override;

  /// @returns the WGSL name for the decoration
  virtual std::string Name() const = 0;

 protected:
  /// Constructor
  /// @param pid the identifier of the program that owns this node
  /// @param src the source of this node
  Decoration(ProgramID pid, const Source& src) : Base(pid, src) {}
};

/// A list of decorations
using DecorationList = std::vector<const Decoration*>;

/// @param decorations the list of decorations to search
/// @returns true if `decorations` includes a decoration of type `T`
template <typename T>
bool HasDecoration(const DecorationList& decorations) {
  for (auto* deco : decorations) {
    if (deco->Is<T>()) {
      return true;
    }
  }
  return false;
}

/// @param decorations the list of decorations to search
/// @returns a pointer to `T` from `decorations` if found, otherwise nullptr.
template <typename T>
const T* GetDecoration(const DecorationList& decorations) {
  for (auto* deco : decorations) {
    if (deco->Is<T>()) {
      return deco->As<T>();
    }
  }
  return nullptr;
}

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_DECORATION_H_
