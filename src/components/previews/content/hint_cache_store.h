// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_STORE_H_
#define COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_STORE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/version.h"

namespace optimization_guide {
namespace proto {
class Hint;
}  // namespace proto
}  // namespace optimization_guide

namespace previews {

// Abstract base class for the HintCache backing store, which is responsible for
// storing all hints that are locally available. While the HintCache itself may
// retain some hints in a memory cache, all of its hints are initially loaded
// asynchronously by the store. All calls to the HintCacheStore must be made
// from the same thread.
class HintCacheStore {
 public:
  using EntryKey = std::string;
  using HintLoadedCallback = base::OnceCallback<void(
      const std::string&,
      std::unique_ptr<optimization_guide::proto::Hint>)>;

  // Abstract base class for storing hint component update data. Concrete
  // derived classes store the data in a form usable by their creating concrete
  // HintCacheStore. The data itself is populated by moving component hints into
  // it on a background thread; it is then used to update the store's component
  // data on the UI thread. Each concrete HintCacheStore class must implement
  // its own concrete ComponentUpdateData, which provides a store-specific
  // implementation of MoveHintIntoUpdateData().
  class ComponentUpdateData {
   public:
    explicit ComponentUpdateData(const base::Version& version)
        : version_(version) {
      DCHECK(version_.IsValid());
    }
    virtual ~ComponentUpdateData() = default;

    const base::Version& version() const { return version_; }

    // Pure virtual function for moving a hint into ComponentUpdateData. After
    // MoveHintIntoUpdateData() is called, |hint| is no longer valid.
    virtual void MoveHintIntoUpdateData(
        optimization_guide::proto::Hint&& hint) = 0;

   private:
    // The component version of the update data.
    base::Version version_;
  };

  HintCacheStore() = default;
  virtual ~HintCacheStore() = default;

  // Pure virtual function for initializing the store. What initialization
  // entails is dependent upon the concrete store. If |purge_existing_data| is
  // set to true, then the cache is purged during initialization and starts in
  // a fresh state. When initialization completes, the provided callback is run
  // asynchronously.
  virtual void Initialize(bool purge_existing_data,
                          base::OnceClosure callback) = 0;

  // Pure virtual function for creating and returning a concrete
  // ComponentUpdateData object. This object is used to collect hints within a
  // component in a format usable by the concrete HintCacheStore on a background
  // thread and is later returned to the store in UpdateComponentData(). The
  // ComponentUpdateData object is only created when the provided component
  // version is newer than the store's version, indicating fresh hints. If the
  // component's version is not newer than the store's version, then no
  // ComponentUpdateData is created and nullptr is returned. This prevents
  // unnecessary processing of the component's hints by the caller.
  std::unique_ptr<ComponentUpdateData> virtual MaybeCreateComponentUpdateData(
      const base::Version& version) const = 0;

  // Pure virtual function for updating the component data (both version and
  // hints) contained within the store. When this is called, all pre-existing
  // component data within the store is purged and only the new data is
  // retained. After the store is fully updated with the new component data,
  // the callback is run asynchronously
  virtual void UpdateComponentData(
      std::unique_ptr<ComponentUpdateData> component_data,
      base::OnceClosure callback) = 0;

  // Pure virtual function for finding a hint entry key associated with the
  // specified host suffix. Returns true if a hint entry key is found, in which
  // case |out_hint_entry_key| is populated with the key.
  virtual bool FindHintEntryKey(const std::string& host_suffix,
                                EntryKey* out_hint_entry_key) const = 0;

  // Pure virtual function for loading the hint specified by |hint_entry_key|.
  // After the load finishes, the hint data is passed to |callback|. In the case
  // where the hint cannot be loaded, the callback is run with a nullptr.
  // Depending on the load result, the callback may be synchronous or
  // asynchronous.
  virtual void LoadHint(const EntryKey& hint_entry_key,
                        HintLoadedCallback callback) = 0;
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_STORE_H_
