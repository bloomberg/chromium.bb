// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_STREAMING_VP8_ENCODER_H_
#define CAST_STANDALONE_SENDER_STREAMING_VP8_ENCODER_H_

#include <vpx/vpx_encoder.h>
#include <vpx/vpx_image.h>

#include <algorithm>
#include <condition_variable>  // NOLINT
#include <functional>
#include <memory>
#include <mutex>  // NOLINT
#include <queue>
#include <thread>  // NOLINT
#include <vector>

#include "absl/base/thread_annotations.h"
#include "cast/streaming/frame_id.h"
#include "cast/streaming/rtp_time.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {

class TaskRunner;

namespace cast {

class Sender;

// Uses libvpx to encode VP8 video and streams it to a Sender. Includes
// extensive logic for fine-tuning the encoder parameters in real-time, to
// provide the best quality results given external, uncontrollable factors:
// CPU/network availability, and the complexity of the video frame content.
//
// Internally, a separate encode thread is created and used to prevent blocking
// the main thread while frames are being encoded. All public API methods are
// assumed to be called on the same sequence/thread as the main TaskRunner
// (injected via the constructor).
//
// Usage:
//
// 1. EncodeAndSend() is used to queue-up video frames for encoding and sending,
// which will be done on a best-effort basis.
//
// 2. The client is expected to call SetTargetBitrate() frequently based on its
// own bandwidth estimates and congestion control logic. In addition, a client
// may provide a callback for each frame's encode statistics, which can be used
// to further optimize the user experience. For example, the stats can be used
// as a signal to reduce the data volume (i.e., resolution and/or frame rate)
// coming from the video capture source.
class StreamingVp8Encoder {
 public:
  // Configurable parameters passed to the StreamingVp8Encoder constructor.
  struct Parameters {
    // Number of threads to parallelize frame encoding. This should be set based
    // on the number of CPU cores available for encoding, but no more than 8.
    int num_encode_threads =
        std::min(std::max<int>(std::thread::hardware_concurrency(), 1), 8);

    // Best-quality quantizer (lower is better quality). Range: [0,63]
    int min_quantizer = 4;

    // Worst-quality quantizer (lower is better quality). Range: [0,63]
    int max_quantizer = 63;

    // Worst-quality quantizer to use when the CPU is extremely constrained.
    // Range: [min_quantizer,max_quantizer]
    int max_cpu_saver_quantizer = 25;

    // Maximum amount of wall-time a frame's encode can take, relative to the
    // frame's duration, before the CPU-saver logic is activated. The default
    // (70%) is appropriate for systems with four or more cores, but should be
    // reduced (e.g., 50%) for systems with fewer than three cores.
    //
    // Example: For 30 FPS (continuous) video, the frame duration is ~33.3ms,
    // and a value of 0.5 here would mean that the CPU-saver logic starts
    // sacrificing quality when frame encodes start taking longer than ~16.7ms.
    double max_time_utilization = 0.7;
  };

  // Represents an input VideoFrame, passed to EncodeAndSend().
  struct VideoFrame {
    // Image width and height.
    int width;
    int height;

    // I420 format image pointers and row strides (the number of bytes between
    // the start of successive rows). The pointers only need to remain valid
    // until the EncodeAndSend() call returns.
    const uint8_t* yuv_planes[3];
    int yuv_strides[3];

    // How long this frame will be held before the next frame will be displayed,
    // or zero if unknown. The frame duration is passed to the VP8 codec,
    // affecting a number of important behaviors, including: per-frame
    // bandwidth, CPU time spent encoding, temporal quality trade-offs, and
    // key/golden/alt-ref frame generation intervals.
    Clock::duration duration;
  };

