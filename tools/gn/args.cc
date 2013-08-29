// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/args.h"

#include "tools/gn/variables.h"

const char kBuildArgs_Help[] =
    "Build Arguments Overview.\n"
    "\n"
    "  Build arguments are variables passed in from outside of the build\n"
    "  that build files can query to determine how the build works.\n"
    "\n"
    "How build arguments are set:\n"
    "\n"
    "  First, system default arguments are set based on the current system.\n"
    "  The built-in arguments are:\n"
    "   - is_linux\n"
    "   - is_mac\n"
    "   - is_posix (set for Linux and Mac)\n"
    "   - is_win\n"
    "\n"
    "  Second, arguments specified on the command-line via \"--args\" are\n"
    "  applied. These can override the system default ones, and add new ones.\n"
    "  These are whitespace-separated. For example:\n"
    "\n"
    "    gn --args=\"is_win=true enable_doom_melon=false\"\n"
    "\n"
    "  Third, toolchain overrides are applied. These are specified in the\n"
    "  toolchain_args section of a toolchain definition. The use-case for\n"
    "  this is that a toolchain may be building code for a different\n"
    "  platform, and that it may want to always specify Posix, for example.\n"
    "  See \"gn help toolchain_args\" for more.\n"
    "\n"
    "  It is an error to specify an override for a build argument that never\n"
    "  appears in a \"declare_args\" call.\n"
    "\n"
    "How build arguments are used:\n"
    "\n"
    "  If you want to use an argument, you use declare_args() and specify\n"
    "  default values. These default values will apply if none of the steps\n"
    "  listed in the \"How build arguments are set\" section above apply to\n"
    "  the given argument, but the defaults will not override any of these.\n"
    "\n"
    "  Often, the root build config file will declare global arguments that\n"
    "  will be passed to all buildfiles. Individual build files can also\n"
    "  specify arguments that apply only to those files. It is also usedful\n"
    "  to specify build args in an \"import\"-ed file if you want such\n"
    "  arguments to apply to multiple buildfiles.\n";

Args::Args() {
}

Args::~Args() {
}

void Args::AddArgOverrides(const Scope::KeyValueMap& overrides) {
  for (Scope::KeyValueMap::const_iterator i = overrides.begin();
       i != overrides.end(); ++i) {
    overrides_.insert(*i);
    all_overrides_.insert(*i);
  }
}

void Args::SetupRootScope(Scope* dest,
                          const Scope::KeyValueMap& toolchain_overrides) const {
  SetSystemVars(dest);
  ApplyOverrides(overrides_, dest);
  ApplyOverrides(toolchain_overrides, dest);
  SaveOverrideRecord(toolchain_overrides);
}

bool Args::DeclareArgs(const Scope::KeyValueMap& args,
                       Scope* scope_to_set,
                       Err* err) const {
  base::AutoLock lock(lock_);

  for (Scope::KeyValueMap::const_iterator i = args.begin();
       i != args.end(); ++i) {
    // Verify that the value hasn't already been declared. We want each value
    // to be declared only once.
    //
    // The tricky part is that a buildfile can be interpreted multiple times
    // when used from different toolchains, so we can't just check that we've
    // seen it before. Instead, we check that the location matches. We
    // additionally check that the value matches to prevent people from
    // declaring defaults based on other parameters that may change. The
    // rationale is that you should have exactly one default value for each
    // argument that we can display in the help.
    Scope::KeyValueMap::iterator previously_declared =
        declared_arguments_.find(i->first);
    if (previously_declared != declared_arguments_.end()) {
      if (previously_declared->second.origin() != i->second.origin()) {
        // Declaration location mismatch.
        *err = Err(i->second.origin(), "Duplicate build arg declaration.",
            "Here you're declaring an argument that was already declared "
            "elsewhere.\nYou can only declare each argument once in the entire "
            "build so there is one\ncanonical place for documentation and the "
            "default value. Either move this\nargument to the build config "
            "file (for visibility everywhere) or to a .gni file\nthat you "
            "\"import\" from the files where you need it (preferred).");
        err->AppendSubErr(Err(previously_declared->second.origin(),
                              "Previous declaration.",
                              "See also \"gn help buildargs\" for more on how "
                              "build args work."));
        return false;
      } else if (previously_declared->second != i->second) {
        // Default value mismatch.
        *err = Err(i->second.origin(),
            "Non-constant default value for build arg.",
            "Each build arg should have one default value so we report it "
            "nicely in the\n\"gn args\" command. Please make this value "
            "constant.");
        return false;
      }
    } else {
      declared_arguments_.insert(*i);
    }

    // Only set on the current scope to the new value if it hasn't been already
    // set.
    if (!scope_to_set->GetValue(i->first))
      scope_to_set->SetValue(i->first, i->second, i->second.origin());
  }

  return true;
}

bool Args::VerifyAllOverridesUsed(Err* err) const {
  base::AutoLock lock(lock_);

  for (Scope::KeyValueMap::const_iterator i = all_overrides_.begin();
       i != all_overrides_.end(); ++i) {
    if (declared_arguments_.find(i->first) == declared_arguments_.end()) {
      *err = Err(i->second.origin(), "Build arg has no effect.",
          "The value \"" + i->first.as_string() + "\" was set a build "
          "argument\nbut never appeared in a declare_args() block in any "
          "buildfile.");
      return false;
    }
  }
  return true;
}

void Args::SetSystemVars(Scope* dest) const {
#if defined(OS_WIN)
  Value is_win(NULL, true);
  Value is_posix(NULL, false);
#else
  Value is_win(NULL, false);
  Value is_posix(NULL, true);
#endif
  dest->SetValue(variables::kIsWin, is_win, NULL);
  dest->SetValue(variables::kIsPosix, is_posix, NULL);
  declared_arguments_[variables::kIsWin] = is_win;
  declared_arguments_[variables::kIsPosix] = is_posix;

#if defined(OS_MACOSX)
  Value is_mac(NULL, true);
#else
  Value is_mac(NULL, false);
#endif
  dest->SetValue(variables::kIsMac, is_mac, NULL);
  declared_arguments_[variables::kIsMac] = is_mac;

#if defined(OS_LINUX)
  Value is_linux(NULL, true);
#else
  Value is_linux(NULL, false);
#endif
  dest->SetValue(variables::kIsLinux, is_linux, NULL);
  declared_arguments_[variables::kIsLinux] = is_linux;
}

void Args::ApplyOverrides(const Scope::KeyValueMap& values,
                          Scope* scope) const {
  for (Scope::KeyValueMap::const_iterator i = values.begin();
       i != values.end(); ++i)
    scope->SetValue(i->first, i->second, i->second.origin());
}

void Args::SaveOverrideRecord(const Scope::KeyValueMap& values) const {
  base::AutoLock lock(lock_);
  for (Scope::KeyValueMap::const_iterator i = values.begin();
       i != values.end(); ++i)
    all_overrides_[i->first] = i->second;
}
