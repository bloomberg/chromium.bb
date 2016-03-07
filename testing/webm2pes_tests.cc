// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "webm2pes.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

#include "gtest/gtest.h"

#include "common/file_util.h"
#include "common/libwebm_utils.h"
#include "testing/test_util.h"

namespace {

struct PesOptionalHeader {
  int marker = 0;
  int scrambling = 0;
  int priority = 0;
  int data_alignment = 0;
  int copyright = 0;
  int original = 0;
  int has_pts = 0;
  int has_dts = 0;
  int unused_fields = 0;
  int remaining_size = 0;
  int pts_dts_flag = 0;
  std::uint64_t pts = 0;
  int stuffing_byte = 0;
};

struct BcmvHeader {
  BcmvHeader() = default;
  ~BcmvHeader() = default;
  BcmvHeader(const BcmvHeader&) = delete;
  BcmvHeader(BcmvHeader&&) = delete;

  // Convenience ctor for quick validation of expected values via operator==
  // after parsing input.
  explicit BcmvHeader(std::uint32_t len) : length(len) {
    id[0] = 'B';
    id[1] = 'C';
    id[2] = 'M';
    id[3] = 'V';
  }

  bool operator==(const BcmvHeader& other) const {
    return (other.length == length && other.id[0] == id[0] &&
            other.id[1] == id[1] && other.id[2] == id[2] &&
            other.id[3] == id[3]);
  }

  bool Valid() const {
    return (length > 0 && length <= std::numeric_limits<std::uint16_t>::max() &&
            id[0] == 'B' && id[1] == 'C' && id[2] == 'M' && id[3] == 'V');
  }

  char id[4] = {0};
  std::uint32_t length = 0;
};

struct PesHeader {
  std::uint16_t packet_length = 0;
  PesOptionalHeader opt_header;
  BcmvHeader bcmv_header;
};

class Webm2PesTests : public ::testing::Test {
 public:
  typedef std::vector<std::uint8_t> PesFileData;

  enum ParseState {
    kParsePesHeader,
    kParsePesOptionalHeader,
    kParseBcmvHeader,
  };

  // Constants for validating known values from input data.
  const std::uint8_t kMinVideoStreamId = 0xE0;
  const std::uint8_t kMaxVideoStreamId = 0xEF;
  const int kPesHeaderSize = 6;
  const int kPesOptionalHeaderStartOffset = kPesHeaderSize;
  const int kPesOptionalHeaderSize = 9;
  const int kPesOptionalHeaderMarkerValue = 0x2;
  const int kWebm2PesOptHeaderRemainingSize = 6;
  const int kBcmvHeaderSize = 10;

  Webm2PesTests() : read_pos_(0), parse_state_(kParsePesHeader) {}

  void CreateAndLoadTestInput() {
    libwebm::Webm2Pes converter(input_file_name_, temp_file_name_.name());
    ASSERT_TRUE(converter.ConvertToFile());
    pes_file_size_ = libwebm::GetFileSize(pes_file_name());
    ASSERT_GT(pes_file_size_, 0);
    pes_file_data_.reserve(pes_file_size_);
    libwebm::FilePtr file = libwebm::FilePtr(
        std::fopen(pes_file_name().c_str(), "rb"), libwebm::FILEDeleter());

    int byte;
    while ((byte = fgetc(file.get())) != EOF)
      pes_file_data_.push_back(static_cast<std::uint8_t>(byte));

    ASSERT_TRUE(feof(file.get()) && !ferror(file.get()));
    ASSERT_EQ(pes_file_size_, static_cast<std::int64_t>(pes_file_data_.size()));
    read_pos_ = 0;
    parse_state_ = kParsePesHeader;
  }

  bool VerifyPacketStartCode() {
    EXPECT_LT(read_pos_ + 2, pes_file_data_.size());

    // PES packets all start with the byte sequence 0x0 0x0 0x1.
    if (pes_file_data_[read_pos_] != 0 || pes_file_data_[read_pos_ + 1] != 0 ||
        pes_file_data_[read_pos_ + 2] != 1) {
      return false;
    }
    return true;
  }

  std::uint8_t ReadStreamId() {
    EXPECT_LT(read_pos_ + 3, pes_file_data_.size());
    return pes_file_data_[read_pos_ + 3]; }

  std::uint16_t ReadPacketLength() {
    EXPECT_LT(read_pos_ + 5, pes_file_data_.size());

    // Read and byte swap 16 bit big endian length.
    return (pes_file_data_[read_pos_ + 4] << 8) | pes_file_data_[read_pos_ + 5];
  }

  void ParsePesHeader(PesHeader* header) {
    ASSERT_TRUE(header != nullptr);
    EXPECT_EQ(kParsePesHeader, parse_state_);
    EXPECT_TRUE(VerifyPacketStartCode());
    // PES Video stream IDs start at E0.
    EXPECT_GE(ReadStreamId(), kMinVideoStreamId);
    EXPECT_LE(ReadStreamId(), kMaxVideoStreamId);
    header->packet_length = ReadPacketLength();
    read_pos_ += kPesHeaderSize;
    parse_state_ = kParsePesOptionalHeader;
  }

