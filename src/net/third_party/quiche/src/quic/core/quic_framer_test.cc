// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/quic_framer.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "net/third_party/quiche/src/quic/core/crypto/null_decrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_framer_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/simple_data_producer.h"

using testing::_;
using testing::Return;
using testing::Truly;

namespace quic {
namespace test {
namespace {

const uint64_t kEpoch = UINT64_C(1) << 32;
const uint64_t kMask = kEpoch - 1;

const QuicUint128 kTestStatelessResetToken = 1010101;  // 0x0F69B5

// Use fields in which each byte is distinct to ensure that every byte is
// framed correctly. The values are otherwise arbitrary.
QuicConnectionId FramerTestConnectionId() {
  return TestConnectionId(UINT64_C(0xFEDCBA9876543210));
}

QuicConnectionId FramerTestConnectionIdPlusOne() {
  return TestConnectionId(UINT64_C(0xFEDCBA9876543211));
}

QuicConnectionId FramerTestConnectionIdNineBytes() {
  char connection_id_bytes[9] = {0xFE, 0xDC, 0xBA, 0x98, 0x76,
                                 0x54, 0x32, 0x10, 0x42};
  return QuicConnectionId(connection_id_bytes, sizeof(connection_id_bytes));
}

const QuicPacketNumber kPacketNumber = QuicPacketNumber(UINT64_C(0x12345678));
const QuicPacketNumber kSmallLargestObserved =
    QuicPacketNumber(UINT16_C(0x1234));
const QuicPacketNumber kSmallMissingPacket = QuicPacketNumber(UINT16_C(0x1233));
const QuicPacketNumber kLeastUnacked = QuicPacketNumber(UINT64_C(0x012345670));
const QuicStreamId kStreamId = UINT64_C(0x01020304);
// Note that the high 4 bits of the stream offset must be less than 0x40
// in order to ensure that the value can be encoded using VarInt62 encoding.
const QuicStreamOffset kStreamOffset = UINT64_C(0x3A98FEDC32107654);
const QuicPublicResetNonceProof kNonceProof = UINT64_C(0xABCDEF0123456789);

// In testing that we can ack the full range of packets...
// This is the largest packet number that can be represented in IETF QUIC
// varint62 format.
const QuicPacketNumber kLargestIetfLargestObserved =
    QuicPacketNumber(UINT64_C(0x3fffffffffffffff));
// Encodings for the two bits in a VarInt62 that
// describe the length of the VarInt61. For binary packet
// formats in this file, the convention is to code the
// first byte as
//   kVarInt62FourBytes + 0x<value_in_that_byte>
const uint8_t kVarInt62OneByte = 0x00;
const uint8_t kVarInt62TwoBytes = 0x40;
const uint8_t kVarInt62FourBytes = 0x80;
const uint8_t kVarInt62EightBytes = 0xc0;

class TestEncrypter : public QuicEncrypter {
 public:
  ~TestEncrypter() override {}
  bool SetKey(QuicStringPiece key) override { return true; }
  bool SetNoncePrefix(QuicStringPiece nonce_prefix) override { return true; }
  bool SetIV(QuicStringPiece iv) override { return true; }
  bool SetHeaderProtectionKey(QuicStringPiece key) override { return true; }
  bool EncryptPacket(uint64_t packet_number,
                     QuicStringPiece associated_data,
                     QuicStringPiece plaintext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override {
    packet_number_ = QuicPacketNumber(packet_number);
    associated_data_ = std::string(associated_data);
    plaintext_ = std::string(plaintext);
    memcpy(output, plaintext.data(), plaintext.length());
    *output_length = plaintext.length();
    return true;
  }
  std::string GenerateHeaderProtectionMask(QuicStringPiece sample) override {
    return std::string(5, 0);
  }
  size_t GetKeySize() const override { return 0; }
  size_t GetNoncePrefixSize() const override { return 0; }
  size_t GetIVSize() const override { return 0; }
  size_t GetMaxPlaintextSize(size_t ciphertext_size) const override {
    return ciphertext_size;
  }
  size_t GetCiphertextSize(size_t plaintext_size) const override {
    return plaintext_size;
  }
  QuicStringPiece GetKey() const override { return QuicStringPiece(); }
  QuicStringPiece GetNoncePrefix() const override { return QuicStringPiece(); }

  QuicPacketNumber packet_number_;
  std::string associated_data_;
  std::string plaintext_;
};

class TestDecrypter : public QuicDecrypter {
 public:
  ~TestDecrypter() override {}
  bool SetKey(QuicStringPiece key) override { return true; }
  bool SetNoncePrefix(QuicStringPiece nonce_prefix) override { return true; }
  bool SetIV(QuicStringPiece iv) override { return true; }
  bool SetHeaderProtectionKey(QuicStringPiece key) override { return true; }
  bool SetPreliminaryKey(QuicStringPiece key) override {
    QUIC_BUG << "should not be called";
    return false;
  }
  bool SetDiversificationNonce(const DiversificationNonce& key) override {
    return true;
  }
  bool DecryptPacket(uint64_t packet_number,
                     QuicStringPiece associated_data,
                     QuicStringPiece ciphertext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override {
    packet_number_ = QuicPacketNumber(packet_number);
    associated_data_ = std::string(associated_data);
    ciphertext_ = std::string(ciphertext);
    memcpy(output, ciphertext.data(), ciphertext.length());
    *output_length = ciphertext.length();
    return true;
  }
  std::string GenerateHeaderProtectionMask(
      QuicDataReader* sample_reader) override {
    return std::string(5, 0);
  }
  size_t GetKeySize() const override { return 0; }
  size_t GetIVSize() const override { return 0; }
  QuicStringPiece GetKey() const override { return QuicStringPiece(); }
  QuicStringPiece GetNoncePrefix() const override { return QuicStringPiece(); }
  // Use a distinct value starting with 0xFFFFFF, which is never used by TLS.
  uint32_t cipher_id() const override { return 0xFFFFFFF2; }
  QuicPacketNumber packet_number_;
  std::string associated_data_;
  std::string ciphertext_;
};

class TestQuicVisitor : public QuicFramerVisitorInterface {
 public:
  TestQuicVisitor()
      : error_count_(0),
        version_mismatch_(0),
        packet_count_(0),
        frame_count_(0),
        complete_packets_(0),
        accept_packet_(true),
        accept_public_header_(true) {}

  ~TestQuicVisitor() override {}

  void OnError(QuicFramer* f) override {
    QUIC_DLOG(INFO) << "QuicFramer Error: " << QuicErrorCodeToString(f->error())
                    << " (" << f->error() << ")";
    ++error_count_;
  }

  void OnPacket() override {}

  void OnPublicResetPacket(const QuicPublicResetPacket& packet) override {
    public_reset_packet_ = QuicMakeUnique<QuicPublicResetPacket>((packet));
  }

  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {
    version_negotiation_packet_ =
        QuicMakeUnique<QuicVersionNegotiationPacket>((packet));
  }

  void OnRetryPacket(QuicConnectionId original_connection_id,
                     QuicConnectionId new_connection_id,
                     QuicStringPiece retry_token) override {
    retry_original_connection_id_ =
        QuicMakeUnique<QuicConnectionId>(original_connection_id);
    retry_new_connection_id_ =
        QuicMakeUnique<QuicConnectionId>(new_connection_id);
    retry_token_ = QuicMakeUnique<std::string>(std::string(retry_token));
  }

  bool OnProtocolVersionMismatch(ParsedQuicVersion received_version,
                                 PacketHeaderFormat /*form*/) override {
    QUIC_DLOG(INFO) << "QuicFramer Version Mismatch, version: "
                    << received_version;
    ++version_mismatch_;
    return true;
  }

  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override {
    header_ = QuicMakeUnique<QuicPacketHeader>((header));
    return accept_public_header_;
  }

  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override {
    return true;
  }

  void OnDecryptedPacket(EncryptionLevel level) override {}

  bool OnPacketHeader(const QuicPacketHeader& header) override {
    ++packet_count_;
    header_ = QuicMakeUnique<QuicPacketHeader>((header));
    return accept_packet_;
  }

  void OnCoalescedPacket(const QuicEncryptedPacket& packet) override {
    size_t coalesced_data_length = packet.length();
    char* coalesced_data = new char[coalesced_data_length];
    memcpy(coalesced_data, packet.data(), coalesced_data_length);
    coalesced_packets_.push_back(QuicMakeUnique<QuicEncryptedPacket>(
        coalesced_data, coalesced_data_length,
        /*owns_buffer=*/true));
  }

  bool OnStreamFrame(const QuicStreamFrame& frame) override {
    ++frame_count_;
    // Save a copy of the data so it is valid after the packet is processed.
    std::string* string_data =
        new std::string(frame.data_buffer, frame.data_length);
    stream_data_.push_back(QuicWrapUnique(string_data));
    stream_frames_.push_back(QuicMakeUnique<QuicStreamFrame>(
        frame.stream_id, frame.fin, frame.offset, *string_data));
    return true;
  }

  bool OnCryptoFrame(const QuicCryptoFrame& frame) override {
    ++frame_count_;
    // Save a copy of the data so it is valid after the packet is processed.
    std::string* string_data =
        new std::string(frame.data_buffer, frame.data_length);
    crypto_data_.push_back(QuicWrapUnique(string_data));
    crypto_frames_.push_back(QuicMakeUnique<QuicCryptoFrame>(
        ENCRYPTION_INITIAL, frame.offset, *string_data));
    return true;
  }

  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta ack_delay_time) override {
    ++frame_count_;
    QuicAckFrame ack_frame;
    ack_frame.largest_acked = largest_acked;
    ack_frame.ack_delay_time = ack_delay_time;
    ack_frames_.push_back(QuicMakeUnique<QuicAckFrame>(ack_frame));
    return true;
  }

  bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) override {
    DCHECK(!ack_frames_.empty());
    ack_frames_[ack_frames_.size() - 1]->packets.AddRange(start, end);
    return true;
  }

  bool OnAckTimestamp(QuicPacketNumber packet_number,
                      QuicTime timestamp) override {
    ack_frames_[ack_frames_.size() - 1]->received_packet_times.push_back(
        std::make_pair(packet_number, timestamp));
    return true;
  }

  bool OnAckFrameEnd(QuicPacketNumber /*start*/) override { return true; }

  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override {
    ++frame_count_;
    stop_waiting_frames_.push_back(QuicMakeUnique<QuicStopWaitingFrame>(frame));
    return true;
  }

  bool OnPaddingFrame(const QuicPaddingFrame& frame) override {
    padding_frames_.push_back(QuicMakeUnique<QuicPaddingFrame>(frame));
    return true;
  }

  bool OnPingFrame(const QuicPingFrame& frame) override {
    ++frame_count_;
    ping_frames_.push_back(QuicMakeUnique<QuicPingFrame>(frame));
    return true;
  }

  bool OnMessageFrame(const QuicMessageFrame& frame) override {
    ++frame_count_;
    message_frames_.push_back(
        QuicMakeUnique<QuicMessageFrame>(frame.data, frame.message_length));
    return true;
  }

  void OnPacketComplete() override { ++complete_packets_; }

  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override {
    rst_stream_frame_ = frame;
    return true;
  }

  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override {
    connection_close_frame_ = frame;
    return true;
  }

  bool OnStopSendingFrame(const QuicStopSendingFrame& frame) override {
    stop_sending_frame_ = frame;
    return true;
  }

  bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) override {
    path_challenge_frame_ = frame;
    return true;
  }

  bool OnPathResponseFrame(const QuicPathResponseFrame& frame) override {
    path_response_frame_ = frame;
    return true;
  }

  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override {
    goaway_frame_ = frame;
    return true;
  }

  bool OnMaxStreamsFrame(const QuicMaxStreamsFrame& frame) override {
    max_streams_frame_ = frame;
    return true;
  }

  bool OnStreamsBlockedFrame(const QuicStreamsBlockedFrame& frame) override {
    streams_blocked_frame_ = frame;
    return true;
  }

  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override {
    window_update_frame_ = frame;
    return true;
  }

  bool OnBlockedFrame(const QuicBlockedFrame& frame) override {
    blocked_frame_ = frame;
    return true;
  }

  bool OnNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame) override {
    new_connection_id_ = frame;
    return true;
  }

  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) override {
    retire_connection_id_ = frame;
    return true;
  }

  bool OnNewTokenFrame(const QuicNewTokenFrame& frame) override {
    new_token_ = frame;
    return true;
  }

  bool IsValidStatelessResetToken(QuicUint128 token) const override {
    return token == kTestStatelessResetToken;
  }

  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {
    stateless_reset_packet_ =
        QuicMakeUnique<QuicIetfStatelessResetPacket>(packet);
  }

  // Counters from the visitor_ callbacks.
  int error_count_;
  int version_mismatch_;
  int packet_count_;
  int frame_count_;
  int complete_packets_;
  bool accept_packet_;
  bool accept_public_header_;

  std::unique_ptr<QuicPacketHeader> header_;
  std::unique_ptr<QuicPublicResetPacket> public_reset_packet_;
  std::unique_ptr<QuicIetfStatelessResetPacket> stateless_reset_packet_;
  std::unique_ptr<QuicVersionNegotiationPacket> version_negotiation_packet_;
  std::unique_ptr<QuicConnectionId> retry_original_connection_id_;
  std::unique_ptr<QuicConnectionId> retry_new_connection_id_;
  std::unique_ptr<std::string> retry_token_;
  std::vector<std::unique_ptr<QuicStreamFrame>> stream_frames_;
  std::vector<std::unique_ptr<QuicCryptoFrame>> crypto_frames_;
  std::vector<std::unique_ptr<QuicAckFrame>> ack_frames_;
  std::vector<std::unique_ptr<QuicStopWaitingFrame>> stop_waiting_frames_;
  std::vector<std::unique_ptr<QuicPaddingFrame>> padding_frames_;
  std::vector<std::unique_ptr<QuicPingFrame>> ping_frames_;
  std::vector<std::unique_ptr<QuicMessageFrame>> message_frames_;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> coalesced_packets_;
  QuicRstStreamFrame rst_stream_frame_;
  QuicConnectionCloseFrame connection_close_frame_;
  QuicStopSendingFrame stop_sending_frame_;
  QuicGoAwayFrame goaway_frame_;
  QuicPathChallengeFrame path_challenge_frame_;
  QuicPathResponseFrame path_response_frame_;
  QuicWindowUpdateFrame window_update_frame_;
  QuicBlockedFrame blocked_frame_;
  QuicStreamsBlockedFrame streams_blocked_frame_;
  QuicMaxStreamsFrame max_streams_frame_;
  QuicNewConnectionIdFrame new_connection_id_;
  QuicRetireConnectionIdFrame retire_connection_id_;
  QuicNewTokenFrame new_token_;
  std::vector<std::unique_ptr<std::string>> stream_data_;
  std::vector<std::unique_ptr<std::string>> crypto_data_;
};

// Simple struct for defining a packet's content, and associated
// parse error.
struct PacketFragment {
  std::string error_if_missing;
  std::vector<unsigned char> fragment;
};

using PacketFragments = std::vector<struct PacketFragment>;

ParsedQuicVersionVector AllSupportedVersionsIncludingTls() {
  QuicFlagSaver flags;
  SetQuicFlag(FLAGS_quic_supports_tls_handshake, true);
  return AllSupportedVersions();
}

class QuicFramerTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicFramerTest()
      : encrypter_(new test::TestEncrypter()),
        decrypter_(new test::TestDecrypter()),
        version_(GetParam()),
        start_(QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(0x10)),
        framer_(AllSupportedVersionsIncludingTls(),
                start_,
                Perspective::IS_SERVER,
                kQuicDefaultConnectionIdLength) {
    SetQuicFlag(FLAGS_quic_supports_tls_handshake, true);
    framer_.set_version(version_);
    if (framer_.version().KnowsWhichDecrypterToUse()) {
      framer_.InstallDecrypter(ENCRYPTION_INITIAL,
                               std::unique_ptr<QuicDecrypter>(decrypter_));
    } else {
      framer_.SetDecrypter(ENCRYPTION_INITIAL,
                           std::unique_ptr<QuicDecrypter>(decrypter_));
    }
    framer_.SetEncrypter(ENCRYPTION_INITIAL,
                         std::unique_ptr<QuicEncrypter>(encrypter_));

    framer_.set_visitor(&visitor_);
    framer_.InferPacketHeaderTypeFromVersion();
  }

  void SetDecrypterLevel(EncryptionLevel level) {
    if (!framer_.version().KnowsWhichDecrypterToUse()) {
      return;
    }
    decrypter_ = new TestDecrypter();
    framer_.InstallDecrypter(level, std::unique_ptr<QuicDecrypter>(decrypter_));
  }

  // Helper function to get unsigned char representation of the handshake
  // protocol byte of the current QUIC version number.
  unsigned char GetQuicVersionProtocolByte() {
    return (CreateQuicVersionLabel(version_) >> 24) & 0xff;
  }

  // Helper function to get unsigned char representation of digit in the
  // units place of the current QUIC version number.
  unsigned char GetQuicVersionDigitOnes() {
    return CreateQuicVersionLabel(version_) & 0xff;
  }

  // Helper function to get unsigned char representation of digit in the
  // tens place of the current QUIC version number.
  unsigned char GetQuicVersionDigitTens() {
    return (CreateQuicVersionLabel(version_) >> 8) & 0xff;
  }

  bool CheckEncryption(QuicPacketNumber packet_number, QuicPacket* packet) {
    if (packet_number != encrypter_->packet_number_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect packet number.  expected "
                      << packet_number
                      << " actual: " << encrypter_->packet_number_;
      return false;
    }
    if (packet->AssociatedData(framer_.transport_version()) !=
        encrypter_->associated_data_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect associated data.  expected "
                      << packet->AssociatedData(framer_.transport_version())
                      << " actual: " << encrypter_->associated_data_;
      return false;
    }
    if (packet->Plaintext(framer_.transport_version()) !=
        encrypter_->plaintext_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect plaintext data.  expected "
                      << packet->Plaintext(framer_.transport_version())
                      << " actual: " << encrypter_->plaintext_;
      return false;
    }
    return true;
  }

  bool CheckDecryption(const QuicEncryptedPacket& encrypted,
                       bool includes_version,
                       bool includes_diversification_nonce,
                       QuicConnectionIdLength destination_connection_id_length,
                       QuicConnectionIdLength source_connection_id_length) {
    return CheckDecryption(
        encrypted, includes_version, includes_diversification_nonce,
        destination_connection_id_length, source_connection_id_length,
        VARIABLE_LENGTH_INTEGER_LENGTH_0, 0, VARIABLE_LENGTH_INTEGER_LENGTH_0);
  }

  bool CheckDecryption(
      const QuicEncryptedPacket& encrypted,
      bool includes_version,
      bool includes_diversification_nonce,
      QuicConnectionIdLength destination_connection_id_length,
      QuicConnectionIdLength source_connection_id_length,
      QuicVariableLengthIntegerLength retry_token_length_length,
      size_t retry_token_length,
      QuicVariableLengthIntegerLength length_length) {
    if (visitor_.header_->packet_number != decrypter_->packet_number_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect packet number.  expected "
                      << visitor_.header_->packet_number
                      << " actual: " << decrypter_->packet_number_;
      return false;
    }
    QuicStringPiece associated_data =
        QuicFramer::GetAssociatedDataFromEncryptedPacket(
            framer_.transport_version(), encrypted,
            destination_connection_id_length, source_connection_id_length,
            includes_version, includes_diversification_nonce,
            PACKET_4BYTE_PACKET_NUMBER, retry_token_length_length,
            retry_token_length, length_length);
    if (associated_data != decrypter_->associated_data_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect associated data.  expected "
                      << QuicTextUtils::HexEncode(associated_data)
                      << " actual: "
                      << QuicTextUtils::HexEncode(decrypter_->associated_data_);
      return false;
    }
    QuicStringPiece ciphertext(
        encrypted.AsStringPiece().substr(GetStartOfEncryptedData(
            framer_.transport_version(), destination_connection_id_length,
            source_connection_id_length, includes_version,
            includes_diversification_nonce, PACKET_4BYTE_PACKET_NUMBER,
            retry_token_length_length, retry_token_length, length_length)));
    if (ciphertext != decrypter_->ciphertext_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect ciphertext data.  expected "
                      << QuicTextUtils::HexEncode(ciphertext) << " actual: "
                      << QuicTextUtils::HexEncode(decrypter_->ciphertext_)
                      << " associated data: "
                      << QuicTextUtils::HexEncode(associated_data);
      return false;
    }
    return true;
  }

  char* AsChars(unsigned char* data) { return reinterpret_cast<char*>(data); }

  // Creates a new QuicEncryptedPacket by concatenating the various
  // packet fragments in |fragments|.
  std::unique_ptr<QuicEncryptedPacket> AssemblePacketFromFragments(
      const PacketFragments& fragments) {
    char* buffer = new char[kMaxOutgoingPacketSize + 1];
    size_t len = 0;
    for (const auto& fragment : fragments) {
      memcpy(buffer + len, fragment.fragment.data(), fragment.fragment.size());
      len += fragment.fragment.size();
    }
    return QuicMakeUnique<QuicEncryptedPacket>(buffer, len, true);
  }

  void CheckFramingBoundaries(const PacketFragments& fragments,
                              QuicErrorCode error_code) {
    std::unique_ptr<QuicEncryptedPacket> packet(
        AssemblePacketFromFragments(fragments));
    // Check all the various prefixes of |packet| for the expected
    // parse error and error code.
    for (size_t i = 0; i < packet->length(); ++i) {
      std::string expected_error;
      size_t len = 0;
      for (const auto& fragment : fragments) {
        len += fragment.fragment.size();
        if (i < len) {
          expected_error = fragment.error_if_missing;
          break;
        }
      }

      if (expected_error.empty())
        continue;

      CheckProcessingFails(*packet, i, expected_error, error_code);
    }
  }

  void CheckProcessingFails(const QuicEncryptedPacket& packet,
                            size_t len,
                            std::string expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(packet.data(), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  void CheckProcessingFails(unsigned char* packet,
                            size_t len,
                            std::string expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(AsChars(packet), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  // Checks if the supplied string matches data in the supplied StreamFrame.
  void CheckStreamFrameData(std::string str, QuicStreamFrame* frame) {
    EXPECT_EQ(str, std::string(frame->data_buffer, frame->data_length));
  }

  void CheckCalculatePacketNumber(uint64_t expected_packet_number,
                                  QuicPacketNumber last_packet_number) {
    uint64_t wire_packet_number = expected_packet_number & kMask;
    EXPECT_EQ(expected_packet_number,
              QuicFramerPeer::CalculatePacketNumberFromWire(
                  &framer_, PACKET_4BYTE_PACKET_NUMBER, last_packet_number,
                  wire_packet_number))
        << "last_packet_number: " << last_packet_number
        << " wire_packet_number: " << wire_packet_number;
  }

  std::unique_ptr<QuicPacket> BuildDataPacket(const QuicPacketHeader& header,
                                              const QuicFrames& frames) {
    return BuildUnsizedDataPacket(&framer_, header, frames);
  }

  std::unique_ptr<QuicPacket> BuildDataPacket(const QuicPacketHeader& header,
                                              const QuicFrames& frames,
                                              size_t packet_size) {
    return BuildUnsizedDataPacket(&framer_, header, frames, packet_size);
  }

  // N starts at 1.
  QuicStreamId GetNthStreamid(QuicTransportVersion transport_version,
                              Perspective perspective,
                              bool bidirectional,
                              int n) {
    if (bidirectional) {
      return QuicUtils::GetFirstBidirectionalStreamId(transport_version,
                                                      perspective) +
             ((n - 1) * QuicUtils::StreamIdDelta(transport_version));
    }
    // Unidirectional
    return QuicUtils::GetFirstUnidirectionalStreamId(transport_version,
                                                     perspective) +
           ((n - 1) * QuicUtils::StreamIdDelta(transport_version));
  }

  test::TestEncrypter* encrypter_;
  test::TestDecrypter* decrypter_;
  ParsedQuicVersion version_;
  QuicTime start_;
  QuicFramer framer_;
  test::TestQuicVisitor visitor_;
  SimpleBufferAllocator allocator_;
};

// Multiple test cases of QuicFramerTest use byte arrays to define packets for
// testing, and these byte arrays contain the QUIC version. This macro explodes
// the 32-bit version into four bytes in network order. Since it uses methods of
// QuicFramerTest, it is only valid to use this in a QuicFramerTest.
#define QUIC_VERSION_BYTES                                      \
  GetQuicVersionProtocolByte(), '0', GetQuicVersionDigitTens(), \
      GetQuicVersionDigitOnes()

// Run all framer tests with all supported versions of QUIC.
INSTANTIATE_TEST_SUITE_P(
    QuicFramerTests,
    QuicFramerTest,
    ::testing::ValuesIn(AllSupportedVersionsIncludingTls()));

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearEpochStart) {
  // A few quick manual sanity checks.
  CheckCalculatePacketNumber(UINT64_C(1), QuicPacketNumber());
  CheckCalculatePacketNumber(kEpoch + 1, QuicPacketNumber(kMask));
  CheckCalculatePacketNumber(kEpoch, QuicPacketNumber(kMask));
  for (uint64_t j = 0; j < 10; j++) {
    CheckCalculatePacketNumber(j, QuicPacketNumber());
    CheckCalculatePacketNumber(kEpoch - 1 - j, QuicPacketNumber());
  }

  // Cases where the last number was close to the start of the range.
  for (QuicPacketNumber last = QuicPacketNumber(1); last < QuicPacketNumber(10);
       last++) {
    // Small numbers should not wrap (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(j, last);
    }

    // Large numbers should not wrap either (because we're near 0 already).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch - 1 - j, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearEpochEnd) {
  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    QuicPacketNumber last = QuicPacketNumber(kEpoch - i);

    // Small numbers should wrap.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch + j, last);
    }

    // Large numbers should not (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch - 1 - j, last);
    }
  }
}

