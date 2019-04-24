/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_SCENARIO_CONFIG_H_
#define TEST_SCENARIO_SCENARIO_CONFIG_H_

#include <stddef.h>
#include <string>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/transport/network_control.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "common_types.h"  // NOLINT(build/include)
#include "test/frame_generator.h"
#include "test/scenario/quality_info.h"

namespace webrtc {
namespace test {
struct PacketOverhead {
  static constexpr size_t kIpv4 = 20;
  static constexpr size_t kIpv6 = 40;
  static constexpr size_t kUdp = 8;
  static constexpr size_t kSrtp = 10;
  static constexpr size_t kStun = 4;
  // TURN messages can be sent either with or without an establieshed channel.
  // In the latter case, a TURN Send/Data Indication is sent which has
  // significantly more overhead.
  static constexpr size_t kTurnChannelMessage = 4;
  static constexpr size_t kTurnIndicationMessage = 36;
  static constexpr size_t kDefault = kIpv4 + kUdp + kSrtp;
};
struct TransportControllerConfig {
  struct Rates {
    Rates();
    Rates(const Rates&);
    ~Rates();
    DataRate min_rate = DataRate::kbps(30);
    DataRate max_rate = DataRate::kbps(3000);
    DataRate start_rate = DataRate::kbps(300);
    DataRate max_padding_rate = DataRate::Zero();
  } rates;
  enum CongestionController {
    kGoogCc,
    kGoogCcFeedback,
    kInjected
  } cc = kGoogCc;
  NetworkControllerFactoryInterface* cc_factory = nullptr;
  TimeDelta state_log_interval = TimeDelta::ms(100);
};

struct CallClientConfig {
  TransportControllerConfig transport;
};

struct SimulatedTimeClientConfig {
  TransportControllerConfig transport;
  struct Feedback {
    TimeDelta interval = TimeDelta::ms(100);
  } feedback;
};

struct PacketStreamConfig {
  PacketStreamConfig();
  PacketStreamConfig(const PacketStreamConfig&);
  ~PacketStreamConfig();
  int frame_rate = 30;
  DataRate max_data_rate = DataRate::Infinity();
  DataSize max_packet_size = DataSize::bytes(1400);
  DataSize min_frame_size = DataSize::bytes(100);
  double keyframe_multiplier = 1;
  DataSize packet_overhead = DataSize::bytes(PacketOverhead::kDefault);
};

struct VideoStreamConfig {
  bool autostart = true;
  struct Source {
    enum Capture {
      kGenerator,
      kVideoFile,
      kGenerateSlides,
      kImageSlides,
      // Support for explicit frame triggers should be added here if needed.
    } capture = Capture::kGenerator;
    struct Slides {
      TimeDelta change_interval = TimeDelta::seconds(10);
      struct Generator {
        int width = 1600;
        int height = 1200;
      } generator;
      struct Images {
        struct Crop {
          TimeDelta scroll_duration = TimeDelta::seconds(0);
          absl::optional<int> width;
          absl::optional<int> height;
        } crop;
        int width = 1850;
        int height = 1110;
        std::vector<std::string> paths = {
            "web_screenshot_1850_1110",
            "presentation_1850_1110",
            "photo_1850_1110",
            "difficult_photo_1850_1110",
        };
      } images;
    } slides;
    struct Generator {
      using PixelFormat = FrameGenerator::OutputType;
      PixelFormat pixel_format = PixelFormat::I420;
      int width = 320;
      int height = 180;
    } generator;
    struct VideoFile {
      std::string name;
      // Must be set to width and height of the source video file.
      int width = 0;
      int height = 0;
    } video_file;
    int framerate = 30;
  } source;
  struct Encoder {
    Encoder();
    Encoder(const Encoder&);
    ~Encoder();
    enum class ContentType {
      kVideo,
      kScreen,
    } content_type = ContentType::kVideo;
    enum Implementation { kFake, kSoftware, kHardware } implementation = kFake;
    struct Fake {
      DataRate max_rate = DataRate::Infinity();
    } fake;

