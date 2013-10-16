// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ARGS_H_
#define TOOLS_GN_ARGS_H_

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/synchronization/lock.h"
#include "tools/gn/err.h"
#include "tools/gn/scope.h"

extern const char kBuildArgs_Help[];

// Manages build arguments. It stores the global arguments specified on the
// command line, and sets up the root scope with the proper values.
//
// This class tracks accesses so we can report errors about unused variables.
// The use case is if the user specifies an override on the command line, but
// no buildfile actually uses that variable. We want to be able to report that
// the argument was unused.
class Args {
 public:
  Args();
  Args(const Args& other);
  ~Args();

  // Specifies overrides of the build arguments. These are normally specified
  // on the command line.
  void AddArgOverride(const char* name, const Value& value);
  void AddArgOverrides(const Scope::KeyValueMap& overrides);

  // Sets up the root scope for a toolchain. This applies the default system
  // flags, then any overrides stored in this object, then applies any
  // toolchain overrides specified in the argument.
  void SetupRootScope(Scope* dest,
                      const Scope::KeyValueMap& toolchain_overrides) const;

  // Sets up the given scope with arguments passed in.
  //
  // If the values specified in the args are not already set, the values in
  // the args list will be used (which are assumed to be the defaults), but
  // they will not override the system defaults or the current overrides.
  //
  // All args specified in the input will be marked as "used".
  //
  // On failure, the err will be set and it will return false.
  bool DeclareArgs(const Scope::KeyValueMap& args,
                   Scope* scope_to_set,
                   Err* err) const;

  // Checks to see if any of the overrides ever used were never declared as
  // arguments. If there are, this returns false and sets the error.
  bool VerifyAllOverridesUsed(Err* err) const;

  // This function is not threadsafe, it must only be used when
  // single-threaded. It's used to implement the "args" command.
  const Scope::KeyValueMap& declared_arguments() const {
    return declared_arguments_;
  }

 private:
  // Sets the default config based on the current system.
  void SetSystemVars(Scope* scope) const;

  // Sets the given vars on the given scope.
  void ApplyOverrides(const Scope::KeyValueMap& values, Scope* scope) const;

  void SaveOverrideRecord(const Scope::KeyValueMap& values) const;

  // Since this is called during setup which we assume is single-threaded,
  // this is not protected by the lock. It should be set only during init.
  Scope::KeyValueMap overrides_;

  mutable base::Lock lock_;

  // Maintains a list of all overrides we've ever seen. This is the main
  // |overrides_| as well as toolchain overrides. Tracking this allows us to
  // check for overrides that were specified but never used.
  mutable Scope::KeyValueMap all_overrides_;

  // Tracks all variables declared in any buildfile. This is so we can see if
  // the user set variables on the command line that are not used anywhere.
  mutable Scope::KeyValueMap declared_arguments_;

  Args& operator=(const Args& other);  // Disallow assignment.
};

#endif  // TOOLS_GN_ARGS_H_