// Next check where we're in a non-zero epoch to verify we handle
// reverse wrapping, too.
TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearPrevEpoch) {
  const uint64_t prev_epoch = 1 * kEpoch;
  const uint64_t cur_epoch = 2 * kEpoch;
  // Cases where the last number was close to the start of the range
  for (uint64_t i = 0; i < 10; i++) {
    QuicPacketNumber last = QuicPacketNumber(cur_epoch + i);
    // Small number should not wrap (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(cur_epoch + j, last);
    }

    // But large numbers should reverse wrap.
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(prev_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearNextEpoch) {
  const uint64_t cur_epoch = 2 * kEpoch;
  const uint64_t next_epoch = 3 * kEpoch;
  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    QuicPacketNumber last = QuicPacketNumber(next_epoch - 1 - i);

    // Small numbers should wrap.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(next_epoch + j, last);
    }

    // but large numbers should not (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(cur_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearNextMax) {
  const uint64_t max_number = std::numeric_limits<uint64_t>::max();
  const uint64_t max_epoch = max_number & ~kMask;

  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    // Subtract 1, because the expected next packet number is 1 more than the
    // last packet number.
    QuicPacketNumber last = QuicPacketNumber(max_number - i - 1);

    // Small numbers should not wrap, because they have nowhere to go.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(max_epoch + j, last);
    }

    // Large numbers should not wrap either.
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(max_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, EmptyPacket) {
  char packet[] = {0x00};
  QuicEncryptedPacket encrypted(packet, 0, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
}

TEST_P(QuicFramerTest, LargePacket) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[kMaxIncomingPacketSize + 1] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,
    // private flags
    0x00,
  };
  unsigned char packet44[kMaxIncomingPacketSize + 1] = {
    // type (short header 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,
  };
  unsigned char packet46[kMaxIncomingPacketSize + 1] = {
    // type (short header 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  const size_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER,
      VARIABLE_LENGTH_INTEGER_LENGTH_0, 0, VARIABLE_LENGTH_INTEGER_LENGTH_0);

  memset(p + header_size, 0, kMaxIncomingPacketSize - header_size);

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  ASSERT_TRUE(visitor_.header_.get());
  // Make sure we've parsed the packet header, so we can send an error.
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  // Make sure the correct error is propagated.
  EXPECT_EQ(QUIC_PACKET_TOO_LARGE, framer_.error());
  EXPECT_EQ("Packet too large.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, PacketHeader) {
  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"Unable to read public flags.",
       {0x28}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  PacketFragments& fragments = packet;

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);

  PacketHeaderFormat format;
  bool version_flag;
  uint8_t destination_connection_id_length;
  QuicConnectionId destination_connection_id;
  QuicVersionLabel version_label;
  std::string detailed_error;
  EXPECT_EQ(QUIC_NO_ERROR, QuicFramer::ProcessPacketDispatcher(
                               *encrypted, kQuicDefaultConnectionIdLength,
                               &format, &version_flag, &version_label,
                               &destination_connection_id_length,
                               &destination_connection_id, &detailed_error));
  EXPECT_EQ(GOOGLE_QUIC_PACKET, format);
  EXPECT_FALSE(version_flag);
  EXPECT_EQ(kQuicDefaultConnectionIdLength, destination_connection_id_length);
  EXPECT_EQ(FramerTestConnectionId(), destination_connection_id);
}

TEST_P(QuicFramerTest, LongPacketHeader) {
  // clang-format off
  PacketFragments packet44 = {
    // type (long header with packet type INITIAL)
    {"Unable to read type.",
     {0xFF}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"Unable to read ConnectionId length.",
     {0x50}},
    // connection_id
    {"Unable to read Destination ConnectionId.",
     {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
    // packet number
    {"Unable to read packet number.",
     {0x12, 0x34, 0x56, 0x78}},
  };
  PacketFragments packet46 = {
    // type (long header with packet type INITIAL)
    {"Unable to read type.",
     {0xC3}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"Unable to read ConnectionId length.",
     {0x50}},
    // connection_id
    {"Unable to read Destination ConnectionId.",
     {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
    // packet number
    {"Unable to read packet number.",
     {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  if (framer_.transport_version() <= QUIC_VERSION_43 ||
      QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_44 ? packet46 : packet44;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_TRUE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(
      framer_.transport_version() > QUIC_VERSION_44 ? packet46 : packet44,
      QUIC_INVALID_PACKET_HEADER);

  PacketHeaderFormat format;
  bool version_flag;
  uint8_t destination_connection_id_length;
  QuicConnectionId destination_connection_id;
  QuicVersionLabel version_label;
  std::string detailed_error;
  EXPECT_EQ(QUIC_NO_ERROR, QuicFramer::ProcessPacketDispatcher(
                               *encrypted, kQuicDefaultConnectionIdLength,
                               &format, &version_flag, &version_label,
                               &destination_connection_id_length,
                               &destination_connection_id, &detailed_error));
  EXPECT_EQ(IETF_QUIC_LONG_HEADER_PACKET, format);
  EXPECT_TRUE(version_flag);
  EXPECT_EQ(kQuicDefaultConnectionIdLength, destination_connection_id_length);
  EXPECT_EQ(FramerTestConnectionId(), destination_connection_id);
}

TEST_P(QuicFramerTest, PacketHeaderWith0ByteConnectionId) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLastSerializedServerConnectionId(&framer_,
                                                      FramerTestConnectionId());
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  // clang-format off
  PacketFragments packet = {
      // public flags (0 byte connection_id)
      {"Unable to read public flags.",
       {0x20}},
      // connection_id
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet44 = {
        // type (short header, 4 byte packet number)
        {"Unable to read type.",
         {0x32}},
        // connection_id
        // packet number
        {"Unable to read packet number.",
         {0x12, 0x34, 0x56, 0x78}},
   };

  PacketFragments packet46 = {
        // type (short header, 4 byte packet number)
        {"Unable to read type.",
         {0x43}},
        // connection_id
        // packet number
        {"Unable to read packet number.",
         {0x12, 0x34, 0x56, 0x78}},
   };

  PacketFragments packet_hp = {
        // type (short header, 4 byte packet number)
        {"Unable to read type.",
         {0x43}},
        // connection_id
        // packet number
        {"",
         {0x12, 0x34, 0x56, 0x78}},
   };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection()
          ? packet_hp
          : framer_.transport_version() > QUIC_VERSION_44
                ? packet46
                : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                 : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  if (!GetQuicRestartFlag(quic_do_not_override_connection_id)) {
    EXPECT_EQ(FramerTestConnectionId(),
              visitor_.header_->destination_connection_id);
  } else {
    EXPECT_EQ(FramerTestConnectionId(), visitor_.header_->source_connection_id);
  }
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWithVersionFlag) {
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  PacketFragments packet = {
      // public flags (0 byte connection_id)
      {"Unable to read public flags.",
       {0x29}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet44 = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      {"Unable to read type.",
       {0xFC}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"Unable to read ConnectionId length.",
       {0x50}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet46 = {
      // type (long header with packet type ZERO_RTT_PROTECTED and 4 bytes
      // packet number)
      {"Unable to read type.",
       {0xD3}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"Unable to read ConnectionId length.",
       {0x50}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet99 = {
      // type (long header with packet type ZERO_RTT_PROTECTED and 4 bytes
      // packet number)
      {"Unable to read type.",
       {0xD3}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"Unable to read ConnectionId length.",
       {0x50}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // long header packet length
      {"Unable to read long header payload length.",
       {0x04}},
      // packet number
      {"Long header payload length longer than packet.",
       {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : framer_.transport_version() > QUIC_VERSION_44
                ? packet46
                : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                 : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_TRUE(visitor_.header_->version_flag);
  EXPECT_EQ(GetParam(), visitor_.header_->version);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith4BytePacketNumber) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id and 4 byte packet number)
      {"Unable to read public flags.",
       {0x28}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"Unable to read type.",
       {0x32}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"Unable to read type.",
       {0x43}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet_hp = {
      // type (short header, 4 byte packet number)
      {"Unable to read type.",
       {0x43}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection()
          ? packet_hp
          : framer_.transport_version() > QUIC_VERSION_44
                ? packet46
                : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                 : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith2BytePacketNumber) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id and 2 byte packet number)
      {"Unable to read public flags.",
       {0x18}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x56, 0x78}},
  };

  PacketFragments packet44 = {
      // type (short header, 2 byte packet number)
      {"Unable to read type.",
       {0x31}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x56, 0x78}},
  };

  PacketFragments packet46 = {
      // type (short header, 2 byte packet number)
      {"Unable to read type.",
       {0x41}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x56, 0x78}},
  };

  PacketFragments packet_hp = {
      // type (short header, 2 byte packet number)
      {"Unable to read type.",
       {0x41}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78}},
      // padding
      {"", {0x00, 0x00}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection()
          ? packet_hp
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  if (framer_.version().HasHeaderProtection()) {
    EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
    EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  } else {
    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
    EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  }
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith1BytePacketNumber) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id and 1 byte packet number)
      {"Unable to read public flags.",
       {0x08}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78}},
  };

  PacketFragments packet44 = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read type.",
       {0x30}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78}},
  };

  PacketFragments packet46 = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read type.",
       {0x40}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78}},
  };

  PacketFragments packet_hp = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read type.",
       {0x40}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78}},
      // padding
      {"", {0x00, 0x00, 0x00}},
  };

  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection()
          ? packet_hp
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  if (framer_.version().HasHeaderProtection()) {
    EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
    EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  } else {
    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
    EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  }
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketNumberDecreasesThenIncreases) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // Test the case when a packet is received from the past and future packet
  // numbers are still calculated relative to the largest received packet.
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber - 2;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  QuicEncryptedPacket encrypted(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 2, visitor_.header_->packet_number);

  // Receive a 1 byte packet number.
  header.packet_number = kPacketNumber;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted1(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted1));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  // Process a 2 byte packet number 256 packets ago.
  header.packet_number = kPacketNumber - 256;
  header.packet_number_length = PACKET_2BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted2(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted2));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 256, visitor_.header_->packet_number);

  // Process another 1 byte packet number and ensure it works.
  header.packet_number = kPacketNumber - 1;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted3(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted3));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 1, visitor_.header_->packet_number);
}

TEST_P(QuicFramerTest, PacketWithDiversificationNonce) {
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // public flags: includes nonce flag
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[] = {
    // type: Long header with packet type ZERO_RTT_PROTECTED
    0xFC,
    // version tag
    QUIC_VERSION_BYTES,
    // connection_id length
    0x05,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet46[] = {
    // type: Long header with packet type ZERO_RTT_PROTECTED and 1 byte packet
    // number.
    0xD0,
    // version tag
    QUIC_VERSION_BYTES,
    // connection_id length
    0x05,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet99[] = {
    // type: Long header with packet type ZERO_RTT_PROTECTED and 1 byte packet
    // number.
    0xD0,
    // version tag
    QUIC_VERSION_BYTES,
    // connection_id length
    0x05,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x26,
    // packet number
    0x78,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  if (framer_.version().handshake_protocol != PROTOCOL_QUIC_CRYPTO) {
    return;
  }

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_TRUE(visitor_.header_->nonce != nullptr);
  for (char i = 0; i < 32; ++i) {
    EXPECT_EQ(i, (*visitor_.header_->nonce)[static_cast<size_t>(i)]);
  }
  EXPECT_EQ(1u, visitor_.padding_frames_.size());
  EXPECT_EQ(5, visitor_.padding_frames_[0]->num_padding_bytes);
}

TEST_P(QuicFramerTest, LargePublicFlagWithMismatchedVersions) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id, version flag and an unknown flag)
    0x29,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // version tag
    'Q', '0', '0', '0',
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[] = {
    // type (long header with packet type ZERO_RTT_PROTECTED)
    0xFC,
    // version tag
    'Q', '0', '0', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet45[] = {
    // type (long header, ZERO_RTT_PROTECTED, 4-byte packet number)
    0xD3,
    // version tag
    'Q', '0', '0', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet45;
    p_size = QUIC_ARRAYSIZE(packet45);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(0, visitor_.frame_count_);
  EXPECT_EQ(1, visitor_.version_mismatch_);
  ASSERT_EQ(1u, visitor_.padding_frames_.size());
  EXPECT_EQ(5, visitor_.padding_frames_[0]->num_padding_bytes);
}

TEST_P(QuicFramerTest, PaddingFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type - IETF_STREAM with FIN, LEN, and OFFSET bits set.
    0x08 | 0x01 | 0x02 | 0x04,

    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62OneByte + 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(2u, visitor_.padding_frames_.size());
  EXPECT_EQ(2, visitor_.padding_frames_[0]->num_padding_bytes);
  EXPECT_EQ(2, visitor_.padding_frames_[1]->num_padding_bytes);
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());
}

TEST_P(QuicFramerTest, StreamFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFF}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFF}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFF}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type - IETF_STREAM with FIN, LEN, and OFFSET bits set.
      {"",
       { 0x08 | 0x01 | 0x02 | 0x04 }},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

// Test an empty (no data) stream frame.
TEST_P(QuicFramerTest, EmptyStreamFrame) {
  // Only the IETF QUIC spec explicitly says that empty
  // stream frames are supported.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type - IETF_STREAM with FIN, LEN, and OFFSET bits set.
      {"",
       { 0x08 | 0x01 | 0x02 | 0x04 }},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  EXPECT_EQ(visitor_.stream_frames_[0].get()->data_length, 0u);

  CheckFramingBoundaries(packet, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, MissingDiversificationNonce) {
  if (framer_.version().handshake_protocol != PROTOCOL_QUIC_CRYPTO) {
    // TLS does not use diversification nonces.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  decrypter_ = new test::TestDecrypter();
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_INITIAL, QuicMakeUnique<NullDecrypter>(
                                                     Perspective::IS_CLIENT));
    framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                             std::unique_ptr<QuicDecrypter>(decrypter_));
  } else {
    framer_.SetDecrypter(ENCRYPTION_INITIAL,
                         QuicMakeUnique<NullDecrypter>(Perspective::IS_CLIENT));
    framer_.SetAlternativeDecrypter(
        ENCRYPTION_ZERO_RTT, std::unique_ptr<QuicDecrypter>(decrypter_), false);
  }

  // clang-format off
  unsigned char packet[] = {
        // public flags (8 byte connection_id)
        0x28,
        // connection_id
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        // packet number
        0x12, 0x34, 0x56, 0x78,
        // padding frame
        0x00,
    };

  unsigned char packet44[] = {
        // type (long header, ZERO_RTT_PROTECTED, 4-byte packet number)
        0xFC,
        // version tag
        QUIC_VERSION_BYTES,
        // connection_id length
        0x05,
        // connection_id
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        // packet number
        0x12, 0x34, 0x56, 0x78,
        // padding frame
        0x00,
    };

  unsigned char packet46[] = {
        // type (long header, ZERO_RTT_PROTECTED, 4-byte packet number)
        0xD3,
        // version tag
        QUIC_VERSION_BYTES,
        // connection_id length
        0x05,
        // connection_id
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        // packet number
        0x12, 0x34, 0x56, 0x78,
        // padding frame
        0x00,
    };

  unsigned char packet99[] = {
        // type (long header, ZERO_RTT_PROTECTED, 4-byte packet number)
        0xD3,
        // version tag
        QUIC_VERSION_BYTES,
        // connection_id length
        0x05,
        // connection_id
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        // IETF long header payload length
        0x05,
        // packet number
        0x12, 0x34, 0x56, 0x78,
        // padding frame
        0x00,
    };
  // clang-format on

  unsigned char* p = packet;
  size_t p_length = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_length = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() >= QUIC_VERSION_46) {
    p = packet46;
    p_length = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() >= QUIC_VERSION_44) {
    p = packet44;
    p_length = QUIC_ARRAYSIZE(packet44);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  if (framer_.version().HasHeaderProtection()) {
    EXPECT_EQ(QUIC_DECRYPTION_FAILURE, framer_.error());
    EXPECT_EQ("Unable to decrypt header protection.", framer_.detailed_error());
  } else if (framer_.transport_version() >= QUIC_VERSION_44) {
    // Cannot read diversification nonce.
    EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
    EXPECT_EQ("Unable to read nonce.", framer_.detailed_error());
  } else {
    EXPECT_EQ(QUIC_DECRYPTION_FAILURE, framer_.error());
  }
}

