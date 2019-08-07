// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_BUNDLE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_BUNDLE_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

namespace policy {

// Maps policy namespaces to PolicyMaps.
class POLICY_EXPORT PolicyBundle {
 public:
  using MapType = std::map<PolicyNamespace, std::unique_ptr<PolicyMap>>;
  using iterator = MapType::iterator;
  using const_iterator = MapType::const_iterator;

  PolicyBundle();
  virtual ~PolicyBundle();

  // Returns the PolicyMap for namespace |ns|. Creates a new map if necessary.
  PolicyMap& Get(const PolicyNamespace& ns);
  const PolicyMap& Get(const PolicyNamespace& ns) const;

  // Swaps the internal representation of |this| with |other|.
  void Swap(PolicyBundle* other);

  // |this| becomes a copy of |other|. Any existing PolicyMaps are dropped.
  void CopyFrom(const PolicyBundle& other);

  // Merges the PolicyMaps of |this| with those of |other| for each namespace
  // in common. Also adds copies of the (namespace, PolicyMap) pairs in |other|
  // that don't have an entry in |this|.
  // Each policy in each PolicyMap is replaced only if the policy from |other|
  // has a higher priority.
  // See PolicyMap::MergeFrom for details on merging individual PolicyMaps.
  void MergeFrom(const PolicyBundle& other);

  // Returns true if |other| has the same keys and value as |this|.
  bool Equals(const PolicyBundle& other) const;

  // Returns iterators to the beginning and end of the underlying container.
  iterator begin() { return policy_bundle_.begin(); }
  iterator end() { return policy_bundle_.end(); }

  // These can be used to iterate over and read the PolicyMaps, but not to
  // modify them.
  const_iterator begin() const { return policy_bundle_.begin(); }
  const_iterator end() const { return policy_bundle_.end(); }

  // Erases all the existing pairs.
  void Clear();

 private:
  MapType policy_bundle_;

  // An empty PolicyMap that is returned by const Get() for namespaces that
  // do not exist in |policy_bundle_|.
  const PolicyMap kEmpty_;

  DISALLOW_COPY_AND_ASSIGN(PolicyBundle);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_BUNDLE_H_
