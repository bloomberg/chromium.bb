// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_HANDLER_H_

#include <memory>

#include "components/optimization_guide/proto/models.pb.h"

namespace history {
class HistoryService;
}

namespace segmentation_platform {

class DefaultModelManager;
class HistogramSignalHandler;
class HistoryServiceObserver;
class SegmentInfoDatabase;
class SignalDatabase;
class SignalFilterProcessor;
class UkmDataManager;
class UserActionSignalHandler;

// Finds and observes the right signals needed for the models, and stores to the
// database. Only handles signals for profile specific signals. See
// `UkmDataManager` for other UKM related signals.
class SignalHandler {
 public:
  SignalHandler();
  ~SignalHandler();

  SignalHandler(SignalHandler&) = delete;
  SignalHandler& operator=(SignalHandler&) = delete;

  void Initialize(
      SignalDatabase* signal_database,
      SegmentInfoDatabase* segment_info_database,
      UkmDataManager* ukm_data_manager,
      history::HistoryService* history_service,
      DefaultModelManager* default_model_manager,
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          segment_ids);

  void TearDown();

  // Called to enable or disable metrics collection for segmentation platform.
  // This is often invoked early even before the signal list is obtained. Must
  // be explicitly called on startup.
  void EnableMetrics(bool signal_collection_allowed);

  // Called whenever the metadata about the models are updated. Registers
  // handlers for the relevant signals specified in the metadata. If handlers
  // are already registered, it will reset and register again with the new set
  // of signals.
  void OnSignalListUpdated();

  // TODO(ssid): This is used for training data observation. Create an observer
  // for this class and remove observer in the internal classes, then remove
  // this method.
  HistogramSignalHandler* deprecated_histogram_signal_handler() {
    return histogram_signal_handler_.get();
  }

 private:
  std::unique_ptr<UserActionSignalHandler> user_action_signal_handler_;
  std::unique_ptr<HistogramSignalHandler> histogram_signal_handler_;
  std::unique_ptr<SignalFilterProcessor> signal_filter_processor_;
  std::unique_ptr<HistoryServiceObserver> history_service_observer_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_HANDLER_H_