TEST_P(QuicFramerTest, StreamFrame3ByteStreamId) {
  if (framer_.transport_version() > QUIC_VERSION_43) {
    // This test is nonsensical for IETF Quic.
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments = packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, StreamFrame2ByteStreamId) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFD}},
      // stream id
      {"Unable to read stream_id.",
       {0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (stream frame with fin)
       {"",
        {0xFD}},
       // stream id
       {"Unable to read stream_id.",
        {0x03, 0x04}},
       // offset
       {"Unable to read offset.",
        {0x3A, 0x98, 0xFE, 0xDC,
         0x32, 0x10, 0x76, 0x54}},
       {"Unable to read frame data.",
        {
          // data length
          0x00, 0x0c,
          // data
          'h',  'e',  'l',  'l',
          'o',  ' ',  'w',  'o',
          'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (stream frame with fin)
       {"",
        {0xFD}},
       // stream id
       {"Unable to read stream_id.",
        {0x03, 0x04}},
       // offset
       {"Unable to read offset.",
        {0x3A, 0x98, 0xFE, 0xDC,
         0x32, 0x10, 0x76, 0x54}},
       {"Unable to read frame data.",
        {
          // data length
          0x00, 0x0c,
          // data
          'h',  'e',  'l',  'l',
          'o',  ' ',  'w',  'o',
          'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_STREAM frame with LEN, FIN, and OFFSET bits set)
      {"",
       {0x08 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62TwoBytes + 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 2 bytes of kStreamId.
  EXPECT_EQ(0x0000FFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, StreamFrame1ByteStreamId) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFC}},
      // stream id
      {"Unable to read stream_id.",
       {0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFC}},
      // stream id
      {"Unable to read stream_id.",
       {0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFC}},
      // stream id
      {"Unable to read stream_id.",
       {0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_STREAM frame with LEN, FIN, and OFFSET bits set)
      {"",
       {0x08 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62OneByte + 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 1 byte of kStreamId.
  EXPECT_EQ(0x000000FF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, StreamFrameWithVersion) {
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  PacketFragments packet = {
      // public flags (version, 8 byte connection_id)
      {"",
       {0x29}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet44 = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED)
      {"",
       {0xFC}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"",
       {0x50}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet46 = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      {"",
       {0xD3}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"",
       {0x50}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet99 = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      {"",
       {0xD3}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"",
       {0x50}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // long header packet length
      {"",
       {0x1E}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      {"",
       {0x08 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Long header payload length longer than packet.",
       {kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04}},
      // offset
      {"Long header payload length longer than packet.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Long header payload length longer than packet.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Long header payload length longer than packet.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  QuicVariableLengthIntegerLength retry_token_length_length =
      VARIABLE_LENGTH_INTEGER_LENGTH_0;
  size_t retry_token_length = 0;
  QuicVariableLengthIntegerLength length_length =
      QuicVersionHasLongHeaderLengths(framer_.transport_version())
          ? VARIABLE_LENGTH_INTEGER_LENGTH_1
          : VARIABLE_LENGTH_INTEGER_LENGTH_0;

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID,
      retry_token_length_length, retry_token_length, length_length));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments,
                         framer_.transport_version() == QUIC_VERSION_99
                             ? QUIC_INVALID_PACKET_HEADER
                             : QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, RejectPacket) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  visitor_.accept_packet_ = false;

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin)
      0xFF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (STREAM Frame with FIN, LEN, and OFFSET bits set)
      0x10 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
  };

  unsigned char packet46[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (STREAM Frame with FIN, LEN, and OFFSET bits set)
      0x10 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }
  QuicEncryptedPacket encrypted(AsChars(p),
                                framer_.transport_version() > QUIC_VERSION_43
                                    ? QUIC_ARRAYSIZE(packet44)
                                    : QUIC_ARRAYSIZE(packet),
                                false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
}

TEST_P(QuicFramerTest, RejectPublicHeader) {
  visitor_.accept_public_header_ = false;

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
  };

  unsigned char packet44[] = {
    // type (short header, 1 byte packet number)
    0x30,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x01,
  };

  unsigned char packet46[] = {
    // type (short header, 1 byte packet number)
    0x40,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x01,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(
      framer_.transport_version() > QUIC_VERSION_44
          ? AsChars(packet46)
          : (framer_.transport_version() > QUIC_VERSION_43 ? AsChars(packet44)
                                                           : AsChars(packet)),
      framer_.transport_version() > QUIC_VERSION_44
          ? QUIC_ARRAYSIZE(packet46)
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? QUIC_ARRAYSIZE(packet44)
                 : QUIC_ARRAYSIZE(packet)),
      false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_FALSE(visitor_.header_->packet_number.IsInitialized());
}

TEST_P(QuicFramerTest, AckFrameOneAckBlock) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet44 = {
      // type (short packet, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet46 = {
      // type (short packet, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet99 = {
      // type (short packet, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK)
       // (one ack block, 2 byte largest observed, 2 byte block length)
       // IETF-Quic ignores the bit-fields in the ack type, all of
       // that information is encoded elsewhere in the frame.
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62TwoBytes  + 0x12, 0x34}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
      // Ack block count (0 -- no blocks after the first)
      {"Unable to read ack block count.",
       {kVarInt62OneByte + 0x00}},
       // first ack block length - 1.
       // IETF Quic defines the ack block's value as the "number of
       // packets that preceed the largest packet number in the block"
       // which for the 1st ack block is the largest acked field,
       // above. This means that if we are acking just packet 0x1234
       // then the 1st ack block will be 0.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes + 0x12, 0x33}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kSmallLargestObserved, LargestAcked(frame));
  ASSERT_EQ(4660u, frame.packets.NumPacketsSlow());

  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the first ack block is larger than the
// largest_acked packet.
TEST_P(QuicFramerTest, FirstAckFrameUnderflow) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x88, 0x88}},
      // num timestamps.
      {"Underflow with first ack block length 34952 largest acked is 4660.",
       {0x00}}
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x88, 0x88}},
      // num timestamps.
      {"Underflow with first ack block length 34952 largest acked is 4660.",
       {0x00}}
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x88, 0x88}},
      // num timestamps.
      {"Underflow with first ack block length 34952 largest acked is 4660.",
       {0x00}}
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62TwoBytes  + 0x12, 0x34}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (0 -- no blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x00}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes + 0x28, 0x88}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the third ack block's gap is larger than the
// available space in the ack range.
TEST_P(QuicFramerTest, ThirdAckBlockUnderflowGap) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 63}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (2 -- 2 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x02}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 13}},  // Ack 14 packets, range 50..63 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 9}},  // Gap 10 packets, 40..49 (inclusive)
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 9}},  // Ack 10 packets, 30..39 (inclusive)
       {"Unable to read gap block value.",
        {kVarInt62OneByte + 29}},  // A gap of 30 packets (0..29 inclusive)
                                   // should be too big, leaving no room
                                   // for the ack.
       {"Underflow with gap block length 30 previous ack block start is 30.",
        {kVarInt62OneByte + 10}},  // Don't care
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(
      framer_.detailed_error(),
      "Underflow with gap block length 30 previous ack block start is 30.");
  CheckFramingBoundaries(packet99, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the third ack block's length is larger than the
// available space in the ack range.
TEST_P(QuicFramerTest, ThirdAckBlockUnderflowAck) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 63}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (2 -- 2 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x02}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 13}},  // only 50 packet numbers "left"

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 10}},  // Only 40 packet numbers left
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 10}},  // only 30 packet numbers left.
       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap is OK, 29 packet numbers left
      {"Unable to read ack block value.",
        {kVarInt62OneByte + 30}},  // Use up all 30, should be an error
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with ack block length 31 latest ack block end is 25.");
  CheckFramingBoundaries(packet99, QUIC_INVALID_ACK_DATA);
}

// Tests a variety of ack block wrap scenarios. For example, if the
// N-1th block causes packet 0 to be acked, then a gap would wrap
// around to 0x3fffffff ffffffff... Make sure we detect this
// condition.
TEST_P(QuicFramerTest, AckBlockUnderflowGapWrap) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 10}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (1 -- 1 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 1}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 9}},  // Ack packets 1..10 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap of 2 packets (-1...0), should wrap
       {"Underflow with gap block length 2 previous ack block start is 1.",
        {kVarInt62OneByte + 9}},  // irrelevant
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with gap block length 2 previous ack block start is 1.");
  CheckFramingBoundaries(packet99, QUIC_INVALID_ACK_DATA);
}

// As AckBlockUnderflowGapWrap, but in this test, it's the ack
// component of the ack-block that causes the wrap, not the gap.
TEST_P(QuicFramerTest, AckBlockUnderflowAckWrap) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 10}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (1 -- 1 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 1}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 6}},  // Ack packets 4..10 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap of 2 packets (2..3)
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 9}},  // Should wrap.
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with ack block length 10 latest ack block end is 1.");
  CheckFramingBoundaries(packet99, QUIC_INVALID_ACK_DATA);
}

// An ack block that acks the entire range, 1...0x3fffffffffffffff
TEST_P(QuicFramerTest, AckBlockAcksEverything) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62EightBytes  + 0x3f, 0xff, 0xff, 0xff,
         0xff, 0xff, 0xff, 0xff}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count No additional blocks
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62EightBytes  + 0x3f, 0xff, 0xff, 0xff,
         0xff, 0xff, 0xff, 0xfe}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(1u, frame.packets.NumIntervals());
  EXPECT_EQ(kLargestIetfLargestObserved, LargestAcked(frame));
  EXPECT_EQ(kLargestIetfLargestObserved.ToUint64(),
            frame.packets.NumPacketsSlow());
}

// This test looks for a malformed ack where
//  - There is a largest-acked value (that is, the frame is acking
//    something,
//  - But the length of the first ack block is 0 saying that no frames
//    are being acked with the largest-acked value or there are no
//    additional ack blocks.
//
TEST_P(QuicFramerTest, AckFrameFirstAckBlockLengthZero) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // Not applicable to version 99 -- first ack block contains the
    // number of packets that preceed the largest_acked packet.
    // A value of 0 means no packets preceed --- that the block's
    // length is 1. Therefore the condition that this test checks can
    // not arise.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       { 0x2C }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x01 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x00 }},
      // gap to next block.
      { "First block length is zero.",
        { 0x01 }},
      // ack block length.
      { "First block length is zero.",
        { 0x0e, 0xaf }},
      // Number of timestamps.
      { "First block length is zero.",
        { 0x00 }},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x32 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x01 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x00 }},
      // gap to next block.
      { "First block length is zero.",
        { 0x01 }},
      // ack block length.
      { "First block length is zero.",
        { 0x0e, 0xaf }},
      // Number of timestamps.
      { "First block length is zero.",
        { 0x00 }},
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x01 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x00 }},
      // gap to next block.
      { "First block length is zero.",
        { 0x01 }},
      // ack block length.
      { "First block length is zero.",
        { 0x0e, 0xaf }},
      // Number of timestamps.
      { "First block length is zero.",
        { 0x00 }},
  };

  // clang-format on
  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_44
          ? packet46
          : (framer_.transport_version() > QUIC_VERSION_43 ? packet44 : packet);

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_INVALID_ACK_DATA, framer_.error());

  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

TEST_P(QuicFramerTest, AckFrameOneAckBlockMaxLength) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 4 byte largest observed, 2 byte block length)
      {"",
       {0x49}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34, 0x56, 0x78}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78, 0x9A, 0xBC}},
      // frame type (ack frame)
      // (one ack block, 4 byte largest observed, 2 byte block length)
      {"",
       {0x49}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34, 0x56, 0x78}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78, 0x9A, 0xBC}},
      // frame type (ack frame)
      // (one ack block, 4 byte largest observed, 2 byte block length)
      {"",
       {0x49}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34, 0x56, 0x78}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78, 0x9A, 0xBC}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62FourBytes  + 0x12, 0x34, 0x56, 0x78}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Number of ack blocks after first
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x00}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes  + 0x12, 0x33}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kPacketNumber, LargestAcked(frame));
  ASSERT_EQ(4660u, frame.packets.NumPacketsSlow());

  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// Tests ability to handle multiple ackblocks after the first ack
// block. Non-version-99 tests include multiple timestamps as well.
TEST_P(QuicFramerTest, AckFrameTwoTimeStampsMultipleAckBlocks) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       { 0x2C }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x04 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x01 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x01 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x0e, 0xaf }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0xff }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x00 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x91 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x01, 0xea }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x05 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x04 }},
      // Number of timestamps.
      { "Unable to read num received packets.",
        { 0x02 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x01 }},
      // Delta time.
      { "Unable to read time delta in received packets.",
        { 0x76, 0x54, 0x32, 0x10 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x02 }},
      // Delta time.
      { "Unable to read incremental time delta in received packets.",
        { 0x32, 0x10 }},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x32 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x04 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x01 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x01 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x0e, 0xaf }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0xff }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x00 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x91 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x01, 0xea }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x05 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x04 }},
      // Number of timestamps.
      { "Unable to read num received packets.",
        { 0x02 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x01 }},
      // Delta time.
      { "Unable to read time delta in received packets.",
        { 0x76, 0x54, 0x32, 0x10 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x02 }},
      // Delta time.
      { "Unable to read incremental time delta in received packets.",
        { 0x32, 0x10 }},
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x04 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x01 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x01 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x0e, 0xaf }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0xff }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x00 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x91 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x01, 0xea }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x05 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x04 }},
      // Number of timestamps.
      { "Unable to read num received packets.",
        { 0x02 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x01 }},
      // Delta time.
      { "Unable to read time delta in received packets.",
        { 0x76, 0x54, 0x32, 0x10 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x02 }},
      // Delta time.
      { "Unable to read incremental time delta in received packets.",
        { 0x32, 0x10 }},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (IETF_ACK frame)
      {"",
       { 0x02 }},
       // largest acked
       {"Unable to read largest acked.",
        { kVarInt62TwoBytes + 0x12, 0x34 }},   // = 4660
       // Zero delta time.
       {"Unable to read ack delay time.",
        { kVarInt62OneByte + 0x00 }},
       // number of additional ack blocks
       {"Unable to read ack block count.",
        { kVarInt62OneByte + 0x03 }},
       // first ack block length.
       {"Unable to read first ack block length.",
        { kVarInt62OneByte + 0x00 }},  // 1st block length = 1

       // Additional ACK Block #1
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62OneByte + 0x00 }},   // gap of 1 packet
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62TwoBytes + 0x0e, 0xae }},   // 3759

       // pre-version-99 test includes an ack block of 0 length. this
       // can not happen in version 99. ergo the second block is not
       // present in the v99 test and the gap length of the next block
       // is the sum of the two gaps in the pre-version-99 tests.
       // Additional ACK Block #2
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62TwoBytes + 0x01, 0x8f }},  // Gap is 400 (0x190) pkts
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62TwoBytes + 0x01, 0xe9 }},  // block is 389 (x1ea) pkts

       // Additional ACK Block #3
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62OneByte + 0x04 }},   // Gap is 5 packets.
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62OneByte + 0x03 }},   // block is 3 packets.
  };

  // clang-format on
  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  framer_.set_process_timestamps(true);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kSmallLargestObserved, LargestAcked(frame));
  ASSERT_EQ(4254u, frame.packets.NumPacketsSlow());
  EXPECT_EQ(4u, frame.packets.NumIntervals());
  if (framer_.transport_version() == QUIC_VERSION_99) {
    EXPECT_EQ(0u, frame.received_packet_times.size());
  } else {
    EXPECT_EQ(2u, frame.received_packet_times.size());
  }
  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

TEST_P(QuicFramerTest, AckFrameTimeStampDeltaTooHigh) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 1 byte largest observed, 1 byte block length)
      0x40,
      // largest acked
      0x01,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x01,
      // num timestamps.
      0x01,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 1 byte largest observed, 1 byte block length)
      0x40,
      // largest acked
      0x01,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x01,
      // num timestamps.
      0x01,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
  };

  unsigned char packet46[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 1 byte largest observed, 1 byte block length)
      0x40,
      // largest acked
      0x01,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x01,
      // num timestamps.
      0x01,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
  };
  // clang-format on
  if (framer_.transport_version() == QUIC_VERSION_99) {
    return;
  }
  QuicEncryptedPacket encrypted(
      AsChars(framer_.transport_version() > QUIC_VERSION_44
                  ? packet46
                  : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                   : packet)),
      QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_TRUE(QuicTextUtils::StartsWith(
      framer_.detailed_error(), "delta_from_largest_observed too high"));
}

TEST_P(QuicFramerTest, AckFrameTimeStampSecondDeltaTooHigh) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 1 byte largest observed, 1 byte block length)
      0x40,
      // largest acked
      0x03,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x03,
      // num timestamps.
      0x02,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
      // Delta from largest observed.
      0x03,
      // Delta time.
      0x10, 0x32,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 1 byte largest observed, 1 byte block length)
      0x40,
      // largest acked
      0x03,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x03,
      // num timestamps.
      0x02,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
      // Delta from largest observed.
      0x03,
      // Delta time.
      0x10, 0x32,
  };

  unsigned char packet46[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 1 byte largest observed, 1 byte block length)
      0x40,
      // largest acked
      0x03,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x03,
      // num timestamps.
      0x02,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
      // Delta from largest observed.
      0x03,
      // Delta time.
      0x10, 0x32,
  };
  // clang-format on
  if (framer_.transport_version() == QUIC_VERSION_99) {
    return;
  }
  QuicEncryptedPacket encrypted(
      AsChars(framer_.transport_version() > QUIC_VERSION_44
                  ? packet46
                  : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                   : packet)),
      QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_TRUE(QuicTextUtils::StartsWith(
      framer_.detailed_error(), "delta_from_largest_observed too high"));
}

TEST_P(QuicFramerTest, NewStopWaitingFrame) {
  if (version_.transport_version == QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stop waiting frame)
      {"",
       {0x06}},
      // least packet number awaiting an ack, delta from packet number.
      {"Unable to read least unacked delta.",
        {0x00, 0x00, 0x00, 0x08}}
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stop waiting frame)
      {"",
       {0x06}},
      // least packet number awaiting an ack, delta from packet number.
      {"Unable to read least unacked delta.",
        {0x00, 0x00, 0x00, 0x08}}
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stop waiting frame)
      {"",
       {0x06}},
      // least packet number awaiting an ack, delta from packet number.
      {"Unable to read least unacked delta.",
        {0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_44
          ? packet46
          : (framer_.transport_version() > QUIC_VERSION_43 ? packet44 : packet);

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  if (GetQuicReloadableFlag(quic_do_not_accept_stop_waiting) &&
      version_.transport_version >= QUIC_VERSION_44) {
    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
    EXPECT_EQ(QUIC_INVALID_STOP_WAITING_DATA, framer_.error());
    EXPECT_EQ("STOP WAITING not supported in version 44+.",
              framer_.detailed_error());
    return;
  }

  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.stop_waiting_frames_.size());
  const QuicStopWaitingFrame& frame = *visitor_.stop_waiting_frames_[0];
  EXPECT_EQ(kLeastUnacked, frame.least_unacked);

  CheckFramingBoundaries(fragments, QUIC_INVALID_STOP_WAITING_DATA);
}

