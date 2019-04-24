// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_TARGET_DISTRIBUTION_H_
#define MEDIA_LEARNING_IMPL_TARGET_DISTRIBUTION_H_

#include <ostream>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// TargetDistribution of target values.
class COMPONENT_EXPORT(LEARNING_IMPL) TargetDistribution {
 private:
  // We use a flat_map since this will often have only one or two TargetValues,
  // such as "true" or "false".
  using DistributionMap = base::flat_map<TargetValue, size_t>;

 public:
  TargetDistribution();
  TargetDistribution(const TargetDistribution& rhs);
  TargetDistribution(TargetDistribution&& rhs);
  ~TargetDistribution();

  TargetDistribution& operator=(const TargetDistribution& rhs);
  TargetDistribution& operator=(TargetDistribution&& rhs);

  bool operator==(const TargetDistribution& rhs) const;

  // Add |rhs| to our counts.
  TargetDistribution& operator+=(const TargetDistribution& rhs);

  // Increment |rhs| by one.
  TargetDistribution& operator+=(const TargetValue& rhs);

  // Increment the distribution by |example|'s target value and weight.
  TargetDistribution& operator+=(const LabelledExample& example);

  // Return the number of counts for |value|.
  size_t operator[](const TargetValue& value) const;
  size_t& operator[](const TargetValue& value);

  // Return the total counts in the map.
  size_t total_counts() const {
    size_t total = 0.;
    for (auto& entry : counts_)
      total += entry.second;
    return total;
  }

  DistributionMap::const_iterator begin() const { return counts_.begin(); }

  DistributionMap::const_iterator end() const { return counts_.end(); }

  // Return the number of buckets in the distribution.
  // TODO(liberato): Do we want this?
  size_t size() const { return counts_.size(); }

  // Find the singular value with the highest counts, and copy it into
  // |value_out| and (optionally) |counts_out|.  Returns true if there is a
  // singular maximum, else returns false with the out params undefined.
  bool FindSingularMax(TargetValue* value_out,
                       size_t* counts_out = nullptr) const;

  // Return the average value of the entries in this distribution.  Of course,
  // this only makes sense if the TargetValues can be interpreted as numeric.
  double Average() const;

  std::string ToString() const;

 private:
  const DistributionMap& counts() const { return counts_; }

  // [value] == counts
  DistributionMap counts_;

  // Allow copy and assign.
};

COMPONENT_EXPORT(LEARNING_IMPL)
std::ostream& operator<<(std::ostream& out, const TargetDistribution& dist);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_TARGET_DISTRIBUTION_H_