  // Performance statistics for a single frame's encode.
  //
  // For full details on how to use these stats in an end-to-end system, see:
  // https://www.chromium.org/developers/design-documents/
  //     auto-throttled-screen-capture-and-mirroring
  // and https://source.chromium.org/chromium/chromium/src/+/master:
  //     media/cast/sender/performance_metrics_overlay.h
  struct Stats {
    // The Cast Streaming ID that was assigned to the frame.
    FrameId frame_id;

    // The RTP timestamp of the frame.
    RtpTimeTicks rtp_timestamp;

    // How long the frame took to encode. This is wall time, not CPU time or
    // some other load metric.
    Clock::duration encode_wall_time;

    // The frame's predicted duration; or, the actual duration if it was
    // provided in the VideoFrame.
    Clock::duration frame_duration;

    // The encoded frame's size in bytes.
    int encoded_size;

    // The average size of an encoded frame in bytes, having this
    // |frame_duration| and current target bitrate.
    double target_size;

    // The actual quantizer the VP8 encoder used, in the range [0,63].
    int quantizer;

    // The "hindsight" quantizer value that would have produced the best quality
    // encoding of the frame at the current target bitrate. The nominal range is
    // [0.0,63.0]. If it is larger than 63.0, then it was impossible for VP8 to
    // encode the frame within the current target bitrate (e.g., too much
    // "entropy" in the image, or too low a target bitrate).
    double perfect_quantizer;

    // Utilization feedback metrics. The nominal range for each of these is
    // [0.0,1.0] where 1.0 means "the entire budget available for the frame was
    // exhausted." Going above 1.0 is okay for one or a few frames, since it's
    // the average over many frames that matters before the system is considered
    // "redlining."
    //
    // The max of these three provides an overall utilization control signal.
    // The usual approach is for upstream control logic to increase/decrease the
    // data volume (e.g., video resolution and/or frame rate) to maintain a good
    // target point.
    double time_utilization() const {
      return static_cast<double>(encode_wall_time.count()) /
             frame_duration.count();
    }
    double space_utilization() const { return encoded_size / target_size; }
    double entropy_utilization() const {
      return perfect_quantizer / kMaxQuantizer;
    }
  };

  StreamingVp8Encoder(const Parameters& params,
                      TaskRunner* task_runner,
                      Sender* sender);

  ~StreamingVp8Encoder();

  // Get/Set the target bitrate. This may be changed at any time, as frequently
  // as desired, and it will take effect internally as soon as possible.
  int GetTargetBitrate() const;
  void SetTargetBitrate(int new_bitrate);

  // Encode |frame| using the VP8 encoder, assemble an EncodedFrame, and enqueue
  // into the Sender. The frame may be dropped if too many frames are in-flight.
  // If provided, the |stats_callback| is run after the frame is enqueued in the
  // Sender (via the main TaskRunner).
  void EncodeAndSend(const VideoFrame& frame,
                     Clock::time_point reference_time,
                     std::function<void(Stats)> stats_callback);

  static constexpr int kMinQuantizer = 0;
  static constexpr int kMaxQuantizer = 63;

 private:
  // Syntactic convenience to wrap the vpx_image_t alloc/free API in a smart
  // pointer.
  struct VpxImageDeleter {
    void operator()(vpx_image_t* ptr) const { vpx_img_free(ptr); }
  };
  using VpxImageUniquePtr = std::unique_ptr<vpx_image_t, VpxImageDeleter>;

  // Represents the state of one frame encode. This is created in
  // EncodeAndSend(), and passed to the encode thread via the |encode_queue_|.
  struct WorkUnit {
    VpxImageUniquePtr image;
    Clock::duration duration;
    Clock::time_point reference_time;
    RtpTimeTicks rtp_timestamp;
    std::function<void(Stats)> stats_callback;
  };

  // Same as WorkUnit, but with additional fields to carry the encode results.
  struct WorkUnitWithResults : public WorkUnit {
    std::vector<uint8_t> payload;
    bool is_key_frame;
    Stats stats;
  };

  bool is_encoder_initialized() const { return config_.g_threads != 0; }

