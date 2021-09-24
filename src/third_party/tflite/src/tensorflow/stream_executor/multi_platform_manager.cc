/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/stream_executor/multi_platform_manager.h"

#include "absl/container/flat_hash_map.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "tensorflow/core/platform/thread_annotations.h"
#include "tensorflow/stream_executor/lib/error.h"
#include "tensorflow/stream_executor/lib/initialize.h"

namespace stream_executor {
namespace {

class MultiPlatformManagerImpl {
 public:
  port::Status RegisterPlatform(std::unique_ptr<Platform> platform)
      TF_LOCKS_EXCLUDED(mu_);

  port::StatusOr<Platform*> PlatformWithName(absl::string_view target)
      TF_LOCKS_EXCLUDED(mu_);

  port::StatusOr<Platform*> PlatformWithId(const Platform::Id& id)
      TF_LOCKS_EXCLUDED(mu_);

  port::StatusOr<Platform*> PlatformWithName(absl::string_view target,
                                             bool initialize_platform)
      TF_LOCKS_EXCLUDED(mu_);

  port::StatusOr<Platform*> PlatformWithId(const Platform::Id& id,
                                           bool initialize_platform)
      TF_LOCKS_EXCLUDED(mu_);

  port::StatusOr<Platform*> InitializePlatformWithName(
      absl::string_view target,
      const std::map<std::string, std::string>& options) TF_LOCKS_EXCLUDED(mu_);
  port::StatusOr<Platform*> InitializePlatformWithId(
      const Platform::Id& id, const std::map<std::string, std::string>& options)
      TF_LOCKS_EXCLUDED(mu_);

  port::StatusOr<std::vector<Platform*>> PlatformsWithFilter(
      const std::function<bool(const Platform*)>& filter,
      bool initialize_platform) TF_LOCKS_EXCLUDED(mu_);

  using Listener = MultiPlatformManager::Listener;
  port::Status RegisterListener(std::unique_ptr<Listener> listener)
      TF_LOCKS_EXCLUDED(mu_);

