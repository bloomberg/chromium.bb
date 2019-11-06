// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_AVAILABILITY_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_AVAILABILITY_MODEL_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/feature_engagement/internal/availability_model.h"

namespace feature_engagement {

// An AvailabilityModel that never has any data, and is ready after having been
// initialized.
class NeverAvailabilityModel : public AvailabilityModel {
 public:
  NeverAvailabilityModel();
  ~NeverAvailabilityModel() override;

  // AvailabilityModel implementation.
  void Initialize(AvailabilityModel::OnInitializedCallback callback,
                  uint32_t current_day) override;
  bool IsReady() const override;
  base::Optional<uint32_t> GetAvailability(
      const base::Feature& feature) const override;

 private:
  // Sets |ready_| to true and posts the result to |callback|. This method
  // exists to ensure that |ready_| is not updated directly in the
  // Initialize(...) method.
  void ForwardedOnInitializedCallback(OnInitializedCallback callback);

  // Whether the model has been successfully initialized.
  bool ready_;

  DISALLOW_COPY_AND_ASSIGN(NeverAvailabilityModel);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_AVAILABILITY_MODEL_H_