    using Codec = VideoCodecType;
    Codec codec = Codec::kVideoCodecGeneric;
    absl::optional<DataRate> max_data_rate;
    absl::optional<int> max_framerate;
    // Counted in frame count.
    absl::optional<int> key_frame_interval = 3000;
    bool frame_dropping = true;
    struct SingleLayer {
      bool denoising = true;
      bool automatic_scaling = true;
    } single;
    struct Layers {
      int temporal = 1;
      int spatial = 1;
      enum class Prediction {
        kTemporalOnly,
        kSpatialOnKey,
        kFull,
      } prediction = Prediction::kFull;
    } layers;

    using DegradationPreference = DegradationPreference;
    DegradationPreference degradation_preference =
        DegradationPreference::MAINTAIN_FRAMERATE;
  } encoder;
  struct Stream {
    Stream();
    Stream(const Stream&);
    ~Stream();
    bool packet_feedback = true;
    bool use_rtx = true;
    DataRate pad_to_rate = DataRate::Zero();
    TimeDelta nack_history_time = TimeDelta::ms(1000);
    bool use_flexfec = false;
    bool use_ulpfec = false;
  } stream;
  struct Renderer {
    enum Type { kFake } type = kFake;
  };
  struct analyzer {
    bool log_to_file = false;
    std::function<void(const VideoFrameQualityInfo&)> frame_quality_handler;
  } analyzer;
};

struct AudioStreamConfig {
  AudioStreamConfig();
  AudioStreamConfig(const AudioStreamConfig&);
  ~AudioStreamConfig();
  bool autostart = true;
  struct Source {
    int channels = 1;
  } source;
  bool network_adaptation = false;
  struct NetworkAdaptation {
    struct FrameLength {
      double min_packet_loss_for_decrease = 0;
      double max_packet_loss_for_increase = 1;
      DataRate min_rate_for_20_ms = DataRate::Zero();
      DataRate max_rate_for_60_ms = DataRate::Infinity();
      DataRate min_rate_for_60_ms = DataRate::Zero();
      DataRate max_rate_for_120_ms = DataRate::Infinity();
    } frame;
  } adapt;
  struct Encoder {
    Encoder();
    Encoder(const Encoder&);
    ~Encoder();
    bool allocate_bitrate = false;
    bool enable_dtx = false;
    absl::optional<DataRate> fixed_rate;
    absl::optional<DataRate> min_rate;
    absl::optional<DataRate> max_rate;
    absl::optional<DataRate> priority_rate;
    TimeDelta initial_frame_length = TimeDelta::ms(20);
  } encoder;
  struct Stream {
    Stream();
    Stream(const Stream&);
    ~Stream();
    bool in_bandwidth_estimation = false;
  } stream;
  struct Render {
    std::string sync_group;
  } render;
};

struct NetworkNodeConfig {
  NetworkNodeConfig();
  NetworkNodeConfig(const NetworkNodeConfig&);
  ~NetworkNodeConfig();
  enum class TrafficMode {
    kSimulation,
    kCustom
  } mode = TrafficMode::kSimulation;
  struct Simulation {
    Simulation();
    Simulation(const Simulation&);
    ~Simulation();
    DataRate bandwidth = DataRate::Infinity();
    TimeDelta delay = TimeDelta::Zero();
    TimeDelta delay_std_dev = TimeDelta::Zero();
    double loss_rate = 0;
    bool codel_active_queue_management = false;
  } simulation;
  DataSize packet_overhead = DataSize::Zero();
  TimeDelta update_frequency = TimeDelta::ms(1);
};

struct CrossTrafficConfig {
  CrossTrafficConfig();
  CrossTrafficConfig(const CrossTrafficConfig&);
  ~CrossTrafficConfig();
  enum Mode { kRandomWalk, kPulsedPeaks } mode = kRandomWalk;
  int random_seed = 1;
  DataRate peak_rate = DataRate::kbps(100);
  DataSize min_packet_size = DataSize::bytes(200);
  TimeDelta min_packet_interval = TimeDelta::ms(1);
  struct RandomWalk {
    TimeDelta update_interval = TimeDelta::ms(200);
    double variance = 0.6;
    double bias = -0.1;
  } random_walk;
  struct PulsedPeaks {
    TimeDelta send_duration = TimeDelta::ms(100);
    TimeDelta hold_duration = TimeDelta::ms(2000);
  } pulsed;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_SCENARIO_CONFIG_H_
