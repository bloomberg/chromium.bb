// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/toolchain_manager.h"

#include <set>

#include "base/bind.h"
#include "build/build_config.h"
#include "tools/gn/err.h"
#include "tools/gn/item.h"
#include "tools/gn/item_node.h"
#include "tools/gn/item_tree.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/scope_per_file_provider.h"
#include "tools/gn/trace.h"

// How toolchain loading works
// ---------------------------
// When we start loading a build, we'll load the build config file and that
// will call set_default_toolchain. We'll schedule a load of the file
// containing the default toolchain definition, and can do this in parallel
// with all other build files. Targets will have an implicit dependency on the
// toolchain so we won't write out any files until we've loaded the toolchain
// definition.
//
// When we see a reference to a target using a different toolchain, it gets
// more complicated. In this case, the toolchain definition contains arguments
// to pass into the build config file when it is invoked in the context of that
// toolchain. As a result, we have to actually see the definition of the
// toolchain before doing anything else.
//
// So when we see a reference to a non-default toolchain we do the following:
//
//  1. Schedule a load of the file containing the toolchain definition, if it
//     isn't loaded already.
//  2. When the toolchain definition is loaded, we schedule a load of the
//     build config file in the context of that toolchain. We'll use the
//     arguments from the toolchain definition to execute it.
//  3. When the build config is set up, then we can load all of the individual
//     buildfiles in the context of that config that we require.

namespace {

enum ToolchainState {
  // Toolchain settings have not requested to be loaded. This means we
  // haven't seen any targets that require this toolchain yet. This means that
  // we have seen a toolchain definition, but no targets that use it. Not
  // loading the settings automatically allows you to define a bunch of
  // toolchains and potentially not use them without much overhead.
  TOOLCHAIN_NOT_LOADED,

  // The toolchain definition for non-default toolchains has been scheduled
  // to be loaded but has not completed. When this is done, we can load the
  // settings file. This is needed to get the arguments out of the toolchain
  // definition. This is skipped for the default toolchain which has no
  // arguments (see summary above).
  TOOLCHAIN_DEFINITION_LOADING,

  // The settings have been scheduled to be loaded but have not completed.
  TOOLCHAIN_SETTINGS_LOADING,

  // The settings are done being loaded.
  TOOLCHAIN_SETTINGS_LOADED
};

SourceFile DirToBuildFile(const SourceDir& dir) {
  return SourceFile(dir.value() + "BUILD.gn");
}

}  // namespace

struct ToolchainManager::Info {
  Info(const BuildSettings* build_settings,
       const Label& toolchain_name,
       const std::string& output_subdir_name)
      : state(TOOLCHAIN_NOT_LOADED),
        settings(build_settings, output_subdir_name),
        toolchain(new Toolchain(&settings, toolchain_name)),
        toolchain_set(false),
        toolchain_file_loaded(false),
        item_node(NULL) {
    settings.set_toolchain_label(toolchain_name);
  }

  ~Info() {
    if (!item_node)  // See toolchain definition for more.
      delete toolchain;
  }

  // Makes sure that an ItemNode is created for the toolchain, which lets
  // targets depend on the (potentially future) loading of the toolchain.
  //
  // We can't always do this at the beginning since when doing the default
  // build config, we don't know the toolchain name yet. We also need to go
  // through some effort to avoid doing this inside the toolchain manager's
  // lock (to avoid holding two locks at once).
  void EnsureItemNode() {
    if (!item_node) {
      ItemTree& tree = settings.build_settings()->item_tree();
      item_node = new ItemNode(toolchain);
      tree.AddNodeLocked(item_node);
    }
  }

  ToolchainState state;

  // The first place in the build that we saw a reference for this toolchain.
  // This allows us to report errors if it can't be loaded and blame some
  // reasonable place of the code. This will be empty for the default toolchain.
  LocationRange specified_from;

  // When the state is TOOLCHAIN_SETTINGS_LOADED, the settings should be
  // considered read-only and can be read without locking. Otherwise, they
  // should not be accessed at all except to load them (which can therefore
  // also be done outside of the lock). This works as long as the state flag
  // is only ever read or written inside the lock.
  Settings settings;

