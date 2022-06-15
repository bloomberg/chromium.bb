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

#ifndef SRC_TINT_SEM_STATEMENT_H_
#define SRC_TINT_SEM_STATEMENT_H_

#include "src/tint/sem/behavior.h"
#include "src/tint/sem/node.h"

// Forward declarations
namespace tint::ast {
class Statement;
}  // namespace tint::ast
namespace tint::sem {
class BlockStatement;
class CompoundStatement;
class Function;
}  // namespace tint::sem

namespace tint::sem {
namespace detail {
/// FindFirstParentReturn is a traits helper for determining the return type for
/// the template member function Statement::FindFirstParent().
/// For zero or multiple template arguments, FindFirstParentReturn::type
/// resolves to CompoundStatement.
template <typename... TYPES>
struct FindFirstParentReturn {
    /// The pointer type returned by Statement::FindFirstParent()
    using type = CompoundStatement;
};

/// A specialization of FindFirstParentReturn for a single template argument.
/// FindFirstParentReturn::type resolves to the single template argument.
template <typename T>
struct FindFirstParentReturn<T> {
    /// The pointer type returned by Statement::FindFirstParent()
    using type = T;
};

template <typename... TYPES>
using FindFirstParentReturnType = typename FindFirstParentReturn<TYPES...>::type;
}  // namespace detail

/// Statement holds the semantic information for a statement.
class Statement : public Castable<Statement, Node> {
  public:
    /// Constructor
    /// @param declaration the AST node for this statement
    /// @param parent the owning statement
    /// @param function the owning function
    Statement(const ast::Statement* declaration,
              const CompoundStatement* parent,
              const sem::Function* function);

    /// Destructor
    ~Statement() override;

    /// @return the AST node for this statement
    const ast::Statement* Declaration() const { return declaration_; }

    /// @return the statement that encloses this statement
    const CompoundStatement* Parent() const { return parent_; }

    /// @returns the closest enclosing parent that satisfies the given predicate,
    /// which may be the statement itself, or nullptr if no match is found.
    /// @param pred a predicate that the resulting block must satisfy
    template <typename Pred>
    const CompoundStatement* FindFirstParent(Pred&& pred) const;

    /// @returns the closest enclosing parent that is of one of the types in
    /// `TYPES`, which may be the statement itself, or nullptr if no match is
    /// found. If `TYPES` is a single template argument, the return type is a
    /// pointer to that template argument type, otherwise a CompoundStatement
    /// pointer is returned.
    template <typename... TYPES>
    const detail::FindFirstParentReturnType<TYPES...>* FindFirstParent() const;

    /// @return the closest enclosing block for this statement
    const BlockStatement* Block() const;

    /// @returns the function that owns this statement
    const sem::Function* Function() const { return function_; }

    /// @return the behaviors of this statement
    const sem::Behaviors& Behaviors() const { return behaviors_; }

    /// @return the behaviors of this statement
    sem::Behaviors& Behaviors() { return behaviors_; }

    /// @returns true if this statement is reachable by control flow according to
    /// the behavior analysis
    bool IsReachable() const { return is_reachable_; }

    /// @param is_reachable whether this statement is reachable by control flow
    /// according to the behavior analysis
    void SetIsReachable(bool is_reachable) { is_reachable_ = is_reachable; }

  private:
    const ast::Statement* const declaration_;
    const CompoundStatement* const parent_;
    const sem::Function* const function_;
    sem::Behaviors behaviors_{sem::Behavior::kNext};
    bool is_reachable_ = true;
};

/// CompoundStatement is the base class of statements that can hold other
/// statements.
class CompoundStatement : public Castable<Statement, Statement> {
  public:
    /// Constructor
    /// @param declaration the AST node for this statement
    /// @param statement the owning statement
    /// @param function the owning function
    CompoundStatement(const ast::Statement* declaration,
                      const CompoundStatement* statement,
                      const sem::Function* function);

    /// Destructor
    ~CompoundStatement() override;
};

template <typename Pred>
const CompoundStatement* Statement::FindFirstParent(Pred&& pred) const {
    if (auto* self = As<CompoundStatement>()) {
        if (pred(self)) {
            return self;
        }
    }
    const auto* curr = parent_;
    while (curr && !pred(curr)) {
        curr = curr->Parent();
    }
    return curr;
}

template <typename... TYPES>
const detail::FindFirstParentReturnType<TYPES...>* Statement::FindFirstParent() const {
    using ReturnType = detail::FindFirstParentReturnType<TYPES...>;
    if (sizeof...(TYPES) == 1) {
        if (auto* p = As<ReturnType>()) {
            return p;
        }
        const auto* curr = parent_;
        while (curr) {
            if (auto* p = curr->As<ReturnType>()) {
                return p;
            }
            curr = curr->Parent();
        }
    } else {
        if (IsAnyOf<TYPES...>()) {
            return As<ReturnType>();
        }
        const auto* curr = parent_;
        while (curr) {
            if (curr->IsAnyOf<TYPES...>()) {
                return curr->As<ReturnType>();
            }
            curr = curr->Parent();
        }
    }
    return nullptr;
}

}  // namespace tint::sem

#endif  // SRC_TINT_SEM_STATEMENT_H_
