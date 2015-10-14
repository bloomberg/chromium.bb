// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/substitution_writer.h"

namespace {

typedef std::set<const Config*> ConfigSet;

// Merges the public configs from the given target to the given config list.
void MergePublicConfigsFrom(const Target* from_target,
                            UniqueVector<LabelConfigPair>* dest) {
  const UniqueVector<LabelConfigPair>& pub = from_target->public_configs();
  dest->Append(pub.begin(), pub.end());
}

// Like MergePublicConfigsFrom above except does the "all dependent" ones. This
// additionally adds all configs to the all_dependent_configs_ of the dest
// target given in *all_dest.
void MergeAllDependentConfigsFrom(const Target* from_target,
                                  UniqueVector<LabelConfigPair>* dest,
                                  UniqueVector<LabelConfigPair>* all_dest) {
  for (const auto& pair : from_target->all_dependent_configs()) {
    all_dest->push_back(pair);
    dest->push_back(pair);
  }
}

Err MakeTestOnlyError(const Target* from, const Target* to) {
  return Err(from->defined_from(), "Test-only dependency not allowed.",
      from->label().GetUserVisibleName(false) + "\n"
      "which is NOT marked testonly can't depend on\n" +
      to->label().GetUserVisibleName(false) + "\n"
      "which is marked testonly. Only targets with \"testonly = true\"\n"
      "can depend on other test-only targets.\n"
      "\n"
      "Either mark it test-only or don't do this dependency.");
}

Err MakeStaticLibDepsError(const Target* from, const Target* to) {
  return Err(from->defined_from(),
             "Complete static libraries can't depend on static libraries.",
             from->label().GetUserVisibleName(false) +
                 "\n"
                 "which is a complete static library can't depend on\n" +
                 to->label().GetUserVisibleName(false) +
                 "\n"
                 "which is a static library.\n"
                 "\n"
                 "Use source sets for intermediate targets instead.");
}

// Set check_private_deps to true for the first invocation since a target
// can see all of its dependencies. For recursive invocations this will be set
// to false to follow only public dependency paths.
//
// Pass a pointer to an empty set for the first invocation. This will be used
// to avoid duplicate checking.
bool EnsureFileIsGeneratedByDependency(const Target* target,
                                       const OutputFile& file,
                                       bool check_private_deps,
                                       std::set<const Target*>* seen_targets) {
  if (seen_targets->find(target) != seen_targets->end())
    return false;  // Already checked this one and it's not found.
  seen_targets->insert(target);

  // Assume that we have relatively few generated inputs so brute-force
  // searching here is OK. If this becomes a bottleneck, consider storing
  // computed_outputs as a hash set.
  for (const OutputFile& cur : target->computed_outputs()) {
    if (file == cur)
      return true;
  }

  // Check all public dependencies (don't do data ones since those are
  // runtime-only).
  for (const auto& pair : target->public_deps()) {
    if (EnsureFileIsGeneratedByDependency(pair.ptr, file, false,
                                          seen_targets))
      return true;  // Found a path.
  }

  // Only check private deps if requested.
  if (check_private_deps) {
    for (const auto& pair : target->private_deps()) {
      if (EnsureFileIsGeneratedByDependency(pair.ptr, file, false,
                                            seen_targets))
        return true;  // Found a path.
    }
  }
  return false;
}

}  // namespace