  // When we create an item node, this pointer will be owned by that node
  // so it's lifetime is managed by the dependency graph. Before we've created
  // the ItemNode, this class has to takre responsibility for this pointer.
  Toolchain* toolchain;
  bool toolchain_set;
  LocationRange toolchain_definition_location;

  // Set when the file corresponding to the toolchain definition is loaded.
  // This will normally be set right after "toolchain_set". However, if the
  // toolchain definition is missing, the file might be marked loaded but the
  // toolchain definition could still be unset.
  bool toolchain_file_loaded;

  // While state == TOOLCHAIN_SETTINGS_LOADING || TOOLCHAIN_DEFINITION_LOADING,
  // this will collect all scheduled invocations using this toolchain. They'll
  // be issued once the settings file has been interpreted.
  //
  // The map maps the source file to "some" location it was invoked from (so
  // we can give good error messages). It does NOT map to the root of the
  // file to be invoked (the file still needs loading). This will be NULL
  // for internally invoked files.
  typedef std::map<SourceFile, LocationRange> ScheduledInvocationMap;
  ScheduledInvocationMap scheduled_invocations;

  // Tracks all scheduled and executed invocations for this toolchain. This
  // is used to avoid invoking a file more than once for a toolchain.
  std::set<SourceFile> all_invocations;

  // Filled in by EnsureItemNode, see that for more.
  ItemNode* item_node;
};

ToolchainManager::ToolchainManager(const BuildSettings* build_settings)
    : build_settings_(build_settings) {
}

ToolchainManager::~ToolchainManager() {
  for (ToolchainMap::iterator i = toolchains_.begin();
       i != toolchains_.end(); ++i)
    delete i->second;
  toolchains_.clear();
}

void ToolchainManager::StartLoadingUnlocked(const SourceFile& build_file_name) {
  // How the default build config works: Initially we don't have a toolchain
  // name to call the settings for the default build config. So we create one
  // with an empty toolchain name and execute the default build config file.
  // When that's done, we'll go and fix up the name to the default build config
  // that the script set.
  base::AutoLock lock(GetLock());
  Err err;
  Info* info = LoadNewToolchainLocked(LocationRange(), Label(), &err);
  if (err.has_error())
    g_scheduler->FailWithError(err);
  CHECK(info);
  info->scheduled_invocations[build_file_name] = LocationRange();
  info->all_invocations.insert(build_file_name);

  if (!ScheduleBuildConfigLoadLocked(info, true, &err))
    g_scheduler->FailWithError(err);
}

const Settings* ToolchainManager::GetSettingsForToolchainLocked(
    const LocationRange& from_here,
    const Label& toolchain_name,
    Err* err) {
  GetLock().AssertAcquired();
  ToolchainMap::iterator found = toolchains_.find(toolchain_name);
  Info* info = NULL;
  if (found == toolchains_.end()) {
    info = LoadNewToolchainLocked(from_here, toolchain_name, err);
    if (!info)
      return NULL;
  } else {
    info = found->second;
  }
  info->EnsureItemNode();

  return &info->settings;
}

const Toolchain* ToolchainManager::GetToolchainDefinitionUnlocked(
    const Label& toolchain_name) {
  base::AutoLock lock(GetLock());
  ToolchainMap::iterator found = toolchains_.find(toolchain_name);
  if (found == toolchains_.end() || !found->second->toolchain_set)
    return NULL;

  // Since we don't allow defining a toolchain more than once, we know that
  // once it's set it won't be mutated, so we can safely return this pointer
  // for reading outside the lock.
  return found->second->toolchain;
}

bool ToolchainManager::SetDefaultToolchainUnlocked(
    const Label& default_toolchain,
    const LocationRange& defined_here,
    Err* err) {
  base::AutoLock lock(GetLock());
  if (!default_toolchain_.is_null()) {
    *err = Err(defined_here, "Default toolchain already set.");
    err->AppendSubErr(Err(default_toolchain_defined_here_,
                          "Previously defined here.",
                          "You can only set this once."));
    return false;
  }

  if (default_toolchain.is_null()) {
    *err = Err(defined_here, "Bad default toolchain name.",
        "You can't set the default toolchain name to nothing.");
    return false;
  }
  if (!default_toolchain.toolchain_dir().is_null() ||
      !default_toolchain.toolchain_name().empty()) {
    *err = Err(defined_here, "Toolchain name has toolchain.",
        "You can't specify a toolchain (inside the parens) for a toolchain "
        "name. I got:\n" + default_toolchain.GetUserVisibleName(true));
    return false;
  }

  default_toolchain_ = default_toolchain;
  default_toolchain_defined_here_ = defined_here;
  return true;
}

