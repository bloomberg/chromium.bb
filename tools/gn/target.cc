// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target.h"

#include "base/bind.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/scheduler.h"

namespace {

typedef std::set<const Config*> ConfigSet;

// Merges the dependent configs from the given target to the given config list.
// The unique_configs list is used for de-duping so values already added will
// not be added again.
void MergeDirectDependentConfigsFrom(const Target* from_target,
                                     ConfigSet* unique_configs,
                                     LabelConfigVector* dest) {
  const LabelConfigVector& direct = from_target->direct_dependent_configs();
  for (size_t i = 0; i < direct.size(); i++) {
    if (unique_configs->find(direct[i].ptr) == unique_configs->end()) {
      unique_configs->insert(direct[i].ptr);
      dest->push_back(direct[i]);
    }
  }
}

// Like MergeDirectDependentConfigsFrom above except does the "all dependent"
// ones. This additionally adds all configs to the all_dependent_configs_ of
// the dest target given in *all_dest.
void MergeAllDependentConfigsFrom(const Target* from_target,
                                  ConfigSet* unique_configs,
                                  LabelConfigVector* dest,
                                  LabelConfigVector* all_dest) {
  const LabelConfigVector& all = from_target->all_dependent_configs();
  for (size_t i = 0; i < all.size(); i++) {
    // Always add it to all_dependent_configs_ since it might not be in that
    // list even if we've seen it applied to this target before. This may
    // introduce some duplicates in all_dependent_configs_, but those will
    // we removed when they're actually applied to a target.
    all_dest->push_back(all[i]);
    if (unique_configs->find(all[i].ptr) == unique_configs->end()) {
      // One we haven't seen yet, also apply it to ourselves.
      dest->push_back(all[i]);
      unique_configs->insert(all[i].ptr);
    }
  }
}

}  // namespace

Target::Target(const Settings* settings, const Label& label)
    : Item(settings, label),
      output_type_(UNKNOWN),
      all_headers_public_(true),
      hard_dep_(false) {
}

Target::~Target() {
}

// static
const char* Target::GetStringForOutputType(OutputType type) {
  switch (type) {
    case UNKNOWN:
      return "Unknown";
    case GROUP:
      return "Group";
    case EXECUTABLE:
      return "Executable";
    case SHARED_LIBRARY:
      return "Shared library";
    case STATIC_LIBRARY:
      return "Static library";
    case SOURCE_SET:
      return "Source set";
    case COPY_FILES:
      return "Copy";
    case ACTION:
      return "Action";
    case ACTION_FOREACH:
      return "ActionForEach";
    default:
      return "";
  }
}

Target* Target::AsTarget() {
  return this;
}

const Target* Target::AsTarget() const {
  return this;
}

void Target::OnResolved() {
  DCHECK(output_type_ != UNKNOWN);

  // Convert any groups we depend on to just direct dependencies on that
  // group's deps. We insert the new deps immediately after the group so that
  // the ordering is preserved. We need to keep the original group so that any
  // flags, etc. that it specifies itself are applied to us.
  for (size_t i = 0; i < deps_.size(); i++) {
    const Target* dep = deps_[i].ptr;
    if (dep->output_type_ == GROUP) {
      deps_.insert(deps_.begin() + i + 1, dep->deps_.begin(), dep->deps_.end());
      i += dep->deps_.size();
    }
  }

  // Only add each config once. First remember the target's configs.
  ConfigSet unique_configs;
  for (size_t i = 0; i < configs_.size(); i++)
    unique_configs.insert(configs_[i].ptr);

  // Copy our own dependent configs to the list of configs applying to us.
  for (size_t i = 0; i < all_dependent_configs_.size(); i++) {
    if (unique_configs.find(all_dependent_configs_[i].ptr) ==
        unique_configs.end()) {
      unique_configs.insert(all_dependent_configs_[i].ptr);
      configs_.push_back(all_dependent_configs_[i]);
    }
  }
  for (size_t i = 0; i < direct_dependent_configs_.size(); i++) {
    if (unique_configs.find(direct_dependent_configs_[i].ptr) ==
        unique_configs.end()) {
      unique_configs.insert(direct_dependent_configs_[i].ptr);
      configs_.push_back(direct_dependent_configs_[i]);
    }
  }

  // Copy our own libs and lib_dirs to the final set. This will be from our
  // target and all of our configs. We do this specially since these must be
  // inherited through the dependency tree (other flags don't work this way).
  for (ConfigValuesIterator iter(this); !iter.done(); iter.Next()) {
    const ConfigValues& cur = iter.cur();
    all_lib_dirs_.append(cur.lib_dirs().begin(), cur.lib_dirs().end());
    all_libs_.append(cur.libs().begin(), cur.libs().end());
  }

  if (output_type_ != GROUP) {
    // Don't pull target info like libraries and configs from dependencies into
    // a group target. When A depends on a group G, the G's dependents will
    // be treated as direct dependencies of A, so this is unnecessary and will
    // actually result in duplicated settings (since settings will also be
    // pulled from G to A in case G has configs directly on it).
    PullDependentTargetInfo(&unique_configs);
  }
  PullForwardedDependentConfigs();
  PullRecursiveHardDeps();
}

