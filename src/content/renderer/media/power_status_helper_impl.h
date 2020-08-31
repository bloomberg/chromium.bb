// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_POWER_STATUS_HELPER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_POWER_STATUS_HELPER_IMPL_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "media/blink/power_status_helper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"

namespace content {

class PowerStatusHelperImplBucketTest;

class CONTENT_EXPORT PowerStatusHelperImpl : public media::PowerStatusHelper {
 public:
  using CreateBatteryMonitorCB = base::RepeatingCallback<
      mojo::PendingRemote<device::mojom::BatteryMonitor>()>;

  // Bits used to construct UMA buckets.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum /* not class */ Bits {
    // Bit layout is: [msb] xx f F RR CC [lsb]
    // R == resolution
    // C == codec
    // F == frame rate
    // f == full screen
    // x == unused
    // Remember that we can't use more than 6 bits, since we shouldn't go over
    // 100 UMA buckets.

    // Codec bits, values 0x00 to 0x03
    // Named "CodecBits" to prevent a name collision with media::VideoCodec.
    kCodecBitsH264 = (0x00) << 0,
    kCodecBitsVP9Profile0 = (0x01) << 0,
    // Ignore Profile1
    kCodecBitsVP9Profile2 = (0x02) << 0,
    // TODO(liberato): add AV1

    // Resolution bits, values 0x00 to 0x03
    kResolution360p = (0x00) << 2,
    kResolution720p = (0x01) << 2,
    kResolution1080p = (0x02) << 2,

    // Frame rate bits, values 0x00 to 0x01
    kFrameRate30 = (0x00) << 4,
    kFrameRate60 = (0x01) << 4,

    // Fullscreen bits, values 0x00 to 0x01
    kFullScreenNo = (0x00) << 5,
    kFullScreenYes = (0x01) << 5,

    // This is not a valid bit for, you know, testing.
    kNotAValidBitForTesting = (0x01) << 10,
  };

  // If |stats_cb| is not provided, then we'll record to UMA.  It's just for
  // the tests.
  PowerStatusHelperImpl(CreateBatteryMonitorCB create_battery_monitor_cb);
  ~PowerStatusHelperImpl() override;

  // media::PowerStatusHelper
  void SetIsPlaying(bool is_playing) override;
  void SetMetadata(const media::PipelineMetadata& metadata) override;
  void SetIsFullscreen(bool is_fullscreen) override;
  void SetAverageFrameRate(base::Optional<int> average_fps) override;
  void UpdatePowerExperimentState(bool state) override;

 private:
  friend class PowerStatusHelperImplTest;
  friend class PowerStatusHelperImplBucketTest;

  // Return the UMA bucket for the given video configuration, or nullopt if we
  // don't want to record it.
  static base::Optional<int> BucketFor(bool is_playing,
                                       bool has_video,
                                       media::VideoCodec codec,
                                       media::VideoCodecProfile profile,
                                       gfx::Size natural_size,
                                       bool is_fullscreen,
                                       base::Optional<int> average_fps);

  // Return the histogram names.  Here so that tests can find them too.
  static const char* BatteryDeltaHistogram();
  static const char* ElapsedTimeHistogram();

  // Recompute everything when playback state or power experiment state changes.
  void OnAnyStateChange();

  // Handle updates about the current battery status.
  void OnBatteryStatus(device::mojom::BatteryStatusPtr battery_status);

  // Start monitoring if we haven't already.  Any outstanding callbacks will be
  // cancelled if monitoring was already in progress.
  void StartMonitoring();
  void StopMonitoring();

  // Register to receive a power update the next time it changes.
  void QueryNextStatus();

  CreateBatteryMonitorCB create_battery_monitor_cb_;

  // Most recent parameters we were given.
  bool is_playing_ = false;
  bool has_video_ = false;
  media::VideoCodec codec_ = media::VideoCodec::kUnknownVideoCodec;
  media::VideoCodecProfile profile_ =
      media::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
  gfx::Size natural_size_;
  bool is_fullscreen_ = false;
  // For estimating fps.  Can be unset if we don't know.
  base::Optional<int> average_fps_;

  // Current UMA bucket, if any.
  base::Optional<int> current_bucket_;

  // If set, our previous battery level, from 0-100.
  base::Optional<float> battery_level_baseline_;
  // The time at which we last got an update from |battery_monitor_|.
  base::TimeTicks last_update_;

  // Are we currently the player that should be recording power for the power
  // experiment, according to the MediaPowerExperimentManager?
  bool experiment_state_ = false;

  mojo::Remote<device::mojom::BatteryMonitor> battery_monitor_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PowerStatusHelperImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_POWER_STATUS_HELPER_IMPL_H_
