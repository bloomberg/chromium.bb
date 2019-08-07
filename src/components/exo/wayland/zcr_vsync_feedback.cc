// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_vsync_feedback.h"

#include <vsync-feedback-unstable-v1-server-protocol.h>

#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// vsync_timing_interface:

// Implements VSync timing interface by monitoring updates to VSync parameters.
class VSyncTiming final : public ui::CompositorVSyncManager::Observer {
 public:
  ~VSyncTiming() override {
    WMHelper::GetInstance()->RemoveVSyncObserver(this);
  }

  static std::unique_ptr<VSyncTiming> Create(wl_resource* timing_resource) {
    std::unique_ptr<VSyncTiming> vsync_timing(new VSyncTiming(timing_resource));
    // Note: AddObserver() will call OnUpdateVSyncParameters.
    WMHelper::GetInstance()->AddVSyncObserver(vsync_timing.get());
    return vsync_timing;
  }

  // Overridden from ui::CompositorVSyncManager::Observer:
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override {
    uint64_t timebase_us = timebase.ToInternalValue();
    uint64_t interval_us = interval.ToInternalValue();

    // Ignore updates with interval 0.
    if (!interval_us)
      return;

    uint64_t offset_us = timebase_us % interval_us;

    // Avoid sending update events if interval did not change.
    if (interval_us == last_interval_us_) {
      int64_t offset_delta_us =
          static_cast<int64_t>(last_offset_us_ - offset_us);

      // Reduce the amount of events by only sending an update if the offset
      // changed compared to the last offset sent to the client by this amount.
      const int64_t kOffsetDeltaThresholdInMicroseconds = 25;

      if (std::abs(offset_delta_us) < kOffsetDeltaThresholdInMicroseconds)
        return;
    }

    zcr_vsync_timing_v1_send_update(timing_resource_, timebase_us & 0xffffffff,
                                    timebase_us >> 32, interval_us & 0xffffffff,
                                    interval_us >> 32);
    wl_client_flush(wl_resource_get_client(timing_resource_));

    last_interval_us_ = interval_us;
    last_offset_us_ = offset_us;
  }

 private:
  explicit VSyncTiming(wl_resource* timing_resource)
      : timing_resource_(timing_resource) {}

  // The VSync timing resource.
  wl_resource* const timing_resource_;

  uint64_t last_interval_us_{0};
  uint64_t last_offset_us_{0};

  DISALLOW_COPY_AND_ASSIGN(VSyncTiming);
};

void vsync_timing_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_vsync_timing_v1_interface vsync_timing_implementation = {
    vsync_timing_destroy};

////////////////////////////////////////////////////////////////////////////////
// vsync_feedback_interface:

void vsync_feedback_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void vsync_feedback_get_vsync_timing(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* output) {
  wl_resource* timing_resource =
      wl_resource_create(client, &zcr_vsync_timing_v1_interface, 1, id);
  SetImplementation(timing_resource, &vsync_timing_implementation,
                    VSyncTiming::Create(timing_resource));
}

const struct zcr_vsync_feedback_v1_interface vsync_feedback_implementation = {
    vsync_feedback_destroy, vsync_feedback_get_vsync_timing};

}  // namespace

void bind_vsync_feedback(wl_client* client,
                         void* data,
                         uint32_t version,
                         uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_vsync_feedback_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &vsync_feedback_implementation,
                                 nullptr, nullptr);
}

}  // namespace wayland
}  // namespace exo
