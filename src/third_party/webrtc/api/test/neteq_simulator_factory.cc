/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/neteq_simulator_factory.h"

#include "absl/memory/memory.h"
#include "modules/audio_coding/neteq/tools/neteq_test_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"

namespace {

WEBRTC_DEFINE_string(replacement_audio_file,
                     "",
                     "A PCM file that will be used to populate dummy"
                     " RTP packets");
WEBRTC_DEFINE_int(max_nr_packets_in_buffer,
                  50,
                  "Maximum allowed number of packets in the buffer");

}  // namespace

namespace webrtc {
namespace test {

NetEqSimulatorFactory::NetEqSimulatorFactory()
    : factory_(absl::make_unique<NetEqTestFactory>()) {}

NetEqSimulatorFactory::~NetEqSimulatorFactory() = default;

std::unique_ptr<NetEqSimulator> NetEqSimulatorFactory::CreateSimulator(
    int argc,
    char* argv[]) {
  RTC_CHECK(!rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true))
      << "Error while parsing command-line flags";
  RTC_CHECK_EQ(argc, 3) << "Wrong number of input arguments. Expected 3, got "
                        << argc;
  // TODO(ivoc) Stop (ab)using command-line flags in this function.
  NetEqTestFactory::Config config;
  config.replacement_audio_file = FLAG_replacement_audio_file;
  config.max_nr_packets_in_buffer = FLAG_max_nr_packets_in_buffer;
  return factory_->InitializeTest(argv[1], argv[2], config);
}

}  // namespace test
}  // namespace webrtc
