// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"

#include <iostream>

#include "base/strings/string_util.h"
#include "tools/gn/config.h"
#include "tools/gn/config_values_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/input_file.h"
#include "tools/gn/item_tree.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/target_manager.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"

namespace {

void FillNeedsBlockError(const FunctionCallNode* function, Err* err) {
  *err = Err(function->function(), "This function call requires a block.",
      "The block's \"{\" must be on the same line as the function "
      "call's \")\".");
}

Value ExecuteAssert(const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    Err* err) {
  if (args.size() != 1) {
    *err = Err(function->function(), "Wrong number of arguments.",
               "assert() takes one argument, "
               "were you expecting somethig else?");
  } else if (args[0].InterpretAsInt() == 0) {
    *err = Err(function->function(), "Assertion failed.");
    if (args[0].origin()) {
      // If you do "assert(foo)" we'd ideally like to show you where foo was
      // set, and in this case the origin of the args will tell us that.
      // However, if you do "assert(foo && bar)" the source of the value will
      // be the assert like, which isn't so helpful.
      //
      // So we try to see if the args are from the same line or not. This will
      // break if you do "assert(\nfoo && bar)" and we may show the second line
      // as the source, oh well. The way around this is to check to see if the
      // origin node is inside our function call block.
      Location origin_location = args[0].origin()->GetRange().begin();
      if (origin_location.file() != function->function().location().file() ||
          origin_location.line_number() !=
              function->function().location().line_number()) {
        err->AppendSubErr(Err(args[0].origin()->GetRange(), "",
                              "This is where it was set."));
      }
    }
  }
  return Value();
}

Value ExecuteConfig(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    Err* err) {
  if (!EnsureSingleStringArg(function, args, err) ||
      !EnsureNotProcessingImport(function, scope, err))
    return Value();

  Label label(MakeLabelForScope(scope, function, args[0].string_value()));

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Generating config", label.GetUserVisibleName(true));

  // Create the empty config object.
  ItemTree* tree = &scope->settings()->build_settings()->item_tree();
  Config* config = Config::GetConfig(scope->settings(), function->GetRange(),
                                     label, NULL, err);
  if (err->has_error())
    return Value();

  // Fill it.
  const SourceDir input_dir = SourceDirForFunctionCall(function);
  ConfigValuesGenerator gen(&config->config_values(), scope,
                            function->function(), input_dir, err);
  gen.Run();
  if (err->has_error())
    return Value();

  // Mark as complete.
  {
    base::AutoLock lock(tree->lock());
    tree->MarkItemGeneratedLocked(label);
  }
  return Value();
}

Value ExecuteDeclareArgs(Scope* scope,
                         const FunctionCallNode* function,
                         const std::vector<Value>& args,
                         Err* err) {
  // Only allow this to be called once. We use a variable in the current scope
  // with a name the parser will reject if the user tried to type it.
  const char did_declare_args_var[] = "@@declared_args";
  if (scope->GetValue(did_declare_args_var)) {
    *err = Err(function->function(), "Duplicate call to declared_args.");
    err->AppendSubErr(
        Err(scope->GetValue(did_declare_args_var)->origin()->GetRange(),
                            "See the original call."));
    return Value();
  }

  // Find the root scope where the values will be set.
  Scope* root = scope->mutable_containing();
  if (!root || root->containing() || !scope->IsProcessingBuildConfig()) {
    *err = Err(function->function(), "declare_args called incorrectly."
        "It must be called only from the build config script and in the "
        "root scope.");
    return Value();
  }

  // Take all variables set in the current scope as default values and put
  // them in the parent scope. The values in the current scope are the defaults,
  // then we apply the external args to this list.
  Scope::KeyValueVector values;
  scope->GetCurrentScopeValues(&values);
  for (size_t i = 0; i < values.size(); i++) {
    // TODO(brettw) actually import the arguments from the command line rather
    // than only using the defaults.
    root->SetValue(values[i].first, values[i].second,
                   values[i].second.origin());
  }

  scope->SetValue(did_declare_args_var, Value(function, 1), NULL);
  return Value();
}

Value ExecuteImport(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    Err* err) {
  if (!EnsureSingleStringArg(function, args, err) ||
      !EnsureNotProcessingImport(function, scope, err))
    return Value();

  const SourceDir input_dir = SourceDirForFunctionCall(function);
  SourceFile import_file =
      input_dir.ResolveRelativeFile(args[0].string_value());
  scope->settings()->import_manager().DoImport(import_file, function,
                                               scope, err);
  return Value();
}

Value ExecuteTemplate(Scope* scope,
                      const FunctionCallNode* invocation,
                      const std::vector<Value>& args,
                      BlockNode* block,
                      const FunctionCallNode* rule,
                      Err* err) {
  if (!EnsureNotProcessingImport(invocation, scope, err))
    return Value();
  Scope block_scope(scope);
  if (!FillTargetBlockScope(scope, invocation,
                            invocation->function().value().data(),
                            block, args, &block_scope, err))
    return Value();

  // Run the block for the rule invocation.
  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  // Now run the rule itself with that block as the current scope.
  rule->block()->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  return Value();
}

Value ExecuteSetDefaults(Scope* scope,
                         const FunctionCallNode* function,
                         const std::vector<Value>& args,
                         BlockNode* block,
                         Err* err) {
  if (!EnsureSingleStringArg(function, args, err))
    return Value();
  const std::string& target_type(args[0].string_value());

  // Ensure there aren't defaults already set.
  if (scope->GetTargetDefaults(target_type)) {
    *err = Err(function->function(),
               "This target type defaults were already set.");
    return Value();
  }

  // Execute the block in a new scope that has a parent of the containing
  // scope.
  Scope block_scope(scope);
  if (!FillTargetBlockScope(scope, function,
                            function->function().value().data(),
                            block, args, &block_scope, err))
    return Value();

  // Run the block for the rule invocation.
  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  // Now copy the values set on the scope we made into the free-floating one
  // (with no containing scope) used to hold the target defaults.
  Scope* dest = scope->MakeTargetDefaults(target_type);
  block_scope.NonRecursiveMergeTo(dest, function, "<SHOULD NOT FAIL>", err);
  return Value();
}

Value ExecuteSetSourcesAssignmentFilter(Scope* scope,
                                        const FunctionCallNode* function,
                                        const std::vector<Value>& args,
                                        Err* err) {
  if (args.size() != 1) {
    *err = Err(function, "set_sources_assignment_filter takes one argument.");
  } else {
    scoped_ptr<PatternList> f(new PatternList);
    f->SetFromValue(args[0], err);
    if (!err->has_error())
      scope->set_sources_assignment_filter(f.Pass());
  }
  return Value();
}

// void print(...)
// prints all arguments to the console separated by spaces.
Value ExecutePrint(const std::vector<Value>& args, Err* err) {
  for (size_t i = 0; i < args.size(); i++) {
    if (i != 0)
      std::cout << " ";
    std::cout << args[i].ToString();
  }
  std::cout << std::endl;
  return Value();
}

}  // namespace

