// Copyright 2018 Google LLC
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     https://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Helper script to generate time sequence plot using gnuplot.
// Accepts the trace on stdin, and outputs the gnuplot-consumable time series
// file into stdout.

#include <iostream>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "quic_trace/quic_trace.pb.h"

ABSL_FLAG(std::string,
          sequence,
          "",
          "Which time sequence to plot: send, ack or loss.");
ABSL_FLAG(bool,
          filter_old_acks,
          true,
          "Do not show an ack for a packet if it was already previously "
          "acknowledged");

namespace quic_trace {
namespace {

// Calculates the amount of actual data in the packet.
size_t FrameDataInSentPacket(const Event& packet) {
  if (packet.event_type() != PACKET_SENT) {
    return 0;
  }

  size_t sent_in_packet = 0;
  for (const Frame& frame : packet.frames()) {
    if (frame.frame_type() != STREAM || !frame.has_stream_frame_info()) {
      continue;
    }
    sent_in_packet += frame.stream_frame_info().length();
  }
  return sent_in_packet;
}

struct SentPacket {
  // Offset of the stream data sent in the frame with respect to the beginning
  // of the connection.
  size_t offset;
  // Size of frame data in the packet.
  size_t size;
};
// Map of the sent packets, keyed by packet number.
using SentPacketMap = absl::flat_hash_map<uint64_t, SentPacket>;

void PrintSentPacket(const SentPacketMap& packet_map,
                     uint64_t packet_number,
                     uint64_t time) {
  auto original_packet_it = packet_map.find(packet_number);
  if (original_packet_it == packet_map.end()) {
    return;
  }

  const SentPacket& original_packet = original_packet_it->second;

  std::cout << time << " " << original_packet.offset << std::endl;
  std::cout << time << " " << (original_packet.offset + original_packet.size)
            << std::endl;
  std::cout << std::endl;
}

void PrintTimeSequence(std::istream* trace_source) {
  Trace trace;
  trace.ParseFromIstream(trace_source);

  size_t total_sent = 0;
  SentPacketMap packet_map;
  absl::flat_hash_set<uint64_t> already_acknowledged;
  // In a single pass, compute |packet_map| and output the requested sequence.
  for (const Event& event : trace.events()) {
    // Track all sent packets and their offsets in the plot.
    size_t sent_in_packet = FrameDataInSentPacket(event);
    if (sent_in_packet != 0) {
      packet_map.emplace(event.packet_number(),
                         SentPacket{total_sent, sent_in_packet});
    }
    total_sent += sent_in_packet;

    // Output sent packets.
    if (sent_in_packet != 0 && absl::GetFlag(FLAGS_sequence) == "send") {
      std::cout << event.time_us() << " " << (total_sent - sent_in_packet)
                << std::endl;
      std::cout << event.time_us() << " " << total_sent << std::endl;
      std::cout << std::endl;
    }

    // Output loss events.
    if (event.event_type() == PACKET_LOST &&
        absl::GetFlag(FLAGS_sequence) == "loss") {
      PrintSentPacket(packet_map, event.packet_number(), event.time_us());
    }

    // Output acks.
    if (event.event_type() == PACKET_RECEIVED) {
      for (const Frame& frame : event.frames()) {
        if (frame.frame_type() == ACK &&
            absl::GetFlag(FLAGS_sequence) == "ack") {
          for (const AckBlock& block : frame.ack_info().acked_packets()) {
            for (uint64_t packet = block.first_packet();
                 packet <= block.last_packet(); packet++) {
              if (absl::GetFlag(FLAGS_filter_old_acks)) {
                if (already_acknowledged.count(packet) > 0) {
                  continue;
                }
                already_acknowledged.insert(packet);
              }
              PrintSentPacket(packet_map, packet, event.time_us());
            }
          }
        }
      }
    }
  }
}

}  // namespace
}  // namespace quic_trace

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  if (absl::GetFlag(FLAGS_sequence) != "send" &&
      absl::GetFlag(FLAGS_sequence) != "ack" &&
      absl::GetFlag(FLAGS_sequence) != "loss") {
    std::cerr << "The --sequence parameter has to be set to either 'send', "
                 "'ack' or 'loss'."
              << std::endl;
    return 1;
  }

  quic_trace::PrintTimeSequence(&std::cin);
  return 0;
}