TEST_P(QuicFramerTest, InvalidNewStopWaitingFrame) {
  if (version_.transport_version == QUIC_VERSION_99 ||
      (GetQuicReloadableFlag(quic_do_not_accept_stop_waiting) &&
       version_.transport_version >= QUIC_VERSION_44)) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x13, 0x34, 0x56, 0x78,
    0x9A, 0xA8,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x57, 0x78, 0x9A, 0xA8,
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x57, 0x78, 0x9A, 0xA8,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(
      AsChars(framer_.transport_version() > QUIC_VERSION_44
                  ? packet46
                  : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                   : packet)),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet),
      false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_STOP_WAITING_DATA, framer_.error());
  EXPECT_EQ("Invalid unacked delta.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, RstStreamFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (rst stream frame)
      {"",
       {0x01}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // sent byte offset
      {"Unable to read rst stream sent byte offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // error code
      {"Unable to read rst stream error code.",
       {0x00, 0x00, 0x00, 0x01}}
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (rst stream frame)
      {"",
       {0x01}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // sent byte offset
      {"Unable to read rst stream sent byte offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // error code
      {"Unable to read rst stream error code.",
       {0x00, 0x00, 0x00, 0x01}}
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (rst stream frame)
      {"",
       {0x01}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // sent byte offset
      {"Unable to read rst stream sent byte offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // error code
      {"Unable to read rst stream error code.",
       {0x00, 0x00, 0x00, 0x01}}
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_RST_STREAM frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read rst stream stream id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // application error code
      {"Unable to read rst stream error code.",
       {0x00, 0x01}},   // Not varint62 encoded
      // Final Offset
      {"Unable to read rst stream sent byte offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.rst_stream_frame_.stream_id);
  EXPECT_EQ(0x01, visitor_.rst_stream_frame_.error_code);
  EXPECT_EQ(kStreamOffset, visitor_.rst_stream_frame_.byte_offset);
  CheckFramingBoundaries(fragments, QUIC_INVALID_RST_STREAM_DATA);
}

TEST_P(QuicFramerTest, ConnectionCloseFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x00, 0x00, 0x11}},
      {"Unable to read connection close error details.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x00, 0x00, 0x11}},
      {"Unable to read connection close error details.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x00, 0x00, 0x11}},
      {"Unable to read connection close error details.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_CONNECTION_CLOSE frame)
      {"",
       {0x1c}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x11}},
      {"Unable to read connection close frame type.",
       {kVarInt62TwoBytes + 0x12, 0x34 }},
      {"Unable to read connection close error details.",
       {
         // error details length
         kVarInt62OneByte + 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0x11u, static_cast<unsigned>(
                       visitor_.connection_close_frame_.quic_error_code));
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    EXPECT_EQ(0x1234u,
              visitor_.connection_close_frame_.transport_close_frame_type);
  }

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(fragments, QUIC_INVALID_CONNECTION_CLOSE_DATA);
}

// Test the CONNECTION_CLOSE/Application variant.
TEST_P(QuicFramerTest, ApplicationCloseFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame does not exist in versions other than 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_CONNECTION_CLOSE/Application frame)
      {"",
       {0x1d}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x11}},
      {"Unable to read connection close error details.",
       {
         // error details length
         kVarInt62OneByte + 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(IETF_QUIC_APPLICATION_CONNECTION_CLOSE,
            visitor_.connection_close_frame_.close_type);
  EXPECT_EQ(122u, visitor_.connection_close_frame_.extracted_error_code);
  EXPECT_EQ(0x11, visitor_.connection_close_frame_.quic_error_code);
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet99, QUIC_INVALID_CONNECTION_CLOSE_DATA);
}

TEST_P(QuicFramerTest, GoAwayFrame) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This frame is not supported in version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (go away frame)
      {"",
       {0x03}},
      // error code
      {"Unable to read go away error code.",
       {0x00, 0x00, 0x00, 0x09}},
      // stream id
      {"Unable to read last good stream id.",
       {0x01, 0x02, 0x03, 0x04}},
      // stream id
      {"Unable to read goaway reason.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (go away frame)
      {"",
       {0x03}},
      // error code
      {"Unable to read go away error code.",
       {0x00, 0x00, 0x00, 0x09}},
      // stream id
      {"Unable to read last good stream id.",
       {0x01, 0x02, 0x03, 0x04}},
      // stream id
      {"Unable to read goaway reason.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (go away frame)
      {"",
       {0x03}},
      // error code
      {"Unable to read go away error code.",
       {0x00, 0x00, 0x00, 0x09}},
      // stream id
      {"Unable to read last good stream id.",
       {0x01, 0x02, 0x03, 0x04}},
      // stream id
      {"Unable to read goaway reason.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_44
          ? packet46
          : (framer_.transport_version() > QUIC_VERSION_43 ? packet44 : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.goaway_frame_.last_good_stream_id);
  EXPECT_EQ(0x9, visitor_.goaway_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.goaway_frame_.reason_phrase);

  CheckFramingBoundaries(fragments, QUIC_INVALID_GOAWAY_DATA);
}

TEST_P(QuicFramerTest, WindowUpdateFrame) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This frame is not in version 99, see MaxDataFrame and MaxStreamDataFrame
    // for Version 99 equivalents.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (window update frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Unable to read window byte_offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (window update frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Unable to read window byte_offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (window update frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Unable to read window byte_offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };

  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_44
          ? packet46
          : (framer_.transport_version() > QUIC_VERSION_43 ? packet44 : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.byte_offset);

  CheckFramingBoundaries(fragments, QUIC_INVALID_WINDOW_UPDATE_DATA);
}

TEST_P(QuicFramerTest, MaxDataFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is available only in version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_MAX_DATA frame)
      {"",
       {0x10}},
      // byte offset
      {"Can not read MAX_DATA byte-offset",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(QuicUtils::GetInvalidStreamId(framer_.transport_version()),
            visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.byte_offset);

  CheckFramingBoundaries(packet99, QUIC_INVALID_MAX_DATA_FRAME_DATA);
}

TEST_P(QuicFramerTest, MaxStreamDataFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame available only in version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_MAX_STREAM_DATA frame)
      {"",
       {0x11}},
      // stream id
      {"Can not read MAX_STREAM_DATA stream id",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Can not read MAX_STREAM_DATA byte-count",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.byte_offset);

  CheckFramingBoundaries(packet99, QUIC_INVALID_MAX_STREAM_DATA_FRAME_DATA);
}

TEST_P(QuicFramerTest, BlockedFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (blocked frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (blocked frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
  };

  PacketFragments packet46 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (blocked frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_STREAM_BLOCKED frame)
      {"",
       {0x15}},
      // stream id
      {"Can not read stream blocked stream id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // Offset
      {"Can not read stream blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? packet46
                 : (framer_.transport_version() > QUIC_VERSION_43 ? packet44
                                                                  : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  if (framer_.transport_version() == QUIC_VERSION_99) {
    EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);
  } else {
    EXPECT_EQ(0u, visitor_.blocked_frame_.offset);
  }
  EXPECT_EQ(kStreamId, visitor_.blocked_frame_.stream_id);

  if (framer_.transport_version() == QUIC_VERSION_99) {
    CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_BLOCKED_DATA);
  } else {
    CheckFramingBoundaries(fragments, QUIC_INVALID_BLOCKED_DATA);
  }
}

TEST_P(QuicFramerTest, PingFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
     // public flags (8 byte connection_id)
     0x28,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // frame type (ping frame)
     0x07,
    };

  unsigned char packet44[] = {
     // type (short header, 4 byte packet number)
     0x32,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // frame type
     0x07,
    };

  unsigned char packet46[] = {
     // type (short header, 4 byte packet number)
     0x43,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // frame type
     0x07,
    };

  unsigned char packet99[] = {
     // type (short header, 4 byte packet number)
     0x43,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // frame type (IETF_PING frame)
     0x01,
    };
  // clang-format on

  QuicEncryptedPacket encrypted(
      AsChars(framer_.transport_version() == QUIC_VERSION_99
                  ? packet99
                  : (framer_.transport_version() > QUIC_VERSION_44
                         ? packet46
                         : framer_.transport_version() > QUIC_VERSION_43
                               ? packet44
                               : packet)),
      framer_.transport_version() == QUIC_VERSION_99
          ? QUIC_ARRAYSIZE(packet99)
          : (framer_.transport_version() > QUIC_VERSION_44
                 ? QUIC_ARRAYSIZE(packet46)
                 : framer_.transport_version() > QUIC_VERSION_43
                       ? QUIC_ARRAYSIZE(packet44)
                       : QUIC_ARRAYSIZE(packet)),
      false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(1u, visitor_.ping_frames_.size());

  // No need to check the PING frame boundaries because it has no payload.
}

TEST_P(QuicFramerTest, MessageFrame) {
  if (framer_.transport_version() <= QUIC_VERSION_44) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet45 = {
       // type (short header, 4 byte packet number)
       {"",
        {0x32}},
       // connection_id
       {"",
        {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
       // packet number
       {"",
        {0x12, 0x34, 0x56, 0x78}},
       // message frame type.
       {"",
        { 0x21 }},
       // message length
       {"Unable to read message length",
        {0x07}},
       // message data
       {"Unable to read message data",
        {'m', 'e', 's', 's', 'a', 'g', 'e'}},
        // message frame no length.
        {"",
         { 0x20 }},
        // message data
        {{},
         {'m', 'e', 's', 's', 'a', 'g', 'e', '2'}},
   };

  PacketFragments packet46 = {
       // type (short header, 4 byte packet number)
       {"",
        {0x43}},
       // connection_id
       {"",
        {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
       // packet number
       {"",
        {0x12, 0x34, 0x56, 0x78}},
       // message frame type.
       {"",
        { 0x21 }},
       // message length
       {"Unable to read message length",
        {0x07}},
       // message data
       {"Unable to read message data",
        {'m', 'e', 's', 's', 'a', 'g', 'e'}},
        // message frame no length.
        {"",
         { 0x20 }},
        // message data
        {{},
         {'m', 'e', 's', 's', 'a', 'g', 'e', '2'}},
   };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(AssemblePacketFromFragments(
      framer_.transport_version() > QUIC_VERSION_44 ? packet46 : packet45));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(2u, visitor_.message_frames_.size());
  EXPECT_EQ(7u, visitor_.message_frames_[0]->message_length);
  EXPECT_EQ(8u, visitor_.message_frames_[1]->message_length);

  CheckFramingBoundaries(
      framer_.transport_version() > QUIC_VERSION_44 ? packet46 : packet45,
      QUIC_INVALID_MESSAGE_DATA);
}

TEST_P(QuicFramerTest, PublicResetPacketV33) {
  // clang-format off
  PacketFragments packet = {
      // public flags (public reset, 8 byte connection_id)
      {"",
       {0x0A}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      {"Unable to read reset message.",
       {
         // message tag (kPRST)
         'P', 'R', 'S', 'T',
         // num_entries (2) + padding
         0x02, 0x00, 0x00, 0x00,
         // tag kRNON
         'R', 'N', 'O', 'N',
         // end offset 8
         0x08, 0x00, 0x00, 0x00,
         // tag kRSEQ
         'R', 'S', 'E', 'Q',
         // end offset 16
         0x10, 0x00, 0x00, 0x00,
         // nonce proof
         0x89, 0x67, 0x45, 0x23,
         0x01, 0xEF, 0xCD, 0xAB,
         // rejected packet number
         0xBC, 0x9A, 0x78, 0x56,
         0x34, 0x12, 0x00, 0x00,
       }
      }
  };
  // clang-format on
  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.public_reset_packet_->connection_id);
  EXPECT_EQ(kNonceProof, visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ(
      IpAddressFamily::IP_UNSPEC,
      visitor_.public_reset_packet_->client_address.host().address_family());

  CheckFramingBoundaries(packet, QUIC_INVALID_PUBLIC_RST_PACKET);
}

TEST_P(QuicFramerTest, PublicResetPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  // clang-format off
  PacketFragments packet = {
      // public flags (public reset, 8 byte connection_id)
      {"",
       {0x0E}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      {"Unable to read reset message.",
       {
         // message tag (kPRST)
         'P', 'R', 'S', 'T',
         // num_entries (2) + padding
         0x02, 0x00, 0x00, 0x00,
         // tag kRNON
         'R', 'N', 'O', 'N',
         // end offset 8
         0x08, 0x00, 0x00, 0x00,
         // tag kRSEQ
         'R', 'S', 'E', 'Q',
         // end offset 16
         0x10, 0x00, 0x00, 0x00,
         // nonce proof
         0x89, 0x67, 0x45, 0x23,
         0x01, 0xEF, 0xCD, 0xAB,
         // rejected packet number
         0xBC, 0x9A, 0x78, 0x56,
         0x34, 0x12, 0x00, 0x00,
       }
      }
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.public_reset_packet_->connection_id);
  EXPECT_EQ(kNonceProof, visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ(
      IpAddressFamily::IP_UNSPEC,
      visitor_.public_reset_packet_->client_address.host().address_family());

  CheckFramingBoundaries(packet, QUIC_INVALID_PUBLIC_RST_PACKET);
}

TEST_P(QuicFramerTest, PublicResetPacketWithTrailingJunk) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (public reset, 8 byte connection_id)
    0x0A,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (2) + padding
    0x02, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // tag kRSEQ
    'R', 'S', 'E', 'Q',
    // end offset 16
    0x10, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected packet number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12, 0x00, 0x00,
    // trailing junk
    'j', 'u', 'n', 'k',
  };
  // clang-format on
  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_INVALID_PUBLIC_RST_PACKET, framer_.error());
  EXPECT_EQ("Unable to read reset message.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, PublicResetPacketWithClientAddress) {
  // clang-format off
  PacketFragments packet = {
      // public flags (public reset, 8 byte connection_id)
      {"",
       {0x0A}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      {"Unable to read reset message.",
       {
         // message tag (kPRST)
         'P', 'R', 'S', 'T',
         // num_entries (2) + padding
         0x03, 0x00, 0x00, 0x00,
         // tag kRNON
         'R', 'N', 'O', 'N',
         // end offset 8
         0x08, 0x00, 0x00, 0x00,
         // tag kRSEQ
         'R', 'S', 'E', 'Q',
         // end offset 16
         0x10, 0x00, 0x00, 0x00,
         // tag kCADR
         'C', 'A', 'D', 'R',
         // end offset 24
         0x18, 0x00, 0x00, 0x00,
         // nonce proof
         0x89, 0x67, 0x45, 0x23,
         0x01, 0xEF, 0xCD, 0xAB,
         // rejected packet number
         0xBC, 0x9A, 0x78, 0x56,
         0x34, 0x12, 0x00, 0x00,
         // client address: 4.31.198.44:443
         0x02, 0x00,
         0x04, 0x1F, 0xC6, 0x2C,
         0xBB, 0x01,
       }
      }
  };
  // clang-format on
  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.public_reset_packet_->connection_id);
  EXPECT_EQ(kNonceProof, visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ("4.31.198.44",
            visitor_.public_reset_packet_->client_address.host().ToString());
  EXPECT_EQ(443, visitor_.public_reset_packet_->client_address.port());

  CheckFramingBoundaries(packet, QUIC_INVALID_PUBLIC_RST_PACKET);
}

TEST_P(QuicFramerTest, IetfStatelessResetPacket) {
  // clang-format off
  unsigned char packet[] = {
      // type (short packet, 1 byte packet number)
      0x50,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // Random bytes
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      // stateless reset token
      0xB5, 0x69, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on
  if (framer_.transport_version() <= QUIC_VERSION_43) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  decrypter_ = new test::TestDecrypter();
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_INITIAL, QuicMakeUnique<NullDecrypter>(
                                                     Perspective::IS_CLIENT));
    framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                             std::unique_ptr<QuicDecrypter>(decrypter_));
  } else {
    framer_.SetDecrypter(ENCRYPTION_INITIAL,
                         QuicMakeUnique<NullDecrypter>(Perspective::IS_CLIENT));
    framer_.SetAlternativeDecrypter(
        ENCRYPTION_ZERO_RTT, std::unique_ptr<QuicDecrypter>(decrypter_), false);
  }
  // This packet cannot be decrypted because diversification nonce is missing.
  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.stateless_reset_packet_.get());
  EXPECT_EQ(kTestStatelessResetToken,
            visitor_.stateless_reset_packet_->stateless_reset_token);
}

TEST_P(QuicFramerTest, IetfStatelessResetPacketInvalidStatelessResetToken) {
  // clang-format off
  unsigned char packet[] = {
      // type (short packet, 1 byte packet number)
      0x50,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // stateless reset token
      0xB6, 0x69, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on
  if (framer_.transport_version() <= QUIC_VERSION_43) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  decrypter_ = new test::TestDecrypter();
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_INITIAL, QuicMakeUnique<NullDecrypter>(
                                                     Perspective::IS_CLIENT));
    framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                             std::unique_ptr<QuicDecrypter>(decrypter_));
  } else {
    framer_.SetDecrypter(ENCRYPTION_INITIAL,
                         QuicMakeUnique<NullDecrypter>(Perspective::IS_CLIENT));
    framer_.SetAlternativeDecrypter(
        ENCRYPTION_ZERO_RTT, std::unique_ptr<QuicDecrypter>(decrypter_), false);
  }
  // This packet cannot be decrypted because diversification nonce is missing.
  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_DECRYPTION_FAILURE, framer_.error());
  ASSERT_FALSE(visitor_.stateless_reset_packet_);
}

TEST_P(QuicFramerTest, VersionNegotiationPacketClient) {
  // clang-format off
  PacketFragments packet = {
      // public flags (version, 8 byte connection_id)
      {"",
       {0x29}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"Unable to read supported version in negotiation.",
       {QUIC_VERSION_BYTES,
        'Q', '2', '.', '0'}},
  };

  PacketFragments packet44 = {
      // type (long header)
      {"",
       {0x8F}},
      // version tag
      {"",
       {0x00, 0x00, 0x00, 0x00}},
      {"",
       {0x05}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // Supported versions
      {"Unable to read supported version in negotiation.",
       {QUIC_VERSION_BYTES,
        'Q', '2', '.', '0'}},
  };
  // clang-format on

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43 ? packet44 : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.version_negotiation_packet_.get());
  EXPECT_EQ(2u, visitor_.version_negotiation_packet_->versions.size());
  EXPECT_EQ(GetParam(), visitor_.version_negotiation_packet_->versions[0]);

  // Remove the last version from the packet so that every truncated
  // version of the packet is invalid, otherwise checking boundaries
  // is annoyingly complicated.
  for (size_t i = 0; i < 4; ++i) {
    fragments.back().fragment.pop_back();
  }
  CheckFramingBoundaries(fragments, QUIC_INVALID_VERSION_NEGOTIATION_PACKET);
}

TEST_P(QuicFramerTest, VersionNegotiationPacketServer) {
  if (!GetQuicRestartFlag(quic_server_drop_version_negotiation)) {
    return;
  }
  if (framer_.transport_version() < QUIC_VERSION_44) {
    return;
  }

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // clang-format off
  unsigned char packet[] = {
      // public flags (long header with all ignored bits set)
      0xFF,
      // version
      0x00, 0x00, 0x00, 0x00,
      // connection ID lengths
      0x50,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // supported versions
      QUIC_VERSION_BYTES,
      'Q', '2', '.', '0',
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_VERSION_NEGOTIATION_PACKET, framer_.error());
  EXPECT_EQ("Server received version negotiation packet.",
            framer_.detailed_error());
  EXPECT_FALSE(visitor_.version_negotiation_packet_.get());
}

TEST_P(QuicFramerTest, OldVersionNegotiationPacket) {
  // clang-format off
  PacketFragments packet = {
      // public flags (version, 8 byte connection_id)
      {"",
       {0x2D}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"Unable to read supported version in negotiation.",
       {QUIC_VERSION_BYTES,
        'Q', '2', '.', '0'}},
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.version_negotiation_packet_.get());
  EXPECT_EQ(2u, visitor_.version_negotiation_packet_->versions.size());
  EXPECT_EQ(GetParam(), visitor_.version_negotiation_packet_->versions[0]);

  // Remove the last version from the packet so that every truncated
  // version of the packet is invalid, otherwise checking boundaries
  // is annoyingly complicated.
  for (size_t i = 0; i < 4; ++i) {
    packet.back().fragment.pop_back();
  }
  CheckFramingBoundaries(packet, QUIC_INVALID_VERSION_NEGOTIATION_PACKET);
}

TEST_P(QuicFramerTest, ParseIetfRetryPacket) {
  if (!framer_.version().SupportsRetry()) {
    return;
  }
  // IETF RETRY is only sent from client to server.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  // clang-format off
  unsigned char packet[] = {
      // public flags (long header with packet type RETRY and ODCIL=8)
      0xF5,
      // version
      QUIC_VERSION_BYTES,
      // connection ID lengths
      0x05,
      // source connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // original destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // retry token
      'H', 'e', 'l', 'l', 'o', ' ', 't', 'h', 'i', 's',
      ' ', 'i', 's', ' ', 'R', 'E', 'T', 'R', 'Y', '!',
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_TRUE(visitor_.retry_original_connection_id_.get());
  ASSERT_TRUE(visitor_.retry_new_connection_id_.get());
  ASSERT_TRUE(visitor_.retry_token_.get());

  EXPECT_EQ(FramerTestConnectionId(),
            *visitor_.retry_original_connection_id_.get());
  EXPECT_EQ(FramerTestConnectionIdPlusOne(),
            *visitor_.retry_new_connection_id_.get());
  EXPECT_EQ("Hello this is RETRY!", *visitor_.retry_token_.get());
}

TEST_P(QuicFramerTest, RejectIetfRetryPacketAsServer) {
  if (!framer_.version().SupportsRetry()) {
    return;
  }
  // IETF RETRY is only sent from client to server.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // clang-format off
  unsigned char packet[] = {
      // public flags (long header with packet type RETRY and ODCIL=8)
      0xF5,
      // version
      QUIC_VERSION_BYTES,
      // connection ID lengths
      0x05,
      // source connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // original destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // retry token
      'H', 'e', 'l', 'l', 'o', ' ', 't', 'h', 'i', 's',
      ' ', 'i', 's', ' ', 'R', 'E', 'T', 'R', 'Y', '!',
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
  EXPECT_EQ("Client-initiated RETRY is invalid.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, BuildPaddingFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxOutgoingPacketSize] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet46[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet99[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER,
      VARIABLE_LENGTH_INTEGER_LENGTH_0, 0, VARIABLE_LENGTH_INTEGER_LENGTH_0);
  memset(p + header_size + 1, 0x00, kMaxOutgoingPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacketWithNewPaddingFrame) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               QuicStringPiece("hello world!"));
  QuicPaddingFrame padding_frame(2);
  QuicFrames frames = {QuicFrame(padding_frame), QuicFrame(stream_frame),
                       QuicFrame(padding_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (IETF_STREAM with FIN, LEN, and OFFSET bits set)
    0x08 | 0x01 | 0x02 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62OneByte + 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, Build4ByteSequenceNumberPaddingFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_4BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxOutgoingPacketSize] = {
    // public flags (8 byte connection_id and 4 byte packet number)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet46[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet99[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER,
      VARIABLE_LENGTH_INTEGER_LENGTH_0, 0, VARIABLE_LENGTH_INTEGER_LENGTH_0);
  memset(p + header_size + 1, 0x00, kMaxOutgoingPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, Build2ByteSequenceNumberPaddingFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_2BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxOutgoingPacketSize] = {
    // public flags (8 byte connection_id and 2 byte packet number)
    0x1C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[kMaxOutgoingPacketSize] = {
    // type (short header, 2 byte packet number)
    0x31,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet46[kMaxOutgoingPacketSize] = {
    // type (short header, 2 byte packet number)
    0x41,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet99[kMaxOutgoingPacketSize] = {
    // type (short header, 2 byte packet number)
    0x41,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_2BYTE_PACKET_NUMBER,
      VARIABLE_LENGTH_INTEGER_LENGTH_0, 0, VARIABLE_LENGTH_INTEGER_LENGTH_0);
  memset(p + header_size + 1, 0x00, kMaxOutgoingPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, Build1ByteSequenceNumberPaddingFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxOutgoingPacketSize] = {
    // public flags (8 byte connection_id and 1 byte packet number)
    0x0C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[kMaxOutgoingPacketSize] = {
    // type (short header, 1 byte packet number)
    0x30,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet46[kMaxOutgoingPacketSize] = {
    // type (short header, 1 byte packet number)
    0x40,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet99[kMaxOutgoingPacketSize] = {
    // type (short header, 1 byte packet number)
    0x40,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_1BYTE_PACKET_NUMBER,
      VARIABLE_LENGTH_INTEGER_LENGTH_0, 0, VARIABLE_LENGTH_INTEGER_LENGTH_0);
  memset(p + header_size + 1, 0x00, kMaxOutgoingPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  if (QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    header.length_length = VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }

  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               QuicStringPiece("hello world!"));

  QuicFrames frames = {QuicFrame(stream_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAM frame with FIN and OFFSET, no length)
    0x08 | 0x01 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildStreamFramePacketWithVersionFlag) {
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = true;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    header.long_packet_type = ZERO_RTT_PROTECTED;
  }
  header.packet_number = kPacketNumber;
  if (QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    header.length_length = VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }

  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               QuicStringPiece("hello world!"));
  QuicFrames frames = {QuicFrame(stream_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (version, 8 byte connection_id)
      0x2D,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // version tag
      QUIC_VERSION_BYTES,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  unsigned char packet44[] = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      0xFC,
      // version tag
      QUIC_VERSION_BYTES,
      // connection_id length
      0x50,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  unsigned char packet46[] = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      0xD3,
      // version tag
      QUIC_VERSION_BYTES,
      // connection_id length
      0x50,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  unsigned char packet99[] = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      0xD3,
      // version tag
      QUIC_VERSION_BYTES,
      // connection_id length
      0x50,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // length
      0x40, 0x1D,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (IETF_STREAM frame with fin and offset, no length)
      0x08 | 0x01 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };
  // clang-format on

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildCryptoFramePacket) {
  if (framer_.transport_version() < QUIC_VERSION_99) {
    // CRYPTO frames aren't supported prior to v46.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  SimpleDataProducer data_producer;
  framer_.set_data_producer(&data_producer);

  QuicStringPiece crypto_frame_contents("hello world!");
  QuicCryptoFrame crypto_frame(ENCRYPTION_INITIAL, kStreamOffset,
                               crypto_frame_contents.length());
  data_producer.SaveCryptoData(ENCRYPTION_INITIAL, kStreamOffset,
                               crypto_frame_contents);

  QuicFrames frames = {QuicFrame(&crypto_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_CRYPTO frame)
    0x06,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // length
    kVarInt62OneByte + 12,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  // clang-format on

  size_t packet_size = QUIC_ARRAYSIZE(packet);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      packet_size);
}

TEST_P(QuicFramerTest, CryptoFrame) {
  if (framer_.transport_version() < QUIC_VERSION_99) {
    // CRYPTO frames aren't supported prior to v46.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_CRYPTO frame)
      {"",
       {0x06}},
      // offset
      {"",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Invalid data length.",
       {kVarInt62OneByte + 12}},
      // data
      {"Unable to read frame data.",
       {'h',  'e',  'l',  'l',
        'o',  ' ',  'w',  'o',
        'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));
  ASSERT_EQ(1u, visitor_.crypto_frames_.size());
  QuicCryptoFrame* frame = visitor_.crypto_frames_[0].get();
  EXPECT_EQ(kStreamOffset, frame->offset);
  EXPECT_EQ("hello world!",
            std::string(frame->data_buffer, frame->data_length));

  CheckFramingBoundaries(packet, QUIC_INVALID_FRAME_DATA);
}

TEST_P(QuicFramerTest, BuildVersionNegotiationPacket) {
  // clang-format off
  unsigned char packet[] = {
      // public flags (version, 8 byte connection_id)
      0x0D,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // version tag
      QUIC_VERSION_BYTES,
  };
  unsigned char type44 = 0x80;
  if (GetQuicReloadableFlag(quic_send_version_negotiation_fixed_bit)) {
    type44 = 0xC0;
  }
  unsigned char packet44[] = {
      // type (long header)
      type44,
      // version tag
      0x00, 0x00, 0x00, 0x00,
      // connection_id length
      0x05,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // version tag
      QUIC_VERSION_BYTES,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  QuicConnectionId connection_id = FramerTestConnectionId();
  std::unique_ptr<QuicEncryptedPacket> data(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id, EmptyQuicConnectionId(),
          framer_.transport_version() > QUIC_VERSION_43,
          SupportedVersions(GetParam())));
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildVersionNegotiationPacketWithClientConnectionId) {
  if (framer_.transport_version() <= QUIC_VERSION_43) {
    // The GQUIC encoding does not support encoding client connection IDs.
    return;
  }

  // Client connection IDs cannot be used unless this flag is true.
  SetQuicRestartFlag(quic_do_not_override_connection_id, true);

  unsigned char type_byte = 0x80;
  if (GetQuicReloadableFlag(quic_send_version_negotiation_fixed_bit)) {
    type_byte = 0xC0;
  }
  // clang-format off
  unsigned char packet[] = {
      // type (long header)
      type_byte,
      // version tag
      0x00, 0x00, 0x00, 0x00,
      // connection ID lengths
      0x55,
      // client/destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // server/source connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // version tag
      QUIC_VERSION_BYTES,
  };
  // clang-format on

  QuicConnectionId server_connection_id = FramerTestConnectionId();
  QuicConnectionId client_connection_id = FramerTestConnectionIdPlusOne();
  std::unique_ptr<QuicEncryptedPacket> data(
      QuicFramer::BuildVersionNegotiationPacket(server_connection_id,
                                                client_connection_id, true,
                                                SupportedVersions(GetParam())));
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildAckFramePacketOneAckBlock) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x2C,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 2 byte largest observed, 2 byte block length)
      0x45,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34,
      // num timestamps.
      0x00,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 2 byte largest observed, 2 byte block length)
      0x45,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34,
      // num timestamps.
      0x00,
  };

  unsigned char packet46[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 2 byte largest observed, 2 byte block length)
      0x45,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34,
      // num timestamps.
      0x00,
  };

  unsigned char packet99[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (IETF_ACK frame)
      0x02,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // Number of additional ack blocks.
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62TwoBytes + 0x12, 0x33,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketOneAckBlockMaxLength) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kPacketNumber);
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x2C,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 4 byte largest observed, 4 byte block length)
      0x4A,
      // largest acked
      0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34, 0x56, 0x78,
      // num timestamps.
      0x00,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 4 byte largest observed, 4 byte block length)
      0x4A,
      // largest acked
      0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34, 0x56, 0x78,
      // num timestamps.
      0x00,
  };

  unsigned char packet46[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 4 byte largest observed, 4 byte block length)
      0x4A,
      // largest acked
      0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34, 0x56, 0x78,
      // num timestamps.
      0x00,
  };


  unsigned char packet99[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (IETF_ACK frame)
      0x02,
      // largest acked
      kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // Nr. of additional ack blocks
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x77,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketMultipleAckBlocks) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(5)},
                    {QuicPacketNumber(10), QuicPacketNumber(500)},
                    {QuicPacketNumber(900), kSmallMissingPacket},
                    {kSmallMissingPacket + 1, kSmallLargestObserved + 1}});
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x2C,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0x04,
      // first ack block length.
      0x00, 0x01,
      // gap to next block.
      0x01,
      // ack block length.
      0x0e, 0xaf,
      // gap to next block.
      0xff,
      // ack block length.
      0x00, 0x00,
      // gap to next block.
      0x91,
      // ack block length.
      0x01, 0xea,
      // gap to next block.
      0x05,
      // ack block length.
      0x00, 0x04,
      // num timestamps.
      0x00,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0x04,
      // first ack block length.
      0x00, 0x01,
      // gap to next block.
      0x01,
      // ack block length.
      0x0e, 0xaf,
      // gap to next block.
      0xff,
      // ack block length.
      0x00, 0x00,
      // gap to next block.
      0x91,
      // ack block length.
      0x01, 0xea,
      // gap to next block.
      0x05,
      // ack block length.
      0x00, 0x04,
      // num timestamps.
      0x00,
  };

  unsigned char packet46[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0x04,
      // first ack block length.
      0x00, 0x01,
      // gap to next block.
      0x01,
      // ack block length.
      0x0e, 0xaf,
      // gap to next block.
      0xff,
      // ack block length.
      0x00, 0x00,
      // gap to next block.
      0x91,
      // ack block length.
      0x01, 0xea,
      // gap to next block.
      0x05,
      // ack block length.
      0x00, 0x04,
      // num timestamps.
      0x00,
  };

  unsigned char packet99[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (IETF_ACK frame)
      0x02,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // num additional ack blocks.
      kVarInt62OneByte + 0x03,
      // first ack block length.
      kVarInt62OneByte + 0x00,

      // gap to next block.
      kVarInt62OneByte + 0x00,
      // ack block length.
      kVarInt62TwoBytes + 0x0e, 0xae,

      // gap to next block.
      kVarInt62TwoBytes + 0x01, 0x8f,
      // ack block length.
      kVarInt62TwoBytes + 0x01, 0xe9,

      // gap to next block.
      kVarInt62OneByte + 0x04,
      // ack block length.
      kVarInt62OneByte + 0x03,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketMaxAckBlocks) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObservedto make this test finished in a short time.
  QuicAckFrame ack_frame;
  ack_frame.largest_acked = kSmallLargestObserved;
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  // 300 ack blocks.
  for (size_t i = 2; i < 2 * 300; i += 2) {
    ack_frame.packets.Add(QuicPacketNumber(i));
  }
  ack_frame.packets.AddRange(QuicPacketNumber(600), kSmallLargestObserved + 1);

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x2C,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0xff,
      // first ack block length.
      0x0f, 0xdd,
      // 255 = 4 * 63 + 3
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      // num timestamps.
      0x00,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0xff,
      // first ack block length.
      0x0f, 0xdd,
      // 255 = 4 * 63 + 3
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      // num timestamps.
      0x00,
  };

  unsigned char packet46[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0xff,
      // first ack block length.
      0x0f, 0xdd,
      // 255 = 4 * 63 + 3
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      // num timestamps.
      0x00,
  };

  unsigned char packet99[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_ACK frame)
      0x02,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // num ack blocks ranges.
      kVarInt62TwoBytes + 0x01, 0x2b,
      // first ack block length.
      kVarInt62TwoBytes + 0x0f, 0xdc,
      // 255 added blocks of gap_size == 1, ack_size == 1
#define V99AddedBLOCK kVarInt62OneByte + 0x00, kVarInt62OneByte + 0x00
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

#undef V99AddedBLOCK
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildNewStopWaitingPacket) {
  if (version_.transport_version > QUIC_VERSION_43) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStopWaitingFrame stop_waiting_frame;
  stop_waiting_frame.least_unacked = kLeastUnacked;

  QuicFrames frames = {QuicFrame(stop_waiting_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x00, 0x00, 0x00, 0x08,
  };

  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildRstFramePacketQuic) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicRstStreamFrame rst_frame;
  rst_frame.stream_id = kStreamId;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    rst_frame.ietf_error_code = 0x01;
  } else {
    rst_frame.error_code = static_cast<QuicRstStreamErrorCode>(0x05060708);
  }
  rst_frame.byte_offset = 0x0807060504030201;

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // sent byte offset
    0x08, 0x07, 0x06, 0x05,
    0x04, 0x03, 0x02, 0x01,
    // error code
    0x05, 0x06, 0x07, 0x08,
  };

  unsigned char packet44[] = {
    // type (short packet, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // sent byte offset
    0x08, 0x07, 0x06, 0x05,
    0x04, 0x03, 0x02, 0x01,
    // error code
    0x05, 0x06, 0x07, 0x08,
  };

  unsigned char packet46[] = {
    // type (short packet, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // sent byte offset
    0x08, 0x07, 0x06, 0x05,
    0x04, 0x03, 0x02, 0x01,
    // error code
    0x05, 0x06, 0x07, 0x08,
  };

  unsigned char packet99[] = {
    // type (short packet, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_RST_STREAM frame)
    0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // error code (not VarInt32 encoded)
    0x00, 0x01,
    // sent byte offset
    kVarInt62EightBytes + 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
  };
  // clang-format on

  QuicFrames frames = {QuicFrame(&rst_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildCloseFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame close_frame;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    close_frame.transport_error_code =
        static_cast<QuicIetfTransportErrorCodes>(0x11);
    close_frame.transport_close_frame_type = 0x05;
    close_frame.close_type = IETF_QUIC_TRANSPORT_CONNECTION_CLOSE;
  } else {
    close_frame.quic_error_code = static_cast<QuicErrorCode>(0x05060708);
  }
  close_frame.error_details = "because I can";

  QuicFrames frames = {QuicFrame(&close_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_CONNECTION_CLOSE frame)
    0x1c,
    // error code
    0x00, 0x11,
    // Frame type within the CONNECTION_CLOSE frame
    kVarInt62OneByte + 0x05,
    // error details length
    kVarInt62OneByte + 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildTruncatedCloseFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame close_frame;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    close_frame.transport_error_code = PROTOCOL_VIOLATION;  // value is 0x0a
    EXPECT_EQ(0u, close_frame.transport_close_frame_type);
    close_frame.close_type = IETF_QUIC_TRANSPORT_CONNECTION_CLOSE;
  } else {
    close_frame.quic_error_code = static_cast<QuicErrorCode>(0x05060708);
  }
  close_frame.error_details = std::string(2048, 'A');
  QuicFrames frames = {QuicFrame(&close_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_CONNECTION_CLOSE frame)
    0x1c,
    // error code
    0x00, 0x0a,
    // Frame type within the CONNECTION_CLOSE frame
    kVarInt62OneByte + 0x00,
    // error details length
    kVarInt62TwoBytes + 0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildApplicationCloseFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // Versions other than 99 do not have ApplicationClose
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame app_close_frame;
  app_close_frame.quic_error_code = static_cast<QuicErrorCode>(0x11);
  app_close_frame.error_details = "because I can";
  app_close_frame.close_type = IETF_QUIC_APPLICATION_CONNECTION_CLOSE;

  QuicFrames frames = {QuicFrame(&app_close_frame)};

  // clang-format off

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_APPLICATION_CLOSE frame)
    0x1d,
    // error code
    0x00, 0x11,
    // error details length
    kVarInt62OneByte + 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildTruncatedApplicationCloseFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // Versions other than 99 do not have this frame.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame app_close_frame;
  app_close_frame.quic_error_code = static_cast<QuicErrorCode>(0x11);
  app_close_frame.error_details = std::string(2048, 'A');
  app_close_frame.close_type = IETF_QUIC_APPLICATION_CONNECTION_CLOSE;

  QuicFrames frames = {QuicFrame(&app_close_frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_APPLICATION_CLOSE frame)
    0x1d,
    // error code
    0x00, 0x11,
    // error details length
    kVarInt62TwoBytes + 0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildGoAwayPacket) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This frame type is not supported in version 99.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicGoAwayFrame goaway_frame;
  goaway_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  goaway_frame.last_good_stream_id = kStreamId;
  goaway_frame.reason_phrase = "because I can";

  QuicFrames frames = {QuicFrame(&goaway_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildTruncatedGoAwayPacket) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This frame type is not supported in version 99.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicGoAwayFrame goaway_frame;
  goaway_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  goaway_frame.last_good_stream_id = kStreamId;
  goaway_frame.reason_phrase = std::string(2048, 'A');

  QuicFrames frames = {QuicFrame(&goaway_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildWindowUpdatePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id = kStreamId;
  window_update_frame.byte_offset = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(&window_update_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (window update frame)
    0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // byte offset
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (window update frame)
    0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // byte offset
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (window update frame)
    0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // byte offset
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_STREAM_DATA frame)
    0x11,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildMaxStreamDataPacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is available only in this version.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id = kStreamId;
  window_update_frame.byte_offset = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(&window_update_frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_STREAM_DATA frame)
    0x11,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildMaxDataPacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is available only in this version.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id =
      QuicUtils::GetInvalidStreamId(framer_.transport_version());
  window_update_frame.byte_offset = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(&window_update_frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_DATA frame)
    0x10,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildBlockedPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame blocked_frame;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // For V99, the stream ID must be <invalid> for the frame
    // to be a BLOCKED frame. if it's valid, it will be a
    // STREAM_BLOCKED frame.
    blocked_frame.stream_id =
        QuicUtils::GetInvalidStreamId(framer_.transport_version());
  } else {
    blocked_frame.stream_id = kStreamId;
  }
  blocked_frame.offset = kStreamOffset;

  QuicFrames frames = {QuicFrame(&blocked_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x01, 0x02, 0x03, 0x04,
  };

  unsigned char packet44[] = {
    // type (short packet, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x01, 0x02, 0x03, 0x04,
  };

  unsigned char packet46[] = {
    // type (short packet, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x01, 0x02, 0x03, 0x04,
  };

  unsigned char packet99[] = {
    // type (short packet, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_BLOCKED frame)
    0x14,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildPingPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPingFrame())};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ping frame)
    0x07,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PING frame)
    0x01,
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildMessagePacket) {
  if (framer_.transport_version() <= QUIC_VERSION_44) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicMemSliceStorage storage(nullptr, 0, nullptr, 0);

  QuicMessageFrame frame(1, MakeSpan(&allocator_, "message", &storage));
  QuicMessageFrame frame2(2, MakeSpan(&allocator_, "message2", &storage));
  QuicFrames frames = {QuicFrame(&frame), QuicFrame(&frame2)};

  // clang-format off
  unsigned char packet45[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (message frame)
    0x21,
    // Length
    0x07,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e',
    // frame type (message frame no length)
    0x20,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e', '2'
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (message frame)
    0x21,
    // Length
    0x07,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e',
    // frame type (message frame no length)
    0x20,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e', '2'
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MESSAGE frame)
    0x21,
    // Length
    0x07,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e',
    // frame type (message frame no length)
    0x20,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e', '2'
  };
  // clang-format on

  unsigned char* p = packet45;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p),
                                      QUIC_ARRAYSIZE(packet45));
}

// Test that the connectivity probing packet is serialized correctly as a
// padded PING packet.
TEST_P(QuicFramerTest, BuildConnectivityProbingPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ping frame)
    0x07,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PING frame)
    0x01,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  size_t packet_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    packet_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    packet_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    packet_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);

  size_t length = framer_.BuildConnectivityProbingPacket(
      header, buffer.get(), packet_size, ENCRYPTION_INITIAL);

  EXPECT_NE(0u, length);
  QuicPacket data(framer_.transport_version(), buffer.release(), length, true,
                  header);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(p), packet_size);
}