// ----------------------------------------------------------------------------

namespace functions {

const char kAssert[] = "assert";
const char kComponent[] = "component";
const char kConfig[] = "config";
const char kCopy[] = "copy";
const char kCustom[] = "custom";
const char kDeclareArgs[] = "declare_args";
const char kExecScript[] = "exec_script";
const char kExecutable[] = "executable";
const char kGroup[] = "group";
const char kImport[] = "import";
const char kPrint[] = "print";
const char kProcessFileTemplate[] = "process_file_template";
const char kReadFile[] = "read_file";
const char kSetDefaults[] = "set_defaults";
const char kSetDefaultToolchain[] = "set_default_toolchain";
const char kSetSourcesAssignmentFilter[] = "set_sources_assignment_filter";
const char kSharedLibrary[] = "shared_library";
const char kStaticLibrary[] = "static_library";
const char kTemplate[] = "template";
const char kTool[] = "tool";
const char kToolchain[] = "toolchain";
const char kTest[] = "test";
const char kWriteFile[] = "write_file";

}  // namespace functions

// ----------------------------------------------------------------------------

bool EnsureNotProcessingImport(const ParseNode* node,
                               const Scope* scope,
                               Err* err) {
  if (scope->IsProcessingImport()) {
    *err = Err(node, "Not valid from an import.",
        "We need to talk about this thing you are doing here. Doing this\n"
        "kind of thing from an imported file makes me feel like you are\n"
        "abusing me. Imports are for defining defaults, variables, and rules.\n"
        "The appropriate place for this kind of thing is really in a normal\n"
        "BUILD file.");
    return false;
  }
  return true;
}

bool EnsureNotProcessingBuildConfig(const ParseNode* node,
                                    const Scope* scope,
                                    Err* err) {
  if (scope->IsProcessingBuildConfig()) {
    *err = Err(node, "Not valid from the build config.",
        "You can't do this kind of thing from the build config script, "
        "silly!\nPut it in a regular BUILD file.");
    return false;
  }
  return true;
}