Label ToolchainManager::GetDefaultToolchainUnlocked() const {
  base::AutoLock lock(GetLock());
  return default_toolchain_;
}

bool ToolchainManager::SetToolchainDefinitionLocked(
    const Toolchain& tc,
    const LocationRange& defined_from,
    Err* err) {
  GetLock().AssertAcquired();

  ToolchainMap::iterator found = toolchains_.find(tc.label());
  Info* info = NULL;
  if (found == toolchains_.end()) {
    // New toolchain.
    info = LoadNewToolchainLocked(defined_from, tc.label(), err);
    if (!info)
      return false;
  } else {
    // It's important to preserve the exact Toolchain object in our tree since
    // it will be in the ItemTree and targets may have dependencies on it.
    info = found->second;
  }

  // The labels should match or else we're setting the wrong one!
  CHECK(info->toolchain->label() == tc.label());

  // Save the toolchain. We can just overwrite our definition.
  *info->toolchain = tc;

  if (info->toolchain_set) {
    *err = Err(defined_from, "Duplicate toolchain definition.");
    err->AppendSubErr(Err(
        info->toolchain_definition_location,
        "Previously defined here.",
        "A toolchain can only be defined once. One tricky way that this could\n"
        "happen is if your definition is itself in a file that's interpreted\n"
        "under different toolchains, which would result in multiple\n"
        "definitions as the file is loaded multiple times. So be sure your\n"
        "toolchain definitions are in files that either don't define any\n"
        "targets (probably best) or at least don't contain targets executed\n"
        "with more than one toolchain."));
    return false;
  }

  info->EnsureItemNode();

  info->toolchain_set = true;
  info->toolchain_definition_location = defined_from;
  return true;
}

bool ToolchainManager::ScheduleInvocationLocked(
    const LocationRange& specified_from,
    const Label& toolchain_name,
    const SourceDir& dir,
    Err* err) {
  GetLock().AssertAcquired();
  SourceFile build_file(DirToBuildFile(dir));

  // If there's no specified toolchain name, use the default.
  ToolchainMap::iterator found;
  if (toolchain_name.is_null())
    found = toolchains_.find(default_toolchain_);
  else
    found = toolchains_.find(toolchain_name);

  Info* info = NULL;
  if (found == toolchains_.end()) {
    // New toolchain.
    info = LoadNewToolchainLocked(specified_from, toolchain_name, err);
    if (!info)
      return false;
  } else {
    // Use existing one.
    info = found->second;
    if (info->all_invocations.find(build_file) !=
        info->all_invocations.end()) {
      // We've already seen this source file for this toolchain, don't need
      // to do anything.
      return true;
    }
  }

  info->all_invocations.insert(build_file);

  switch (info->state) {
    case TOOLCHAIN_NOT_LOADED:
      // Toolchain needs to be loaded. Start loading it and push this buildfile
      // on the wait queue. The actual toolchain build file will have been
      // scheduled to be loaded by LoadNewToolchainLocked() above, so we just
      // need to request that the build settings be loaded when that toolchain
      // file is done executing (recall toolchain files are executed in the
      // context of the default toolchain, which is why we need to do the extra
      // Info lookup in the make_pair).
      DCHECK(!default_toolchain_.is_null());
      pending_build_config_map_[
              std::make_pair(DirToBuildFile(toolchain_name.dir()),
                             toolchains_[default_toolchain_])] =
          info;
      info->scheduled_invocations[build_file] = specified_from;

      // Transition to the loading state.
      info->specified_from = specified_from;
      info->state = TOOLCHAIN_DEFINITION_LOADING;
      return true;

    case TOOLCHAIN_DEFINITION_LOADING:
    case TOOLCHAIN_SETTINGS_LOADING:
      // Toolchain is in the process of loading, push this buildfile on the
      // wait queue to run when the config is ready.
      info->scheduled_invocations[build_file] = specified_from;
      return true;

    case TOOLCHAIN_SETTINGS_LOADED:
      // Everything is ready, just schedule the build file to load.
      return ScheduleBackgroundInvoke(info, specified_from, build_file, err);

    default:
      NOTREACHED();
      return false;
  }
}

