// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/err.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"

namespace functions {

namespace {


}  // namespace

const char kForEach[] = "foreach";
const char kForEach_HelpShort[] =
    "foreach: Iterate over a list.";
const char kForEach_Help[] =
    "foreach: Iterate over a list.\n"
    "\n"
    "  foreach(<loop_var>, <list>) {\n"
    "    <loop contents>\n"
    "  }\n"
    "\n"
    "  Executes the loop contents block over each item in the list,\n"
    "  assigning the loop_var to each item in sequence.\n"
    "\n"
    "  The block does not introduce a new scope, so that variable assignments\n"
    "  inside the loop will be visible once the loop terminates.\n"
    "\n"
    "  The loop variable will temporarily shadow any existing variables with\n"
    "  the same name for the duration of the loop. After the loop terminates\n"
    "  the loop variable will no longer be in scope, and the previous value\n"
    "  (if any) will be restored.\n"
    "\n"
    "Example\n"
    "\n"
    "  mylist = [ \"a\", \"b\", \"c\" ]\n"
    "  foreach(i, mylist) {\n"
    "    print(i)\n"
    "  }\n"
    "\n"
    "  Prints:\n"
    "  a\n"
    "  b\n"
    "  c\n";
Value RunForEach(Scope* scope,
                 const FunctionCallNode* function,
                 const ListNode* args_list,
                 Err* err) {
  const std::vector<const ParseNode*>& args_vector = args_list->contents();
  if (args_vector.size() != 2) {
    *err = Err(function, "Wrong number of arguments to foreach().",
               "Expecting exactly two.");
    return Value();
  }

  // Extract the loop variable.
  const IdentifierNode* identifier = args_vector[0]->AsIdentifier();
  if (!identifier) {
    *err = Err(args_vector[0], "Expected an identifier for the loop var.");
    return Value();
  }
  base::StringPiece loop_var(identifier->value().value());

  // Extract the list, avoid a copy if it's an identifier (common case).
  Value value_storage_for_exec;  // Backing for list_value when we need to exec.
  const Value* list_value = NULL;
  const IdentifierNode* list_identifier = args_vector[1]->AsIdentifier();
  if (list_identifier) {
    list_value = scope->GetValue(list_identifier->value().value());
    if (!list_value) {
      *err = Err(args_vector[1], "Undefined identifier.");
      return Value();
    }
  } else {
    // Not an identifier, evaluate the node to get the result.
    Scope list_exec_scope(scope);
    value_storage_for_exec = args_vector[1]->Execute(scope, err);
    if (err->has_error())
      return Value();
    list_value = &value_storage_for_exec;
  }
  if (!list_value->VerifyTypeIs(Value::LIST, err))
    return Value();
  const std::vector<Value>& list = list_value->list_value();

  // Block to execute.
  const BlockNode* block = function->block();
  if (!block) {
    *err = Err(function, "Expected { after foreach.");
    return Value();
  }

  // If the loop variable was previously defined in this scope, save it so we
  // can put it back after the loop is done.
  const Value* old_loop_value_ptr = scope->GetValue(loop_var);
  Value old_loop_value;
  if (old_loop_value_ptr)
    old_loop_value = *old_loop_value_ptr;

  for (size_t i = 0; i < list.size(); i++) {
    scope->SetValue(loop_var, list[i], function);
    block->ExecuteBlockInScope(scope, err);
    if (err->has_error())
      return Value();
  }

  // Put back loop var.
  if (old_loop_value_ptr) {
    // Put back old value. Use the copy we made, rather than use the pointer,
    // which will probably point to the new value now in the scope.
    scope->SetValue(loop_var, old_loop_value, old_loop_value.origin());
  } else {
    // Loop variable was undefined before loop, delete it.
    scope->RemoveIdentifier(loop_var);
  }

  return Value();
}

}  // namespace functions