bool FillTargetBlockScope(const Scope* scope,
                          const FunctionCallNode* function,
                          const char* target_type,
                          const BlockNode* block,
                          const std::vector<Value>& args,
                          Scope* block_scope,
                          Err* err) {
  if (!block) {
    FillNeedsBlockError(function, err);
    return false;
  }

  // Copy the target defaults, if any, into the scope we're going to execute
  // the block in.
  const Scope* default_scope = scope->GetTargetDefaults(target_type);
  if (default_scope) {
    if (!default_scope->NonRecursiveMergeTo(block_scope, function,
                                            "target defaults", err))
      return false;
  }

  // The name is the single argument to the target function.
  if (!EnsureSingleStringArg(function, args, err))
    return false;

  // Set the target name variable to the current target, and mark it used
  // because we don't want to issue an error if the script ignores it.
  const base::StringPiece target_name("target_name");
  block_scope->SetValue(target_name, Value(function, args[0].string_value()),
                        function);
  block_scope->MarkUsed(target_name);
  return true;
}

bool EnsureSingleStringArg(const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           Err* err) {
  if (args.size() != 1) {
    *err = Err(function->function(), "Incorrect arguments.",
               "This function requires a single string argument.");
    return false;
  }
  return args[0].VerifyTypeIs(Value::STRING, err);
}

const SourceDir& SourceDirForFunctionCall(const FunctionCallNode* function) {
  return function->function().location().file()->dir();
}

const Label& ToolchainLabelForScope(const Scope* scope) {
  return scope->settings()->toolchain()->label();
}

Label MakeLabelForScope(const Scope* scope,
                        const FunctionCallNode* function,
                        const std::string& name) {
  const SourceDir& input_dir = SourceDirForFunctionCall(function);
  const Label& toolchain_label = ToolchainLabelForScope(scope);
  return Label(input_dir, name, toolchain_label.dir(), toolchain_label.name());
}

Value ExecuteFunction(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      BlockNode* block,
                      Err* err) {
  const Token& name = function->function();
  if (block) {
    // These target generators need to execute the block themselves.
    if (name.IsIdentifierEqualTo(functions::kComponent))
      return ExecuteComponent(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kCustom))
      return ExecuteCustom(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kExecutable))
      return ExecuteExecutable(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kSetDefaults))
      return ExecuteSetDefaults(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kSharedLibrary))
      return ExecuteSharedLibrary(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kStaticLibrary))
      return ExecuteStaticLibrary(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kGroup))
      return ExecuteGroup(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kTest))
      return ExecuteExecutable(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kTemplate))
      return ExecuteTemplate(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kTool))
      return ExecuteTool(scope, function, args, block, err);
    if (name.IsIdentifierEqualTo(functions::kToolchain))
      return ExecuteToolchain(scope, function, args, block, err);

    const FunctionCallNode* rule =
        scope->GetTemplate(function->function().value().as_string());
    if (rule)
      return ExecuteTemplate(scope, function, args, block, rule, err);

    // FIXME(brettw) This is not right, what if you specify a function that
    // doesn't take a block but specify one?!?!?

    // The rest of the functions can take a pre-executed block for simplicity.
    Scope block_scope(scope);
    block->ExecuteBlockInScope(&block_scope, err);
    if (err->has_error())
      return Value();

    if (name.IsIdentifierEqualTo(functions::kConfig))
      return ExecuteConfig(&block_scope, function, args, err);
    if (name.IsIdentifierEqualTo(functions::kCopy))
      return ExecuteCopy(&block_scope, function, args, err);
    if (name.IsIdentifierEqualTo(functions::kDeclareArgs))
      return ExecuteDeclareArgs(&block_scope, function, args, err);

    *err = Err(name, "Unknown function.");
    return Value();
  }

  if (name.IsIdentifierEqualTo(functions::kAssert))
    return ExecuteAssert(function, args, err);
  if (name.IsIdentifierEqualTo(functions::kExecScript))
    return ExecuteExecScript(scope, function, args, err);
  if (name.IsIdentifierEqualTo(functions::kImport))
    return ExecuteImport(scope, function, args, err);
  if (name.IsIdentifierEqualTo(functions::kPrint))
    return ExecutePrint(args, err);
  if (name.IsIdentifierEqualTo(functions::kProcessFileTemplate))
    return ExecuteProcessFileTemplate(scope, function, args, err);
  if (name.IsIdentifierEqualTo(functions::kReadFile))
    return ExecuteReadFile(scope, function, args, err);
  if (name.IsIdentifierEqualTo(functions::kSetDefaultToolchain))
    return ExecuteSetDefaultToolchain(scope, function, args, err);
  if (name.IsIdentifierEqualTo(functions::kSetSourcesAssignmentFilter))
    return ExecuteSetSourcesAssignmentFilter(scope, function, args, err);
  if (name.IsIdentifierEqualTo(functions::kWriteFile))
    return ExecuteWriteFile(scope, function, args, err);

  *err = Err(function, "Unknown function.");
  return Value();
}
