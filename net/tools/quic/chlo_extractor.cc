// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/chlo_extractor.h"

#include "net/quic/core/crypto/crypto_framer.h"
#include "net/quic/core/crypto/crypto_handshake_message.h"
#include "net/quic/core/crypto/crypto_protocol.h"
#include "net/quic/core/crypto/quic_decrypter.h"
#include "net/quic/core/crypto/quic_encrypter.h"
#include "net/quic/core/quic_framer.h"
#include "net/quic/platform/api/quic_string_piece.h"
#include "net/quic/platform/api/quic_text_utils.h"

namespace net {

namespace {

class ChloFramerVisitor : public QuicFramerVisitorInterface,
                          public CryptoFramerVisitorInterface {
 public:
  ChloFramerVisitor(QuicFramer* framer,
                    const QuicTagVector& create_session_tag_indicators,
                    ChloExtractor::Delegate* delegate);

  ~ChloFramerVisitor() override = default;

  // QuicFramerVisitorInterface implementation
  void OnError(QuicFramer* framer) override {}
  bool OnProtocolVersionMismatch(ParsedQuicVersion version) override;
  void OnPacket() override {}
  void OnPublicResetPacket(const QuicPublicResetPacket& packet) override {}
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {}
  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override;
  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override;
  void OnDecryptedPacket(EncryptionLevel level) override {}
  bool OnPacketHeader(const QuicPacketHeader& header) override;
  bool OnStreamFrame(const QuicStreamFrame& frame) override;
  bool OnAckFrame(const QuicAckFrame& frame) override;
  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta ack_delay_time) override;
  bool OnAckRange(QuicPacketNumber start,
                  QuicPacketNumber end,
                  bool last_range) override;
  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override;
  bool OnPingFrame(const QuicPingFrame& frame) override;
  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override;
  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override;
  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override;
  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override;
  bool OnBlockedFrame(const QuicBlockedFrame& frame) override;
  bool OnPaddingFrame(const QuicPaddingFrame& frame) override;
  void OnPacketComplete() override {}

  // CryptoFramerVisitorInterface implementation.
  void OnError(CryptoFramer* framer) override;
  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override;

  bool found_chlo() { return found_chlo_; }
  bool chlo_contains_tags() { return chlo_contains_tags_; }

 private:
  QuicFramer* framer_;
  const QuicTagVector& create_session_tag_indicators_;
  ChloExtractor::Delegate* delegate_;
  bool found_chlo_;
  bool chlo_contains_tags_;
  QuicConnectionId connection_id_;
};

ChloFramerVisitor::ChloFramerVisitor(
    QuicFramer* framer,
    const QuicTagVector& create_session_tag_indicators,
    ChloExtractor::Delegate* delegate)
    : framer_(framer),
      create_session_tag_indicators_(create_session_tag_indicators),
      delegate_(delegate),
      found_chlo_(false),
      chlo_contains_tags_(false),
      connection_id_(0) {}

bool ChloFramerVisitor::OnProtocolVersionMismatch(ParsedQuicVersion version) {
  if (!framer_->IsSupportedVersion(version)) {
    return false;
  }
  framer_->set_version(version);
  return true;
}

bool ChloFramerVisitor::OnUnauthenticatedPublicHeader(
    const QuicPacketHeader& header) {
  connection_id_ = header.connection_id;
  return true;
}
bool ChloFramerVisitor::OnUnauthenticatedHeader(
    const QuicPacketHeader& header) {
  return true;
}
bool ChloFramerVisitor::OnPacketHeader(const QuicPacketHeader& header) {
  return true;
}
bool ChloFramerVisitor::OnStreamFrame(const QuicStreamFrame& frame) {
  QuicStringPiece data(frame.data_buffer, frame.data_length);
  if (frame.stream_id == kCryptoStreamId && frame.offset == 0 &&
      QuicTextUtils::StartsWith(data, "CHLO")) {
    CryptoFramer crypto_framer;
    crypto_framer.set_visitor(this);
    if (!crypto_framer.ProcessInput(data, Perspective::IS_SERVER)) {
      return false;
    }
    if (FLAGS_quic_reloadable_flag_quic_inspect_chlo_tags) {
      // Interrogate the crypto framer and see if there are any
      // intersecting tags between what we saw in the maybe-CHLO and the
      // indicator set.
      for (const QuicTag tag : create_session_tag_indicators_) {
        if (crypto_framer.HasTag(tag)) {
          chlo_contains_tags_ = true;
        }
      }
      if (chlo_contains_tags_ && delegate_) {
        // Unfortunately, because this is a partial CHLO,
        // OnHandshakeMessage was never called, so the ALPN was never
        // extracted. Fake it up a bit and send it to the delegate so that
        // the correct dispatch can happen.
        crypto_framer.ForceHandshake();
      }
    }
  }

  return true;
}

bool ChloFramerVisitor::OnAckFrame(const QuicAckFrame& frame) {
  return true;
}

bool ChloFramerVisitor::OnAckFrameStart(QuicPacketNumber /*largest_acked*/,
                                        QuicTime::Delta /*ack_delay_time*/) {
  return true;
}

bool ChloFramerVisitor::OnAckRange(QuicPacketNumber /*start*/,
                                   QuicPacketNumber /*end*/,
                                   bool /*last_range*/) {
  return true;
}

bool ChloFramerVisitor::OnStopWaitingFrame(const QuicStopWaitingFrame& frame) {
  return true;
}

bool ChloFramerVisitor::OnPingFrame(const QuicPingFrame& frame) {
  return true;
}

bool ChloFramerVisitor::OnRstStreamFrame(const QuicRstStreamFrame& frame) {
  return true;
}

bool ChloFramerVisitor::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  return true;
}

bool ChloFramerVisitor::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  return true;
}

bool ChloFramerVisitor::OnWindowUpdateFrame(
    const QuicWindowUpdateFrame& frame) {
  return true;
}

bool ChloFramerVisitor::OnBlockedFrame(const QuicBlockedFrame& frame) {
  return true;
}

bool ChloFramerVisitor::OnPaddingFrame(const QuicPaddingFrame& frame) {
  return true;
}

void ChloFramerVisitor::OnError(CryptoFramer* framer) {}

void ChloFramerVisitor::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  if (delegate_ != nullptr) {
    delegate_->OnChlo(framer_->transport_version(), connection_id_, message);
  }
  found_chlo_ = true;
}

}  // namespace

// static
bool ChloExtractor::Extract(const QuicEncryptedPacket& packet,
                            const ParsedQuicVersionVector& versions,
                            const QuicTagVector& create_session_tag_indicators,
                            Delegate* delegate) {
  QuicFramer framer(versions, QuicTime::Zero(), Perspective::IS_SERVER);
  ChloFramerVisitor visitor(&framer, create_session_tag_indicators, delegate);
  framer.set_visitor(&visitor);
  if (!framer.ProcessPacket(packet)) {
    return false;
  }
  return visitor.found_chlo() || visitor.chlo_contains_tags();
}

}  // namespace net
