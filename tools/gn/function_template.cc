// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"

#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"

Value ExecuteTemplate(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      BlockNode* block,
                      Err* err) {
  // TODO(brettw) determine if the function is built-in and throw an error if
  // it is.
  if (args.size() != 1) {
    *err = Err(function->function(),
               "Need exactly one string arg to template.");
    return Value();
  }
  if (!args[0].VerifyTypeIs(Value::STRING, err))
    return Value();
  std::string template_name = args[0].string_value();

  const FunctionCallNode* existing_template = scope->GetTemplate(template_name);
  if (existing_template) {
    *err = Err(function, "Duplicate template definition.",
               "A template with this name was already defined.");
    err->AppendSubErr(Err(existing_template->function(),
                          "Previous definition."));
    return Value();
  }

  scope->AddTemplate(template_name, function);
  return Value();
}
