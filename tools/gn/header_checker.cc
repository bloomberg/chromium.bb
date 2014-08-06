// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/header_checker.h"

#include <algorithm>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/builder.h"
#include "tools/gn/c_include_iterator.h"
#include "tools/gn/config.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

namespace {

// This class makes InputFiles on the stack as it reads files to check. When
// we throw an error, the Err indicates a locatin which has a pointer to
// an InputFile that must persist as long as the Err does.
//
// To make this work, this function creates a clone of the InputFile managed
// by the InputFileManager so the error can refer to something that
// persists. This means that the current file contents will live as long as
// the program, but this is OK since we're erroring out anyway.
LocationRange CreatePersistentRange(const InputFile& input_file,
                                    const LocationRange& range) {
  InputFile* clone_input_file;
  std::vector<Token>* tokens;  // Don't care about this.
  scoped_ptr<ParseNode>* parse_root;  // Don't care about this.

  g_scheduler->input_file_manager()->AddDynamicInput(
      input_file.name(), &clone_input_file, &tokens, &parse_root);
  clone_input_file->SetContents(input_file.contents());

  return LocationRange(
      Location(clone_input_file, range.begin().line_number(),
               range.begin().char_offset()),
      Location(clone_input_file, range.end().line_number(),
               range.end().char_offset()));
}

// Returns true if the given config could affect how the compiler runs (rather
// than being empty or just affecting linker flags).
bool ConfigHasCompilerSettings(const Config* config) {
  const ConfigValues& values = config->config_values();
  return
      !values.cflags().empty() ||
      !values.cflags_c().empty() ||
      !values.cflags_cc().empty() ||
      !values.cflags_objc().empty() ||
      !values.cflags_objcc().empty() ||
      !values.defines().empty() ||
      !values.include_dirs().empty();
}

// Returns true if the given target has any direct dependent configs with
// compiler settings in it.
bool HasDirectDependentCompilerSettings(const Target* target) {
  const UniqueVector<LabelConfigPair>& direct =
      target->direct_dependent_configs();
  for (size_t i = 0; i < direct.size(); i++) {
    if (ConfigHasCompilerSettings(direct[i].ptr))
      return true;
  }
  return false;
}

// Given a reverse dependency chain where the target chain[0]'s dependent
// configs don't apply to chain[end], returns the string describing the error.
// The problematic index is the target where the dependent configs were lost.
std::string GetDependentConfigChainError(
    const std::vector<const Target*>& chain,
    size_t problematic_index) {
  // Erroneous dependent config chains are always at least three long, since
  // dependent configs would apply if it was length two.
  DCHECK(chain.size() >= 3);

  std::string from_label =
      chain[chain.size() - 1]->label().GetUserVisibleName(false);
  std::string to_label =
      chain[0]->label().GetUserVisibleName(false);
  std::string problematic_label =
      chain[problematic_index]->label().GetUserVisibleName(false);
  std::string problematic_upstream_label =
      chain[problematic_index - 1]->label().GetUserVisibleName(false);

  return
      "You have the dependency tree:  SOURCE -> MID -> DEST\n"
      "Where a file from:\n"
      "  SOURCE = " + from_label + "\n"
      "is including a header from:\n"
      "  DEST = " + to_label + "\n"
      "\n"
      "DEST has direct_dependent_configs, and they don't apply to SOURCE "
      "because\nSOURCE is more than one hop away. This means that DEST's "
      "headers might not\nreceive the expected compiler flags.\n"
      "\n"
      "To fix this, make SOURCE depend directly on DEST.\n"
      "\n"
      "Alternatively, if the target:\n"
      "  MID = " + problematic_label + "\n"
      "exposes DEST as part of its public API, you can declare this by "
      "adding:\n"
      "  forward_dependent_configs_from = [\n"
      "    \"" + problematic_upstream_label + "\"\n"
      "  ]\n"
      "to MID. This will apply DEST's direct dependent configs to SOURCE.\n";
}

}  // namespace