Target::Target(const Settings* settings, const Label& label)
    : Item(settings, label),
      output_type_(UNKNOWN),
      all_headers_public_(true),
      check_includes_(true),
      complete_static_lib_(false),
      testonly_(false),
      toolchain_(nullptr) {
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
    case LOADABLE_MODULE:
      return "Loadable module";
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

bool Target::OnResolved(Err* err) {
  DCHECK(output_type_ != UNKNOWN);
  DCHECK(toolchain_) << "Toolchain should have been set before resolving.";

  // Copy our own dependent configs to the list of configs applying to us.
  configs_.Append(all_dependent_configs_.begin(), all_dependent_configs_.end());
  MergePublicConfigsFrom(this, &configs_);

  // Copy our own libs and lib_dirs to the final set. This will be from our
  // target and all of our configs. We do this specially since these must be
  // inherited through the dependency tree (other flags don't work this way).
  for (ConfigValuesIterator iter(this); !iter.done(); iter.Next()) {
    const ConfigValues& cur = iter.cur();
    all_lib_dirs_.append(cur.lib_dirs().begin(), cur.lib_dirs().end());
    all_libs_.append(cur.libs().begin(), cur.libs().end());
  }

  PullDependentTargets();
  PullPublicConfigs();
  PullRecursiveHardDeps();
  if (!ResolvePrecompiledHeaders(err))
    return false;

  FillOutputFiles();

  if (settings()->build_settings()->check_for_bad_items()) {
    if (!CheckVisibility(err))
      return false;
    if (!CheckTestonly(err))
      return false;
    if (!CheckNoNestedStaticLibs(err))
      return false;
    CheckSourcesGenerated();
  }

  return true;
}

bool Target::IsLinkable() const {
  return output_type_ == STATIC_LIBRARY || output_type_ == SHARED_LIBRARY;
}

bool Target::IsFinal() const {
  return output_type_ == EXECUTABLE ||
         output_type_ == SHARED_LIBRARY ||
         output_type_ == LOADABLE_MODULE ||
         (output_type_ == STATIC_LIBRARY && complete_static_lib_);
}

DepsIteratorRange Target::GetDeps(DepsIterationType type) const {
  if (type == DEPS_LINKED) {
    return DepsIteratorRange(DepsIterator(
        &public_deps_, &private_deps_, nullptr));
  }
  // All deps.
  return DepsIteratorRange(DepsIterator(
      &public_deps_, &private_deps_, &data_deps_));
}

std::string Target::GetComputedOutputName(bool include_prefix) const {
  DCHECK(toolchain_)
      << "Toolchain must be specified before getting the computed output name.";

  const std::string& name = output_name_.empty() ? label().name()
                                                 : output_name_;

  std::string result;
  if (include_prefix) {
    const Tool* tool = toolchain_->GetToolForTargetFinalOutput(this);
    if (tool) {
      // Only add the prefix if the name doesn't already have it.
      if (!base::StartsWith(name, tool->output_prefix(),
                            base::CompareCase::SENSITIVE))
        result = tool->output_prefix();
    }
  }
  result.append(name);
  return result;
}

bool Target::SetToolchain(const Toolchain* toolchain, Err* err) {
  DCHECK(!toolchain_);
  DCHECK_NE(UNKNOWN, output_type_);
  toolchain_ = toolchain;

  const Tool* tool = toolchain->GetToolForTargetFinalOutput(this);
  if (tool)
    return true;

  // Tool not specified for this target type.
  if (err) {
    *err = Err(defined_from(), "This target uses an undefined tool.",
        base::StringPrintf(
            "The target %s\n"
            "of type \"%s\"\n"
            "uses toolchain %s\n"
            "which doesn't have the tool \"%s\" defined.\n\n"
            "Alas, I can not continue.",
            label().GetUserVisibleName(false).c_str(),
            GetStringForOutputType(output_type_),
            label().GetToolchainLabel().GetUserVisibleName(false).c_str(),
            Toolchain::ToolTypeToName(
                toolchain->GetToolTypeForTargetFinalOutput(this)).c_str()));
  }
  return false;
}

void Target::PullDependentTarget(const Target* dep, bool is_public) {
  MergeAllDependentConfigsFrom(dep, &configs_, &all_dependent_configs_);
  MergePublicConfigsFrom(dep, &configs_);

  // Direct dependent libraries.
  if (dep->output_type() == STATIC_LIBRARY ||
      dep->output_type() == SHARED_LIBRARY ||
      dep->output_type() == SOURCE_SET)
    inherited_libraries_.Append(dep, is_public);

  if (dep->output_type() == SHARED_LIBRARY) {
    // Shared library dependendencies are inherited across public shared
    // library boundaries.
    //
    // In this case:
    //   EXE -> INTERMEDIATE_SHLIB --[public]--> FINAL_SHLIB
    // The EXE will also link to to FINAL_SHLIB. The public dependeny means
    // that the EXE can use the headers in FINAL_SHLIB so the FINAL_SHLIB
    // will need to appear on EXE's link line.
    //
    // However, if the dependency is private:
    //   EXE -> INTERMEDIATE_SHLIB --[private]--> FINAL_SHLIB
    // the dependency will not be propagated because INTERMEDIATE_SHLIB is
    // not granting permission to call functiosn from FINAL_SHLIB. If EXE
    // wants to use functions (and link to) FINAL_SHLIB, it will need to do
    // so explicitly.
    //
    // Static libraries and source sets aren't inherited across shared
    // library boundaries because they will be linked into the shared
    // library.
    inherited_libraries_.AppendPublicSharedLibraries(
        dep->inherited_libraries(), is_public);
  } else if (!dep->IsFinal()) {
    // The current target isn't linked, so propogate linked deps and
    // libraries up the dependency tree.
    inherited_libraries_.AppendInherited(dep->inherited_libraries(), is_public);

    // Inherited library settings.
    all_lib_dirs_.append(dep->all_lib_dirs());
    all_libs_.append(dep->all_libs());
  }
}

void Target::PullDependentTargets() {
  for (const auto& dep : public_deps_)
    PullDependentTarget(dep.ptr, true);
  for (const auto& dep : private_deps_)
    PullDependentTarget(dep.ptr, false);
}

void Target::PullPublicConfigs() {
  // Pull public configs from each of our dependency's public deps.
  for (const auto& dep : public_deps_)
    PullPublicConfigsFrom(dep.ptr);
}

void Target::PullPublicConfigsFrom(const Target* from) {
  public_configs_.Append(from->public_configs().begin(),
                         from->public_configs().end());
}

void Target::PullRecursiveHardDeps() {
  for (const auto& pair : GetDeps(DEPS_LINKED)) {
    if (pair.ptr->hard_dep())
      recursive_hard_deps_.insert(pair.ptr);

    // Android STL doesn't like insert(begin, end) so do it manually.
    // TODO(brettw) this can be changed to
    // insert(iter.target()->begin(), iter.target()->end())
    // when Android uses a better STL.
    for (std::set<const Target*>::const_iterator cur =
             pair.ptr->recursive_hard_deps().begin();
         cur != pair.ptr->recursive_hard_deps().end(); ++cur)
      recursive_hard_deps_.insert(*cur);
  }
}

void Target::FillOutputFiles() {
  const Tool* tool = toolchain_->GetToolForTargetFinalOutput(this);
  bool check_tool_outputs = false;
  switch (output_type_) {
    case GROUP:
    case SOURCE_SET:
    case COPY_FILES:
    case ACTION:
    case ACTION_FOREACH: {
      // These don't get linked to and use stamps which should be the first
      // entry in the outputs. These stamps are named
      // "<target_out_dir>/<targetname>.stamp".
      dependency_output_file_ = GetTargetOutputDirAsOutputFile(this);
      dependency_output_file_.value().append(GetComputedOutputName(true));
      dependency_output_file_.value().append(".stamp");
      break;
    }
    case EXECUTABLE:
    case LOADABLE_MODULE:
      // Executables and loadable modules don't get linked to, but the first
      // output is used for dependency management.
      CHECK_GE(tool->outputs().list().size(), 1u);
      check_tool_outputs = true;
      dependency_output_file_ =
          SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
              this, tool, tool->outputs().list()[0]);
      break;
    case STATIC_LIBRARY:
      // Static libraries both have dependencies and linking going off of the
      // first output.
      CHECK(tool->outputs().list().size() >= 1);
      check_tool_outputs = true;
      link_output_file_ = dependency_output_file_ =
          SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
              this, tool, tool->outputs().list()[0]);
      break;
    case SHARED_LIBRARY:
      CHECK(tool->outputs().list().size() >= 1);
      check_tool_outputs = true;
      if (tool->link_output().empty() && tool->depend_output().empty()) {
        // Default behavior, use the first output file for both.
        link_output_file_ = dependency_output_file_ =
            SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
                this, tool, tool->outputs().list()[0]);
      } else {
        // Use the tool-specified ones.
        if (!tool->link_output().empty()) {
          link_output_file_ =
              SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
                  this, tool, tool->link_output());
        }
        if (!tool->depend_output().empty()) {
          dependency_output_file_ =
              SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
                  this, tool, tool->depend_output());
        }
      }
      break;
    case UNKNOWN:
    default:
      NOTREACHED();
  }

  // Count all outputs from this tool as something generated by this target.
  if (check_tool_outputs) {
    SubstitutionWriter::ApplyListToLinkerAsOutputFile(
        this, tool, tool->outputs(), &computed_outputs_);

    // Output names aren't canonicalized in the same way that source files
    // are. For example, the tool outputs often use
    // {{some_var}}/{{output_name}} which expands to "./foo", but this won't
    // match "foo" which is what we'll compute when converting a SourceFile to
    // an OutputFile.
    for (auto& out : computed_outputs_)
      NormalizePath(&out.value());
  }

  // Also count anything the target has declared to be an output.
  std::vector<SourceFile> outputs_as_sources;
  action_values_.GetOutputsAsSourceFiles(this, &outputs_as_sources);
  for (const SourceFile& out : outputs_as_sources)
    computed_outputs_.push_back(OutputFile(settings()->build_settings(), out));
}