// Test that the path challenge connectivity probing packet is serialized
// correctly as a padded PATH CHALLENGE packet.
TEST_P(QuicFramerTest, BuildPaddedPathChallengePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload;

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Challenge Frame type (IETF_PATH_CHALLENGE)
    0x1a,
    // 8 "random" bytes, MockRandom makes lots of r's
    'r', 'r', 'r', 'r', 'r', 'r', 'r', 'r',
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  MockRandom randomizer;

  size_t length = framer_.BuildPaddedPathChallengePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), &payload, &randomizer,
      ENCRYPTION_INITIAL);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));

  // Payload has the random bytes that were generated. Copy them into packet,
  // above, before checking that the generated packet is correct.
  EXPECT_EQ(kQuicPathFrameBufferSize, payload.size());

  QuicPacket data(framer_.transport_version(), buffer.release(), length, true,
                  header);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

// Several tests that the path response connectivity probing packet is
// serialized correctly as either a padded and unpadded PATH RESPONSE
// packet. Also generates packets with 1 and 3 PATH_RESPONSES in them to
// exercised the single- and multiple- payload cases.
TEST_P(QuicFramerTest, BuildPathResponsePacket1ResponseUnpadded) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};

  // Build 1 PATH RESPONSE, not padded
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Response Frame type (IETF_PATH_RESPONSE)
    0x1b,
    // 8 "random" bytes
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  };
  // clang-format on
  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  QuicDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  size_t length = framer_.BuildPathResponsePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), payloads,
      /*is_padded=*/false, ENCRYPTION_INITIAL);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));
  QuicPacket data(framer_.transport_version(), buffer.release(), length, true,
                  header);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPathResponsePacket1ResponsePadded) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};

  // Build 1 PATH RESPONSE, padded
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Response Frame type (IETF_PATH_RESPONSE)
    0x1b,
    // 8 "random" bytes
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    // Padding type and pad
    0x00, 0x00, 0x00, 0x00, 0x00
  };
  // clang-format on
  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  QuicDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  size_t length = framer_.BuildPathResponsePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), payloads,
      /*is_padded=*/true, ENCRYPTION_INITIAL);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));
  QuicPacket data(framer_.transport_version(), buffer.release(), length, true,
                  header);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPathResponsePacket3ResponsesUnpadded) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};
  QuicPathFrameBuffer payload1 = {
      {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18}};
  QuicPathFrameBuffer payload2 = {
      {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}};

  // Build one packet with 3 PATH RESPONSES, no padding
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // 3 path response frames (IETF_PATH_RESPONSE type byte and payload)
    0x1b, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x1b, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x1b, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  QuicDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);
  payloads.push_back(payload2);
  size_t length = framer_.BuildPathResponsePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), payloads,
      /*is_padded=*/false, ENCRYPTION_INITIAL);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));
  QuicPacket data(framer_.transport_version(), buffer.release(), length, true,
                  header);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPathResponsePacket3ResponsesPadded) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};
  QuicPathFrameBuffer payload1 = {
      {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18}};
  QuicPathFrameBuffer payload2 = {
      {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}};

  // Build one packet with 3 PATH RESPONSES, with padding
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // 3 path response frames (IETF_PATH_RESPONSE byte and payload)
    0x1b, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x1b, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x1b, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    // Padding
    0x00, 0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  QuicDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);
  payloads.push_back(payload2);
  size_t length = framer_.BuildPathResponsePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), payloads,
      /*is_padded=*/true, ENCRYPTION_INITIAL);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));
  QuicPacket data(framer_.transport_version(), buffer.release(), length, true,
                  header);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

