// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CONFIGURE_DISPLAYS_TASK_H_
#define UI_DISPLAY_MANAGER_CONFIGURE_DISPLAYS_TASK_H_

#include <stddef.h>

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/gfx/geometry/point.h"

namespace display {

class DisplayMode;
class DisplaySnapshot;
class NativeDisplayDelegate;

struct DISPLAY_MANAGER_EXPORT DisplayConfigureRequest {
  DisplayConfigureRequest(DisplaySnapshot* display,
                          const DisplayMode* mode,
                          const gfx::Point& origin);

  DisplaySnapshot* display;
  const DisplayMode* mode;
  gfx::Point origin;
};

using RequestAndStatusList = std::pair<DisplayConfigureRequest, bool>;

// ConfigureDisplaysTask is in charge of applying the display configuration as
// requested by Ash. If the original request fails, the task will attempt to
// modify the request by downgrading the resolution of one or more of the
// displays and try again until it either succeeds a modeset or exhaust all
// available options.
//
// Displays are bandwidth constrained in 2 ways: (1) system memory bandwidth
// (ie: scanning pixels from memory), and (2) link bandwidth (ie: scanning
// pixels from the SoC to the display). Naturally all displays share (1),
// however with DisplayPort Multi-stream Transport (DP MST), displays may also
// share (2). The introduction of MST support drastically increases the
// likelihood of modeset failures due to (2) since multiple displays will all be
// sharing the same physical connection.
//
// If we're constrained by (1), reducing the resolution of any display will
// relieve pressure. However if we're constrained by (2), only those displays on
// the saturated link can relieve pressure.
class DISPLAY_MANAGER_EXPORT ConfigureDisplaysTask
    : public NativeDisplayObserver {
 public:
  enum Status {
    // At least one of the displays failed to apply any mode it supports.
    ERROR,

    // The requested configuration was applied.
    SUCCESS,

    // At least one of the displays failed to apply the requested
    // configuration, but it managed to fall back to another mode.
    PARTIAL_SUCCESS,
  };

  using ResponseCallback = base::OnceCallback<void(Status)>;
  using PartitionedRequestsQueue =
      std::queue<std::vector<DisplayConfigureRequest>>;

  ConfigureDisplaysTask(NativeDisplayDelegate* delegate,
                        const std::vector<DisplayConfigureRequest>& requests,
                        ResponseCallback callback);

  ConfigureDisplaysTask(const ConfigureDisplaysTask&) = delete;
  ConfigureDisplaysTask& operator=(const ConfigureDisplaysTask&) = delete;

  ~ConfigureDisplaysTask() override;

  // Starts the configuration task.
  void Run();

  // display::NativeDisplayObserver:
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override;

 private:
  // Deals with the aftermath of the initial configuration, which attempts to
  // configure all displays together.
  // Upon failure, partitions the original request from Ash into smaller
  // requests where the displays are grouped by the physical connector they
  // connect to and initiates the retry sequence.
  void OnFirstAttemptConfigured(bool config_status);

  // Deals with the aftermath of a configuration retry, which attempts to
  // configure a subset of the displays grouped together by the physical
  // connector they connect to.
  // Upon success, initiates the retry sequence on the next group of displays.
  // Otherwise, downgrades the display with the largest bandwidth requirement
  // and tries again.
  // If any of the display groups entirely fail to modeset (i.e. exhaust all
  // available modes during retry), the configuration will fail as a whole, but
  // will continue to try to modeset the remaining display groups.
  void OnRetryConfigured(bool config_status);

  // Partition |requests_| by their base connector id (i.e. the physical
  // connector the displays are connected to) and populate the result in
  // |pending_display_group_requests_|. We assume the order of requests
  // submitted by Ash is important, so the partitioning is done in order.
  void PartitionRequests();

  // Downgrade the request with the highest bandwidth requirement AND
  // alternative modes in |requests_| (excluding internal displays and disable
  // requests). Return false if no request was downgraded.
  bool DowngradeLargestRequestWithAlternativeModes();

  NativeDisplayDelegate* delegate_;  // Not owned.

  // Initially, |requests_| holds the configuration request submitted by Ash.
  // During retry, |requests_| will represent a group of displays that are
  // currently attempting configuration.
  std::vector<DisplayConfigureRequest> requests_;

  // A queue of display requests grouped by their
  // |requests_[index]->display->base_connector_id()|.
  PartitionedRequestsQueue pending_display_group_requests_;

  // The final requests and their configuration status for UMA.
  std::vector<RequestAndStatusList> final_requests_status_;

  // When the task finishes executing it runs the callback to notify that the
  // task is done and the task status.
  ResponseCallback callback_;

  Status task_status_;

  base::WeakPtrFactory<ConfigureDisplaysTask> weak_ptr_factory_{this};
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CONFIGURE_DISPLAYS_TASK_H_
