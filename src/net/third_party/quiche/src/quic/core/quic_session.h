// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A QuicSession, which demuxes a single connection to individual streams.

#ifndef QUICHE_QUIC_CORE_QUIC_SESSION_H_
#define QUICHE_QUIC_CORE_QUIC_SESSION_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "net/third_party/quiche/src/quic/core/handshaker_delegate_interface.h"
#include "net/third_party/quiche/src/quic/core/legacy_quic_stream_id_manager.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_control_frame_manager.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_datagram_queue.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_creator.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_stream_frame_data_producer.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/core/quic_write_blocked_list.h"
#include "net/third_party/quiche/src/quic/core/session_notifier_interface.h"
#include "net/third_party/quiche/src/quic/core/stream_delegate_interface.h"
#include "net/third_party/quiche/src/quic/core/uber_quic_stream_id_manager.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_containers.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"

namespace quic {

class QuicCryptoStream;
class QuicFlowController;
class QuicStream;
class QuicStreamIdManager;

namespace test {
class QuicSessionPeer;
}  // namespace test

class QUIC_EXPORT_PRIVATE QuicSession
    : public QuicConnectionVisitorInterface,
      public SessionNotifierInterface,
      public QuicStreamFrameDataProducer,
      public QuicStreamIdManager::DelegateInterface,
      public HandshakerDelegateInterface,
      public StreamDelegateInterface {
 public:
  // An interface from the session to the entity owning the session.
  // This lets the session notify its owner (the Dispatcher) when the connection
  // is closed, blocked, or added/removed from the time-wait list.
  class QUIC_EXPORT_PRIVATE Visitor {
   public:
    virtual ~Visitor() {}

    // Called when the connection is closed after the streams have been closed.
    virtual void OnConnectionClosed(QuicConnectionId server_connection_id,
                                    QuicErrorCode error,
                                    const std::string& error_details,
                                    ConnectionCloseSource source) = 0;

    // Called when the session has become write blocked.
    virtual void OnWriteBlocked(QuicBlockedWriterInterface* blocked_writer) = 0;

    // Called when the session receives reset on a stream from the peer.
    virtual void OnRstStreamReceived(const QuicRstStreamFrame& frame) = 0;

    // Called when the session receives a STOP_SENDING for a stream from the
    // peer.
    virtual void OnStopSendingReceived(const QuicStopSendingFrame& frame) = 0;
  };

  // Does not take ownership of |connection| or |visitor|.
  QuicSession(QuicConnection* connection,
              Visitor* owner,
              const QuicConfig& config,
              const ParsedQuicVersionVector& supported_versions,
              QuicStreamCount num_expected_unidirectional_static_streams);
  QuicSession(const QuicSession&) = delete;
  QuicSession& operator=(const QuicSession&) = delete;

  ~QuicSession() override;

  virtual void Initialize();

  // QuicConnectionVisitorInterface methods:
  void OnStreamFrame(const QuicStreamFrame& frame) override;
  void OnCryptoFrame(const QuicCryptoFrame& frame) override;
  void OnRstStream(const QuicRstStreamFrame& frame) override;
  void OnGoAway(const QuicGoAwayFrame& frame) override;
  void OnMessageReceived(quiche::QuicheStringPiece message) override;
  void OnHandshakeDoneReceived() override;
  void OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override;
  void OnBlockedFrame(const QuicBlockedFrame& frame) override;
  void OnConnectionClosed(const QuicConnectionCloseFrame& frame,
                          ConnectionCloseSource source) override;
  void OnWriteBlocked() override;
  void OnSuccessfulVersionNegotiation(
      const ParsedQuicVersion& version) override;
  void OnPacketReceived(const QuicSocketAddress& self_address,
                        const QuicSocketAddress& peer_address,
                        bool is_connectivity_probe) override;
  void OnCanWrite() override;
  bool SendProbingData() override;
  void OnCongestionWindowChange(QuicTime /*now*/) override {}
  void OnConnectionMigration(AddressChangeType /*type*/) override {}
  // Adds a connection level WINDOW_UPDATE frame.
  void OnAckNeedsRetransmittableFrame() override;
  void SendPing() override;
  bool WillingAndAbleToWrite() const override;
  bool HasPendingHandshake() const override;
  void OnPathDegrading() override;
  bool AllowSelfAddressChange() const override;
  HandshakeState GetHandshakeState() const override;
  void OnForwardProgressConfirmed() override;
  bool OnMaxStreamsFrame(const QuicMaxStreamsFrame& frame) override;
  bool OnStreamsBlockedFrame(const QuicStreamsBlockedFrame& frame) override;
  void OnStopSendingFrame(const QuicStopSendingFrame& frame) override;
  void OnPacketDecrypted(EncryptionLevel level) override;
  void OnOneRttPacketAcknowledged() override;
  void OnHandshakePacketSent() override;

  // QuicStreamFrameDataProducer
  WriteStreamDataResult WriteStreamData(QuicStreamId id,
                                        QuicStreamOffset offset,
                                        QuicByteCount data_length,
                                        QuicDataWriter* writer) override;
  bool WriteCryptoData(EncryptionLevel level,
                       QuicStreamOffset offset,
                       QuicByteCount data_length,
                       QuicDataWriter* writer) override;

  // SessionNotifierInterface methods:
  bool OnFrameAcked(const QuicFrame& frame,
                    QuicTime::Delta ack_delay_time,
                    QuicTime receive_timestamp) override;
  void OnStreamFrameRetransmitted(const QuicStreamFrame& frame) override;
  void OnFrameLost(const QuicFrame& frame) override;
  void RetransmitFrames(const QuicFrames& frames,
                        TransmissionType type) override;
  bool IsFrameOutstanding(const QuicFrame& frame) const override;
  bool HasUnackedCryptoData() const override;
  bool HasUnackedStreamData() const override;

  void SendMaxStreams(QuicStreamCount stream_count,
                      bool unidirectional) override;
  // The default implementation does nothing. Subclasses should override if
  // for example they queue up stream requests.
  virtual void OnCanCreateNewOutgoingStream(bool /*unidirectional*/) {}

  // Called on every incoming packet. Passes |packet| through to |connection_|.
  virtual void ProcessUdpPacket(const QuicSocketAddress& self_address,
                                const QuicSocketAddress& peer_address,
                                const QuicReceivedPacket& packet);

  // Called by application to send |message|. Data copy can be avoided if
  // |message| is provided in reference counted memory.
  // Please note, |message| provided in reference counted memory would be moved
  // internally when message is successfully sent. Thereafter, it would be
  // undefined behavior if callers try to access the slices through their own
  // copy of the span object.
  // Returns the message result which includes the message status and message ID
  // (valid if the write succeeds). SendMessage flushes a message packet even it
  // is not full. If the application wants to bundle other data in the same
  // packet, please consider adding a packet flusher around the SendMessage
  // and/or WritevData calls.
  //
  // OnMessageAcked and OnMessageLost are called when a particular message gets
  // acked or lost.
  //
  // Note that SendMessage will fail with status = MESSAGE_STATUS_BLOCKED
  // if connection is congestion control blocked or underlying socket is write
  // blocked. In this case the caller can retry sending message again when
  // connection becomes available, for example after getting OnCanWrite()
  // callback.
  MessageResult SendMessage(QuicMemSliceSpan message);

  // Same as above SendMessage, except caller can specify if the given |message|
  // should be flushed even if the underlying connection is deemed unwritable.
  MessageResult SendMessage(QuicMemSliceSpan message, bool flush);

  // Called when message with |message_id| gets acked.
  virtual void OnMessageAcked(QuicMessageId message_id,
                              QuicTime receive_timestamp);

  // Called when message with |message_id| is considered as lost.
  virtual void OnMessageLost(QuicMessageId message_id);

  // Called by control frame manager when it wants to write control frames to
  // the peer. Returns true if |frame| is consumed, false otherwise. The frame
  // will be sent in specified transmission |type|.
  bool WriteControlFrame(const QuicFrame& frame, TransmissionType type);

  // Called by stream to send RST_STREAM (and STOP_SENDING).
  virtual void SendRstStream(QuicStreamId id,
                             QuicRstStreamErrorCode error,
                             QuicStreamOffset bytes_written);

  // Called to send RST_STREAM (and STOP_SENDING) and close stream. If stream
  // |id| does not exist, just send RST_STREAM (and STOP_SENDING).
  virtual void ResetStream(QuicStreamId id,
                           QuicRstStreamErrorCode error,
                           QuicStreamOffset bytes_written);

  // Called when the session wants to go away and not accept any new streams.
  virtual void SendGoAway(QuicErrorCode error_code, const std::string& reason);

  // Sends a BLOCKED frame.
  virtual void SendBlocked(QuicStreamId id);

  // Sends a WINDOW_UPDATE frame.
  virtual void SendWindowUpdate(QuicStreamId id, QuicStreamOffset byte_offset);

  // Create and transmit a STOP_SENDING frame
  virtual void SendStopSending(uint16_t code, QuicStreamId stream_id);

  // Close stream |stream_id|. Whether sending RST_STREAM (and STOP_SENDING)
  // depends on the sending and receiving states.
  // TODO(fayang): Deprecate CloseStream, instead always use ResetStream to
  // close a stream from session.
  virtual void CloseStream(QuicStreamId stream_id);

  // Called by stream |stream_id| when it gets closed.
  virtual void OnStreamClosed(QuicStreamId stream_id);

  // Returns true if outgoing packets will be encrypted, even if the server
  // hasn't confirmed the handshake yet.
  virtual bool IsEncryptionEstablished() const;

  // Returns true if 1RTT keys are available.
  bool OneRttKeysAvailable() const;

  // Called by the QuicCryptoStream when a new QuicConfig has been negotiated.
  virtual void OnConfigNegotiated();

  // From HandshakerDelegateInterface
  bool OnNewDecryptionKeyAvailable(EncryptionLevel level,
                                   std::unique_ptr<QuicDecrypter> decrypter,
                                   bool set_alternative_decrypter,
                                   bool latch_once_used) override;
  void OnNewEncryptionKeyAvailable(
      EncryptionLevel level,
      std::unique_ptr<QuicEncrypter> encrypter) override;
  void SetDefaultEncryptionLevel(EncryptionLevel level) override;
  void OnOneRttKeysAvailable() override;
  void DiscardOldDecryptionKey(EncryptionLevel level) override;
  void DiscardOldEncryptionKey(EncryptionLevel level) override;
  void NeuterUnencryptedData() override;
  void NeuterHandshakeData() override;

  // Implement StreamDelegateInterface.
  void OnStreamError(QuicErrorCode error_code,
                     std::string error_details) override;
  // Sets priority in the write blocked list.
  void RegisterStreamPriority(
      QuicStreamId id,
      bool is_static,
      const spdy::SpdyStreamPrecedence& precedence) override;
  // Clears priority from the write blocked list.
  void UnregisterStreamPriority(QuicStreamId id, bool is_static) override;
  // Updates priority on the write blocked list.
  void UpdateStreamPriority(
      QuicStreamId id,
      const spdy::SpdyStreamPrecedence& new_precedence) override;

  // Called by streams when they want to write data to the peer.
  // Returns a pair with the number of bytes consumed from data, and a boolean
  // indicating if the fin bit was consumed.  This does not indicate the data
  // has been sent on the wire: it may have been turned into a packet and queued
  // if the socket was unexpectedly blocked.
  QuicConsumedData WritevData(
      QuicStreamId id,
      size_t write_length,
      QuicStreamOffset offset,
      StreamSendingState state,
      TransmissionType type,
      quiche::QuicheOptional<EncryptionLevel> level) override;

  size_t SendCryptoData(EncryptionLevel level,
                        size_t write_length,
                        QuicStreamOffset offset,
                        TransmissionType type) override;

  // Called by the QuicCryptoStream when a handshake message is sent.
  virtual void OnCryptoHandshakeMessageSent(
      const CryptoHandshakeMessage& message);

  // Called by the QuicCryptoStream when a handshake message is received.
  virtual void OnCryptoHandshakeMessageReceived(
      const CryptoHandshakeMessage& message);

  // Returns mutable config for this session. Returned config is owned
  // by QuicSession.
  QuicConfig* config();

  // Returns true if the stream existed previously and has been closed.
  // Returns false if the stream is still active or if the stream has
  // not yet been created.
  bool IsClosedStream(QuicStreamId id);

  QuicConnection* connection() { return connection_; }
  const QuicConnection* connection() const { return connection_; }
  const QuicSocketAddress& peer_address() const {
    return connection_->peer_address();
  }
  const QuicSocketAddress& self_address() const {
    return connection_->self_address();
  }
  QuicConnectionId connection_id() const {
    return connection_->connection_id();
  }

  // Returns the number of currently open streams, excluding static streams, and
  // never counting unfinished streams.
  size_t GetNumActiveStreams() const;

  // Returns the number of currently draining streams.
  size_t GetNumDrainingStreams() const;

  // Returns the number of currently open peer initiated streams, excluding
  // static streams.
  // TODO(fayang): remove this and instead use
  // LegacyStreamIdManager::num_open_incoming_streams() in tests when
  // deprecating quic_stream_id_manager_handles_accounting.
  size_t GetNumOpenIncomingStreams() const;

  // Returns the number of currently open self initiated streams, excluding
  // static streams.
  // TODO(fayang): remove this and instead use
  // LegacyStreamIdManager::num_open_outgoing_streams() in tests when
  // deprecating quic_stream_id_manager_handles_accounting.
  size_t GetNumOpenOutgoingStreams() const;

  // Returns the number of open peer initiated static streams.
  size_t num_incoming_static_streams() const {
    return num_incoming_static_streams_;
  }

  // Returns the number of open self initiated static streams.
  size_t num_outgoing_static_streams() const {
    return num_outgoing_static_streams_;
  }

  // Add the stream to the session's write-blocked list because it is blocked by
  // connection-level flow control but not by its own stream-level flow control.
  // The stream will be given a chance to write when a connection-level
  // WINDOW_UPDATE arrives.
  virtual void MarkConnectionLevelWriteBlocked(QuicStreamId id);

  // Called when stream |id| is done waiting for acks either because all data
  // gets acked or is not interested in data being acked (which happens when
  // a stream is reset because of an error).
  void OnStreamDoneWaitingForAcks(QuicStreamId id);

  // Called when stream |id| is newly waiting for acks.
  void OnStreamWaitingForAcks(QuicStreamId id);

  // Returns true if the session has data to be sent, either queued in the
  // connection, or in a write-blocked stream.
  bool HasDataToWrite() const;

  // Returns the largest payload that will fit into a single MESSAGE frame.
  // Because overhead can vary during a connection, this method should be
  // checked for every message.
  QuicPacketLength GetCurrentLargestMessagePayload() const;

  // Returns the largest payload that will fit into a single MESSAGE frame at
  // any point during the connection.  This assumes the version and
  // connection ID lengths do not change.
  QuicPacketLength GetGuaranteedLargestMessagePayload() const;

  bool goaway_sent() const { return goaway_sent_; }

  bool goaway_received() const { return goaway_received_; }

  // Returns the Google QUIC error code
  QuicErrorCode error() const { return on_closed_frame_.quic_error_code; }
  const std::string& error_details() const {
    return on_closed_frame_.error_details;
  }
  uint64_t transport_close_frame_type() const {
    return on_closed_frame_.transport_close_frame_type;
  }
  QuicConnectionCloseType close_type() const {
    return on_closed_frame_.close_type;
  }

  Perspective perspective() const { return perspective_; }

  QuicFlowController* flow_controller() { return &flow_controller_; }

  // Returns true if connection is flow controller blocked.
  bool IsConnectionFlowControlBlocked() const;

  // Returns true if any stream is flow controller blocked.
  bool IsStreamFlowControlBlocked();

  size_t max_open_incoming_bidirectional_streams() const;
  size_t max_open_incoming_unidirectional_streams() const;

  size_t MaxAvailableBidirectionalStreams() const;
  size_t MaxAvailableUnidirectionalStreams() const;

  // Returns existing stream with id = |stream_id|. If no
  // such stream exists, and |stream_id| is a peer-created stream id,
  // then a new stream is created and returned. In all other cases, nullptr is
  // returned.
  // Caller does not own the returned stream.
  QuicStream* GetOrCreateStream(const QuicStreamId stream_id);

  // Mark a stream as draining.
  void StreamDraining(QuicStreamId id, bool unidirectional);

  // Returns true if this stream should yield writes to another blocked stream.
  virtual bool ShouldYield(QuicStreamId stream_id);

  // Clean up closed_streams_.
  void CleanUpClosedStreams();

  const ParsedQuicVersionVector& supported_versions() const {
    return supported_versions_;
  }

  QuicStreamId next_outgoing_bidirectional_stream_id() const;
  QuicStreamId next_outgoing_unidirectional_stream_id() const;

  // Return true if given stream is peer initiated.
  bool IsIncomingStream(QuicStreamId id) const;

  size_t GetNumLocallyClosedOutgoingStreamsHighestOffset() const;

  size_t num_locally_closed_incoming_streams_highest_offset() const {
    return num_locally_closed_incoming_streams_highest_offset_;
  }

  // Record errors when a connection is closed at the server side, should only
  // be called from server's perspective.
  // Noop if |error| is QUIC_NO_ERROR.
  static void RecordConnectionCloseAtServer(QuicErrorCode error,
                                            ConnectionCloseSource source);

  inline QuicTransportVersion transport_version() const {
    return connection_->transport_version();
  }

  inline ParsedQuicVersion version() const { return connection_->version(); }

  bool use_http2_priority_write_scheduler() const {
    return use_http2_priority_write_scheduler_;
  }

  bool is_configured() const { return is_configured_; }

  // Called to neuter crypto data of encryption |level|.
  void NeuterCryptoDataOfEncryptionLevel(EncryptionLevel level);

  // Returns the ALPN values to negotiate on this session.
  virtual std::vector<std::string> GetAlpnsToOffer() const {
    // TODO(vasilvv): this currently sets HTTP/3 by default.  Switch all
    // non-HTTP applications to appropriate ALPNs.
    return std::vector<std::string>({AlpnForVersion(connection()->version())});
  }

  // Provided a list of ALPNs offered by the client, selects an ALPN from the
  // list, or alpns.end() if none of the ALPNs are acceptable.
  virtual std::vector<quiche::QuicheStringPiece>::const_iterator SelectAlpn(
      const std::vector<quiche::QuicheStringPiece>& alpns) const;

  // Called when the ALPN of the connection is established for a connection that
  // uses TLS handshake.
  virtual void OnAlpnSelected(quiche::QuicheStringPiece alpn);

  bool deprecate_draining_streams() const {
    return deprecate_draining_streams_;
  }

  bool break_close_loop() const { return break_close_loop_; }

  // Called on clients by the crypto handshaker to provide application state
  // necessary for sending application data in 0-RTT. The state provided here is
  // the same state that was provided to the crypto handshaker in
  // QuicCryptoClientStream::OnApplicationState on a previous connection.
  // Application protocols that require state to be carried over from the
  // previous connection to support 0-RTT data must implement this method to
  // ingest this state. For example, an HTTP/3 QuicSession would implement this
  // function to process the remembered server SETTINGS frame and apply those
  // SETTINGS to 0-RTT data. This function returns true if the application state
  // has been successfully processed, and false if there was an error processing
  // the cached state and the connection should be closed.
  virtual bool SetApplicationState(ApplicationState* /*cached_state*/) {
    return true;
  }

 protected:
  using StreamMap = QuicSmallMap<QuicStreamId, std::unique_ptr<QuicStream>, 10>;

  using PendingStreamMap =
      QuicSmallMap<QuicStreamId, std::unique_ptr<PendingStream>, 10>;

  using ClosedStreams = std::vector<std::unique_ptr<QuicStream>>;

  using ZombieStreamMap =
      QuicSmallMap<QuicStreamId, std::unique_ptr<QuicStream>, 10>;

  // Creates a new stream to handle a peer-initiated stream.
  // Caller does not own the returned stream.
  // Returns nullptr and does error handling if the stream can not be created.
  virtual QuicStream* CreateIncomingStream(QuicStreamId id) = 0;
  virtual QuicStream* CreateIncomingStream(PendingStream* pending) = 0;

  // Return the reserved crypto stream.
  virtual QuicCryptoStream* GetMutableCryptoStream() = 0;

  // Return the reserved crypto stream as a constant pointer.
  virtual const QuicCryptoStream* GetCryptoStream() const = 0;

  // Adds |stream| to the stream map.
  virtual void ActivateStream(std::unique_ptr<QuicStream> stream);

  // Set transmission type of next sending packets.
  void SetTransmissionType(TransmissionType type);

  // Returns the stream ID for a new outgoing bidirectional/unidirectional
  // stream, and increments the underlying counter.
  QuicStreamId GetNextOutgoingBidirectionalStreamId();
  QuicStreamId GetNextOutgoingUnidirectionalStreamId();

  // Indicates whether the next outgoing bidirectional/unidirectional stream ID
  // can be allocated or not. The test for version-99/IETF QUIC is whether it
  // will exceed the maximum-stream-id or not. For non-version-99 (Google) QUIC
  // it checks whether the next stream would exceed the limit on the number of
  // open streams.
  bool CanOpenNextOutgoingBidirectionalStream();
  bool CanOpenNextOutgoingUnidirectionalStream();

  // Returns the number of open dynamic streams.
  uint64_t GetNumOpenDynamicStreams() const;

  // Returns the maximum bidirectional streams parameter sent with the handshake
  // as a transport parameter, or in the most recent MAX_STREAMS frame.
  QuicStreamCount GetAdvertisedMaxIncomingBidirectionalStreams() const;

  // Performs the work required to close |stream_id|.  If |rst_sent| then a
  // Reset Stream frame has already been sent for this stream.
  // TODO(fayang): Remove CloseStreamInner.
  virtual void CloseStreamInner(QuicStreamId stream_id, bool rst_sent);

  // When a stream is closed locally, it may not yet know how many bytes the
  // peer sent on that stream.
  // When this data arrives (via stream frame w. FIN, trailing headers, or RST)
  // this method is called, and correctly updates the connection level flow
  // controller.
  virtual void OnFinalByteOffsetReceived(QuicStreamId id,
                                         QuicStreamOffset final_byte_offset);

  // Returns true if incoming unidirectional streams should be buffered until
  // the first byte of the stream arrives.
  // If a subclass returns true here, it should make sure to implement
  // ProcessPendingStream().
  virtual bool UsesPendingStreams() const { return false; }

  spdy::SpdyPriority GetSpdyPriorityofStream(QuicStreamId stream_id) const {
    return write_blocked_streams_.GetSpdyPriorityofStream(stream_id);
  }

  StreamMap& stream_map() { return stream_map_; }
  const StreamMap& stream_map() const { return stream_map_; }

  const PendingStreamMap& pending_streams() const {
    return pending_stream_map_;
  }

  ClosedStreams* closed_streams() { return &closed_streams_; }

  const ZombieStreamMap& zombie_streams() const { return zombie_streams_; }

  void set_largest_peer_created_stream_id(
      QuicStreamId largest_peer_created_stream_id);

  QuicWriteBlockedList* write_blocked_streams() {
    return &write_blocked_streams_;
  }

  size_t GetNumDynamicOutgoingStreams() const;

  size_t GetNumDrainingOutgoingStreams() const;

  // Returns true if the stream is still active.
  bool IsOpenStream(QuicStreamId id);

  // Returns true if the stream is a static stream.
  bool IsStaticStream(QuicStreamId id) const;

  // Close connection when receive a frame for a locally-created nonexistent
  // stream.
  // Prerequisite: IsClosedStream(stream_id) == false
  // Server session might need to override this method to allow server push
  // stream to be promised before creating an active stream.
  virtual void HandleFrameOnNonexistentOutgoingStream(QuicStreamId stream_id);

  virtual bool MaybeIncreaseLargestPeerStreamId(const QuicStreamId stream_id);

  void InsertLocallyClosedStreamsHighestOffset(const QuicStreamId id,
                                               QuicStreamOffset offset);
  // If stream is a locally closed stream, this RST will update FIN offset.
  // Otherwise stream is a preserved stream and the behavior of it depends on
  // derived class's own implementation.
  virtual void HandleRstOnValidNonexistentStream(
      const QuicRstStreamFrame& frame);

  // Returns a stateless reset token which will be included in the public reset
  // packet.
  virtual QuicUint128 GetStatelessResetToken() const;

  QuicControlFrameManager& control_frame_manager() {
    return control_frame_manager_;
  }

  const LegacyQuicStreamIdManager& stream_id_manager() const {
    return stream_id_manager_;
  }

  QuicDatagramQueue* datagram_queue() { return &datagram_queue_; }

  // Processes the stream type information of |pending| depending on
  // different kinds of sessions' own rules. Returns true if the pending stream
  // is converted into a normal stream.
  virtual bool ProcessPendingStream(PendingStream* /*pending*/) {
    return false;
  }

  // Return the largest peer created stream id depending on directionality
  // indicated by |unidirectional|.
  QuicStreamId GetLargestPeerCreatedStreamId(bool unidirectional) const;

  // Deletes the connection and sets it to nullptr, so calling it mulitiple
  // times is safe.
  void DeleteConnection();

  // Call SetPriority() on stream id |id| and return true if stream is active.
  bool MaybeSetStreamPriority(QuicStreamId stream_id,
                              const spdy::SpdyStreamPrecedence& precedence);

  void SetLossDetectionTuner(
      std::unique_ptr<LossDetectionTunerInterface> tuner) {
    connection()->SetLossDetectionTuner(std::move(tuner));
  }

 private:
  friend class test::QuicSessionPeer;

  // Called in OnConfigNegotiated when we receive a new stream level flow
  // control window in a negotiated config. Closes the connection if invalid.
  void OnNewStreamFlowControlWindow(QuicStreamOffset new_window);

  // Called in OnConfigNegotiated when we receive a new unidirectional stream
  // flow control window in a negotiated config.
  void OnNewStreamUnidirectionalFlowControlWindow(QuicStreamOffset new_window);

  // Called in OnConfigNegotiated when we receive a new outgoing bidirectional
  // stream flow control window in a negotiated config.
  void OnNewStreamOutgoingBidirectionalFlowControlWindow(
      QuicStreamOffset new_window);

  // Called in OnConfigNegotiated when we receive a new incoming bidirectional
  // stream flow control window in a negotiated config.
  void OnNewStreamIncomingBidirectionalFlowControlWindow(
      QuicStreamOffset new_window);

  // Called in OnConfigNegotiated when we receive a new connection level flow
  // control window in a negotiated config. Closes the connection if invalid.
  void OnNewSessionFlowControlWindow(QuicStreamOffset new_window);

  // Debug helper for |OnCanWrite()|, check that OnStreamWrite() makes
  // forward progress.  Returns false if busy loop detected.
  bool CheckStreamNotBusyLooping(QuicStream* stream,
                                 uint64_t previous_bytes_written,
                                 bool previous_fin_sent);

  // Debug helper for OnCanWrite. Check that after QuicStream::OnCanWrite(),
  // if stream has buffered data and is not stream level flow control blocked,
  // it has to be in the write blocked list.
  bool CheckStreamWriteBlocked(QuicStream* stream) const;

  // Called in OnConfigNegotiated for Finch trials to measure performance of
  // starting with larger flow control receive windows.
  void AdjustInitialFlowControlWindows(size_t stream_window);

  // Find stream with |id|, returns nullptr if the stream does not exist or
  // closed.
  QuicStream* GetStream(QuicStreamId id) const;

  PendingStream* GetOrCreatePendingStream(QuicStreamId stream_id);

  // Let streams and control frame managers retransmit lost data, returns true
  // if all lost data is retransmitted. Returns false otherwise.
  bool RetransmitLostData();

  // Closes the pending stream |stream_id| before it has been created.
  void ClosePendingStream(QuicStreamId stream_id);

  // Creates or gets pending stream, feeds it with |frame|, and processes the
  // pending stream.
  void PendingStreamOnStreamFrame(const QuicStreamFrame& frame);

  // Creates or gets pending strea, feed it with |frame|, and closes the pending
  // stream.
  void PendingStreamOnRstStream(const QuicRstStreamFrame& frame);

  // Does actual work of sending RESET_STREAM, if the stream type allows.
  void MaybeSendRstStreamFrame(QuicStreamId id,
                               QuicRstStreamErrorCode error,
                               QuicStreamOffset bytes_written);

  // Sends a STOP_SENDING frame if the stream type allows.
  void MaybeSendStopSendingFrame(QuicStreamId id, QuicRstStreamErrorCode error);

  // Keep track of highest received byte offset of locally closed streams, while
  // waiting for a definitive final highest offset from the peer.
  std::map<QuicStreamId, QuicStreamOffset>
      locally_closed_streams_highest_offset_;

  QuicConnection* connection_;

  // Store perspective on QuicSession during the constructor as it may be needed
  // during our destructor when connection_ may have already been destroyed.
  Perspective perspective_;

  // May be null.
  Visitor* visitor_;

  // A list of streams which need to write more data.  Stream register
  // themselves in their constructor, and unregisterm themselves in their
  // destructors, so the write blocked list must outlive all streams.
  QuicWriteBlockedList write_blocked_streams_;

  ClosedStreams closed_streams_;
  // Streams which are closed, but need to be kept alive. Currently, the only
  // reason is the stream's sent data (including FIN) does not get fully acked.
  ZombieStreamMap zombie_streams_;

  QuicConfig config_;

  // Map from StreamId to pointers to streams. Owns the streams.
  StreamMap stream_map_;

  // Map from StreamId to PendingStreams for peer-created unidirectional streams
  // which are waiting for the first byte of payload to arrive.
  PendingStreamMap pending_stream_map_;

  // Set of stream ids that are "draining" -- a FIN has been sent and received,
  // but the stream object still exists because not all the received data has
  // been consumed.
  // TODO(fayang): Remove draining_streams_ when deprecate
  // quic_deprecate_draining_streams.
  QuicHashSet<QuicStreamId> draining_streams_;

  // Set of stream ids that are waiting for acks excluding crypto stream id.
  QuicHashSet<QuicStreamId> streams_waiting_for_acks_;

  // TODO(fayang): Consider moving LegacyQuicStreamIdManager into
  // UberQuicStreamIdManager.
  // Manages stream IDs for Google QUIC.
  LegacyQuicStreamIdManager stream_id_manager_;

  // Manages stream IDs for version99/IETF QUIC
  UberQuicStreamIdManager v99_streamid_manager_;

  // A counter for peer initiated dynamic streams which are in the stream_map_.
  // TODO(fayang): Remove this when deprecating
  // quic_stream_id_manager_handles_accounting.
  size_t num_dynamic_incoming_streams_;

  // A counter for peer initiated streams which have sent and received FIN but
  // waiting for application to consume data.
  // TODO(fayang): Remove this when deprecating
  // quic_stream_id_manager_handles_accounting.
  size_t num_draining_incoming_streams_;

  // A counter for self initiated streams which have sent and received FIN but
  // waiting for application to consume data. Only used when
  // deprecate_draining_streams_ is true.
  // TODO(fayang): Remove this when deprecating
  // quic_stream_id_manager_handles_accounting.
  size_t num_draining_outgoing_streams_;

  // A counter for self initiated static streams which are in
  // stream_map_.
  size_t num_outgoing_static_streams_;

  // A counter for peer initiated static streams which are in
  // stream_map_.
  size_t num_incoming_static_streams_;

  // A counter for peer initiated streams which are in the
  // locally_closed_streams_highest_offset_.
  // TODO(fayang): Remove this when deprecating
  // quic_stream_id_manager_handles_accounting.
  size_t num_locally_closed_incoming_streams_highest_offset_;

  // Received information for a connection close.
  QuicConnectionCloseFrame on_closed_frame_;

  // Used for connection-level flow control.
  QuicFlowController flow_controller_;

  // The stream id which was last popped in OnCanWrite, or 0, if not under the
  // call stack of OnCanWrite.
  QuicStreamId currently_writing_stream_id_;

  // Whether a GoAway has been sent.
  bool goaway_sent_;

  // Whether a GoAway has been received.
  bool goaway_received_;

  QuicControlFrameManager control_frame_manager_;

  // Id of latest successfully sent message.
  QuicMessageId last_message_id_;

  // The buffer used to queue the DATAGRAM frames.
  QuicDatagramQueue datagram_queue_;

  // TODO(fayang): switch to linked_hash_set when chromium supports it. The bool
  // is not used here.
  // List of streams with pending retransmissions.
  QuicLinkedHashMap<QuicStreamId, bool> streams_with_pending_retransmission_;

  // Clean up closed_streams_ when this alarm fires.
  std::unique_ptr<QuicAlarm> closed_streams_clean_up_alarm_;

  // Supported version list used by the crypto handshake only. Please note, this
  // list may be a superset of the connection framer's supported versions.
  ParsedQuicVersionVector supported_versions_;

  // If true, write_blocked_streams_ uses HTTP2 (tree-style) priority write
  // scheduler.
  bool use_http2_priority_write_scheduler_;

  // Initialized to false. Set to true when the session has been properly
  // configured and is ready for general operation.
  bool is_configured_;

  // If true, enables round robin scheduling.
  bool enable_round_robin_scheduling_;

  // Latched value of quic_deprecate_draining_streams.
  const bool deprecate_draining_streams_;

  // Latched value of quic_break_session_stream_close_loop.
  const bool break_close_loop_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QUIC_SESSION_H_
