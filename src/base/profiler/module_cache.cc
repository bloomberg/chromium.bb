// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/module_cache.h"

#include <iterator>
#include <utility>

#include "base/ranges/algorithm.h"

namespace base {

namespace {

// Supports heterogeneous comparisons on modules and addresses, for use in
// binary searching modules sorted by range for a contained address.
struct ModuleAddressCompare {
  bool operator()(const std::unique_ptr<const ModuleCache::Module>& module,
                  uintptr_t address) const {
    return module->GetBaseAddress() + module->GetSize() <= address;
  }

  bool operator()(
      uintptr_t address,
      const std::unique_ptr<const ModuleCache::Module>& module) const {
    return address < module->GetBaseAddress();
  }
};

}  // namespace

ModuleCache::ModuleCache() = default;
ModuleCache::~ModuleCache() = default;

const ModuleCache::Module* ModuleCache::GetModuleForAddress(uintptr_t address) {
  if (const ModuleCache::Module* module = GetExistingModuleForAddress(address))
    return module;

  std::unique_ptr<const Module> new_module = CreateModuleForAddress(address);
  if (!new_module)
    return nullptr;
  const auto result = native_modules_.insert(std::move(new_module));
  // TODO(https://crbug.com/1131769): Reintroduce DCHECK(result.second) after
  // fixing the issue that is causing it to fail.
  return result.first->get();
}

std::vector<const ModuleCache::Module*> ModuleCache::GetModules() const {
  std::vector<const Module*> result;
  result.reserve(native_modules_.size());
  for (const std::unique_ptr<const Module>& module : native_modules_)
    result.push_back(module.get());
  for (const std::unique_ptr<const Module>& module : non_native_modules_)
    result.push_back(module.get());
  return result;
}

void ModuleCache::UpdateNonNativeModules(
    const std::vector<const Module*>& defunct_modules,
    std::vector<std::unique_ptr<const Module>> new_modules) {
  // Insert the modules to remove into a set to support O(log(n)) lookup below.
  flat_set<const Module*> defunct_modules_set(defunct_modules.begin(),
                                              defunct_modules.end());

  // Reorder the modules to be removed to the last slots in the set, then move
  // them to the inactive modules, then erase the moved-from modules from the
  // set. This is a variation on the standard erase-remove idiom, which is
  // explicitly endorsed for implementing erase behavior on flat_sets.
  //
  // stable_partition is O(m*log(r)) where m is the number of current modules
  // and r is the number of modules to remove. insert and erase are both O(r).
  auto first_module_defunct_modules = ranges::stable_partition(
      non_native_modules_,
      [&defunct_modules_set](const std::unique_ptr<const Module>& module) {
        return defunct_modules_set.find(module.get()) ==
               defunct_modules_set.end();
      });
  // All modules requested to be removed should have been found.
  DCHECK_EQ(
      static_cast<ptrdiff_t>(defunct_modules.size()),
      std::distance(first_module_defunct_modules, non_native_modules_.end()));
  inactive_non_native_modules_.insert(
      inactive_non_native_modules_.end(),
      std::make_move_iterator(first_module_defunct_modules),
      std::make_move_iterator(non_native_modules_.end()));
  non_native_modules_.erase(first_module_defunct_modules,
                            non_native_modules_.end());

  // Insert the modules to be added. This operation is O((m + a) + a*log(a))
  // where m is the number of current modules and a is the number of modules to
  // be added.
  const size_t prior_non_native_modules_size = non_native_modules_.size();
  non_native_modules_.insert(std::make_move_iterator(new_modules.begin()),
                             std::make_move_iterator(new_modules.end()));
  // Every module in |new_modules| should have been moved into
  // |non_native_modules_|. This guards against use-after-frees if |new_modules|
  // were to contain any modules equivalent to what's already in
  // |non_native_modules_|, in which case the module would remain in
  // |new_modules| and be deleted on return from the function. While this
  // scenario would be a violation of the API contract, it would present a
  // difficult-to-track-down crash scenario.
  CHECK_EQ(prior_non_native_modules_size + new_modules.size(),
           non_native_modules_.size());
}

void ModuleCache::AddCustomNativeModule(std::unique_ptr<const Module> module) {
  const bool was_inserted = native_modules_.insert(std::move(module)).second;
  // |module| should have been inserted into |native_modules_|, indicating that
  // there was no equivalent module already present. While this scenario would
  // be a violation of the API contract, it would present a
  // difficult-to-track-down crash scenario.
  CHECK(was_inserted);
}

const ModuleCache::Module* ModuleCache::GetExistingModuleForAddress(
    uintptr_t address) const {
  const auto non_native_module_loc = non_native_modules_.find(address);
  if (non_native_module_loc != non_native_modules_.end())
    return non_native_module_loc->get();

  const auto native_module_loc = native_modules_.find(address);
  if (native_module_loc != native_modules_.end())
    return native_module_loc->get();

  return nullptr;
}

bool ModuleCache::ModuleAndAddressCompare::operator()(
    const std::unique_ptr<const Module>& m1,
    const std::unique_ptr<const Module>& m2) const {
  return m1->GetBaseAddress() < m2->GetBaseAddress();
}

bool ModuleCache::ModuleAndAddressCompare::operator()(
    const std::unique_ptr<const Module>& m1,
    uintptr_t address) const {
  return m1->GetBaseAddress() + m1->GetSize() <= address;
}

bool ModuleCache::ModuleAndAddressCompare::operator()(
    uintptr_t address,
    const std::unique_ptr<const Module>& m2) const {
  return address < m2->GetBaseAddress();
}

}  // namespace base