 private:
  // Looks up the platform object with the given name.  Assumes the Platforms
  // mutex is held.
  port::StatusOr<Platform*> LookupByNameLocked(absl::string_view target)
      TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Looks up the platform object with the given id.  Assumes the Platforms
  // mutex is held.
  port::StatusOr<Platform*> LookupByIdLocked(const Platform::Id& id)
      TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Returns the names of the initialied platforms satisfying the given filter.
  // By default, it will return all initialized platform names.
  std::vector<std::string> InitializedPlatformNamesWithFilter(
      const std::function<bool(const Platform*)>& filter = [](const Platform*) {
        return true;
      }) TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  absl::Mutex mu_;
  std::vector<std::unique_ptr<Listener>> listeners_ TF_GUARDED_BY(mu_);
  absl::flat_hash_map<Platform::Id, Platform*> id_map_ TF_GUARDED_BY(mu_);
  absl::flat_hash_map<std::string, Platform*> name_map_ TF_GUARDED_BY(mu_);
};

port::Status MultiPlatformManagerImpl::RegisterPlatform(
    std::unique_ptr<Platform> platform) {
  CHECK(platform != nullptr);
  std::string key = absl::AsciiStrToLower(platform->Name());
  absl::MutexLock lock(&mu_);
  if (name_map_.find(key) != name_map_.end()) {
    return port::Status(port::error::INTERNAL,
                        "platform is already registered with name: \"" +
                            platform->Name() + "\"");
  }
  Platform* platform_ptr = platform.get();
  CHECK(id_map_.emplace(platform->id(), platform_ptr).second);
  // Release ownership/uniqueness to prevent destruction on program exit.
  // This avoids Platforms "cleaning up" on program exit, because otherwise,
  // there are _very_ tricky races between StreamExecutor and underlying
  // platforms (CUDA, OpenCL) during exit. Since these are fixed-size and 1x per
  // program, these are deemed acceptable.
  name_map_[key] = platform.release();
  for (const auto& listener : listeners_) {
    listener->PlatformRegistered(platform_ptr);
  }
  return port::Status::OK();
}

port::StatusOr<Platform*> MultiPlatformManagerImpl::PlatformWithName(
    absl::string_view target) {
  return PlatformWithName(target, /*initialize_platform=*/true);
}

port::StatusOr<Platform*> MultiPlatformManagerImpl::PlatformWithId(
    const Platform::Id& id) {
  return PlatformWithId(id, /*initialize_platform=*/true);
}

port::StatusOr<Platform*> MultiPlatformManagerImpl::PlatformWithName(
    absl::string_view target, bool initialize_platform) {
  absl::MutexLock lock(&mu_);

  SE_ASSIGN_OR_RETURN(Platform * platform, LookupByNameLocked(target));
  if (initialize_platform && !platform->Initialized()) {
    SE_RETURN_IF_ERROR(platform->Initialize({}));
  }

  return platform;
}

port::StatusOr<Platform*> MultiPlatformManagerImpl::PlatformWithId(
    const Platform::Id& id, bool initialize_platform) {
  absl::MutexLock lock(&mu_);

  SE_ASSIGN_OR_RETURN(Platform * platform, LookupByIdLocked(id));
  if (initialize_platform && !platform->Initialized()) {
    SE_RETURN_IF_ERROR(platform->Initialize({}));
  }

  return platform;
}

port::StatusOr<Platform*> MultiPlatformManagerImpl::InitializePlatformWithName(
    absl::string_view target,
    const std::map<std::string, std::string>& options) {
  absl::MutexLock lock(&mu_);

  SE_ASSIGN_OR_RETURN(Platform * platform, LookupByNameLocked(target));
  if (platform->Initialized()) {
    return port::Status(
        port::error::FAILED_PRECONDITION,
        absl::StrCat("platform \"", target, "\" is already initialized"));
  }

  SE_RETURN_IF_ERROR(platform->Initialize(options));

  return platform;
}

port::StatusOr<Platform*> MultiPlatformManagerImpl::InitializePlatformWithId(
    const Platform::Id& id, const std::map<std::string, std::string>& options) {
  absl::MutexLock lock(&mu_);

  SE_ASSIGN_OR_RETURN(Platform * platform, LookupByIdLocked(id));
  if (platform->Initialized()) {
    return port::Status(
        port::error::FAILED_PRECONDITION,
        absl::StrFormat("platform with id %p is already initialized", id));
  }

  SE_RETURN_IF_ERROR(platform->Initialize(options));

  return platform;
}

port::Status MultiPlatformManagerImpl::RegisterListener(
    std::unique_ptr<Listener> listener) {
  absl::MutexLock lock(&mu_);
  CHECK(id_map_.empty());
  CHECK(name_map_.empty());
  listeners_.push_back(std::move(listener));
  return port::Status::OK();
}

port::StatusOr<std::vector<Platform*>>
MultiPlatformManagerImpl::PlatformsWithFilter(
    const std::function<bool(const Platform*)>& filter,
    bool initialize_platform) {
  absl::MutexLock lock(&mu_);
  CHECK_EQ(id_map_.size(), name_map_.size());
  std::vector<Platform*> platforms;
  platforms.reserve(id_map_.size());
  for (const auto& entry : id_map_) {
    Platform* platform = entry.second;
    if (filter(platform)) {
      if (initialize_platform && !platform->Initialized()) {
        SE_RETURN_IF_ERROR(platform->Initialize({}));
      }
      platforms.push_back(platform);
    }
  }
  return platforms;
}

std::vector<std::string>
MultiPlatformManagerImpl::InitializedPlatformNamesWithFilter(
    const std::function<bool(const Platform*)>& filter) {
  CHECK_EQ(id_map_.size(), name_map_.size());
  std::vector<std::string> initialized_platforms_names;
  initialized_platforms_names.reserve(id_map_.size());
  for (const auto& entry : id_map_) {
    Platform* platform = entry.second;
    if (filter(platform)) {
      if (platform->Initialized()) {
        initialized_platforms_names.push_back(platform->Name());
      }
    }
  }
  return initialized_platforms_names;
}

port::StatusOr<Platform*> MultiPlatformManagerImpl::LookupByNameLocked(
    absl::string_view target) {
  auto it = name_map_.find(absl::AsciiStrToLower(target));
  if (it == name_map_.end()) {
    return port::Status(
        port::error::NOT_FOUND,
        absl::StrCat("Could not find registered platform with name: \"", target,
                     "\". Available platform names are: ",
                     absl::StrJoin(InitializedPlatformNamesWithFilter(), " ")));
  }
  return it->second;
}

port::StatusOr<Platform*> MultiPlatformManagerImpl::LookupByIdLocked(
    const Platform::Id& id) {
  auto it = id_map_.find(id);
  if (it == id_map_.end()) {
    return port::Status(
        port::error::NOT_FOUND,
        absl::StrFormat("could not find registered platform with id: %p", id));
  }
  return it->second;
}

MultiPlatformManagerImpl& Impl() {
  static MultiPlatformManagerImpl* impl = new MultiPlatformManagerImpl;
  return *impl;
}

}  // namespace

/*static*/ port::Status MultiPlatformManager::RegisterPlatform(
    std::unique_ptr<Platform> platform) {
  return Impl().RegisterPlatform(std::move(platform));
}

/*static*/ port::StatusOr<Platform*> MultiPlatformManager::PlatformWithName(
    absl::string_view target) {
  return Impl().PlatformWithName(target);
}

/*static*/ port::StatusOr<Platform*> MultiPlatformManager::PlatformWithId(
    const Platform::Id& id) {
  return Impl().PlatformWithId(id);
}

/*static*/ port::StatusOr<Platform*> MultiPlatformManager::PlatformWithId(
    const Platform::Id& id, bool initialize_platform) {
  return Impl().PlatformWithId(id, initialize_platform);
}

/*static*/ port::StatusOr<Platform*> MultiPlatformManager::PlatformWithName(
    absl::string_view target, bool initialize_platform) {
  return Impl().PlatformWithName(target, initialize_platform);
}

/*static*/ port::StatusOr<Platform*>
MultiPlatformManager::InitializePlatformWithName(
    absl::string_view target,
    const std::map<std::string, std::string>& options) {
  return Impl().InitializePlatformWithName(target, options);
}

/*static*/ port::StatusOr<Platform*>
MultiPlatformManager::InitializePlatformWithId(
    const Platform::Id& id, const std::map<std::string, std::string>& options) {
  return Impl().InitializePlatformWithId(id, options);
}

/*static*/ port::Status MultiPlatformManager::RegisterListener(
    std::unique_ptr<Listener> listener) {
  return Impl().RegisterListener(std::move(listener));
}

/*static*/ port::StatusOr<std::vector<Platform*>>
MultiPlatformManager::PlatformsWithFilter(
    const std::function<bool(const Platform*)>& filter) {
  return PlatformsWithFilter(filter, /*initialize_platform=*/true);
}

/*static*/ port::StatusOr<std::vector<Platform*>>
MultiPlatformManager::PlatformsWithFilter(
    const std::function<bool(const Platform*)>& filter,
    bool initialize_platform) {
  return Impl().PlatformsWithFilter(filter, initialize_platform);
}

}  // namespace stream_executor

REGISTER_MODULE_INITIALIZER(
    multi_platform_manager,
    {
        // Nothing -- this is just a module initializer
        // definition to reference for sequencing
        // purposes from Platform subclasses that register
        // themselves with the MultiPlatformManager.
    });

REGISTER_MODULE_INITIALIZER(
    multi_platform_manager_listener,
    {
        // Nothing -- this is just a module initializer definition to reference
        // for sequencing registration of listeners with the
        // MultiPlatformManager.
    });

// Listener registration should happen before platform registration.
REGISTER_MODULE_INITIALIZER_SEQUENCE(multi_platform_manager_listener,
                                     multi_platform_manager);