  // Parses a PES optional header.
  // https://en.wikipedia.org/wiki/Packetized_elementary_stream
  // TODO(tomfinegan): Make these masks constants.
  void ParsePesOptionalHeader(PesOptionalHeader* header) {
    ASSERT_TRUE(header != nullptr);
    EXPECT_EQ(kParsePesOptionalHeader, parse_state_);
    int offset = read_pos_;
    EXPECT_LT(offset, pes_file_size_);

    header->marker = (pes_file_data_[offset] & 0x80) >> 6;
    header->scrambling = (pes_file_data_[offset] & 0x30) >> 4;
    header->priority = (pes_file_data_[offset] & 0x8) >> 3;
    header->data_alignment = (pes_file_data_[offset] & 0xc) >> 2;
    header->copyright = (pes_file_data_[offset] & 0x2) >> 1;
    header->original = pes_file_data_[offset] & 0x1;

    offset++;
    header->has_pts = (pes_file_data_[offset] & 0x80) >> 7;
    header->has_dts = (pes_file_data_[offset] & 0x40) >> 6;
    header->unused_fields = pes_file_data_[offset] & 0x3f;

    offset++;
    header->remaining_size = pes_file_data_[offset];
    EXPECT_EQ(kWebm2PesOptHeaderRemainingSize, header->remaining_size);

    int bytes_left = header->remaining_size;

    if (header->has_pts) {
      // Read PTS markers. Format:
      // PTS: 5 bytes
      //   4 bits (flag: PTS present, but no DTS): 0x2 ('0010')
      //   36 bits (90khz PTS):
      //     top 3 bits
      //     marker ('1')
      //     middle 15 bits
      //     marker ('1')
      //     bottom 15 bits
      //     marker ('1')
      // TODO(tomfinegan): Extract some golden timestamp values and actually
      // read the timestamp.
      offset++;
      header->pts_dts_flag = (pes_file_data_[offset] & 0x20) >> 4;
      // Check the marker bits.
      int marker_bit = pes_file_data_[offset] & 1;
      EXPECT_EQ(1, marker_bit);
      offset += 2;
      marker_bit = pes_file_data_[offset] & 1;
      EXPECT_EQ(1, marker_bit);
      offset += 2;
      marker_bit = pes_file_data_[offset] & 1;
      EXPECT_EQ(1, marker_bit);
      bytes_left -= 5;
      offset++;
    }

    // Validate stuffing byte(s).
    for (int i = 0; i < bytes_left; ++i)
      EXPECT_EQ(0xff, pes_file_data_[offset + i]);

    offset += bytes_left;
    EXPECT_EQ(kPesHeaderSize + kPesOptionalHeaderSize, offset);

    read_pos_ += kPesOptionalHeaderSize;
    parse_state_ = kParseBcmvHeader;
  }

  // Parses and validates a BCMV header.
  void ParseBcmvHeader(BcmvHeader* header) {
    ASSERT_TRUE(header != nullptr);
    EXPECT_EQ(kParseBcmvHeader, kParseBcmvHeader);
    std::size_t offset = read_pos_;
    header->id[0] = pes_file_data_[offset++];
    header->id[1] = pes_file_data_[offset++];
    header->id[2] = pes_file_data_[offset++];
    header->id[3] = pes_file_data_[offset++];

    header->length |= pes_file_data_[offset++] << 24;
    header->length |= pes_file_data_[offset++] << 16;
    header->length |= pes_file_data_[offset++] << 8;
    header->length |= pes_file_data_[offset++];

    // Length stored in the BCMV header is followed by 2 bytes of 0 padding.
    EXPECT_EQ(0, pes_file_data_[offset++]);
    EXPECT_EQ(0, pes_file_data_[offset++]);

    EXPECT_TRUE(header->Valid());

    // TODO(tomfinegan): Verify data instead of jumping to the next packet.
    read_pos_ += kBcmvHeaderSize + header->length;
    parse_state_ = kParsePesHeader;
  }

  ~Webm2PesTests() = default;

  const std::string& pes_file_name() const { return temp_file_name_.name(); }
  std::uint64_t pes_file_size() const { return pes_file_size_; }
  const PesFileData& pes_file_data() const { return pes_file_data_; }

 private:
  const libwebm::TempFileDeleter temp_file_name_;
  const std::string input_file_name_ =
      libwebm::test::GetTestFilePath("bbb_480p_vp9_opus_1second.webm");
  std::int64_t pes_file_size_ = 0;
  PesFileData pes_file_data_;
  std::size_t read_pos_;
  ParseState parse_state_;
};

TEST_F(Webm2PesTests, CreatePesFile) { CreateAndLoadTestInput(); }

TEST_F(Webm2PesTests, CanParseFirstPacket) {
  CreateAndLoadTestInput();

  //
  // Parse the PES Header.
  // This is number of bytes following the final byte in the 6 byte static PES
  // header. PES optional header is 9 bytes. Payload is 83 bytes.
  PesHeader header;
  ParsePesHeader(&header);
  const std::size_t kPesOptionalHeaderLength = 9;
  const std::size_t kFirstFrameLength = 83;
  const std::size_t kPesPayloadLength =
      kPesOptionalHeaderLength + kFirstFrameLength;
  EXPECT_EQ(kPesPayloadLength, header.packet_length);

  //
  // Parse the PES optional header.
  //
  ParsePesOptionalHeader(&header.opt_header);

  EXPECT_EQ(kPesOptionalHeaderMarkerValue, header.opt_header.marker);
  EXPECT_EQ(0, header.opt_header.scrambling);
  EXPECT_EQ(0, header.opt_header.priority);
  EXPECT_EQ(0, header.opt_header.data_alignment);
  EXPECT_EQ(0, header.opt_header.copyright);
  EXPECT_EQ(0, header.opt_header.original);
  EXPECT_EQ(1, header.opt_header.has_pts);
  EXPECT_EQ(0, header.opt_header.has_dts);
  EXPECT_EQ(0, header.opt_header.unused_fields);

  //
  // Parse the BCMV Header
  //
  ParseBcmvHeader(&header.bcmv_header);
  EXPECT_EQ(kFirstFrameLength, header.bcmv_header.length);

  // Parse the next PES header to confirm correct parsing and consumption of
  // payload.
  ParsePesHeader(&header);
}

}  // namespace

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