// Test that the MTU discovery packet is serialized correctly as a PING packet.
TEST_P(QuicFramerTest, BuildMtuDiscoveryPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicMtuDiscoveryFrame())};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ping frame)
    0x07,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PING frame)
    0x01,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacket) {
  QuicPublicResetPacket reset_packet;
  reset_packet.connection_id = FramerTestConnectionId();
  reset_packet.nonce_proof = kNonceProof;

  // clang-format off
  unsigned char packet[] = {
    // public flags (public reset, 8 byte ConnectionId)
    0x0E,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (1) + padding
    0x01, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacketWithClientAddress) {
  QuicPublicResetPacket reset_packet;
  reset_packet.connection_id = FramerTestConnectionId();
  reset_packet.nonce_proof = kNonceProof;
  reset_packet.client_address =
      QuicSocketAddress(QuicIpAddress::Loopback4(), 0x1234);

  // clang-format off
  unsigned char packet[] = {
      // public flags (public reset, 8 byte ConnectionId)
      0x0E,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98,
      0x76, 0x54, 0x32, 0x10,
      // message tag (kPRST)
      'P', 'R', 'S', 'T',
      // num_entries (2) + padding
      0x02, 0x00, 0x00, 0x00,
      // tag kRNON
      'R', 'N', 'O', 'N',
      // end offset 8
      0x08, 0x00, 0x00, 0x00,
      // tag kCADR
      'C', 'A', 'D', 'R',
      // end offset 16
      0x10, 0x00, 0x00, 0x00,
      // nonce proof
      0x89, 0x67, 0x45, 0x23,
      0x01, 0xEF, 0xCD, 0xAB,
      // client address
      0x02, 0x00,
      0x7F, 0x00, 0x00, 0x01,
      0x34, 0x12,
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacketWithEndpointId) {
  QuicPublicResetPacket reset_packet;
  reset_packet.connection_id = FramerTestConnectionId();
  reset_packet.nonce_proof = kNonceProof;
  reset_packet.endpoint_id = "FakeServerId";

  // The tag value map in CryptoHandshakeMessage is a std::map, so the two tags
  // in the packet, kRNON and kEPID, have unspecified ordering w.r.t each other.
  // clang-format off
  unsigned char packet_variant1[] = {
      // public flags (public reset, 8 byte ConnectionId)
      0x0E,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98,
      0x76, 0x54, 0x32, 0x10,
      // message tag (kPRST)
      'P', 'R', 'S', 'T',
      // num_entries (2) + padding
      0x02, 0x00, 0x00, 0x00,
      // tag kRNON
      'R', 'N', 'O', 'N',
      // end offset 8
      0x08, 0x00, 0x00, 0x00,
      // tag kEPID
      'E', 'P', 'I', 'D',
      // end offset 20
      0x14, 0x00, 0x00, 0x00,
      // nonce proof
      0x89, 0x67, 0x45, 0x23,
      0x01, 0xEF, 0xCD, 0xAB,
      // Endpoint ID
      'F', 'a', 'k', 'e', 'S', 'e', 'r', 'v', 'e', 'r', 'I', 'd',
  };
  unsigned char packet_variant2[] = {
      // public flags (public reset, 8 byte ConnectionId)
      0x0E,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98,
      0x76, 0x54, 0x32, 0x10,
      // message tag (kPRST)
      'P', 'R', 'S', 'T',
      // num_entries (2) + padding
      0x02, 0x00, 0x00, 0x00,
      // tag kEPID
      'E', 'P', 'I', 'D',
      // end offset 12
      0x0C, 0x00, 0x00, 0x00,
      // tag kRNON
      'R', 'N', 'O', 'N',
      // end offset 20
      0x14, 0x00, 0x00, 0x00,
      // Endpoint ID
      'F', 'a', 'k', 'e', 'S', 'e', 'r', 'v', 'e', 'r', 'I', 'd',
      // nonce proof
      0x89, 0x67, 0x45, 0x23,
      0x01, 0xEF, 0xCD, 0xAB,
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);

  // Variant 1 ends with char 'd'. Variant 1 ends with char 0xAB.
  if ('d' == data->data()[data->length() - 1]) {
    test::CompareCharArraysWithHexError(
        "constructed packet", data->data(), data->length(),
        AsChars(packet_variant1), QUIC_ARRAYSIZE(packet_variant1));
  } else {
    test::CompareCharArraysWithHexError(
        "constructed packet", data->data(), data->length(),
        AsChars(packet_variant2), QUIC_ARRAYSIZE(packet_variant2));
  }
}

TEST_P(QuicFramerTest, BuildIetfStatelessResetPacket) {
  // clang-format off
  unsigned char packet44[] = {
    // type (short header, 1 byte packet number)
    0x70,
    // random packet number
    0xFE,
    // stateless reset token
    0xB5, 0x69, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildIetfStatelessResetPacket(FramerTestConnectionId(),
                                            kTestStatelessResetToken));
  ASSERT_TRUE(data != nullptr);
  // Skip packet number byte which is random in stateless reset packet.
  test::CompareCharArraysWithHexError("constructed packet", data->data(), 1,
                                      AsChars(packet44), 1);
  const size_t random_bytes_length =
      data->length() - kPacketHeaderTypeSize - sizeof(kTestStatelessResetToken);
  EXPECT_EQ(kMinRandomBytesLengthInStatelessReset, random_bytes_length);
  // Verify stateless reset token is correct.
  test::CompareCharArraysWithHexError(
      "constructed packet",
      data->data() + data->length() - sizeof(kTestStatelessResetToken),
      sizeof(kTestStatelessResetToken),
      AsChars(packet44) + QUIC_ARRAYSIZE(packet44) -
          sizeof(kTestStatelessResetToken),
      sizeof(kTestStatelessResetToken));
}

TEST_P(QuicFramerTest, EncryptPacket) {
  QuicPacketNumber packet_number = kPacketNumber;
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
    'q',  'r',  's',  't',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }

  std::unique_ptr<QuicPacket> raw(new QuicPacket(
      AsChars(p), p_size, false, PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER,
      VARIABLE_LENGTH_INTEGER_LENGTH_0, 0, VARIABLE_LENGTH_INTEGER_LENGTH_0));
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = framer_.EncryptPayload(
      ENCRYPTION_INITIAL, packet_number, *raw, buffer, kMaxOutgoingPacketSize);

  ASSERT_NE(0u, encrypted_length);
  EXPECT_TRUE(CheckEncryption(packet_number, raw.get()));
}

TEST_P(QuicFramerTest, EncryptPacketWithVersionFlag) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketNumber packet_number = kPacketNumber;
  // clang-format off
  unsigned char packet[] = {
    // public flags (version, 8 byte connection_id)
    0x29,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // version tag
    'Q', '.', '1', '0',
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet44[] = {
    // type (long header with packet type ZERO_RTT_PROTECTED)
    0xFC,
    // version tag
    'Q', '.', '1', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet46[] = {
    // type (long header with packet type ZERO_RTT_PROTECTED)
    0xD3,
    // version tag
    'Q', '.', '1', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet99[] = {
    // type (long header with packet type ZERO_RTT_PROTECTED)
    0xD3,
    // version tag
    'Q', '.', '1', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
    'q',  'r',  's',  't',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  std::unique_ptr<QuicPacket> raw(new QuicPacket(
      AsChars(p), p_size, false, PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER,
      VARIABLE_LENGTH_INTEGER_LENGTH_0, 0, VARIABLE_LENGTH_INTEGER_LENGTH_0));
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = framer_.EncryptPayload(
      ENCRYPTION_INITIAL, packet_number, *raw, buffer, kMaxOutgoingPacketSize);

  ASSERT_NE(0u, encrypted_length);
  EXPECT_TRUE(CheckEncryption(packet_number, raw.get()));
}

TEST_P(QuicFramerTest, AckTruncationLargePacket) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame;
  // Create a packet with just the ack.
  ack_frame = MakeAckFrameWithAckBlocks(300, 0u);
  QuicFrames frames = {QuicFrame(&ack_frame)};

  // Build an ack packet with truncation due to limit in number of nack ranges.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                             *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
  ASSERT_NE(0u, encrypted_length);
  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(QuicPacketNumber(600u), LargestAcked(processed_ack_frame));
  ASSERT_EQ(256u, processed_ack_frame.packets.NumPacketsSlow());
  EXPECT_EQ(QuicPacketNumber(90u), processed_ack_frame.packets.Min());
  EXPECT_EQ(QuicPacketNumber(600u), processed_ack_frame.packets.Max());
}

TEST_P(QuicFramerTest, AckTruncationSmallPacket) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Create a packet with just the ack.
  QuicAckFrame ack_frame;
  ack_frame = MakeAckFrameWithAckBlocks(300, 0u);
  QuicFrames frames = {QuicFrame(&ack_frame)};

  // Build an ack packet with truncation due to limit in number of nack ranges.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(
      BuildDataPacket(header, frames, 500));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                             *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
  ASSERT_NE(0u, encrypted_length);
  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(QuicPacketNumber(600u), LargestAcked(processed_ack_frame));
  ASSERT_EQ(240u, processed_ack_frame.packets.NumPacketsSlow());
  EXPECT_EQ(QuicPacketNumber(122u), processed_ack_frame.packets.Min());
  EXPECT_EQ(QuicPacketNumber(600u), processed_ack_frame.packets.Max());
}

TEST_P(QuicFramerTest, CleanTruncation) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(201);

  // Create a packet with just the ack.
  QuicFrames frames = {QuicFrame(&ack_frame)};
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);

  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                             *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
  ASSERT_NE(0u, encrypted_length);

  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));

  // Test for clean truncation of the ack by comparing the length of the
  // original packets to the re-serialized packets.
  frames.clear();
  frames.push_back(QuicFrame(visitor_.ack_frames_[0].get()));

  size_t original_raw_length = raw_ack_packet->length();
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  raw_ack_packet = BuildDataPacket(header, frames);
  ASSERT_TRUE(raw_ack_packet != nullptr);
  EXPECT_EQ(original_raw_length, raw_ack_packet->length());
  ASSERT_TRUE(raw_ack_packet != nullptr);
}

TEST_P(QuicFramerTest, StopPacketProcessing) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x40,
    // least packet number awaiting an ack
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xA0,
    // largest observed packet number
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBF,
    // num missing packets
    0x01,
    // missing packet
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBE,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x40,
    // least packet number awaiting an ack
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xA0,
    // largest observed packet number
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBF,
    // num missing packets
    0x01,
    // missing packet
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBE,
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x40,
    // least packet number awaiting an ack
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xA0,
    // largest observed packet number
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBF,
    // num missing packets
    0x01,
    // missing packet
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBE,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAM frame with fin, length, and offset bits set)
    0x08 | 0x01 | 0x02 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62TwoBytes + 0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x0d,
    // largest observed packet number
    kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x78,
    // Delta time
    kVarInt62OneByte + 0x00,
    // Ack Block count
    kVarInt62OneByte + 0x01,
    // First block size (one packet)
    kVarInt62OneByte + 0x00,

    // Next gap size & ack. Missing all preceding packets
    kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x77,
    kVarInt62OneByte + 0x00,
  };
  // clang-format on

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket());
  EXPECT_CALL(visitor, OnPacketHeader(_));
  EXPECT_CALL(visitor, OnStreamFrame(_)).WillOnce(Return(false));
  EXPECT_CALL(visitor, OnPacketComplete());
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnDecryptedPacket(_));

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
}

static char kTestString[] = "At least 20 characters.";
static QuicStreamId kTestQuicStreamId = 1;
static bool ExpectedStreamFrame(const QuicStreamFrame& frame) {
  return (frame.stream_id == kTestQuicStreamId ||
          QuicUtils::IsCryptoStreamId(QUIC_VERSION_99, frame.stream_id)) &&
         !frame.fin && frame.offset == 0 &&
         std::string(frame.data_buffer, frame.data_length) == kTestString;
  // FIN is hard-coded false in ConstructEncryptedPacket.
  // Offset 0 is hard-coded in ConstructEncryptedPacket.
}

// Verify that the packet returned by ConstructEncryptedPacket() can be properly
// parsed by the framer.
TEST_P(QuicFramerTest, ConstructEncryptedPacket) {
  // Since we are using ConstructEncryptedPacket, we have to set the framer's
  // crypto to be Null.
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(
        ENCRYPTION_FORWARD_SECURE,
        QuicMakeUnique<NullDecrypter>(framer_.perspective()));
  } else {
    framer_.SetDecrypter(ENCRYPTION_INITIAL,
                         QuicMakeUnique<NullDecrypter>(framer_.perspective()));
  }
  ParsedQuicVersionVector versions;
  versions.push_back(framer_.version());
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      TestConnectionId(), EmptyQuicConnectionId(), false, false,
      kTestQuicStreamId, kTestString, CONNECTION_ID_PRESENT,
      CONNECTION_ID_ABSENT, PACKET_4BYTE_PACKET_NUMBER, &versions));

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket()).Times(1);
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnPacketHeader(_)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnDecryptedPacket(_)).Times(1);
  EXPECT_CALL(visitor, OnError(_)).Times(0);
  EXPECT_CALL(visitor, OnStreamFrame(_)).Times(0);
  if (!QuicVersionUsesCryptoFrames(framer_.version().transport_version)) {
    EXPECT_CALL(visitor, OnStreamFrame(Truly(ExpectedStreamFrame))).Times(1);
  } else {
    EXPECT_CALL(visitor, OnCryptoFrame(_)).Times(1);
  }
  EXPECT_CALL(visitor, OnPacketComplete()).Times(1);

  EXPECT_TRUE(framer_.ProcessPacket(*packet));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
}

// Verify that the packet returned by ConstructMisFramedEncryptedPacket()
// does cause the framer to return an error.
TEST_P(QuicFramerTest, ConstructMisFramedEncryptedPacket) {
  // Since we are using ConstructEncryptedPacket, we have to set the framer's
  // crypto to be Null.
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(
        ENCRYPTION_FORWARD_SECURE,
        QuicMakeUnique<NullDecrypter>(framer_.perspective()));
  } else {
    framer_.SetDecrypter(ENCRYPTION_INITIAL,
                         QuicMakeUnique<NullDecrypter>(framer_.perspective()));
  }
  framer_.SetEncrypter(ENCRYPTION_INITIAL,
                       QuicMakeUnique<NullEncrypter>(framer_.perspective()));
  ParsedQuicVersionVector versions;
  versions.push_back(framer_.version());
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructMisFramedEncryptedPacket(
      TestConnectionId(), EmptyQuicConnectionId(), false, false,
      kTestQuicStreamId, kTestString, CONNECTION_ID_PRESENT,
      CONNECTION_ID_ABSENT, PACKET_4BYTE_PACKET_NUMBER, &versions,
      Perspective::IS_CLIENT));

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket()).Times(1);
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnPacketHeader(_)).Times(1);
  EXPECT_CALL(visitor, OnDecryptedPacket(_)).Times(1);
  EXPECT_CALL(visitor, OnError(_)).Times(1);
  EXPECT_CALL(visitor, OnStreamFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnPacketComplete()).Times(0);

  EXPECT_FALSE(framer_.ProcessPacket(*packet));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
}

// Tests for fuzzing with Dr. Fuzz
// Xref http://www.chromium.org/developers/testing/dr-fuzz for more details.
#ifdef __cplusplus
extern "C" {
#endif

// target function to be fuzzed by Dr. Fuzz
void QuicFramerFuzzFunc(unsigned char* data,
                        size_t size,
                        const ParsedQuicVersion& version) {
  QuicFramer framer(AllSupportedVersions(), QuicTime::Zero(),
                    Perspective::IS_SERVER, kQuicDefaultConnectionIdLength);
  ASSERT_EQ(GetQuicFlag(FLAGS_quic_supports_tls_handshake), true);
  const char* const packet_bytes = reinterpret_cast<const char*>(data);

  // Test the CryptoFramer.
  QuicStringPiece crypto_input(packet_bytes, size);
  std::unique_ptr<CryptoHandshakeMessage> handshake_message(
      CryptoFramer::ParseMessage(crypto_input));

  // Test the regular QuicFramer with the same input.
  NoOpFramerVisitor visitor;
  framer.set_visitor(&visitor);
  framer.set_version(version);
  QuicEncryptedPacket packet(packet_bytes, size);
  framer.ProcessPacket(packet);
}

#ifdef __cplusplus
}
#endif

TEST_P(QuicFramerTest, FramerFuzzTest) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin, length, and offset bits set)
    0x10 | 0x01 | 0x02 | 0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin, length, and offset bits set)
    0x10 | 0x01 | 0x02 | 0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAM frame with fin, length, and offset bits set)
    0x08 | 0x01 | 0x02 | 0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }
  QuicFramerFuzzFunc(p,
                     framer_.transport_version() > QUIC_VERSION_43
                         ? QUIC_ARRAYSIZE(packet44)
                         : QUIC_ARRAYSIZE(packet),
                     framer_.version());
}

TEST_P(QuicFramerTest, IetfBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_BLOCKED)
      {"",
       {0x14}},
      // blocked offset
      {"Can not read blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);

  CheckFramingBoundaries(packet99, QUIC_INVALID_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildIetfBlockedPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame frame;
  frame.stream_id = QuicUtils::GetInvalidStreamId(framer_.transport_version());
  frame.offset = kStreamOffset;
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_BLOCKED)
    0x14,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, IetfStreamBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAM_BLOCKED)
      {"",
       {0x15}},
      // blocked offset
      {"Can not read stream blocked stream id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      {"Can not read stream blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.blocked_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);

  CheckFramingBoundaries(packet99, QUIC_INVALID_STREAM_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildIetfStreamBlockedPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame frame;
  frame.stream_id = kStreamId;
  frame.offset = kStreamOffset;
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAM_BLOCKED)
    0x15,
    // Stream ID
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BiDiMaxStreamsFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL)
      {"",
       {0x12}},
      // max. streams
      {"Can not read MAX_STREAMS stream count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(3u, visitor_.max_streams_frame_.stream_count);
  EXPECT_FALSE(visitor_.max_streams_frame_.unidirectional);
  CheckFramingBoundaries(packet99, QUIC_MAX_STREAMS_DATA);
}

TEST_P(QuicFramerTest, UniDiMaxStreamsFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // Test runs in client mode, no connection id
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL)
      {"",
       {0x13}},
      // max. streams
      {"Can not read MAX_STREAMS stream count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_0BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(3u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
  CheckFramingBoundaries(packet99, QUIC_MAX_STREAMS_DATA);
}

TEST_P(QuicFramerTest, ServerUniDiMaxStreamsFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL)
      {"",
       {0x13}},
      // max. streams
      {"Can not read MAX_STREAMS stream count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(3u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
  CheckFramingBoundaries(packet99, QUIC_MAX_STREAMS_DATA);
}