HeaderChecker::HeaderChecker(const BuildSettings* build_settings,
                             const std::vector<const Target*>& targets)
    : main_loop_(base::MessageLoop::current()),
      build_settings_(build_settings) {
  for (size_t i = 0; i < targets.size(); i++)
    AddTargetToFileMap(targets[i]);
}

HeaderChecker::~HeaderChecker() {
}

bool HeaderChecker::Run(std::vector<Err>* errors) {
  ScopedTrace trace(TraceItem::TRACE_CHECK_HEADERS, "Check headers");

  if (file_map_.empty())
    return true;

  scoped_refptr<base::SequencedWorkerPool> pool(
      new base::SequencedWorkerPool(16, "HeaderChecker"));
  for (FileMap::const_iterator file_i = file_map_.begin();
       file_i != file_map_.end(); ++file_i) {
    const TargetVector& vect = file_i->second;

    // Only check C-like source files (RC files also have includes).
    SourceFileType type = GetSourceFileType(file_i->first);
    if (type != SOURCE_CC && type != SOURCE_H && type != SOURCE_C &&
        type != SOURCE_M && type != SOURCE_MM && type != SOURCE_RC)
      continue;

    for (size_t vect_i = 0; vect_i < vect.size(); ++vect_i) {
      pool->PostWorkerTaskWithShutdownBehavior(
          FROM_HERE,
          base::Bind(&HeaderChecker::DoWork, this,
                     vect[vect_i].target, file_i->first),
          base::SequencedWorkerPool::BLOCK_SHUTDOWN);
    }
  }

  // After this call we're single-threaded again.
  pool->Shutdown();

  if (errors_.empty())
    return true;
  *errors = errors_;
  return false;
}

void HeaderChecker::DoWork(const Target* target, const SourceFile& file) {
  Err err;
  if (!CheckFile(target, file, &err)) {
    base::AutoLock lock(lock_);
    errors_.push_back(err);
  }
}

void HeaderChecker::AddTargetToFileMap(const Target* target) {
  // Files in the sources have this public bit by default.
  bool default_public = target->all_headers_public();

  // First collect the normal files, they get the default visibility.
  std::map<SourceFile, bool> files_to_public;
  const Target::FileList& sources = target->sources();
  for (size_t i = 0; i < sources.size(); i++)
    files_to_public[sources[i]] = default_public;

  // Add in the public files, forcing them to public. This may overwrite some
  // entries, and it may add new ones.
  const Target::FileList& public_list = target->public_headers();
  if (default_public)
    DCHECK(public_list.empty());  // List only used when default is not public.
  for (size_t i = 0; i < public_list.size(); i++)
    files_to_public[public_list[i]] = true;

  // Add the merged list to the master list of all files.
  for (std::map<SourceFile, bool>::const_iterator i = files_to_public.begin();
       i != files_to_public.end(); ++i)
    file_map_[i->first].push_back(TargetInfo(target, i->second));
}

bool HeaderChecker::IsFileInOuputDir(const SourceFile& file) const {
  const std::string& build_dir = build_settings_->build_dir().value();
  return file.value().compare(0, build_dir.size(), build_dir) == 0;
}

// This current assumes all include paths are relative to the source root
// which is generally the case for Chromium.
//
// A future enhancement would be to search the include path for the target
// containing the source file containing this include and find the file to
// handle the cases where people do weird things with the paths.
SourceFile HeaderChecker::SourceFileForInclude(
    const base::StringPiece& input) const {
  std::string str("//");
  input.AppendToString(&str);
  return SourceFile(str);
}

