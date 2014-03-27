// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/template.h"

#include "tools/gn/err.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"

Template::Template(const Scope* scope, const FunctionCallNode* def)
    : closure_(scope->MakeClosure()),
      definition_(def) {
}

Template::Template(scoped_ptr<Scope> scope, const FunctionCallNode* def)
    : closure_(scope.Pass()),
      definition_(def) {
}

Template::~Template() {
}

scoped_ptr<Template> Template::Clone() const {
  // We can make a new closure from our closure to copy it.
  return scoped_ptr<Template>(
      new Template(closure_->MakeClosure(), definition_));
}

Value Template::Invoke(Scope* scope,
                       const FunctionCallNode* invocation,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err) const {
  // Don't allow templates to be executed from imported files. Imports are for
  // simple values only.
  if (!EnsureNotProcessingImport(invocation, scope, err))
    return Value();

  // First run the invocation's block.
  Scope invocation_scope(scope);
  if (!FillTargetBlockScope(scope, invocation,
                            invocation->function().value().as_string(),
                            block, args, &invocation_scope, err))
    return Value();
  block->ExecuteBlockInScope(&invocation_scope, err);
  if (err->has_error())
    return Value();

  // Set up the scope to run the template. This should be dependent on the
  // closure, but have the "invoker" and "target_name" values injected, and the
  // current dir matching the invoker.
  Scope template_scope(closure_.get());
  template_scope.SetValue("invoker", Value(NULL, &invocation_scope),
                          invocation);
  template_scope.set_source_dir(scope->GetSourceDir());

  const base::StringPiece target_name("target_name");
  template_scope.SetValue(target_name,
                          Value(invocation, args[0].string_value()),
                          invocation);

  // Run the template code. Don't check for unused variables since the
  // template could be executed in many different ways and it could be that
  // not all executions use all values in the closure.
  return definition_->block()->ExecuteBlockInScope(&template_scope, err);
}

LocationRange Template::GetDefinitionRange() const {
  return definition_->GetRange();
}
