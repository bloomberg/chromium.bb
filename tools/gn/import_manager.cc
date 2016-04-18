// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/import_manager.h"

#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope_per_file_provider.h"

namespace {

// Returns a newly-allocated scope on success, null on failure.
std::unique_ptr<Scope> UncachedImport(const Settings* settings,
                                      const SourceFile& file,
                                      const ParseNode* node_for_err,
                                      Err* err) {
  const ParseNode* node = g_scheduler->input_file_manager()->SyncLoadFile(
      node_for_err->GetRange(), settings->build_settings(), file, err);
  if (!node)
    return nullptr;

  std::unique_ptr<Scope> scope(new Scope(settings->base_config()));
  scope->set_source_dir(file.GetDir());

  // Don't allow ScopePerFileProvider to provide target-related variables.
  // These will be relative to the imported file, which is probably not what
  // people mean when they use these.
  ScopePerFileProvider per_file_provider(scope.get(), false);

  scope->SetProcessingImport();
  node->Execute(scope.get(), err);
  if (err->has_error())
    return nullptr;
  scope->ClearProcessingImport();

  return scope;
}

}  // namesapce

ImportManager::ImportManager() {
}

ImportManager::~ImportManager() {
}

bool ImportManager::DoImport(const SourceFile& file,
                             const ParseNode* node_for_err,
                             Scope* scope,
                             Err* err) {
  // See if we have a cached import, but be careful to actually do the scope
  // copying outside of the lock.
  const Scope* imported_scope = nullptr;
  {
    base::AutoLock lock(lock_);
    ImportMap::const_iterator found = imports_.find(file);
    if (found != imports_.end())
      imported_scope = found->second.get();
  }

  if (!imported_scope) {
    // Do a new import of the file.
    std::unique_ptr<Scope> new_imported_scope =
        UncachedImport(scope->settings(), file, node_for_err, err);
    if (!new_imported_scope)
      return false;

    // We loaded the file outside the lock. This means that there could be a
    // race and the file was already loaded on a background thread. Recover
    // from this and use the existing one if that happens.
    {
      base::AutoLock lock(lock_);
      ImportMap::const_iterator found = imports_.find(file);
      if (found != imports_.end()) {
        imported_scope = found->second.get();
      } else {
        imported_scope = new_imported_scope.get();
        imports_[file] = std::move(new_imported_scope);
      }
    }
  }

  Scope::MergeOptions options;
  options.skip_private_vars = true;
  options.mark_dest_used = true;  // Don't require all imported values be used.
  return imported_scope->NonRecursiveMergeTo(scope, options, node_for_err,
                                             "import", err);
}
