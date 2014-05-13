// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/args.h"

#include "build/build_config.h"
#include "tools/gn/variables.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

const char kBuildArgs_Help[] =
    "Build Arguments Overview\n"
    "\n"
    "  Build arguments are variables passed in from outside of the build\n"
    "  that build files can query to determine how the build works.\n"
    "\n"
    "How build arguments are set\n"
    "\n"
    "  First, system default arguments are set based on the current system.\n"
    "  The built-in arguments are:\n"
    "   - cpu_arch (by default this is the same as \"default_cpu_arch\")\n"
    "   - default_cpu_arch\n"
    "   - default_os\n"
    "   - os (by default this is the same as \"default_os\")\n"
    "\n"
    "  If specified, arguments from the --args command line flag are used. If\n"
    "  that flag is not specified, args from previous builds in the build\n"
    "  directory will be used (this is in the file args.gn in the build\n"
    "  directory).\n"
    "\n"
    "  Last, for targets being compiled with a non-default toolchain, the\n"
    "  toolchain overrides are applied. These are specified in the\n"
    "  toolchain_args section of a toolchain definition. The use-case for\n"
    "  this is that a toolchain may be building code for a different\n"
    "  platform, and that it may want to always specify Posix, for example.\n"
    "  See \"gn help toolchain_args\" for more.\n"
    "\n"
    "  If you specify an override for a build argument that never appears in\n"
    "  a \"declare_args\" call, a nonfatal error will be displayed.\n"
    "\n"
    "Examples\n"
    "\n"
    "  gn args out/FooBar\n"
    "      Create the directory out/FooBar and open an editor. You would type\n"
    "      something like this into that file:\n"
    "          enable_doom_melon=false\n"
    "          os=\"android\"\n"
    "\n"
    "  gn gen out/FooBar --args=\"enable_doom_melon=true os=\\\"android\\\"\"\n"
    "      This will overwrite the build directory with the given arguments.\n"
    "      (Note that the quotes inside the args command will usually need to\n"
    "      be escaped for your shell to pass through strings values.)\n"
    "\n"
    "How build arguments are used\n"
    "\n"
    "  If you want to use an argument, you use declare_args() and specify\n"
    "  default values. These default values will apply if none of the steps\n"
    "  listed in the \"How build arguments are set\" section above apply to\n"
    "  the given argument, but the defaults will not override any of these.\n"
    "\n"
    "  Often, the root build config file will declare global arguments that\n"
    "  will be passed to all buildfiles. Individual build files can also\n"
    "  specify arguments that apply only to those files. It is also useful\n"
    "  to specify build args in an \"import\"-ed file if you want such\n"
    "  arguments to apply to multiple buildfiles.\n";

Args::Args() {
}

Args::Args(const Args& other)
    : overrides_(other.overrides_),
      all_overrides_(other.all_overrides_),
      declared_arguments_(other.declared_arguments_) {
}

Args::~Args() {
}

void Args::AddArgOverride(const char* name, const Value& value) {
  base::AutoLock lock(lock_);

  overrides_[base::StringPiece(name)] = value;
  all_overrides_[base::StringPiece(name)] = value;
}

void Args::AddArgOverrides(const Scope::KeyValueMap& overrides) {
  base::AutoLock lock(lock_);

  for (Scope::KeyValueMap::const_iterator i = overrides.begin();
       i != overrides.end(); ++i) {
    overrides_[i->first] = i->second;
    all_overrides_[i->first] = i->second;
  }
}

const Value* Args::GetArgOverride(const char* name) const {
  base::AutoLock lock(lock_);

  Scope::KeyValueMap::const_iterator found =
      all_overrides_.find(base::StringPiece(name));
  if (found == all_overrides_.end())
    return NULL;
  return &found->second;
}

Scope::KeyValueMap Args::GetAllOverrides() const {
  base::AutoLock lock(lock_);
  return all_overrides_;
}

void Args::SetupRootScope(Scope* dest,
                          const Scope::KeyValueMap& toolchain_overrides) const {
  base::AutoLock lock(lock_);

  SetSystemVarsLocked(dest);
  ApplyOverridesLocked(overrides_, dest);
  ApplyOverridesLocked(toolchain_overrides, dest);
  SaveOverrideRecordLocked(toolchain_overrides);
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
    // seen it before. Instead, we check that the location matches.
    Scope::KeyValueMap::iterator previously_declared =
        declared_arguments_.find(i->first);
    if (previously_declared != declared_arguments_.end()) {
      if (previously_declared->second.origin() != i->second.origin()) {
        // Declaration location mismatch.
        *err = Err(i->second.origin(), "Duplicate build argument declaration.",
            "Here you're declaring an argument that was already declared "
            "elsewhere.\nYou can only declare each argument once in the entire "
            "build so there is one\ncanonical place for documentation and the "
            "default value. Either move this\nargument to the build config "
            "file (for visibility everywhere) or to a .gni file\nthat you "
            "\"import\" from the files where you need it (preferred).");
        err->AppendSubErr(Err(previously_declared->second.origin(),
                              "Previous declaration.",
                              "See also \"gn help buildargs\" for more on how "
                              "build arguments work."));
        return false;
      }
    } else {
      declared_arguments_.insert(*i);
    }

    // Only set on the current scope to the new value if it hasn't been already
    // set. Mark the variable used so the build script can override it in
    // certain cases without getting unused value errors.
    if (!scope_to_set->GetValue(i->first)) {
      scope_to_set->SetValue(i->first, i->second, i->second.origin());
      scope_to_set->MarkUsed(i->first);
    }
  }

  return true;
}