// static
std::string ToolchainManager::ToolchainToOutputSubdir(
    const Label& toolchain_name) {
  // For now just assume the toolchain name is always a valid dir name. We may
  // want to clean up the in the future.
  return toolchain_name.name();
}

ToolchainManager::Info* ToolchainManager::LoadNewToolchainLocked(
    const LocationRange& specified_from,
    const Label& toolchain_name,
    Err* err) {
  GetLock().AssertAcquired();
  Info* info = new Info(build_settings_,
                        toolchain_name,
                        ToolchainToOutputSubdir(toolchain_name));

  toolchains_[toolchain_name] = info;

  // Invoke the file containing the toolchain definition so that it gets
  // defined. The default one (label is empty) will be done spearately.
  if (!toolchain_name.is_null()) {
    // The default toolchain should be specified whenever we're requesting
    // another one. This is how we know under what context we should execute
    // the invoke for the toolchain file.
    CHECK(!default_toolchain_.is_null());
    ScheduleInvocationLocked(specified_from, default_toolchain_,
                             toolchain_name.dir(), err);
  }
  return info;
}

void ToolchainManager::FixupDefaultToolchainLocked() {
  // Now that we've run the default build config, we should know the
  // default toolchain name. Fix up our reference.
  // See Start() for more.
  GetLock().AssertAcquired();
  if (default_toolchain_.is_null()) {
    g_scheduler->FailWithError(Err(Location(),
        "Default toolchain not set.",
        "Your build config file \"" +
        build_settings_->build_config_file().value() +
        "\"\ndid not call set_default_toolchain(). This is needed so "
        "I know how to actually\ncompile your code."));
    return;
  }

  ToolchainMap::iterator old_default = toolchains_.find(Label());
  CHECK(old_default != toolchains_.end());
  Info* info = old_default->second;
  toolchains_[default_toolchain_] = info;
  toolchains_.erase(old_default);

  // Toolchain should not have been loaded in the build config file.
  CHECK(!info->toolchain_set);

  // We need to set the toolchain label now that we know it. There's no way
  // to set the label, but we can assign the toolchain to a new one. Loading
  // the build config can not change the toolchain, so we won't be overwriting
  // anything useful.
  *info->toolchain = Toolchain(&info->settings, default_toolchain_);
  info->settings.set_is_default(true);
  info->settings.set_toolchain_label(default_toolchain_);
  info->EnsureItemNode();

  // The default toolchain is loaded in greedy mode so all targets we
  // encounter are generated. Non-default toolchain settings stay in non-greedy
  // so we only generate the minimally required set.
  info->settings.set_greedy_target_generation(true);

  // Schedule a load of the toolchain build file.
  Err err;
  ScheduleInvocationLocked(LocationRange(), default_toolchain_,
                           default_toolchain_.dir(), &err);
  if (err.has_error())
    g_scheduler->FailWithError(err);
}

bool ToolchainManager::ScheduleBackgroundInvoke(
    Info* info,
    const LocationRange& specified_from,
    const SourceFile& build_file,
    Err* err) {
  g_scheduler->IncrementWorkCount();
  if (!g_scheduler->input_file_manager()->AsyncLoadFile(
           specified_from, build_settings_, build_file,
           base::Bind(&ToolchainManager::BackgroundInvoke,
                      base::Unretained(this), info, build_file),
           err)) {
    g_scheduler->DecrementWorkCount();
    return false;
  }
  return true;
}

bool ToolchainManager::ScheduleBuildConfigLoadLocked(Info* info,
                                                     bool is_default,
                                                     Err* err) {
  GetLock().AssertAcquired();

  g_scheduler->IncrementWorkCount();
  if (!g_scheduler->input_file_manager()->AsyncLoadFile(
           info->specified_from, build_settings_,
           build_settings_->build_config_file(),
           base::Bind(&ToolchainManager::BackgroundLoadBuildConfig,
                      base::Unretained(this), info, is_default),
           err)) {
    g_scheduler->DecrementWorkCount();
    return false;
  }
  info->state = TOOLCHAIN_SETTINGS_LOADING;
  return true;
}

