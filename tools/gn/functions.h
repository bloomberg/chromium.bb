// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_FUNCTIONS_H_
#define TOOLS_GN_FUNCTIONS_H_

#include <string>
#include <vector>

class Err;
class BlockNode;
class FunctionCallNode;
class Label;
class ListNode;
class ParseNode;
class Scope;
class SourceDir;
class Token;
class Value;

Value ExecuteFunction(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      BlockNode* block,  // Optional.
                      Err* err);

// Function executing functions -----------------------------------------------

Value ExecuteTemplate(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      BlockNode* block,
                      Err* err);
Value ExecuteExecScript(Scope* scope,
                        const FunctionCallNode* function,
                        const std::vector<Value>& args,
                        Err* err);
Value ExecuteProcessFileTemplate(Scope* scope,
                                 const FunctionCallNode* function,
                                 const std::vector<Value>& args,
                                 Err* err);
Value ExecuteReadFile(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      Err* err);
Value ExecuteSetDefaultToolchain(Scope* scope,
                                 const FunctionCallNode* function,
                                 const std::vector<Value>& args,
                                 Err* err);
Value ExecuteTool(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  BlockNode* block,
                  Err* err);
Value ExecuteToolchain(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err);
Value ExecuteWriteFile(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       Err* err);

// Target-generating functions.
Value ExecuteComponent(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err);
Value ExecuteCopy(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  Err* err);
Value ExecuteCustom(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    BlockNode* block,
                    Err* err);
Value ExecuteExecutable(Scope* scope,
                        const FunctionCallNode* function,
                        const std::vector<Value>& args,
                        BlockNode* block,
                        Err* err);
Value ExecuteSharedLibrary(Scope* scope,
                           const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           BlockNode* block,
                           Err* err);
Value ExecuteStaticLibrary(Scope* scope,
                           const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           BlockNode* block,
                           Err* err);
Value ExecuteGroup(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   BlockNode* block,
                   Err* err);

// Helper functions -----------------------------------------------------------

// Verifies that the current scope is not processing an import. If it is, it
// will set the error, blame the given parse node for it, and return false.
bool EnsureNotProcessingImport(const ParseNode* node,
                               const Scope* scope,
                               Err* err);

// Like EnsureNotProcessingImport but checks for running the build config.
bool EnsureNotProcessingBuildConfig(const ParseNode* node,
                                    const Scope* scope,
                                    Err* err);

// Sets up the |block_scope| for executing a target (or something like it).
// The |scope| is the containing scope. It should have been already set as the
// parent for the |block_scope| when the |block_scope| was created.
//
// This will set up the target defaults and set the |target_name| variable in
// the block scope to the current target name, which is assumed to be the first
// argument to the function.
//
// On success, returns true. On failure, sets the error and returns false.
bool FillTargetBlockScope(const Scope* scope,
                          const FunctionCallNode* function,
                          const char* target_type,
                          const BlockNode* block,
                          const std::vector<Value>& args,
                          Scope* block_scope,
                          Err* err);

// Validates that the given function call has one string argument. This is
// the most common function signature, so it saves space to have this helper.
// Returns false and sets the error on failure.
bool EnsureSingleStringArg(const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           Err* err);

// Returns the source directory for the file comtaining the given function
// invocation.
const SourceDir& SourceDirForFunctionCall(const FunctionCallNode* function);

// Returns the name of the toolchain for the given scope.
const Label& ToolchainLabelForScope(const Scope* scope);

// Generates a label for the given scope, using the current directory and
// toolchain, and the given name.
Label MakeLabelForScope(const Scope* scope,
                        const FunctionCallNode* function,
                        const std::string& name);

// Function name constants ----------------------------------------------------

namespace functions {

extern const char kAssert[];
extern const char kComponent[];
extern const char kConfig[];
extern const char kCopy[];
extern const char kCustom[];
extern const char kDeclareArgs[];
extern const char kExecScript[];
extern const char kExecutable[];
extern const char kGroup[];
extern const char kImport[];
extern const char kPrint[];
extern const char kProcessFileTemplate[];
extern const char kReadFile[];
extern const char kSetDefaults[];
extern const char kSetDefaultToolchain[];
extern const char kSetSourcesAssignmentFilter[];
extern const char kSharedLibrary[];
extern const char kStaticLibrary[];
extern const char kTemplate[];
extern const char kTest[];
extern const char kTool[];
extern const char kToolchain[];
extern const char kWriteFile[];

}  // namespace functions

#endif  // TOOLS_GN_FUNCTIONS_H_