bool Args::VerifyAllOverridesUsed(Err* err) const {
  base::AutoLock lock(lock_);
  return VerifyAllOverridesUsed(all_overrides_, declared_arguments_, err);
}

bool Args::VerifyAllOverridesUsed(
    const Scope::KeyValueMap& overrides,
    const Scope::KeyValueMap& declared_arguments,
    Err* err) {
  for (Scope::KeyValueMap::const_iterator i = overrides.begin();
       i != overrides.end(); ++i) {
    if (declared_arguments.find(i->first) == declared_arguments.end()) {
      // Get a list of all possible overrides for help with error finding.
      //
      // It might be nice to do edit distance checks to see if we can find one
      // close to what you typed.
      std::string all_declared_str;
      for (Scope::KeyValueMap::const_iterator cur_str =
               declared_arguments.begin();
           cur_str != declared_arguments.end(); ++cur_str) {
        if (cur_str != declared_arguments.begin())
          all_declared_str += ", ";
        all_declared_str += cur_str->first.as_string();
      }

      *err = Err(i->second.origin(), "Build argument has no effect.",
          "The variable \"" + i->first.as_string() + "\" was set as a build "
          "argument\nbut never appeared in a declare_args() block in any "
          "buildfile.\n\nPossible arguments: " + all_declared_str);
      return false;
    }
  }
  return true;
}

void Args::MergeDeclaredArguments(Scope::KeyValueMap* dest) const {
  base::AutoLock lock(lock_);

  for (Scope::KeyValueMap::const_iterator i = declared_arguments_.begin();
       i != declared_arguments_.end(); ++i)
    (*dest)[i->first] = i->second;
}

void Args::SetSystemVarsLocked(Scope* dest) const {
  lock_.AssertAcquired();

  // Host OS.
  const char* os = NULL;
#if defined(OS_WIN)
  os = "win";
#elif defined(OS_MACOSX)
  os = "mac";
#elif defined(OS_LINUX)
  os = "linux";
#elif defined(OS_ANDROID)
  os = "android";
#else
  #error Unknown OS type.
#endif
  Value os_val(NULL, std::string(os));
  dest->SetValue(variables::kBuildOs, os_val, NULL);
  dest->SetValue(variables::kOs, os_val, NULL);

  // Host architecture.
  static const char kX86[] = "x86";
  static const char kX64[] = "x64";
  const char* arch = NULL;
#if defined(OS_WIN)
  // ...on Windows, set the CPU architecture based on the underlying OS, not
  // whatever the current bit-tedness of the GN binary is.
  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  switch (os_info->architecture()) {
    case base::win::OSInfo::X86_ARCHITECTURE:
      arch = kX86;
      break;
    case base::win::OSInfo::X64_ARCHITECTURE:
      arch = kX64;
      break;
    default:
      CHECK(false) << "Windows architecture not handled.";
      break;
  }
#else
  // ...on all other platforms, just use the bit-tedness of the current
  // process.
  #if defined(ARCH_CPU_X86_64)
    arch = kX64;
  #elif defined(ARCH_CPU_X86)
    arch = kX86;
  #elif defined(ARCH_CPU_ARMEL)
    static const char kArm[] = "arm";
    arch = kArm;
  #else
    #error Unknown architecture.
  #endif
#endif
  // Avoid unused var warning.
  (void)kX86;
  (void)kX64;

  Value arch_val(NULL, std::string(arch));
  dest->SetValue(variables::kBuildCpuArch, arch_val, NULL);
  dest->SetValue(variables::kCpuArch, arch_val, NULL);

  // Save the OS and architecture as build arguments that are implicitly
  // declared. This is so they can be overridden in a toolchain build args
  // override, and so that they will appear in the "gn args" output.
  //
  // Do not declare the build* variants since these shouldn't be changed.
  //
  // Mark these variables used so the build config file can override them
  // without geting a warning about overwriting an unused variable.
  declared_arguments_[variables::kOs] = os_val;
  declared_arguments_[variables::kCpuArch] = arch_val;
  dest->MarkUsed(variables::kCpuArch);
  dest->MarkUsed(variables::kOs);
}

void Args::ApplyOverridesLocked(const Scope::KeyValueMap& values,
                                Scope* scope) const {
  lock_.AssertAcquired();
  for (Scope::KeyValueMap::const_iterator i = values.begin();
       i != values.end(); ++i)
    scope->SetValue(i->first, i->second, i->second.origin());
}

void Args::SaveOverrideRecordLocked(const Scope::KeyValueMap& values) const {
  lock_.AssertAcquired();
  for (Scope::KeyValueMap::const_iterator i = values.begin();
       i != values.end(); ++i)
    all_overrides_[i->first] = i->second;
}