void ToolchainManager::BackgroundLoadBuildConfig(Info* info,
                                                 bool is_default,
                                                 const ParseNode* root) {
  // Danger: No early returns without decrementing the work count.
  if (root && !g_scheduler->is_failed()) {
    // Nobody should be accessing settings at this point other than us since we
    // haven't marked it loaded, so we can do it outside the lock.
    Scope* base_config = info->settings.base_config();
    base_config->set_source_dir(SourceDir("//"));

    info->settings.build_settings()->build_args().SetupRootScope(
        base_config, info->toolchain->args());

    base_config->SetProcessingBuildConfig();
    if (is_default)
      base_config->SetProcessingDefaultBuildConfig();

    ScopedTrace trace(TraceItem::TRACE_FILE_EXECUTE,
        info->settings.build_settings()->build_config_file().value());
    trace.SetToolchain(info->settings.toolchain_label());

    const BlockNode* root_block = root->AsBlock();
    Err err;
    root_block->ExecuteBlockInScope(base_config, &err);

    trace.Done();

    base_config->ClearProcessingBuildConfig();
    if (is_default)
      base_config->ClearProcessingDefaultBuildConfig();

    if (err.has_error()) {
      g_scheduler->FailWithError(err);
    } else {
      // Base config processing succeeded.
      Info::ScheduledInvocationMap schedule_these;
      {
        base::AutoLock lock(GetLock());
        schedule_these.swap(info->scheduled_invocations);
        info->state = TOOLCHAIN_SETTINGS_LOADED;
        if (is_default)
          FixupDefaultToolchainLocked();
      }

      // Schedule build files waiting on this settings. There can be many so we
      // want to load them in parallel on the pool.
      for (Info::ScheduledInvocationMap::iterator i = schedule_these.begin();
           i != schedule_these.end() && !g_scheduler->is_failed(); ++i) {
        if (!ScheduleBackgroundInvoke(info, i->second, i->first, &err)) {
          g_scheduler->FailWithError(err);
          break;
        }
      }
    }
  }
  g_scheduler->DecrementWorkCount();
}

void ToolchainManager::BackgroundInvoke(const Info* info,
                                        const SourceFile& file_name,
                                        const ParseNode* root) {
  if (root && !g_scheduler->is_failed()) {
    if (g_scheduler->verbose_logging()) {
      g_scheduler->Log("Running", file_name.value() + " with toolchain " +
                       info->toolchain->label().GetUserVisibleName(false));
    }

    Scope our_scope(info->settings.base_config());
    ScopePerFileProvider per_file_provider(&our_scope);
    our_scope.set_source_dir(file_name.GetDir());

    ScopedTrace trace(TraceItem::TRACE_FILE_EXECUTE, file_name.value());
    trace.SetToolchain(info->settings.toolchain_label());

    Err err;
    root->Execute(&our_scope, &err);
    if (err.has_error())
      g_scheduler->FailWithError(err);

    trace.Done();

    {
      // Check to see if any build config invocations depend on this file and
      // invoke them.
      base::AutoLock lock(GetLock());
      BuildConfigInvokeMap::iterator found_file =
          pending_build_config_map_.find(std::make_pair(file_name, info));
      if (found_file != pending_build_config_map_.end()) {
        // The toolchain state should be waiting on the definition, which
        // should be the thing we just loaded.
        Info* info_to_load = found_file->second;
        DCHECK(info_to_load->state == TOOLCHAIN_DEFINITION_LOADING);
        DCHECK(!info_to_load->toolchain_file_loaded);
        info_to_load->toolchain_file_loaded = true;

        if (!ScheduleBuildConfigLoadLocked(info_to_load, false, &err))
          g_scheduler->FailWithError(err);
        pending_build_config_map_.erase(found_file);
      }
    }
  }

  g_scheduler->DecrementWorkCount();
}

base::Lock& ToolchainManager::GetLock() const {
  return build_settings_->item_tree().lock();
}