bool HeaderChecker::CheckFile(const Target* from_target,
                              const SourceFile& file,
                              Err* err) const {
  ScopedTrace trace(TraceItem::TRACE_CHECK_HEADER, file.value());

  // Sometimes you have generated source files included as sources in another
  // target. These won't exist at checking time. Since we require all generated
  // files to be somewhere in the output tree, we can just check the name to
  // see if they should be skipped.
  if (IsFileInOuputDir(file))
    return true;

  base::FilePath path = build_settings_->GetFullPath(file);
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    *err = Err(from_target->defined_from(), "Source file not found.",
        "This target includes as a source:\n  " + file.value() +
        "\nwhich was not found.");
    return false;
  }

  InputFile input_file(file);
  input_file.SetContents(contents);

  CIncludeIterator iter(&input_file);
  base::StringPiece current_include;
  LocationRange range;
  while (iter.GetNextIncludeString(&current_include, &range)) {
    SourceFile include = SourceFileForInclude(current_include);
    if (!CheckInclude(from_target, input_file, include, range, err))
      return false;
  }

  return true;
}

// If the file exists:
//  - It must be in one or more dependencies of the given target.
//  - Those dependencies must have visibility from the source file.
//  - The header must be in the public section of those dependeices.
//  - Those dependencies must either have no direct dependent configs with
//    flags that affect the compiler, or those direct dependent configs apply
//    to the "from_target" (it's one "hop" away). This ensures that if the
//    include file needs needs compiler settings to compile it, that those
//    settings are applied to the file including it.
bool HeaderChecker::CheckInclude(const Target* from_target,
                                 const InputFile& source_file,
                                 const SourceFile& include_file,
                                 const LocationRange& range,
                                 Err* err) const {
  // Assume if the file isn't declared in our sources that we don't need to
  // check it. It would be nice if we could give an error if this happens, but
  // our include finder is too primitive and returns all includes, even if
  // they're in a #if not executed in the current build. In that case, it's
  // not unusual for the buildfiles to not specify that header at all.
  FileMap::const_iterator found = file_map_.find(include_file);
  if (found == file_map_.end())
    return true;

  const TargetVector& targets = found->second;
  std::vector<const Target*> chain;  // Prevent reallocating in the loop.

  // For all targets containing this file, we require that at least one be
  // a dependency of the current target, and all targets that are dependencies
  // must have the file listed in the public section.
  bool found_dependency = false;
  for (size_t i = 0; i < targets.size(); i++) {
    // We always allow source files in a target to include headers also in that
    // target.
    const Target* to_target = targets[i].target;
    if (to_target == from_target)
      return true;

    if (IsDependencyOf(to_target, from_target, &chain)) {
      DCHECK(chain.size() >= 2);
      DCHECK(chain[0] == to_target);
      DCHECK(chain[chain.size() - 1] == from_target);

      // The include is in a target that's a proper dependency. Verify that
      // the including target has visibility.
      if (!to_target->visibility().CanSeeMe(from_target->label())) {
        std::string msg = "The included file is in " +
            to_target->label().GetUserVisibleName(false) +
            "\nwhich is not visible from " +
            from_target->label().GetUserVisibleName(false) +
            "\n(see \"gn help visibility\").";

        // Danger: must call CreatePersistentRange to put in Err.
        *err = Err(CreatePersistentRange(source_file, range),
            "Including a header from non-visible target.", msg);
        return false;
      }

      // The file must be public in the target.
      if (!targets[i].is_public) {
        // Danger: must call CreatePersistentRange to put in Err.
        *err = Err(CreatePersistentRange(source_file, range),
                   "Including a private header.",
                   "This file is private to the target " +
                       targets[i].target->label().GetUserVisibleName(false));
        return false;
      }

      // If the to_target has direct_dependent_configs, they must apply to the
      // from_target.
      if (HasDirectDependentCompilerSettings(to_target)) {
        size_t problematic_index;
        if (!DoDirectDependentConfigsApply(chain, &problematic_index)) {
          *err = Err(CreatePersistentRange(source_file, range),
                     "Can't include this header from here.",
                     GetDependentConfigChainError(chain, problematic_index));
          return false;
        }
      }

      found_dependency = true;
    }
  }

  if (!found_dependency) {
    std::string msg = "It is not in any dependency of " +
        from_target->label().GetUserVisibleName(false);
    msg += "\nThe include file is in the target(s):\n";
    for (size_t i = 0; i < targets.size(); i++)
      msg += "  " + targets[i].target->label().GetUserVisibleName(false) + "\n";
    if (targets.size() > 1)
      msg += "at least one of ";
    msg += "which should somehow be reachable from " +
        from_target->label().GetUserVisibleName(false);

    // Danger: must call CreatePersistentRange to put in Err.
    *err = Err(CreatePersistentRange(source_file, range),
               "Include not allowed.", msg);
    return false;
  }

  // One thing we didn't check for is targets that expose their dependents
  // headers in their own public headers.
  //
  // Say we have A -> B -> C. If C has direct_dependent_configs, everybody
  // getting headers from C should get the configs also or things could be
  // out-of-sync. Above, we check for A including C's headers directly, but A
  // could also include a header from B that in turn includes a header from C.
  //
  // There are two ways to solve this:
  //  - If a public header in B includes C, force B to forward C's direct
  //    dependent configs. This is possible to check, but might be super
  //    annoying because most targets (especially large leaf-node targets)
  //    don't declare public/private headers and you'll get lots of false
  //    positives.
  //
  //  - Save the includes found in each file and actually compute the graph of
  //    includes to detect when A implicitly includes C's header. This will not
  //    have the annoying false positive problem, but is complex to write.

  return true;
}

