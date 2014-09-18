// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_H_
#define TOOLS_GN_TARGET_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "tools/gn/action_values.h"
#include "tools/gn/config_values.h"
#include "tools/gn/item.h"
#include "tools/gn/label_ptr.h"
#include "tools/gn/ordered_set.h"
#include "tools/gn/output_file.h"
#include "tools/gn/source_file.h"
#include "tools/gn/unique_vector.h"

class InputFile;
class Settings;
class Token;
class Toolchain;

class Target : public Item {
 public:
  enum OutputType {
    UNKNOWN,
    GROUP,
    EXECUTABLE,
    SHARED_LIBRARY,
    STATIC_LIBRARY,
    SOURCE_SET,
    COPY_FILES,
    ACTION,
    ACTION_FOREACH,
  };
  typedef std::vector<SourceFile> FileList;
  typedef std::vector<std::string> StringVector;

  Target(const Settings* settings, const Label& label);
  virtual ~Target();

  // Returns a string naming the output type.
  static const char* GetStringForOutputType(OutputType type);

  // Item overrides.
  virtual Target* AsTarget() OVERRIDE;
  virtual const Target* AsTarget() const OVERRIDE;
  virtual bool OnResolved(Err* err) OVERRIDE;

  OutputType output_type() const { return output_type_; }
  void set_output_type(OutputType t) { output_type_ = t; }

  // Can be linked into other targets.
  bool IsLinkable() const;

  // Can have dependencies linked in.
  bool IsFinal() const;

  // Will be the empty string to use the target label as the output name.
  // See GetComputedOutputName().
  const std::string& output_name() const { return output_name_; }
  void set_output_name(const std::string& name) { output_name_ = name; }

  // Returns the output name for this target, which is the output_name if
  // specified, or the target label if not. If the flag is set, it will also
  // include any output prefix specified on the tool (often "lib" on Linux).
  //
  // Because this depends on the tool for this target, the toolchain must
  // have been set before calling.
  std::string GetComputedOutputName(bool include_prefix) const;

  const std::string& output_extension() const { return output_extension_; }
  void set_output_extension(const std::string& extension) {
    output_extension_ = extension;
  }

  const FileList& sources() const { return sources_; }
  FileList& sources() { return sources_; }

  // Set to true when all sources are public. This is the default. In this case
  // the public headers list should be empty.
  bool all_headers_public() const { return all_headers_public_; }
  void set_all_headers_public(bool p) { all_headers_public_ = p; }

  // When all_headers_public is false, this is the list of public headers. It
  // could be empty which would mean no headers are public.
  const FileList& public_headers() const { return public_headers_; }
  FileList& public_headers() { return public_headers_; }

  // Whether this target's includes should be checked by "gn check".
  bool check_includes() const { return check_includes_; }
  void set_check_includes(bool ci) { check_includes_ = ci; }

  // Whether this static_library target should have code linked in.
  bool complete_static_lib() const { return complete_static_lib_; }
  void set_complete_static_lib(bool complete) {
    DCHECK_EQ(STATIC_LIBRARY, output_type_);
    complete_static_lib_ = complete;
  }

  bool testonly() const { return testonly_; }
  void set_testonly(bool value) { testonly_ = value; }

  // Compile-time extra dependencies.
  const FileList& inputs() const { return inputs_; }
  FileList& inputs() { return inputs_; }

  // Runtime dependencies.
  const FileList& data() const { return data_; }
  FileList& data() { return data_; }

  // Returns true if targets depending on this one should have an order
  // dependency.
  bool hard_dep() const {
    return output_type_ == ACTION ||
           output_type_ == ACTION_FOREACH ||
           output_type_ == COPY_FILES;
  }

  // Linked private dependencies.
  const LabelTargetVector& private_deps() const { return private_deps_; }
  LabelTargetVector& private_deps() { return private_deps_; }

  // Linked public dependencies.
  const LabelTargetVector& public_deps() const { return public_deps_; }
  LabelTargetVector& public_deps() { return public_deps_; }

  // Non-linked dependencies.
  const LabelTargetVector& data_deps() const { return data_deps_; }
  LabelTargetVector& data_deps() { return data_deps_; }

  // List of configs that this class inherits settings from. Once a target is
  // resolved, this will also list all-dependent and public configs.
  const UniqueVector<LabelConfigPair>& configs() const { return configs_; }
  UniqueVector<LabelConfigPair>& configs() { return configs_; }

  // List of configs that all dependencies (direct and indirect) of this
  // target get. These configs are not added to this target. Note that due
  // to the way this is computed, there may be duplicates in this list.
  const UniqueVector<LabelConfigPair>& all_dependent_configs() const {
    return all_dependent_configs_;
  }
  UniqueVector<LabelConfigPair>& all_dependent_configs() {
    return all_dependent_configs_;
  }

  // List of configs that targets depending directly on this one get. These
  // configs are also added to this target.
  const UniqueVector<LabelConfigPair>& public_configs() const {
    return public_configs_;
  }
  UniqueVector<LabelConfigPair>& public_configs() {
    return public_configs_;
  }

  // A list of a subset of deps where we'll re-export public_configs as
  // public_configs of this target.
  const UniqueVector<LabelTargetPair>& forward_dependent_configs() const {
    return forward_dependent_configs_;
  }
  UniqueVector<LabelTargetPair>& forward_dependent_configs() {
    return forward_dependent_configs_;
  }