bool Target::ResolvePrecompiledHeaders(Err* err) {
  // Precompiled headers are stored on a ConfigValues struct. This way, the
  // build can set all the precompiled header settings in a config and apply
  // it to many targets. Likewise, the precompiled header values may be
  // specified directly on a target.
  //
  // Unlike other values on configs which are lists that just get concatenated,
  // the precompiled header settings are unique values. We allow them to be
  // specified anywhere, but if they are specified in more than one place all
  // places must match.

  // Track where the current settings came from for issuing errors.
  const Label* pch_header_settings_from = NULL;
  if (config_values_.has_precompiled_headers())
    pch_header_settings_from = &label();

  for (ConfigValuesIterator iter(this); !iter.done(); iter.Next()) {
    if (!iter.GetCurrentConfig())
      continue;  // Skip the one on the target itself.

    const Config* config = iter.GetCurrentConfig();
    const ConfigValues& cur = config->resolved_values();
    if (!cur.has_precompiled_headers())
      continue;  // This one has no precompiled header info, skip.

    if (config_values_.has_precompiled_headers()) {
      // Already have a precompiled header values, the settings must match.
      if (config_values_.precompiled_header() != cur.precompiled_header() ||
          config_values_.precompiled_source() != cur.precompiled_source()) {
        *err = Err(defined_from(),
            "Precompiled header setting conflict.",
            "The target " + label().GetUserVisibleName(false) + "\n"
            "has conflicting precompiled header settings.\n"
            "\n"
            "From " + pch_header_settings_from->GetUserVisibleName(false) +
            "\n  header: " + config_values_.precompiled_header() +
            "\n  source: " + config_values_.precompiled_source().value() +
            "\n\n"
            "From " + config->label().GetUserVisibleName(false) +
            "\n  header: " + cur.precompiled_header() +
            "\n  source: " + cur.precompiled_source().value());
        return false;
      }
    } else {
      // Have settings from a config, apply them to ourselves.
      pch_header_settings_from = &config->label();
      config_values_.set_precompiled_header(cur.precompiled_header());
      config_values_.set_precompiled_source(cur.precompiled_source());
    }
  }

  return true;
}

