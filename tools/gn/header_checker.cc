// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/header_checker.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/builder.h"
#include "tools/gn/c_include_iterator.h"
#include "tools/gn/config.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/source_file_type.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

namespace {

struct PublicGeneratedPair {
  PublicGeneratedPair() : is_public(false), is_generated(false) {}
  bool is_public;
  bool is_generated;
};

// If the given file is in the "gen" folder, trims this so it treats the gen
// directory as the source root:
//   //out/Debug/gen/foo/bar.h -> //foo/bar.h
// If the file isn't in the generated root, returns the input unchanged.
SourceFile RemoveRootGenDirFromFile(const Target* target,
                                    const SourceFile& file) {
  const SourceDir& gen = target->settings()->toolchain_gen_dir();
  if (!gen.is_null() && StartsWithASCII(file.value(), gen.value(), true))
    return SourceFile("//" + file.value().substr(gen.value().size()));
  return file;
}

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

  return LocationRange(Location(clone_input_file,
                                range.begin().line_number(),
                                range.begin().char_offset(),
                                -1 /* TODO(scottmg) */),
                       Location(clone_input_file,
                                range.end().line_number(),
                                range.end().char_offset(),
                                -1 /* TODO(scottmg) */));
}

// Given a reverse dependency chain where the target chain[0]'s includes are
// being used by chain[end] and not all deps are public, returns the string
// describing the error.
std::string GetDependencyChainPublicError(
    const HeaderChecker::Chain& chain) {
  std::string ret = "The target:\n  " +
      chain[chain.size() - 1].target->label().GetUserVisibleName(false) +
      "\nis including a file from the target:\n  " +
      chain[0].target->label().GetUserVisibleName(false) +
      "\n";

  // Invalid chains should always be 0 (no chain) or more than two
  // (intermediate private dependencies). 1 and 2 are impossible because a
  // target can always include headers from itself and its direct dependents.
  DCHECK(chain.size() != 1 && chain.size() != 2);
  if (chain.empty()) {
    ret += "There is no dependency chain between these targets.";
  } else {
    // Indirect dependency chain, print the chain.
    ret += "\nIt's usually best to depend directly on the destination target.\n"
        "In some cases, the destination target is considered a subcomponent\n"
        "of an intermediate target. In this case, the intermediate target\n"
        "should depend publicly on the destination to forward the ability\n"
        "to include headers.\n"
        "\n"
        "Dependency chain (there may also be others):\n";

    for (int i = static_cast<int>(chain.size()) - 1; i >= 0; i--) {
      ret.append("  " + chain[i].target->label().GetUserVisibleName(false));
      if (i != 0) {
        // Identify private dependencies so the user can see where in the
        // dependency chain things went bad. Don't list this for the first link
        // in the chain since direct dependencies are OK, and listing that as
        // "private" may make people feel like they need to fix it.
        if (i == static_cast<int>(chain.size()) - 1 || chain[i - 1].is_public)
          ret.append(" -->");
        else
          ret.append(" --[private]-->");
      }
      ret.append("\n");
    }
  }
  return ret;
}

}  // namespace

HeaderChecker::HeaderChecker(const BuildSettings* build_settings,
                             const std::vector<const Target*>& targets)
    : main_loop_(base::MessageLoop::current()),
      build_settings_(build_settings) {
  for (size_t i = 0; i < targets.size(); i++)
    AddTargetToFileMap(targets[i], &file_map_);
}

HeaderChecker::~HeaderChecker() {
}

bool HeaderChecker::Run(const std::vector<const Target*>& to_check,
                        bool force_check,
                        std::vector<Err>* errors) {
  if (to_check.empty()) {
    // Check all files.
    RunCheckOverFiles(file_map_, force_check);
  } else {
    // Run only over the files in the given targets.
    FileMap files_to_check;
    for (size_t i = 0; i < to_check.size(); i++)
      AddTargetToFileMap(to_check[i], &files_to_check);
    RunCheckOverFiles(files_to_check, force_check);
  }

  if (errors_.empty())
    return true;
  *errors = errors_;
  return false;
}