bool Target::IsLinkable() const {
  return output_type_ == STATIC_LIBRARY || output_type_ == SHARED_LIBRARY;
}

void Target::PullDependentTargetInfo(std::set<const Config*>* unique_configs) {
  // Gather info from our dependents we need.
  for (size_t dep_i = 0; dep_i < deps_.size(); dep_i++) {
    const Target* dep = deps_[dep_i].ptr;
    MergeAllDependentConfigsFrom(dep, unique_configs, &configs_,
                                 &all_dependent_configs_);
    MergeDirectDependentConfigsFrom(dep, unique_configs, &configs_);

    // Direct dependent libraries.
    if (dep->output_type() == STATIC_LIBRARY ||
        dep->output_type() == SHARED_LIBRARY ||
        dep->output_type() == SOURCE_SET)
      inherited_libraries_.insert(dep);

    // Inherited libraries and flags are inherited across static library
    // boundaries.
    if (dep->output_type() != SHARED_LIBRARY &&
        dep->output_type() != EXECUTABLE) {
      const std::set<const Target*> inherited = dep->inherited_libraries();
      for (std::set<const Target*>::const_iterator i = inherited.begin();
           i != inherited.end(); ++i)
        inherited_libraries_.insert(*i);

      // Inherited library settings.
      all_lib_dirs_.append(dep->all_lib_dirs());
      all_libs_.append(dep->all_libs());
    }
  }
}

void Target::PullForwardedDependentConfigs() {
  // Groups implicitly forward all if its dependency's configs.
  if (output_type() == GROUP)
    forward_dependent_configs_ = deps_;

  // Forward direct dependent configs if requested.
  for (size_t dep = 0; dep < forward_dependent_configs_.size(); dep++) {
    const Target* from_target = forward_dependent_configs_[dep].ptr;

    // The forward_dependent_configs_ must be in the deps already, so we
    // don't need to bother copying to our configs, only forwarding.
    DCHECK(std::find_if(deps_.begin(), deps_.end(),
                        LabelPtrPtrEquals<Target>(from_target)) !=
           deps_.end());
    direct_dependent_configs_.insert(
        direct_dependent_configs_.end(),
        from_target->direct_dependent_configs().begin(),
        from_target->direct_dependent_configs().end());
  }
}

void Target::PullRecursiveHardDeps() {
  for (size_t dep_i = 0; dep_i < deps_.size(); dep_i++) {
    const Target* dep = deps_[dep_i].ptr;
    if (dep->hard_dep())
      recursive_hard_deps_.insert(dep);

    // Android STL doesn't like insert(begin, end) so do it manually.
    // TODO(brettw) this can be changed to insert(dep->begin(), dep->end()) when
    // Android uses a better STL.
    for (std::set<const Target*>::const_iterator cur =
             dep->recursive_hard_deps().begin();
         cur != dep->recursive_hard_deps().end(); ++cur)
      recursive_hard_deps_.insert(*cur);
  }
}