bool Target::CheckVisibility(Err* err) const {
  for (const auto& pair : GetDeps(DEPS_ALL)) {
    if (!Visibility::CheckItemVisibility(this, pair.ptr, err))
      return false;
  }
  return true;
}

bool Target::CheckTestonly(Err* err) const {
  // If the current target is marked testonly, it can include both testonly
  // and non-testonly targets, so there's nothing to check.
  if (testonly())
    return true;

  // Verify no deps have "testonly" set.
  for (const auto& pair : GetDeps(DEPS_ALL)) {
    if (pair.ptr->testonly()) {
      *err = MakeTestOnlyError(this, pair.ptr);
      return false;
    }
  }

  return true;
}

bool Target::CheckNoNestedStaticLibs(Err* err) const {
  // If the current target is not a complete static library, it can depend on
  // static library targets with no problem.
  if (!(output_type() == Target::STATIC_LIBRARY && complete_static_lib()))
    return true;

  // Verify no deps are static libraries.
  for (const auto& pair : GetDeps(DEPS_ALL)) {
    if (pair.ptr->output_type() == Target::STATIC_LIBRARY) {
      *err = MakeStaticLibDepsError(this, pair.ptr);
      return false;
    }
  }

  // Verify no inherited libraries are static libraries.
  for (const auto& lib : inherited_libraries().GetOrdered()) {
    if (lib->output_type() == Target::STATIC_LIBRARY) {
      *err = MakeStaticLibDepsError(this, lib);
      return false;
    }
  }
  return true;
}

void Target::CheckSourcesGenerated() const {
  // Checks that any inputs or sources to this target that are in the build
  // directory are generated by a target that this one transitively depends on
  // in some way. We already guarantee that all generated files are written
  // to the build dir.
  //
  // See Scheduler::AddUnknownGeneratedInput's declaration for more.
  for (const SourceFile& file : sources_)
    CheckSourceGenerated(file);
  for (const SourceFile& file : inputs_)
    CheckSourceGenerated(file);
}

void Target::CheckSourceGenerated(const SourceFile& source) const {
  if (!IsStringInOutputDir(settings()->build_settings()->build_dir(),
                           source.value()))
    return;  // Not in output dir, this is OK.

  // Tell the scheduler about unknown files. This will be noted for later so
  // the list of files written by the GN build itself (often response files)
  // can be filtered out of this list.
  OutputFile out_file(settings()->build_settings(), source);
  std::set<const Target*> seen_targets;
  if (!EnsureFileIsGeneratedByDependency(this, out_file, true, &seen_targets))
    g_scheduler->AddUnknownGeneratedInput(this, source);
}