void HeaderChecker::RunCheckOverFiles(const FileMap& files, bool force_check) {
  if (files.empty())
    return;

  scoped_refptr<base::SequencedWorkerPool> pool(
      new base::SequencedWorkerPool(16, "HeaderChecker"));
  for (FileMap::const_iterator file_i = files.begin();
       file_i != files.end(); ++file_i) {
    const TargetVector& vect = file_i->second;

    // Only check C-like source files (RC files also have includes).
    SourceFileType type = GetSourceFileType(file_i->first);
    if (type != SOURCE_CC && type != SOURCE_H && type != SOURCE_C &&
        type != SOURCE_M && type != SOURCE_MM && type != SOURCE_RC)
      continue;

    // Do a first pass to find if this should be skipped. All targets including
    // this source file must exclude it from checking, or any target
    // must mark it as generated (for cases where one target generates a file,
    // and another lists it as a source to compile it).
    if (!force_check) {
      bool check_includes = false;
      bool is_generated = false;
      for (size_t vect_i = 0; vect_i < vect.size(); ++vect_i) {
        check_includes |= vect[vect_i].target->check_includes();
        is_generated |= vect[vect_i].is_generated;
      }
      if (!check_includes || is_generated)
        continue;
    }

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
}

void HeaderChecker::DoWork(const Target* target, const SourceFile& file) {
  Err err;
  if (!CheckFile(target, file, &err)) {
    base::AutoLock lock(lock_);
    errors_.push_back(err);
  }
}

// static
void HeaderChecker::AddTargetToFileMap(const Target* target, FileMap* dest) {
  // Files in the sources have this public bit by default.
  bool default_public = target->all_headers_public();

  std::map<SourceFile, PublicGeneratedPair> files_to_public;

  // First collect the normal files, they get the default visibility. Always
  // trim the root gen dir if it exists. This will only exist on outputs of an
  // action, but those are often then wired into the sources of a compiled
  // target to actually compile generated code. If you depend on the compiled
  // target, it should be enough to be able to include the header.
  const Target::FileList& sources = target->sources();
  for (size_t i = 0; i < sources.size(); i++) {
    SourceFile file = RemoveRootGenDirFromFile(target, sources[i]);
    files_to_public[file].is_public = default_public;
  }

  // Add in the public files, forcing them to public. This may overwrite some
  // entries, and it may add new ones.
  const Target::FileList& public_list = target->public_headers();
  if (default_public)
    DCHECK(public_list.empty());  // List only used when default is not public.
  for (size_t i = 0; i < public_list.size(); i++) {
    SourceFile file = RemoveRootGenDirFromFile(target, public_list[i]);
    files_to_public[file].is_public = true;
  }

  // Add in outputs from actions. These are treated as public (since if other
  // targets can't use them, then there wouldn't be any point in outputting).
  std::vector<SourceFile> outputs;
  target->action_values().GetOutputsAsSourceFiles(target, &outputs);
  for (size_t i = 0; i < outputs.size(); i++) {
    // For generated files in the "gen" directory, add the filename to the
    // map assuming "gen" is the source root. This means that when files include
    // the generated header relative to there (the recommended practice), we'll
    // find the file.
    SourceFile output_file = RemoveRootGenDirFromFile(target, outputs[i]);
    PublicGeneratedPair* pair = &files_to_public[output_file];
    pair->is_public = true;
    pair->is_generated = true;
  }

  // Add the merged list to the master list of all files.
  for (std::map<SourceFile, PublicGeneratedPair>::const_iterator i =
           files_to_public.begin();
       i != files_to_public.end(); ++i) {
    (*dest)[i->first].push_back(TargetInfo(
        target, i->second.is_public, i->second.is_generated));
  }
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
        "The target:\n  " + from_target->label().GetUserVisibleName(false) +
        "\nhas a source file:\n  " + file.value() +
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
  Chain chain;  // Prevent reallocating in the loop.

  // For all targets containing this file, we require that at least one be
  // a direct or public dependency of the current target, and that the header
  // is public within the target.
  //
  // If there is more than one target containing this header, we may encounter
  // some error cases before finding a good one. This error stores the previous
  // one encountered, which we may or may not throw away.
  Err last_error;

  bool found_dependency = false;
  for (size_t i = 0; i < targets.size(); i++) {
    // We always allow source files in a target to include headers also in that
    // target.
    const Target* to_target = targets[i].target;
    if (to_target == from_target)
      return true;

    bool is_permitted_chain = false;
    if (IsDependencyOf(to_target, from_target, &chain, &is_permitted_chain)) {
      DCHECK(chain.size() >= 2);
      DCHECK(chain[0].target == to_target);
      DCHECK(chain[chain.size() - 1].target == from_target);

      found_dependency = true;

      if (targets[i].is_public && is_permitted_chain) {
        // This one is OK, we're done.
        last_error = Err();
        break;
      }

      // Diagnose the error.
      if (!targets[i].is_public) {
        // Danger: must call CreatePersistentRange to put in Err.
        last_error = Err(
            CreatePersistentRange(source_file, range),
            "Including a private header.",
            "This file is private to the target " +
                targets[i].target->label().GetUserVisibleName(false));
      } else if (!is_permitted_chain) {
        last_error = Err(
            CreatePersistentRange(source_file, range),
            "Can't include this header from here.",
                GetDependencyChainPublicError(chain));
      } else {
        NOTREACHED();
      }
    } else if (
        to_target->allow_circular_includes_from().find(from_target->label()) !=
        to_target->allow_circular_includes_from().end()) {
      // Not a dependency, but this include is whitelisted from the destination.
      found_dependency = true;
      last_error = Err();
      break;
    }
  }

  if (!found_dependency) {
    DCHECK(!last_error.has_error());

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
  if (last_error.has_error()) {
    // Found at least one dependency chain above, but it had an error.
    *err = last_error;
    return false;
  }

  // One thing we didn't check for is targets that expose their dependents
  // headers in their own public headers.
  //
  // Say we have A -> B -> C. If C has public_configs, everybody getting headers
  // from C should get the configs also or things could be out-of-sync. Above,
  // we check for A including C's headers directly, but A could also include a
  // header from B that in turn includes a header from C.
  //
  // There are two ways to solve this:
  //  - If a public header in B includes C, force B to publicly depend on C.
  //    This is possible to check, but might be super annoying because most
  //    targets (especially large leaf-node targets) don't declare
  //    public/private headers and you'll get lots of false positives.
  //
  //  - Save the includes found in each file and actually compute the graph of
  //    includes to detect when A implicitly includes C's header. This will not
  //    have the annoying false positive problem, but is complex to write.

  return true;
}

bool HeaderChecker::IsDependencyOf(const Target* search_for,
                                   const Target* search_from,
                                   Chain* chain,
                                   bool* is_permitted) const {
  if (search_for == search_from) {
    // A target is always visible from itself.
    *is_permitted = true;
    return false;
  }

  // Find the shortest public dependency chain.
  if (IsDependencyOf(search_for, search_from, true, chain)) {
    *is_permitted = true;
    return true;
  }

  // If not, try to find any dependency chain at all.
  if (IsDependencyOf(search_for, search_from, false, chain)) {
    *is_permitted = false;
    return true;
  }

  *is_permitted = false;
  return false;
}

bool HeaderChecker::IsDependencyOf(const Target* search_for,
                                   const Target* search_from,
                                   bool require_permitted,
                                   Chain* chain) const {
  // This method conducts a breadth-first search through the dependency graph
  // to find a shortest chain from search_from to search_for.
  //
  // work_queue maintains a queue of targets which need to be considered as
  // part of this chain, in the order they were first traversed.
  //
  // Each time a new transitive dependency of search_from is discovered for
  // the first time, it is added to work_queue and a "breadcrumb" is added,
  // indicating which target it was reached from when first discovered.
  //
  // Once this search finds search_for, the breadcrumbs are used to reconstruct
  // a shortest dependency chain (in reverse order) from search_from to
  // search_for.

  std::map<const Target*, ChainLink> breadcrumbs;
  std::queue<ChainLink> work_queue;
  work_queue.push(ChainLink(search_from, true));

  bool first_time = true;
  while (!work_queue.empty()) {
    ChainLink cur_link = work_queue.front();
    const Target* target = cur_link.target;
    work_queue.pop();

    if (target == search_for) {
      // Found it! Reconstruct the chain.
      chain->clear();
      while (target != search_from) {
        chain->push_back(cur_link);
        cur_link = breadcrumbs[target];
        target = cur_link.target;
      }
      chain->push_back(ChainLink(search_from, true));
      return true;
    }

    // Always consider public dependencies as possibilities.
    const LabelTargetVector& public_deps = target->public_deps();
    for (size_t i = 0; i < public_deps.size(); i++) {
      if (breadcrumbs.insert(
              std::make_pair(public_deps[i].ptr, cur_link)).second)
        work_queue.push(ChainLink(public_deps[i].ptr, true));
    }

    if (first_time || !require_permitted) {
      // Consider all dependencies since all target paths are allowed, so add
      // in private ones. Also do this the first time through the loop, since
      // a target can include headers from its direct deps regardless of
      // public/private-ness.
      first_time = false;
      const LabelTargetVector& private_deps = target->private_deps();
      for (size_t i = 0; i < private_deps.size(); i++) {
        if (breadcrumbs.insert(
                std::make_pair(private_deps[i].ptr, cur_link)).second)
          work_queue.push(ChainLink(private_deps[i].ptr, false));
      }
    }
  }

  return false;
}