TEST_P(QuicFramerTest, ClientUniDiMaxStreamsFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // Test runs in client mode, no connection id
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL)
      {"",
       {0x13}},
      // max. streams
      {"Can not read MAX_STREAMS stream count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_0BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(3u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
  CheckFramingBoundaries(packet99, QUIC_MAX_STREAMS_DATA);
}

// The following four tests ensure that the framer can deserialize a stream
// count that is large enough to cause the resulting stream ID to exceed the
// current implementation limit(32 bits). The intent is that when this happens,
// the stream limit is pegged to the maximum supported value. There are four
// tests, for the four combinations of uni- and bi-directional, server- and
// client- initiated.
TEST_P(QuicFramerTest, BiDiMaxStreamsFrameTooBig) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL)
    0x12,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet99), QUIC_ARRAYSIZE(packet99),
                                false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0x40000000u, visitor_.max_streams_frame_.stream_count);
  EXPECT_FALSE(visitor_.max_streams_frame_.unidirectional);
}

TEST_P(QuicFramerTest, ClientBiDiMaxStreamsFrameTooBig) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // Test runs in client mode, no connection id
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL)
    0x12,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet99), QUIC_ARRAYSIZE(packet99),
                                false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_0BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0x40000000u, visitor_.max_streams_frame_.stream_count);
  EXPECT_FALSE(visitor_.max_streams_frame_.unidirectional);
}

TEST_P(QuicFramerTest, ServerUniDiMaxStreamsFrameTooBig) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL)
    0x13,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet99), QUIC_ARRAYSIZE(packet99),
                                false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0x40000000u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
}

TEST_P(QuicFramerTest, ClientUniDiMaxStreamsFrameTooBig) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // Test runs in client mode, no connection id
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_UNDIRECTIONAL)
    0x13,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet99), QUIC_ARRAYSIZE(packet99),
                                false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_0BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0x40000000u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
}

// Specifically test that count==0 is accepted.
TEST_P(QuicFramerTest, MaxStreamsFrameZeroCount) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL)
    0x12,
    // max. streams == 0.
    kVarInt62OneByte + 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet99), QUIC_ARRAYSIZE(packet99),
                                false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
}

TEST_P(QuicFramerTest, ServerBiDiStreamsBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL frame)
      {"",
       {0x13}},
      // stream count
      {"Can not read MAX_STREAMS stream count.",
       {kVarInt62OneByte + 0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);

  CheckFramingBoundaries(packet99, QUIC_MAX_STREAMS_DATA);
}

TEST_P(QuicFramerTest, BiDiStreamsBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAMS_BLOCKED_BIDIRECTIONAL frame)
      {"",
       {0x16}},
      // stream id
      {"Can not read STREAMS_BLOCKED stream count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(3u, visitor_.streams_blocked_frame_.stream_count);
  EXPECT_FALSE(visitor_.streams_blocked_frame_.unidirectional);

  CheckFramingBoundaries(packet99, QUIC_STREAMS_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, UniDiStreamsBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAMS_BLOCKED_UNIDIRECTIONAL frame)
      {"",
       {0x17}},
      // stream id
      {"Can not read STREAMS_BLOCKED stream count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(3u, visitor_.streams_blocked_frame_.stream_count);
  EXPECT_TRUE(visitor_.streams_blocked_frame_.unidirectional);
  CheckFramingBoundaries(packet99, QUIC_STREAMS_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, ClientUniDiStreamsBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // Test runs in client mode, no connection id
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAMS_BLOCKED_UNIDIRECTIONAL frame)
      {"",
       {0x17}},
      // stream id
      {"Can not read STREAMS_BLOCKED stream count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_0BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(3u, visitor_.streams_blocked_frame_.stream_count);
  EXPECT_TRUE(visitor_.streams_blocked_frame_.unidirectional);
  CheckFramingBoundaries(packet99, QUIC_STREAMS_BLOCKED_DATA);
}

// Check that when we get a STREAMS_BLOCKED frame that specifies too large
// a stream count, we reject with an appropriate error. There is no need to
// check for different combinations of Uni/Bi directional and client/server
// initiated; the logic does not take these into account.
TEST_P(QuicFramerTest, StreamsBlockedFrameTooBig) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // Test runs in client mode, no connection id
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_STREAMS_BLOCKED_BIDIRECTIONAL)
    0x16,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x01
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet99), QUIC_ARRAYSIZE(packet99),
                                false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_STREAMS_BLOCKED_DATA, framer_.error());
  EXPECT_EQ(framer_.detailed_error(),
            "STREAMS_BLOCKED stream count exceeds implementation limit.");
}

// Specifically test that count==0 is accepted.
TEST_P(QuicFramerTest, StreamsBlockedFrameZeroCount) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAMS_BLOCKED_UNIDIRECTIONAL frame)
      {"",
       {0x17}},
      // stream id
      {"Can not read STREAMS_BLOCKED stream count.",
       {kVarInt62OneByte + 0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.streams_blocked_frame_.stream_count);
  EXPECT_TRUE(visitor_.streams_blocked_frame_.unidirectional);

  CheckFramingBoundaries(packet99, QUIC_STREAMS_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildBiDiStreamsBlockedPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStreamsBlockedFrame frame;
  frame.stream_count = 3;
  frame.unidirectional = false;

  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAMS_BLOCKED_BIDIRECTIONAL frame)
    0x16,
    // Stream count
    kVarInt62OneByte + 0x03
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildUniStreamsBlockedPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStreamsBlockedFrame frame;
  frame.stream_count = 3;
  frame.unidirectional = true;

  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAMS_BLOCKED_UNIDIRECTIONAL frame)
    0x17,
    // Stream count
    kVarInt62OneByte + 0x03
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildBiDiMaxStreamsPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicMaxStreamsFrame frame;
  frame.stream_count = 3;
  frame.unidirectional = false;

  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL frame)
    0x12,
    // Stream count
    kVarInt62OneByte + 0x03
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildUniDiMaxStreamsPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // This test runs in client mode.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicMaxStreamsFrame frame;
  frame.stream_count = 3;
  frame.unidirectional = true;

  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL frame)
    0x13,
    // Stream count
    kVarInt62OneByte + 0x03
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, NewConnectionIdFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_CONNECTION_ID frame)
      {"",
       {0x18}},
      // error code
      {"Unable to read new connection ID frame sequence number.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read new connection ID frame connection id length.",
       {0x08}},  // connection ID length
      {"Unable to read new connection ID frame connection id.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11}},
      {"Can not read new connection ID frame reset token.",
       {0xb5, 0x69, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(FramerTestConnectionIdPlusOne(),
            visitor_.new_connection_id_.connection_id);
  EXPECT_EQ(0x11u, visitor_.new_connection_id_.sequence_number);
  EXPECT_EQ(kTestStatelessResetToken,
            visitor_.new_connection_id_.stateless_reset_token);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet99, QUIC_INVALID_NEW_CONNECTION_ID_DATA);
}

TEST_P(QuicFramerTest, NewConnectionIdFrameVariableLength) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_CONNECTION_ID frame)
      {"",
       {0x18}},
      // error code
      {"Unable to read new connection ID frame sequence number.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read new connection ID frame connection id length.",
       {0x09}},  // connection ID length
      {"Unable to read new connection ID frame connection id.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42}},
      {"Can not read new connection ID frame reset token.",
       {0xb5, 0x69, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(FramerTestConnectionIdNineBytes(),
            visitor_.new_connection_id_.connection_id);
  EXPECT_EQ(0x11u, visitor_.new_connection_id_.sequence_number);
  EXPECT_EQ(kTestStatelessResetToken,
            visitor_.new_connection_id_.stateless_reset_token);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet99, QUIC_INVALID_NEW_CONNECTION_ID_DATA);
}

// Verifies that parsing a NEW_CONNECTION_ID frame with a length above the
// specified maximum fails.
TEST_P(QuicFramerTest, InvalidLongNewConnectionIdFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // The NEW_CONNECTION_ID frame is only for version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_CONNECTION_ID frame)
      {"",
       {0x18}},
      // error code
      {"Unable to read new connection ID frame sequence number.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read new connection ID frame connection id length.",
       {0x13}},  // connection ID length
      {"Unable to read new connection ID frame connection id.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
        0xF0, 0xD2, 0xB4, 0x96, 0x78, 0x5A, 0x3C, 0x1E,
        0x42, 0x33, 0x42}},
      {"Can not read new connection ID frame reset token.",
       {0xb5, 0x69, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_INVALID_NEW_CONNECTION_ID_DATA, framer_.error());
  EXPECT_EQ("New connection ID length too high.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, BuildNewConnectionIdFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 0x11;
  // Use this value to force a 4-byte encoded variable length connection ID
  // in the frame.
  frame.connection_id = FramerTestConnectionIdPlusOne();
  frame.stateless_reset_token = kTestStatelessResetToken;

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_NEW_CONNECTION_ID frame)
    0x18,
    // sequence number
    kVarInt62OneByte + 0x11,
    // new connection id length
    0x08,
    // new connection id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
    // stateless reset token
    0xb5, 0x69, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, NewTokenFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_TOKEN frame)
      {"",
       {0x07}},
      // Length
      {"Unable to read new token length.",
       {kVarInt62OneByte + 0x08}},
      {"Unable to read new token data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}}
  };
  // clang-format on
  uint8_t expected_token_value[] = {0x00, 0x01, 0x02, 0x03,
                                    0x04, 0x05, 0x06, 0x07};

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(sizeof(expected_token_value), visitor_.new_token_.token.length());
  EXPECT_EQ(0, memcmp(expected_token_value, visitor_.new_token_.token.data(),
                      sizeof(expected_token_value)));

  CheckFramingBoundaries(packet, QUIC_INVALID_NEW_TOKEN);
}

TEST_P(QuicFramerTest, BuildNewTokenFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  uint8_t expected_token_value[] = {0x00, 0x01, 0x02, 0x03,
                                    0x04, 0x05, 0x06, 0x07};

  QuicNewTokenFrame frame(0, std::string((const char*)(expected_token_value),
                                         sizeof(expected_token_value)));

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_NEW_TOKEN frame)
    0x07,
    // Length and token
    kVarInt62OneByte + 0x08,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, IetfStopSendingFrame) {
  // This test is only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STOP_SENDING frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read stop sending stream id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      {"Unable to read stop sending application error code.",
       {0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.stop_sending_frame_.stream_id);
  EXPECT_EQ(0x7654, visitor_.stop_sending_frame_.application_error_code);

  CheckFramingBoundaries(packet99, QUIC_INVALID_STOP_SENDING_FRAME_DATA);
}

TEST_P(QuicFramerTest, BuildIetfStopSendingPacket) {
  // This test is only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStopSendingFrame frame;
  frame.stream_id = kStreamId;
  frame.application_error_code = 0xffff;
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STOP_SENDING frame)
    0x05,
    // Stream ID
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // Application error code
    0xff, 0xff
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, IetfPathChallengeFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_PATH_CHALLENGE)
      {"",
       {0x1a}},
      // data
      {"Can not read path challenge data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}}),
            visitor_.path_challenge_frame_.data_buffer);

  CheckFramingBoundaries(packet99, QUIC_INVALID_PATH_CHALLENGE_DATA);
}

TEST_P(QuicFramerTest, BuildIetfPathChallengePacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicPathChallengeFrame frame;
  frame.data_buffer = QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}});
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PATH_CHALLENGE)
    0x1a,
    // Data
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, IetfPathResponseFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_PATH_RESPONSE)
      {"",
       {0x1b}},
      // data
      {"Can not read path response data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}}),
            visitor_.path_response_frame_.data_buffer);

  CheckFramingBoundaries(packet99, QUIC_INVALID_PATH_RESPONSE_DATA);
}

TEST_P(QuicFramerTest, BuildIetfPathResponsePacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicPathResponseFrame frame;
  frame.data_buffer = QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}});
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PATH_RESPONSE)
    0x1b,
    // Data
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, GetRetransmittableControlFrameSize) {
  QuicRstStreamFrame rst_stream(1, 3, QUIC_STREAM_CANCELLED, 1024);
  EXPECT_EQ(QuicFramer::GetRstStreamFrameSize(framer_.transport_version(),
                                              rst_stream),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&rst_stream)));

  std::string error_detail(2048, 'e');
  QuicConnectionCloseFrame connection_close(QUIC_NETWORK_IDLE_TIMEOUT,
                                            error_detail);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    connection_close.close_type = IETF_QUIC_TRANSPORT_CONNECTION_CLOSE;
  }

  EXPECT_EQ(QuicFramer::GetConnectionCloseFrameSize(framer_.transport_version(),
                                                    connection_close),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&connection_close)));

  QuicGoAwayFrame goaway(2, QUIC_PEER_GOING_AWAY, 3, error_detail);
  EXPECT_EQ(QuicFramer::GetMinGoAwayFrameSize() + 256,
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&goaway)));

  QuicWindowUpdateFrame window_update(3, 3, 1024);
  EXPECT_EQ(QuicFramer::GetWindowUpdateFrameSize(framer_.transport_version(),
                                                 window_update),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&window_update)));

  QuicBlockedFrame blocked(4, 3, 1024);
  EXPECT_EQ(
      QuicFramer::GetBlockedFrameSize(framer_.transport_version(), blocked),
      QuicFramer::GetRetransmittableControlFrameSize(
          framer_.transport_version(), QuicFrame(&blocked)));

  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicNewConnectionIdFrame new_connection_id(5, TestConnectionId(), 1, 101111);
  EXPECT_EQ(QuicFramer::GetNewConnectionIdFrameSize(new_connection_id),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&new_connection_id)));

  QuicMaxStreamsFrame max_streams(6, 3, /*unidirectional=*/false);
  EXPECT_EQ(QuicFramer::GetMaxStreamsFrameSize(framer_.transport_version(),
                                               max_streams),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(max_streams)));

  QuicStreamsBlockedFrame streams_blocked(7, 3, /*unidirectional=*/false);
  EXPECT_EQ(QuicFramer::GetStreamsBlockedFrameSize(framer_.transport_version(),
                                                   streams_blocked),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(streams_blocked)));

  QuicPathFrameBuffer buffer = {
      {0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5, 0xe5, 0xf7}};
  QuicPathResponseFrame path_response_frame(8, buffer);
  EXPECT_EQ(QuicFramer::GetPathResponseFrameSize(path_response_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&path_response_frame)));

  QuicPathChallengeFrame path_challenge_frame(9, buffer);
  EXPECT_EQ(QuicFramer::GetPathChallengeFrameSize(path_challenge_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&path_challenge_frame)));

  QuicStopSendingFrame stop_sending_frame(10, 3, 20);
  EXPECT_EQ(QuicFramer::GetStopSendingFrameSize(stop_sending_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&stop_sending_frame)));
}

// A set of tests to ensure that bad frame-type encodings
// are properly detected and handled.
// First, four tests to see that unknown frame types generate
// a QUIC_INVALID_FRAME_DATA error with detailed information
// "Illegal frame type." This regardless of the encoding of the type
// (1/2/4/8 bytes).
// This only for version 99.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown1Byte) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, single-byte encoding)
      {"",
       {0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown2Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x01, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown4Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, four-byte encoding)
      {"",
       {kVarInt62FourBytes + 0x01, 0x00, 0x00, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown8Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, eight-byte encoding)
      {"",
       {kVarInt62EightBytes + 0x01, 0x00, 0x00, 0x01, 0x02, 0x34, 0x56, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

// Three tests to check that known frame types that are not minimally
// encoded generate IETF_QUIC_PROTOCOL_VIOLATION errors with detailed
// information "Frame type not minimally encoded."
// Look at the frame-type encoded in 2, 4, and 8 bytes.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown2Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(IETF_QUIC_PROTOCOL_VIOLATION, framer_.error());
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown4Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, four-byte encoding)
      {"",
       {kVarInt62FourBytes + 0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(IETF_QUIC_PROTOCOL_VIOLATION, framer_.error());
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown8Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, eight-byte encoding)
      {"",
       {kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(IETF_QUIC_PROTOCOL_VIOLATION, framer_.error());
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

// Tests to check that all known OETF frame types that are not minimally
// encoded generate IETF_QUIC_PROTOCOL_VIOLATION errors with detailed
// information "Frame type not minimally encoded."
// Just look at 2-byte encoding.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown2BytesAllTypes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packets[] = {
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x00}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x01}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x02}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x03}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x04}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x05}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x06}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x07}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x08}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x09}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0a}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0b}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0c}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0d}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0e}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0f}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x10}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x11}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x12}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x13}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x14}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x15}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x16}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x17}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x18}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x20}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x21}}
    },
  };
  // clang-format on

  for (PacketFragments& packet : packets) {
    std::unique_ptr<QuicEncryptedPacket> encrypted(
        AssemblePacketFromFragments(packet));

    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

    EXPECT_EQ(IETF_QUIC_PROTOCOL_VIOLATION, framer_.error());
    EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
  }
}

TEST_P(QuicFramerTest, RetireConnectionIdFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_RETIRE_CONNECTION_ID frame)
      {"",
       {0x19}},
      // Sequence number
      {"Unable to read retire connection ID frame sequence number.",
       {kVarInt62TwoBytes + 0x11, 0x22}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(0x1122u, visitor_.retire_connection_id_.sequence_number);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet99, QUIC_INVALID_RETIRE_CONNECTION_ID_DATA);
}

TEST_P(QuicFramerTest, BuildRetireConnectionIdFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicRetireConnectionIdFrame frame;
  frame.sequence_number = 0x1122;

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_RETIRE_CONNECTION_ID frame)
    0x19,
    // sequence number
    kVarInt62TwoBytes + 0x11, 0x22
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, AckFrameWithInvalidLargestObserved) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x45,
    // largest observed
    0x00, 0x00,
    // Zero delta time.
    0x00, 0x00,
    // first ack block length.
    0x00, 0x00,
    // num timestamps.
    0x00
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x45,
    // largest observed
    0x00, 0x00,
    // Zero delta time.
    0x00, 0x00,
    // first ack block length.
    0x00, 0x00,
    // num timestamps.
    0x00
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x45,
    // largest observed
    0x00, 0x00,
    // Zero delta time.
    0x00, 0x00,
    // first ack block length.
    0x00, 0x00,
    // num timestamps.
    0x00
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_ACK frame)
    0x02,
    // Largest acked
    kVarInt62OneByte + 0x00,
    // Zero delta time.
    kVarInt62OneByte + 0x00,
    // Ack block count 0
    kVarInt62OneByte + 0x00,
    // First ack block length
    kVarInt62OneByte + 0x00,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(framer_.detailed_error(), "Largest acked is 0.");
}

TEST_P(QuicFramerTest, FirstAckBlockJustUnderFlow) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x45,
    // largest observed
    0x00, 0x02,
    // Zero delta time.
    0x00, 0x00,
    // first ack block length.
    0x00, 0x03,
    // num timestamps.
    0x00
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x45,
    // largest observed
    0x00, 0x02,
    // Zero delta time.
    0x00, 0x00,
    // first ack block length.
    0x00, 0x03,
    // num timestamps.
    0x00
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x45,
    // largest observed
    0x00, 0x02,
    // Zero delta time.
    0x00, 0x00,
    // first ack block length.
    0x00, 0x03,
    // num timestamps.
    0x00
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_ACK frame)
    0x02,
    // Largest acked
    kVarInt62OneByte + 0x02,
    // Zero delta time.
    kVarInt62OneByte + 0x00,
    // Ack block count 0
    kVarInt62OneByte + 0x00,
    // First ack block length
    kVarInt62OneByte + 0x02,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with first ack block length 3 largest acked is 2.");
}

TEST_P(QuicFramerTest, ThirdAckBlockJustUnderflow) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x60,
    // largest observed
    0x0A,
    // Zero delta time.
    0x00, 0x00,
    // Num of ack blocks
    0x02,
    // first ack block length.
    0x02,
    // gap to next block
    0x01,
    // ack block length
    0x01,
    // gap to next block
    0x01,
    // ack block length
    0x06,
    // num timestamps.
    0x00
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x60,
    // largest observed
    0x0A,
    // Zero delta time.
    0x00, 0x00,
    // Num of ack blocks
    0x02,
    // first ack block length.
    0x02,
    // gap to next block
    0x01,
    // ack block length
    0x01,
    // gap to next block
    0x01,
    // ack block length
    0x06,
    // num timestamps.
    0x00
  };

  unsigned char packet46[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x60,
    // largest observed
    0x0A,
    // Zero delta time.
    0x00, 0x00,
    // Num of ack blocks
    0x02,
    // first ack block length.
    0x02,
    // gap to next block
    0x01,
    // ack block length
    0x01,
    // gap to next block
    0x01,
    // ack block length
    0x06,
    // num timestamps.
    0x00
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_ACK frame)
    0x02,
    // Largest acked
    kVarInt62OneByte + 0x0A,
    // Zero delta time.
    kVarInt62OneByte + 0x00,
    // Ack block count 2
    kVarInt62OneByte + 0x02,
    // First ack block length
    kVarInt62OneByte + 0x01,
    // gap to next block length
    kVarInt62OneByte + 0x00,
    // ack block length
    kVarInt62OneByte + 0x00,
    // gap to next block length
    kVarInt62OneByte + 0x00,
    // ack block length
    kVarInt62OneByte + 0x05,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_44) {
    p = packet46;
    p_size = QUIC_ARRAYSIZE(packet46);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  if (framer_.transport_version() == QUIC_VERSION_99) {
    EXPECT_EQ(framer_.detailed_error(),
              "Underflow with ack block length 6 latest ack block end is 5.");
  } else {
    EXPECT_EQ(framer_.detailed_error(),
              "Underflow with ack block length 6, end of block is 6.");
  }
}

