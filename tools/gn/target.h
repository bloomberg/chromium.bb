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
#include "tools/gn/source_file.h"
#include "tools/gn/unique_vector.h"

class InputFile;
class Settings;
class Token;

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
  virtual void OnResolved() OVERRIDE;

  OutputType output_type() const { return output_type_; }
  void set_output_type(OutputType t) { output_type_ = t; }

  bool IsLinkable() const;

  // Will be the empty string to use the target label as the output name.
  const std::string& output_name() const { return output_name_; }
  void set_output_name(const std::string& name) { output_name_ = name; }

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

  // Linked dependencies.
  const LabelTargetVector& deps() const { return deps_; }
  LabelTargetVector& deps() { return deps_; }

  // Non-linked dependencies.
  const LabelTargetVector& datadeps() const { return datadeps_; }
  LabelTargetVector& datadeps() { return datadeps_; }

  // List of configs that this class inherits settings from. Once a target is
  // resolved, this will also list all- and direct-dependent configs.
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
  // configs are not added to this target.
  const UniqueVector<LabelConfigPair>& direct_dependent_configs() const {
    return direct_dependent_configs_;
  }
  UniqueVector<LabelConfigPair>& direct_dependent_configs() {
    return direct_dependent_configs_;
  }

  // A list of a subset of deps where we'll re-export direct_dependent_configs
  // as direct_dependent_configs of this target.
  const UniqueVector<LabelTargetPair>& forward_dependent_configs() const {
    return forward_dependent_configs_;
  }
  UniqueVector<LabelTargetPair>& forward_dependent_configs() {
    return forward_dependent_configs_;
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

 private:
  // Pulls necessary information from dependencies to this one when all
  // dependencies have been resolved.
  void PullDependentTargetInfo();

  // These each pull specific things from dependencies to this one when all
  // deps have been resolved.
  void PullForwardedDependentConfigs();
  void PullRecursiveHardDeps();

  OutputType output_type_;
  std::string output_name_;
  std::string output_extension_;

  FileList sources_;
  bool all_headers_public_;
  FileList public_headers_;
  FileList inputs_;
  FileList data_;

  bool hard_dep_;

  // Note that if there are any groups in the deps, once the target is resolved
  // these vectors will list *both* the groups as well as the groups' deps.
  //
  // This is because, in general, groups should be "transparent" ways to add
  // groups of dependencies, so adding the groups deps make this happen with
  // no additional complexity when iterating over a target's deps.
  //
  // However, a group may also have specific settings and configs added to it,
  // so we also need the group in the list so we find these things. But you
  // shouldn't need to look inside the deps of the group since those will
  // already be added.
  LabelTargetVector deps_;
  LabelTargetVector datadeps_;

  UniqueVector<LabelConfigPair> configs_;
  UniqueVector<LabelConfigPair> all_dependent_configs_;
  UniqueVector<LabelConfigPair> direct_dependent_configs_;
  UniqueVector<LabelTargetPair> forward_dependent_configs_;

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