  // Destroys the VP8 encoder context if it has been initialized.
  void DestroyEncoder();

  // The procedure for the |encode_thread_| that loops, processing work units
  // from the |encode_queue_| by calling Encode() until it's time to end the
  // thread.
  void ProcessWorkUnitsUntilTimeToQuit();

  // If the |encoder_| is live, attempt reconfiguration to allow it to encode
  // frames at a new frame size, target bitrate, or "CPU encoding speed." If
  // reconfiguration is not possible, destroy the existing instance and
  // re-create a new |encoder_| instance.
  void PrepareEncoder(int width, int height, int target_bitrate);

  // Wraps the complex libvpx vpx_codec_encode() call using inputs from
  // |work_unit| and populating results there.
  void EncodeFrame(bool force_key_frame, WorkUnitWithResults* work_unit);

  // Computes and populates |work_unit.stats| after the last call to
  // EncodeFrame().
  void ComputeFrameEncodeStats(Clock::duration encode_wall_time,
                               int target_bitrate,
                               WorkUnitWithResults* work_unit);

  // Updates the |ideal_speed_setting_|, to take effect with the next frame
  // encode, based on the given performance |stats|.
  void UpdateSpeedSettingForNextFrame(const Stats& stats);

  // Assembles and enqueues an EncodedFrame with the Sender on the main thread.
  void SendEncodedFrame(WorkUnitWithResults results);

  // Allocates a vpx_image_t and copies the content from |frame| to it.
  static VpxImageUniquePtr CloneAsVpxImage(const VideoFrame& frame);

  const Parameters params_;
  TaskRunner* const main_task_runner_;
  Sender* const sender_;

  // The reference time of the first frame passed to EncodeAndSend().
  Clock::time_point start_time_ = Clock::time_point::min();

  // The RTP timestamp of the last frame that was pushed into the
  // |encode_queue_| by EncodeAndSend(). This is used to check whether
  // timestamps are monotonically increasing.
  RtpTimeTicks last_enqueued_rtp_timestamp_;

  // Guards a few members shared by both the main and encode threads.
  std::mutex mutex_;

  // Used by the encode thread to sleep until more work is available.
  std::condition_variable cv_ ABSL_GUARDED_BY(mutex_);

  // These encode parameters not passed in the WorkUnit struct because it is
  // desirable for them to be applied as soon as possible, with the very next
  // WorkUnit popped from the |encode_queue_| on the encode thread, and not to
  // wait until some later WorkUnit is processed.
  bool needs_key_frame_ ABSL_GUARDED_BY(mutex_) = true;
  int target_bitrate_ ABSL_GUARDED_BY(mutex_) = 2 << 20;  // Default: 2 Mbps.

  // The queue of frame encodes. The size of this queue is implicitly bounded by
  // EncodeAndSend(), where it checks for the total in-flight media duration and
  // maybe drops a frame.
  std::queue<WorkUnit> encode_queue_ ABSL_GUARDED_BY(mutex_);

  // Current VP8 encoder configuration. Most of the fields are unchanging, and
  // are populated in the ctor; but thereafter, only the encode thread accesses
  // this struct.
  //
  // The speed setting is controlled via a separate libvpx API (see members
  // below).
  vpx_codec_enc_cfg_t config_{};

  // These represent the magnitude of the VP8 speed setting, where larger values
  // (i.e., faster speed) request less CPU usage but will provide lower video
  // quality. Only the encode thread accesses these.
  double ideal_speed_setting_;  // A time-weighted average, from measurements.
  int current_speed_setting_;   // Current |encoder_| speed setting.

  // libvpx VP8 encoder instance. Only the encode thread accesses this.
  vpx_codec_ctx_t encoder_;

  // This member should be last in the class since the thread should not start
  // until all above members have been initialized by the constructor.
  std::thread encode_thread_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_SENDER_STREAMING_VP8_ENCODER_H_
