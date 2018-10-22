/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
/*
 * This file defines classes for doing temporal layers with VP8.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_

#include <limits>
#include <map>
#include <set>
#include <vector>

#include "modules/video_coding/codecs/vp8/temporal_layers.h"

#include "absl/types/optional.h"

namespace webrtc {

class DefaultTemporalLayers : public TemporalLayers {
 public:
  explicit DefaultTemporalLayers(int number_of_temporal_layers);
  virtual ~DefaultTemporalLayers() {}

  bool SupportsEncoderFrameDropping() const override;

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  TemporalLayers::FrameConfig UpdateLayerConfig(uint32_t timestamp) override;

  // New target bitrate, per temporal layer.
  void OnRatesUpdated(const std::vector<uint32_t>& bitrates_bps,
                      int framerate_fps) override;

  bool UpdateConfiguration(Vp8EncoderConfig* cfg) override;

  void PopulateCodecSpecific(bool frame_is_keyframe,
                             const TemporalLayers::FrameConfig& tl_config,
                             CodecSpecificInfoVP8* vp8_info,
                             uint32_t timestamp) override;

  void FrameEncoded(uint32_t rtp_timestamp, size_t size, int qp) override;

 private:
  static constexpr size_t kKeyframeBuffer = std::numeric_limits<size_t>::max();
  static std::vector<TemporalLayers::FrameConfig> GetTemporalPattern(
      size_t num_layers);
  bool IsSyncFrame(const FrameConfig& config) const;
  void ValidateReferences(BufferFlags* flags, Vp8BufferReference ref) const;
  void UpdateSearchOrder(FrameConfig* config);

  const size_t num_layers_;
  const std::vector<unsigned int> temporal_ids_;
  const std::vector<TemporalLayers::FrameConfig> temporal_pattern_;
  // Set of buffers that are never updated except by keyframes.
  const std::set<Vp8BufferReference> kf_buffers_;

  uint8_t pattern_idx_;
  // Updated cumulative bitrates, per temporal layer.
  absl::optional<std::vector<uint32_t>> new_bitrates_bps_;

  // Map from rtp timestamp to a bitmask of Vp8BufferReference indicating which
  // buffers this frame should update. Reset on pattern loop.
  std::map<uint32_t, uint8_t> pending_frames_;

  // One counter per Vp8BufferReference, indicating number of frames since last
  // refresh. For non-base-layer frames (ie golden, altref buffers), this is
  // reset when the pattern loops.
  std::map<Vp8BufferReference, size_t> frames_since_buffer_refresh_;
};

class DefaultTemporalLayersChecker : public TemporalLayersChecker {
 public:
  explicit DefaultTemporalLayersChecker(int number_of_temporal_layers);
  bool CheckTemporalConfig(
      bool frame_is_keyframe,
      const TemporalLayers::FrameConfig& frame_config) override;

 private:
  struct BufferState {
    BufferState()
        : is_updated_this_cycle(false), is_keyframe(true), pattern_idx(0) {}

    bool is_updated_this_cycle;
    bool is_keyframe;
    uint8_t pattern_idx;
  };
  const size_t num_layers_;
  std::vector<unsigned int> temporal_ids_;
  const std::vector<std::set<uint8_t>> temporal_dependencies_;
  BufferState last_;
  BufferState arf_;
  BufferState golden_;
  uint8_t pattern_idx_;
};

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_