TEST_P(QuicFramerTest, CoalescedPacket) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x50,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x50,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  ASSERT_EQ(visitor_.coalesced_packets_.size(), 1u);
  EXPECT_TRUE(framer_.ProcessPacket(*visitor_.coalesced_packets_[0].get()));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(2u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[1]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[1]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[1]->offset);
  CheckStreamFrameData("HELLO_WORLD?", visitor_.stream_frames_[1].get());
}

TEST_P(QuicFramerTest, MismatchedCoalescedPacket) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x50,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x50,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_QUIC_PEER_BUG(EXPECT_TRUE(framer_.ProcessPacket(encrypted)),
                       "Server: Received mismatched coalesced header.*");

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  ASSERT_EQ(visitor_.coalesced_packets_.size(), 0u);
}

TEST_P(QuicFramerTest, InvalidCoalescedPacket) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x50,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version would be here but we cut off the invalid coalesced header.
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_QUIC_PEER_BUG(EXPECT_TRUE(framer_.ProcessPacket(encrypted)),
                       "Server: Failed to parse received coalesced header.*");

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  ASSERT_EQ(visitor_.coalesced_packets_.size(), 0u);
}

// Some IETF implementations send an initial followed by zeroes instead of
// padding inside the initial. We need to make sure that we still process
// the initial correctly and ignore the zeroes.
TEST_P(QuicFramerTest, CoalescedPacketWithZeroesRoundTrip) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  QuicConnectionId connection_id = FramerTestConnectionId();
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  CrypterPair client_crypters;
  CryptoUtils::CreateTlsInitialCrypters(Perspective::IS_CLIENT,
                                        framer_.transport_version(),
                                        connection_id, &client_crypters);
  framer_.SetEncrypter(ENCRYPTION_INITIAL,
                       std::move(client_crypters.encrypter));

  QuicPacketHeader header;
  header.destination_connection_id = connection_id;
  header.version_flag = true;
  header.packet_number = kPacketNumber;
  header.packet_number_length = PACKET_4BYTE_PACKET_NUMBER;
  header.long_packet_type = INITIAL;
  header.length_length = VARIABLE_LENGTH_INTEGER_LENGTH_2;
  header.retry_token_length_length = VARIABLE_LENGTH_INTEGER_LENGTH_1;
  QuicFrames frames = {QuicFrame(QuicPingFrame())};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_NE(nullptr, data);

  // Add zeroes after the valid initial packet.
  unsigned char packet[kMaxOutgoingPacketSize] = {};
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number, *data,
                             AsChars(packet), QUIC_ARRAYSIZE(packet));
  ASSERT_NE(0u, encrypted_length);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  CrypterPair server_crypters;
  CryptoUtils::CreateTlsInitialCrypters(Perspective::IS_SERVER,
                                        framer_.transport_version(),
                                        connection_id, &server_crypters);
  framer_.InstallDecrypter(ENCRYPTION_INITIAL,
                           std::move(server_crypters.decrypter));

  // Make sure the first long header initial packet parses correctly.
  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);

  // Make sure we discard the subsequent zeroes.
  EXPECT_QUIC_PEER_BUG(EXPECT_TRUE(framer_.ProcessPacket(encrypted)),
                       "Server: Received mismatched coalesced header.*");
}

TEST_P(QuicFramerTest, ClientReceivesInvalidVersion) {
  if (framer_.transport_version() < QUIC_VERSION_44) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  // clang-format off
  unsigned char packet[] = {
       // public flags (long header with packet type INITIAL)
       0xFF,
       // version that is different from the framer's version
       'Q', '0', '4', '3',
       // connection ID lengths
       0x05,
       // source connection ID
       0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
       // packet number
       0x01,
       // padding frame
       0x00,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_INVALID_VERSION, framer_.error());
  EXPECT_EQ("Client received unexpected version.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, PacketHeaderWithVariableLengthConnectionId) {
  if (framer_.transport_version() < QUIC_VERSION_46) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  char connection_id_bytes[9] = {0xFE, 0xDC, 0xBA, 0x98, 0x76,
                                 0x54, 0x32, 0x10, 0x42};
  QuicConnectionId connection_id(connection_id_bytes,
                                 sizeof(connection_id_bytes));
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);
  QuicFramerPeer::SetExpectedServerConnectionIDLength(&framer_,
                                                      connection_id.length());

  // clang-format off
  PacketFragments packet = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read type.",
       {0x40}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42}},
      // packet number
      {"Unable to read packet number.",
       {0x78}},
  };

  PacketFragments packet_with_padding = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read type.",
       {0x40}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42}},
      // packet number
      {"",
       {0x78}},
      // padding
      {"", {0x00, 0x00, 0x00}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection() ? packet_with_padding : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  if (framer_.version().HasHeaderProtection()) {
    EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
    EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  } else {
    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
    EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  }
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(connection_id, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, UpdateExpectedConnectionIdLength) {
  if (framer_.transport_version() < QUIC_VERSION_46) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  framer_.SetShouldUpdateExpectedServerConnectionIdLength(true);

  // clang-format off
  unsigned char long_header_packet[] = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x60,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // padding frame
      0x00,
  };
  unsigned char long_header_packet99[] = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x60,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42,
      // long header packet length
      0x05,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // padding frame
      0x00,
  };
  // clang-format on

  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    EXPECT_TRUE(framer_.ProcessPacket(
        QuicEncryptedPacket(AsChars(long_header_packet),
                            QUIC_ARRAYSIZE(long_header_packet), false)));
  } else {
    EXPECT_TRUE(framer_.ProcessPacket(
        QuicEncryptedPacket(AsChars(long_header_packet99),
                            QUIC_ARRAYSIZE(long_header_packet99), false)));
  }

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(visitor_.header_.get()->destination_connection_id,
            FramerTestConnectionIdNineBytes());
  EXPECT_EQ(visitor_.header_.get()->packet_number,
            QuicPacketNumber(UINT64_C(0x12345678)));

  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char short_header_packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42,
    // packet number
    0x13, 0x37, 0x42, 0x33,
    // padding frame
    0x00,
  };
  // clang-format on

  QuicEncryptedPacket short_header_encrypted(
      AsChars(short_header_packet), QUIC_ARRAYSIZE(short_header_packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(short_header_encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(visitor_.header_.get()->destination_connection_id,
            FramerTestConnectionIdNineBytes());
  EXPECT_EQ(visitor_.header_.get()->packet_number,
            QuicPacketNumber(UINT64_C(0x13374233)));

  PacketHeaderFormat format;
  bool version_flag;
  uint8_t destination_connection_id_length;
  QuicConnectionId destination_connection_id;
  QuicVersionLabel version_label;
  std::string detailed_error;
  EXPECT_EQ(QUIC_NO_ERROR,
            QuicFramer::ProcessPacketDispatcher(
                QuicEncryptedPacket(AsChars(long_header_packet),
                                    QUIC_ARRAYSIZE(long_header_packet)),
                kQuicDefaultConnectionIdLength, &format, &version_flag,
                &version_label, &destination_connection_id_length,
                &destination_connection_id, &detailed_error));
  EXPECT_EQ(IETF_QUIC_LONG_HEADER_PACKET, format);
  EXPECT_TRUE(version_flag);
  EXPECT_EQ(9, destination_connection_id_length);
  EXPECT_EQ(FramerTestConnectionIdNineBytes(), destination_connection_id);

  EXPECT_EQ(QUIC_NO_ERROR,
            QuicFramer::ProcessPacketDispatcher(
                short_header_encrypted, 9, &format, &version_flag,
                &version_label, &destination_connection_id_length,
                &destination_connection_id, &detailed_error));
  EXPECT_EQ(IETF_QUIC_SHORT_HEADER_PACKET, format);
  EXPECT_FALSE(version_flag);
  EXPECT_EQ(9, destination_connection_id_length);
  EXPECT_EQ(FramerTestConnectionIdNineBytes(), destination_connection_id);
}

TEST_P(QuicFramerTest, MultiplePacketNumberSpaces) {
  if (framer_.transport_version() < QUIC_VERSION_46) {
    return;
  }
  framer_.SetShouldUpdateExpectedServerConnectionIdLength(true);
  framer_.EnableMultiplePacketNumberSpacesSupport();

  // clang-format off
  unsigned char long_header_packet[] = {
       // public flags (long header with packet type ZERO_RTT_PROTECTED and
       // 4-byte packet number)
       0xD3,
       // version
       QUIC_VERSION_BYTES,
       // destination connection ID length
       0x60,
       // destination connection ID
       0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42,
       // packet number
       0x12, 0x34, 0x56, 0x78,
       // padding frame
       0x00,
   };
  unsigned char long_header_packet99[] = {
       // public flags (long header with packet type ZERO_RTT_PROTECTED and
       // 4-byte packet number)
       0xD3,
       // version
       QUIC_VERSION_BYTES,
       // destination connection ID length
       0x60,
       // destination connection ID
       0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42,
       // long header packet length
       0x05,
       // packet number
       0x12, 0x34, 0x56, 0x78,
       // padding frame
       0x00,
  };
  // clang-format on

  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                             QuicMakeUnique<TestDecrypter>());
    framer_.RemoveDecrypter(ENCRYPTION_INITIAL);
  } else {
    framer_.SetDecrypter(ENCRYPTION_ZERO_RTT, QuicMakeUnique<TestDecrypter>());
  }
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    EXPECT_TRUE(framer_.ProcessPacket(
        QuicEncryptedPacket(AsChars(long_header_packet),
                            QUIC_ARRAYSIZE(long_header_packet), false)));
  } else {
    EXPECT_TRUE(framer_.ProcessPacket(
        QuicEncryptedPacket(AsChars(long_header_packet99),
                            QUIC_ARRAYSIZE(long_header_packet99), false)));
  }

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  EXPECT_FALSE(
      QuicFramerPeer::GetLargestDecryptedPacketNumber(&framer_, INITIAL_DATA)
          .IsInitialized());
  EXPECT_FALSE(
      QuicFramerPeer::GetLargestDecryptedPacketNumber(&framer_, HANDSHAKE_DATA)
          .IsInitialized());
  EXPECT_EQ(kPacketNumber, QuicFramerPeer::GetLargestDecryptedPacketNumber(
                               &framer_, APPLICATION_DATA));

  // clang-format off
  unsigned char short_header_packet[] = {
     // type (short header, 1 byte packet number)
     0x40,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42,
     // packet number
     0x79,
     // padding frame
     0x00, 0x00, 0x00,
  };
  // clang-format on

  QuicEncryptedPacket short_header_encrypted(
      AsChars(short_header_packet), QUIC_ARRAYSIZE(short_header_packet), false);
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                             QuicMakeUnique<TestDecrypter>());
    framer_.RemoveDecrypter(ENCRYPTION_ZERO_RTT);
  } else {
    framer_.SetDecrypter(ENCRYPTION_FORWARD_SECURE,
                         QuicMakeUnique<TestDecrypter>());
  }
  EXPECT_TRUE(framer_.ProcessPacket(short_header_encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  EXPECT_FALSE(
      QuicFramerPeer::GetLargestDecryptedPacketNumber(&framer_, INITIAL_DATA)
          .IsInitialized());
  EXPECT_FALSE(
      QuicFramerPeer::GetLargestDecryptedPacketNumber(&framer_, HANDSHAKE_DATA)
          .IsInitialized());
  EXPECT_EQ(kPacketNumber + 1, QuicFramerPeer::GetLargestDecryptedPacketNumber(
                                   &framer_, APPLICATION_DATA));
}

TEST_P(QuicFramerTest, IetfRetryPacketRejected) {
  if (!framer_.version().KnowsWhichDecrypterToUse() ||
      framer_.version().SupportsRetry()) {
    return;
  }

  // clang-format off
  PacketFragments packet = {
    // public flags (IETF Retry packet, 0-length original destination CID)
    {"Unable to read type.",
     {0xf0}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"Illegal long header type value.",
     {0x00}},
  };
  // clang-format on

  // clang-format off
  PacketFragments packet45 = {
    // public flags (IETF Retry packet, 0-length original destination CID)
    {"Unable to read type.",
     {0xf0}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"RETRY not supported in this version.",
     {0x00}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_44 ? packet45 : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, RetryPacketRejectedWithMultiplePacketNumberSpaces) {
  if (framer_.transport_version() < QUIC_VERSION_46 ||
      framer_.version().SupportsRetry()) {
    return;
  }
  framer_.EnableMultiplePacketNumberSpacesSupport();

  // clang-format off
  PacketFragments packet = {
    // public flags (IETF Retry packet, 0-length original destination CID)
    {"Unable to read type.",
     {0xf0}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"RETRY not supported in this version.",
     {0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
  CheckFramingBoundaries(packet, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, ProcessPublicHeaderNoVersionInferredType) {
  // The framer needs to have Perspective::IS_SERVER and configured to infer the
  // packet header type from the packet (not the version). The framer's version
  // needs to be one that uses the IETF packet format.
  if (!framer_.version().KnowsWhichDecrypterToUse()) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);

  // Prepare a packet that uses the Google QUIC packet header but has no version
  // field.

  // clang-format off
  PacketFragments packet = {
    // public flags (1-byte packet number, 8-byte connection_id, no version)
    {"Unable to read public flags.",
     {0x08}},
    // connection_id
    {"Unable to read ConnectionId.",
     {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
    // packet number
    {"Unable to read packet number.",
     {0x01}},
    // padding
    {"Invalid public header type for expected version.",
     {0x00}},
  };
  // clang-format on

  PacketFragments& fragments = packet;

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
  EXPECT_EQ("Invalid public header type for expected version.",
            framer_.detailed_error());
  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, ProcessMismatchedHeaderVersion) {
  // The framer needs to have Perspective::IS_SERVER and configured to infer the
  // packet header type from the packet (not the version). The framer's version
  // needs to be one that uses the IETF packet format.
  if (!framer_.version().KnowsWhichDecrypterToUse()) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);

  // clang-format off
  PacketFragments packet = {
    // public flags (Google QUIC header with version present)
    {"Unable to read public flags.",
     {0x09}},
    // connection_id
    {"Unable to read ConnectionId.",
     {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // packet number
    {"Unable to read packet number.",
     {0x01}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  framer_.ProcessPacket(*encrypted);

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
  EXPECT_EQ("Invalid public header type for expected version.",
            framer_.detailed_error());
  CheckFramingBoundaries(packet, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, WriteClientVersionNegotiationProbePacket) {
  // clang-format off
  static const char expected_packet[1200] = {
    // IETF long header with fixed bit set, type initial, all-0 encrypted bits.
    0xc0,
    // Version, part of the IETF space reserved for negotiation.
    0xca, 0xba, 0xda, 0xba,
    // Destination connection ID length 8, source connection ID length 0.
    0x50,
    // 8-byte destination connection ID.
    0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21,
    // 8 bytes of zeroes followed by 8 bytes of ones to ensure that this does
    // not parse with any known version.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    // 2 bytes of zeroes to pad to 16 byte boundary.
    0x00, 0x00,
    // A polite greeting in case a human sees this in tcpdump.
    0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x61, 0x63,
    0x6b, 0x65, 0x74, 0x20, 0x6f, 0x6e, 0x6c, 0x79,
    0x20, 0x65, 0x78, 0x69, 0x73, 0x74, 0x73, 0x20,
    0x74, 0x6f, 0x20, 0x74, 0x72, 0x69, 0x67, 0x67,
    0x65, 0x72, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20,
    0x51, 0x55, 0x49, 0x43, 0x20, 0x76, 0x65, 0x72,
    0x73, 0x69, 0x6f, 0x6e, 0x20, 0x6e, 0x65, 0x67,
    0x6f, 0x74, 0x69, 0x61, 0x74, 0x69, 0x6f, 0x6e,
    0x2e, 0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65,
    0x20, 0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x64,
    0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x61, 0x20,
    0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x20,
    0x4e, 0x65, 0x67, 0x6f, 0x74, 0x69, 0x61, 0x74,
    0x69, 0x6f, 0x6e, 0x20, 0x70, 0x61, 0x63, 0x6b,
    0x65, 0x74, 0x20, 0x69, 0x6e, 0x64, 0x69, 0x63,
    0x61, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x68,
    0x61, 0x74, 0x20, 0x76, 0x65, 0x72, 0x73, 0x69,
    0x6f, 0x6e, 0x73, 0x20, 0x79, 0x6f, 0x75, 0x20,
    0x73, 0x75, 0x70, 0x70, 0x6f, 0x72, 0x74, 0x2e,
    0x20, 0x54, 0x68, 0x61, 0x6e, 0x6b, 0x20, 0x79,
    0x6f, 0x75, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x68,
    0x61, 0x76, 0x65, 0x20, 0x61, 0x20, 0x6e, 0x69,
    0x63, 0x65, 0x20, 0x64, 0x61, 0x79, 0x2e, 0x00,
  };
  // clang-format on
  char packet[1200];
  char destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                            0x6c, 0x7a, 0x20, 0x21};
  EXPECT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), destination_connection_id_bytes,
      sizeof(destination_connection_id_bytes)));
  test::CompareCharArraysWithHexError("constructed packet", expected_packet,
                                      sizeof(expected_packet), packet,
                                      sizeof(packet));
  QuicEncryptedPacket encrypted(reinterpret_cast<const char*>(packet),
                                sizeof(packet), false);
  // Make sure we fail to parse this packet for the version under test.
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  if (framer_.transport_version() <= QUIC_VERSION_43) {
    // We can only parse the connection ID with an IETF parser.
    return;
  }
  ASSERT_TRUE(visitor_.header_.get());
  QuicConnectionId probe_payload_connection_id(
      reinterpret_cast<const char*>(destination_connection_id_bytes),
      sizeof(destination_connection_id_bytes));
  EXPECT_EQ(probe_payload_connection_id,
            visitor_.header_.get()->destination_connection_id);
}

TEST_P(QuicFramerTest, ParseServerVersionNegotiationProbeResponse) {
  // clang-format off
  const char packet[] = {
    // IETF long header with fixed bit set, type initial, all-0 encrypted bits.
    0xc0,
    // Version of 0, indicating version negotiation.
    0x00, 0x00, 0x00, 0x00,
    // Destination connection ID length 0, source connection ID length 8.
    0x05,
    // 8-byte source connection ID.
    0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21,
    // A few supported versions.
    0xaa, 0xaa, 0xaa, 0xaa,
    QUIC_VERSION_BYTES,
  };
  // clang-format on
  char probe_payload_bytes[] = {0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21};
  char parsed_probe_payload_bytes[kQuicMaxConnectionIdLength] = {};
  uint8_t parsed_probe_payload_length = 0;
  std::string parse_detailed_error = "";
  EXPECT_TRUE(QuicFramer::ParseServerVersionNegotiationProbeResponse(
      reinterpret_cast<const char*>(packet), sizeof(packet),
      reinterpret_cast<char*>(parsed_probe_payload_bytes),
      &parsed_probe_payload_length, &parse_detailed_error));
  EXPECT_EQ("", parse_detailed_error);
  test::CompareCharArraysWithHexError(
      "parsed probe", probe_payload_bytes, sizeof(probe_payload_bytes),
      parsed_probe_payload_bytes, parsed_probe_payload_length);
}

TEST_P(QuicFramerTest, ClientConnectionIdNotSupportedYet) {
  if (GetQuicRestartFlag(quic_do_not_override_connection_id)) {
    // This check is currently only performed when this flag is disabled.
    return;
  }
  if (framer_.transport_version() <= QUIC_VERSION_43) {
    // This test requires an IETF long header.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  const unsigned char type_byte =
      framer_.transport_version() == QUIC_VERSION_44 ? 0xFC : 0xD3;
  // clang-format off
  unsigned char packet[] = {
    // public flags (long header with packet type ZERO_RTT_PROTECTED and
    // 4-byte packet number)
    type_byte,
    // version
    QUIC_VERSION_BYTES,
    // destination connection ID length
    0x50,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frame
    0x00,
  };
  // clang-format on
  EXPECT_FALSE(framer_.ProcessPacket(
      QuicEncryptedPacket(AsChars(packet), QUIC_ARRAYSIZE(packet), false)));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
  if (!QuicUtils::VariableLengthConnectionIdAllowedForVersion(
          framer_.transport_version())) {
    EXPECT_EQ("Invalid ConnectionId length.", framer_.detailed_error());
  } else {
    EXPECT_EQ("Client connection ID not supported yet.",
              framer_.detailed_error());
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
