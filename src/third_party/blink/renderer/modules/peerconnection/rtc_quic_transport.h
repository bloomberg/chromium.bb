// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_parameters.h"

namespace blink {

class DOMArrayBuffer;
class ExceptionState;
class RTCCertificate;
class RTCQuicStream;
class P2PQuicTransportFactory;

enum class RTCQuicTransportState {
  kNew,
  kConnecting,
  kConnected,
  kClosed,
  kFailed
};

// The RTCQuicTransport does not need to be ActiveScriptWrappable since the
// RTCIceTransport to which it is attached holds a strong reference to it as
// long as it is alive.
class MODULES_EXPORT RTCQuicTransport final
    : public EventTargetWithInlineData,
      public ContextClient,
      public QuicTransportProxy::Delegate {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RTCQuicTransport);

 public:
  enum class CloseReason {
    // stop() was called.
    kLocalStopped,
    // The remote side closed the QUIC connection.
    kRemoteStopped,
    // The QUIC connection failed.
    kFailed,
    // The RTCIceTransport was closed.
    kIceTransportClosed,
    // The ExecutionContext is being destroyed.
    kContextDestroyed,
  };

  static RTCQuicTransport* Create(ExecutionContext* context,
                                  RTCIceTransport* transport,
                                  ExceptionState& exception_state);
  static RTCQuicTransport* Create(
      ExecutionContext* context,
      RTCIceTransport* transport,
      const HeapVector<Member<RTCCertificate>>& certificates,
      ExceptionState& exception_state);
  static RTCQuicTransport* Create(
      ExecutionContext* context,
      RTCIceTransport* transport,
      const HeapVector<Member<RTCCertificate>>& certificates,
      ExceptionState& exception_state,
      std::unique_ptr<P2PQuicTransportFactory> p2p_quic_transport_factory);

  RTCQuicTransport(
      ExecutionContext* context,
      RTCIceTransport* transport,
      DOMArrayBuffer* key,
      const HeapVector<Member<RTCCertificate>>& certificates,
      ExceptionState& exception_state,
      std::unique_ptr<P2PQuicTransportFactory> p2p_quic_transport_factory);
  ~RTCQuicTransport() override;

  // Called by the RTCIceTransport when it is being closed.
  void OnIceTransportClosed(RTCIceTransport::CloseReason reason);

  // Called by the RTCIceTransport when its start() method is called.
  void OnIceTransportStarted();

  RTCQuicStream* AddStream(QuicStreamProxy* stream_proxy);
  void RemoveStream(RTCQuicStream* stream);

  // https://w3c.github.io/webrtc-quic/#quic-transport*
  RTCIceTransport* transport() const;

  // The pre shared key to be used in the QUIC handshake with connect().
  // This should be signaled to the remote endpoint and used with the remote
  // endpoint's listen() function to begin a connection.
  DOMArrayBuffer* getKey() const;

  String state() const;
  // Note: The listen/connect functions encourage an API user to connect()
  // before the remote endpoint has called listen(), which can result in the
  // CHLO being sent before the server side is ready. Although the CHLO is
  // cached by the RTCIceTransport (if it is connected), the API user is
  // encouraged to not connect() until the remote endpoint has called listen().
  // An API design with the server side generating the pre shared key would
  // enforce this, but we purposely constrained the client side to generate the
  // key to enforce that a browser endpoint is generating the key in the case of
  // communicating with a non-browser, server side endpoint.
  //
  // Begins the QUIC handshake as a client endpoint, using the internal |key_|
  // as the pre shared key for the QUIC handshake.
  void connect(ExceptionState& exception_state);
  // Begins listening for the QUIC handshake as a server endpoint. Uses
  // the |remote_key| from the remote side as a pre shared key in the QUIC
  // handshake.
  void listen(const DOMArrayPiece& remote_key, ExceptionState& exception_state);
  // The following APIs that include certificates/parameters (including start())
  // are not used (or exposed to JavaScript) until QUIC supports both side
  // certificate verification.
  RTCQuicParameters* getLocalParameters() const;
  RTCQuicParameters* getRemoteParameters() const;
  const HeapVector<Member<RTCCertificate>>& getCertificates() const;
  const HeapVector<Member<DOMArrayBuffer>>& getRemoteCertificates() const;
  void start(RTCQuicParameters* remote_parameters,
             ExceptionState& exception_state);

  void stop();
  RTCQuicStream* createStream(ExceptionState& exception_state);
  // Resolves the promise with an RTCQuicTransportStats dictionary.
  ScriptPromise getStats(ScriptState* script_state,
                         ExceptionState& exception_state);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange, kStatechange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(quicstream, kQuicstream)

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // For garbage collection.
  void Trace(blink::Visitor* visitor) override;

 private:
  enum class StartReason {
    // listen() was called with the remote key.
    kServerListening,
    // connect() was called.
    kClientConnecting,
    // start() was called with the remote fingerprints.
    // Note that start() is not currently exposed to JavaScript.
    kP2PWithRemoteFingerprints,
    // Initial default state.
    kNotStarted,
  };

  // QuicTransportProxy::Delegate overrides;
  void OnConnected() override;
  void OnConnectionFailed(const std::string& error_details,
                          bool from_remote) override;
  void OnRemoteStopped() override;
  void OnStream(QuicStreamProxy* stream_proxy) override;
  void OnStats(uint32_t request_id,
               const P2PQuicTransportStats& stats) override;

  // Starts the underlying QUIC connection, by creating the underlying QUIC
  // transport objects and starting the QUIC handshake.
  void StartConnection(quic::Perspective role,
                       P2PQuicTransport::StartConfig start_config);

  // Permenantly closes the RTCQuicTransport with the given reason.
  // The RTCQuicTransport must not already be closed or failed.
  // This will transition the state to either closed or failed according to the
  // reason.
  void Close(CloseReason reason);

  bool IsClosed() const { return state_ == RTCQuicTransportState::kClosed; }
  // The transport is no longer usable once it has reached the "failed" or
  // "closed" state.
  bool IsDisposed() const {
    return (state_ == RTCQuicTransportState::kClosed ||
            state_ == RTCQuicTransportState::kFailed);
  }
  bool RaiseExceptionIfClosed(ExceptionState& exception_state) const;
  bool RaiseExceptionIfStarted(ExceptionState& exception_state) const;
  void RejectPendingStatsPromises();

  Member<RTCIceTransport> transport_;
  RTCQuicTransportState state_ = RTCQuicTransportState::kNew;
  StartReason start_reason_ = StartReason::kNotStarted;
  // The pre shared key to be used in the QUIC handshake. It is used with
  // connect() on the local side, and listen() on the remote side.
  Member<DOMArrayBuffer> key_;
  // The certificates/parameters are not being used until the QUIC library
  // supports both side certificate verification in the crypto handshake.
  HeapVector<Member<RTCCertificate>> certificates_;
  HeapVector<Member<DOMArrayBuffer>> remote_certificates_;
  Member<RTCQuicParameters> remote_parameters_;
  std::unique_ptr<P2PQuicTransportFactory> p2p_quic_transport_factory_;
  std::unique_ptr<QuicTransportProxy> proxy_;
  HeapHashSet<Member<RTCQuicStream>> streams_;
  // Maps from the ID of the stats request to the promise to be resolved.
  HeapHashMap<uint32_t, Member<ScriptPromiseResolver>> stats_promise_map_;
  uint32_t get_stats_id_counter_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_