  // Dependencies that can include files from this target.
  const std::set<Label>& allow_circular_includes_from() const {
    return allow_circular_includes_from_;
  }
  std::set<Label>& allow_circular_includes_from() {
    return allow_circular_includes_from_;
  }

  const UniqueVector<const Target*>& inherited_libraries() const {
    return inherited_libraries_;
  }

  // This config represents the configuration set directly on this target.
  ConfigValues& config_values() { return config_values_; }
  const ConfigValues& config_values() const { return config_values_; }

  ActionValues& action_values() { return action_values_; }
  const ActionValues& action_values() const { return action_values_; }

  const OrderedSet<SourceDir>& all_lib_dirs() const { return all_lib_dirs_; }
  const OrderedSet<std::string>& all_libs() const { return all_libs_; }

  const std::set<const Target*>& recursive_hard_deps() const {
    return recursive_hard_deps_;
  }

  // The toolchain is only known once this target is resolved (all if its
  // dependencies are known). They will be null until then. Generally, this can
  // only be used during target writing.
  const Toolchain* toolchain() const { return toolchain_; }

  // Sets the toolchain. The toolchain must include a tool for this target
  // or the error will be set and the function will return false. Unusually,
  // this function's "err" output is optional since this is commonly used
  // frequently by unit tests which become needlessly verbose.
  bool SetToolchain(const Toolchain* toolchain, Err* err = NULL);

  // Returns outputs from this target. The link output file is the one that
  // other targets link to when they depend on this target. This will only be
  // valid for libraries and will be empty for all other target types.
  //
  // The dependency output file is the file that should be used to express
  // a dependency on this one. It could be the same as the link output file
  // (this will be the case for static libraries). For shared libraries it
  // could be the same or different than the link output file, depending on the
  // system. For actions this will be the stamp file.
  //
  // These are only known once the target is resolved and will be empty before
  // that. This is a cache of the files to prevent every target that depends on
  // a given library from recomputing the same pattern.
  const OutputFile& link_output_file() const {
    return link_output_file_;
  }
  const OutputFile& dependency_output_file() const {
    return dependency_output_file_;
  }

 private:
  // Pulls necessary information from dependencies to this one when all
  // dependencies have been resolved.
  void PullDependentTargetInfo();

  // These each pull specific things from dependencies to this one when all
  // deps have been resolved.
  void PullForwardedDependentConfigs();
  void PullForwardedDependentConfigsFrom(const Target* from);
  void PullRecursiveHardDeps();

  // Fills the link and dependency output files when a target is resolved.
  void FillOutputFiles();

  // Validates the given thing when a target is resolved.
  bool CheckVisibility(Err* err) const;
  bool CheckTestonly(Err* err) const;
  bool CheckNoNestedStaticLibs(Err* err) const;

  OutputType output_type_;
  std::string output_name_;
  std::string output_extension_;

  FileList sources_;
  bool all_headers_public_;
  FileList public_headers_;
  bool check_includes_;
  bool complete_static_lib_;
  bool testonly_;
  FileList inputs_;
  FileList data_;

  bool hard_dep_;

  LabelTargetVector private_deps_;
  LabelTargetVector public_deps_;
  LabelTargetVector data_deps_;

  UniqueVector<LabelConfigPair> configs_;
  UniqueVector<LabelConfigPair> all_dependent_configs_;
  UniqueVector<LabelConfigPair> public_configs_;
  UniqueVector<LabelTargetPair> forward_dependent_configs_;

  std::set<Label> allow_circular_includes_from_;

  bool external_;

  // Static libraries and source sets from transitive deps. These things need
  // to be linked only with the end target (executable, shared library). Source
  // sets do not get pushed beyond static library boundaries, and neither
  // source sets nor static libraries get pushed beyond sahred library
  // boundaries.
  UniqueVector<const Target*> inherited_libraries_;

  // These libs and dirs are inherited from statically linked deps and all
  // configs applying to this target.
  OrderedSet<SourceDir> all_lib_dirs_;
  OrderedSet<std::string> all_libs_;

  // All hard deps from this target and all dependencies. Filled in when this
  // target is marked resolved. This will not include the current target.
  std::set<const Target*> recursive_hard_deps_;

  ConfigValues config_values_;  // Used for all binary targets.
  ActionValues action_values_;  // Used for action[_foreach] targets.

  // Toolchain used by this target. Null until target is resolved.
  const Toolchain* toolchain_;

  // Output files. Null until the target is resolved.
  OutputFile link_output_file_;
  OutputFile dependency_output_file_;

  DISALLOW_COPY_AND_ASSIGN(Target);
};

namespace BASE_HASH_NAMESPACE {

#if defined(COMPILER_GCC)
template<> struct hash<const Target*> {
  std::size_t operator()(const Target* t) const {
    return reinterpret_cast<std::size_t>(t);
  }
};
#elif defined(COMPILER_MSVC)
inline size_t hash_value(const Target* t) {
  return reinterpret_cast<size_t>(t);
}
#endif  // COMPILER...

}  // namespace BASE_HASH_NAMESPACE

#endif  // TOOLS_GN_TARGET_H_
