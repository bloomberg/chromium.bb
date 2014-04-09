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
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

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

  CIncludeIterator iter(contents);
  base::StringPiece current_include;
  while (iter.GetNextIncludeString(&current_include)) {
    SourceFile include = SourceFileForInclude(current_include);
    if (!CheckInclude(from_target, file, include, err))
      return false;
  }

  return true;
}

// If the file exists, it must be in a dependency of the given target, and it
// must be public in that dependency.
bool HeaderChecker::CheckInclude(const Target* from_target,
                                 const SourceFile& source_file,
                                 const SourceFile& include_file,
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

  // For all targets containing this file, we require that at least one be
  // a dependency of the current target, and all targets that are dependencies
  // must have the file listed in the public section.
  bool found_dependency = false;
  for (size_t i = 0; i < targets.size(); i++) {
    // We always allow source files in a target to include headers also in that
    // target.
    if (targets[i].target == from_target)
      return true;

    if (IsDependencyOf(targets[i].target, from_target)) {
      // The include is in a target that's a proper dependency. Now verify
      // that the include is a public file.
      if (!targets[i].is_public) {
        // Depending on a private header.
        std::string msg = "The file " + source_file.value() +
            "\nincludes " + include_file.value() +
            "\nwhich is private to the target " +
            targets[i].target->label().GetUserVisibleName(false);

        // TODO(brettw) blame the including file.
        *err = Err(NULL, "Including a private header.", msg);
        return false;
      }
      found_dependency = true;
    }
  }

  if (!found_dependency) {
    std::string msg =
        source_file.value() + " includes the header\n" +
        include_file.value() + " which is not in any dependency of\n" +
        from_target->label().GetUserVisibleName(false);
    msg += "\n\nThe include file is in the target(s):\n";
    for (size_t i = 0; i < targets.size(); i++)
      msg += "  " + targets[i].target->label().GetUserVisibleName(false) + "\n";

    msg += "\nMake sure one of these is a direct or indirect dependency\n"
        "of " + from_target->label().GetUserVisibleName(false);

    // TODO(brettw) blame the including file.
    // Probably this means making and leaking an input file for it, and also
    // tracking the locations for each include.
    *err = Err(NULL, "Include not allowed.", msg);
    return false;
  }

  return true;
}

bool HeaderChecker::IsDependencyOf(const Target* search_for,
                                   const Target* search_from) const {
  std::set<const Target*> checked;
  return IsDependencyOf(search_for, search_from, &checked);
}

bool HeaderChecker::IsDependencyOf(const Target* search_for,
                                   const Target* search_from,
                                   std::set<const Target*>* checked) const {
  if (checked->find(search_for) != checked->end())
    return false;  // Already checked this subtree.

  const LabelTargetVector& deps = search_from->deps();
  for (size_t i = 0; i < deps.size(); i++) {
    if (deps[i].ptr == search_for)
      return true;  // Found it.

    // Recursive search.
    checked->insert(deps[i].ptr);
    if (IsDependencyOf(search_for, deps[i].ptr, checked))
      return true;
  }

  return false;
}
