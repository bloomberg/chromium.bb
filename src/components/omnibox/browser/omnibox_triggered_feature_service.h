// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_

#include <set>
#include <vector>

// Tracks the features that trigger during an omnibox session and records them
// to the logs. This is used for counterfactual slicing metrics by feature.
class OmniboxTriggeredFeatureService {
 public:
  // The list of features used for counterfactual slicing.
  enum class Feature {};
  using Features = std::set<Feature>;

  OmniboxTriggeredFeatureService();
  ~OmniboxTriggeredFeatureService();

  // Records |features_| for this session to |feature_triggered_in_session|.
  void RecordToLogs(Features* feature_triggered_in_session) const;

  // Invoked to indicate |feature| was triggered.
  void TriggerFeature(Feature feature);

  // Invoked when a new omnibox session starts. Clears |features_|.
  void ResetSession();

 private:
  // The set of features triggered in the current omnibox session via
  // |TriggerFeature()|.
  std::set<Feature> features_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_
