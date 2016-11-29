// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/sdp_message.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace remoting {
namespace protocol {

SdpMessage::SdpMessage(const std::string& sdp) {
  sdp_lines_ =
      SplitString(sdp, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : sdp_lines_) {
    if (base::StartsWith(line, "m=audio", base::CompareCase::SENSITIVE))
      has_audio_ = true;
    if (base::StartsWith(line, "m=video", base::CompareCase::SENSITIVE))
      has_video_ = true;
  }
}

SdpMessage::~SdpMessage() {}

std::string SdpMessage::ToString() const {
  return base::JoinString(sdp_lines_, "\n") + "\n";
}

bool SdpMessage::AddCodecParameter(const std::string& codec,
                                   const std::string& parameters_to_add) {
  int line_num;
  std::string payload_type;
  bool codec_found = FindCodec(codec, &line_num, &payload_type);
  if (!codec_found) {
    return false;
  }
  sdp_lines_.insert(sdp_lines_.begin() + line_num + 1,
                    "a=fmtp:" + payload_type + ' ' + parameters_to_add);
  return true;
}

bool SdpMessage::FindCodec(const std::string& codec,
                           int* line_num,
                           std::string* payload_type) const {
  const std::string kRtpMapPrefix = "a=rtpmap:";
  for (size_t i = 0; i < sdp_lines_.size(); ++i) {
    const auto& line = sdp_lines_[i];
    if (!base::StartsWith(line, kRtpMapPrefix, base::CompareCase::SENSITIVE))
      continue;
    size_t space_pos = line.find(' ');
    if (space_pos == std::string::npos)
      continue;
    if (line.substr(space_pos + 1, codec.size()) == codec &&
        line[space_pos + 1 + codec.size()] == '/') {
      *line_num = i;
      *payload_type =
          line.substr(kRtpMapPrefix.size(), space_pos - kRtpMapPrefix.size());
      return true;
    }
  }
  return false;
}

}  // namespace protocol
}  // namespace remoting
