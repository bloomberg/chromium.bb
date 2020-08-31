// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_PROCESS_H_
#define WEBLAYER_BROWSER_BROWSER_PROCESS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "services/network/public/cpp/network_quality_tracker.h"

class PrefService;

namespace network_time {
class NetworkTimeTracker;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace weblayer {
class SafeBrowsingService;

// Class that holds global state in the browser process. Should be used only on
// the UI thread.
class BrowserProcess {
 public:
  explicit BrowserProcess(std::unique_ptr<PrefService> local_state);
  ~BrowserProcess();

  static BrowserProcess* GetInstance();

  // Called after the threads have been created but before the message loops
  // starts running. Allows the browser process to do any initialization that
  // requires all threads running.
  void PreMainMessageLoopRun();

  // Does cleanup that needs to occur before threads are torn down.
  void StartTearDown();

  PrefService* GetLocalState();
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory();
  network_time::NetworkTimeTracker* GetNetworkTimeTracker();
  network::NetworkQualityTracker* GetNetworkQualityTracker();

#if defined(OS_ANDROID)
  SafeBrowsingService* GetSafeBrowsingService(std::string user_agent);
  void StopSafeBrowsingService();
#endif

 private:
  void CreateNetworkQualityObserver();

  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;
  std::unique_ptr<network::NetworkQualityTracker> network_quality_tracker_;

  // Listens to NetworkQualityTracker and sends network quality updates to the
  // renderer.
  std::unique_ptr<
      network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver>
      network_quality_observer_;

#if defined(OS_ANDROID)
  std::unique_ptr<SafeBrowsingService> safe_browsing_service_;
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserProcess);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_PROCESS_H_