bool HeaderChecker::IsDependencyOf(const Target* search_for,
                                   const Target* search_from,
                                   std::vector<const Target*>* chain) const {
  std::set<const Target*> checked;
  return IsDependencyOf(search_for, search_from, chain, &checked);
}

bool HeaderChecker::IsDependencyOf(const Target* search_for,
                                   const Target* search_from,
                                   std::vector<const Target*>* chain,
                                   std::set<const Target*>* checked) const {
  if (checked->find(search_for) != checked->end())
    return false;  // Already checked this subtree.

  const LabelTargetVector& deps = search_from->deps();
  for (size_t i = 0; i < deps.size(); i++) {
    if (deps[i].ptr == search_for) {
      // Found it.
      chain->clear();
      chain->push_back(deps[i].ptr);
      chain->push_back(search_from);
      return true;
    }

    // Recursive search.
    checked->insert(deps[i].ptr);
    if (IsDependencyOf(search_for, deps[i].ptr, chain, checked)) {
      chain->push_back(search_from);
      return true;
    }
  }

  return false;
}

// static
bool HeaderChecker::DoDirectDependentConfigsApply(
    const std::vector<const Target*>& chain,
    size_t* problematic_index) {
  // Direct dependent configs go up the chain one level with the following
  // exceptions:
  // - Skip over groups
  // - Skip anything that explicitly forwards it

  // All chains should be at least two (or it wouldn't be a chain).
  DCHECK(chain.size() >= 2);

  // A chain of length 2 is always OK as far as direct dependent configs are
  // concerned since the targets are direct dependents.
  if (chain.size() == 2)
    return true;

  // Check the middle configs to make sure they're either groups or configs
  // are forwarded.
  for (size_t i = 1; i < chain.size() - 1; i++) {
    if (chain[i]->output_type() == Target::GROUP)
      continue;  // This one is OK, skip to next one.

    // The forward list on this target should have contained in it the target
    // at the next lower level.
    const UniqueVector<LabelTargetPair>& forwarded =
        chain[i]->forward_dependent_configs();
    if (std::find_if(forwarded.begin(), forwarded.end(),
                     LabelPtrPtrEquals<Target>(chain[i - 1])) ==
        forwarded.end()) {
      *problematic_index = i;
      return false;
    }
  }

  return true;
}
