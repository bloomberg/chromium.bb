// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the RTCQuicTransport Blink bindings, QuicTransportProxy and
// QuicTransportHost by mocking out the underlying P2PQuicTransport.
// Everything is run on a single thread but with separate TestSimpleTaskRunners
// for the main thread / worker thread.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport_test.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_gather_options.h"
#include "third_party/webrtc/rtc_base/rtc_certificate_generator.h"

namespace blink {
namespace {

using testing::_;
using testing::Assign;
using testing::ElementsAre;
using testing::Invoke;
using testing::Mock;

HeapVector<Member<RTCCertificate>> GenerateLocalRTCCertificates() {
  HeapVector<Member<RTCCertificate>> certificates;
  certificates.push_back(MakeGarbageCollected<RTCCertificate>(
      rtc::RTCCertificateGenerator::GenerateCertificate(rtc::KeyParams::ECDSA(),
                                                        absl::nullopt)));
  return certificates;
}

constexpr char kRemoteFingerprintAlgorithm1[] = "sha-256";
constexpr char kRemoteFingerprintValue1[] =
    "8E:57:5F:8E:65:D2:83:7B:05:97:BB:72:DE:09:DE:03:BD:95:9B:A0:03:10:50:82:"
    "5E:73:38:16:4C:E0:C5:84";
const size_t kKeyLength = 16;
const uint8_t kKey[kKeyLength] = {0, 1, 2,  3,  4,  5,  6,  7,
                                  8, 9, 10, 11, 12, 13, 14, 15};

RTCDtlsFingerprint* CreateRemoteFingerprint1() {
  RTCDtlsFingerprint* dtls_fingerprint = RTCDtlsFingerprint::Create();
  dtls_fingerprint->setAlgorithm(kRemoteFingerprintAlgorithm1);
  dtls_fingerprint->setValue(kRemoteFingerprintValue1);
  return dtls_fingerprint;
}

RTCQuicParameters* CreateRemoteRTCQuicParameters1() {
  HeapVector<Member<RTCDtlsFingerprint>> fingerprints;
  fingerprints.push_back(CreateRemoteFingerprint1());
  RTCQuicParameters* quic_parameters = RTCQuicParameters::Create();
  quic_parameters->setFingerprints(fingerprints);
  return quic_parameters;
}

}  // namespace

RTCQuicTransport* RTCQuicTransportTest::CreateQuicTransport(
    V8TestingScope& scope,
    RTCIceTransport* ice_transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    std::unique_ptr<MockP2PQuicTransport> mock_transport,
    P2PQuicTransport::Delegate** delegate_out) {
  return CreateQuicTransport(scope, ice_transport, certificates,
                             std::make_unique<MockP2PQuicTransportFactory>(
                                 std::move(mock_transport), delegate_out));
}

RTCQuicTransport* RTCQuicTransportTest::CreateQuicTransport(
    V8TestingScope& scope,
    RTCIceTransport* ice_transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    std::unique_ptr<MockP2PQuicTransportFactory> mock_factory) {
  return RTCQuicTransport::Create(scope.GetExecutionContext(), ice_transport,
                                  certificates, ASSERT_NO_EXCEPTION,
                                  std::move(mock_factory));
}

RTCQuicTransport* RTCQuicTransportTest::CreateConnectedQuicTransport(
    V8TestingScope& scope,
    P2PQuicTransport::Delegate** delegate_out) {
  return CreateConnectedQuicTransport(
      scope, std::make_unique<MockP2PQuicTransport>(), delegate_out);
}

RTCQuicTransport* RTCQuicTransportTest::CreateConnectedQuicTransport(
    V8TestingScope& scope,
    std::unique_ptr<MockP2PQuicTransport> mock_transport,
    P2PQuicTransport::Delegate** delegate_out) {
  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport), &delegate);
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  DCHECK(delegate);
  delegate->OnConnected();
  RunUntilIdle();
  DCHECK_EQ("connected", quic_transport->state());
  if (delegate_out) {
    *delegate_out = delegate;
  }
  return quic_transport;
}

// Test that calling start() creates a P2PQuicTransport with the correct
// P2PQuicTransportConfig. The config should have:
// 1. The P2PQuicPacketTransport returned by the MockIceTransportAdapter.
// 2. Server mode configured since the ICE role is 'controlling'.
// 3. The certificates passed in the RTCQuicTransport constructor.
TEST_F(RTCQuicTransportTest, P2PQuicTransportConstructedByStart) {
  V8TestingScope scope;

  auto quic_packet_transport = std::make_unique<MockP2PQuicPacketTransport>();
  auto* quic_packet_transport_ptr = quic_packet_transport.get();
  auto ice_transport_adapter_mock = std::make_unique<MockIceTransportAdapter>(
      std::move(quic_packet_transport));
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(ice_transport_adapter_mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificateGenerator::GenerateCertificate(rtc::KeyParams::ECDSA(),
                                                        absl::nullopt);
  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>();
  EXPECT_CALL(*mock_factory, CreateQuicTransport(_, _, _))
      .WillOnce(Invoke([quic_packet_transport_ptr, certificate](
                           P2PQuicTransport::Delegate* delegate,
                           P2PQuicPacketTransport* packet_transport,
                           const P2PQuicTransportConfig& config) {
        EXPECT_EQ(quic_packet_transport_ptr, packet_transport);
        EXPECT_EQ(quic::Perspective::IS_SERVER, config.perspective);
        EXPECT_THAT(config.certificates, ElementsAre(certificate));
        return std::make_unique<MockP2PQuicTransport>();
      }));
  HeapVector<Member<RTCCertificate>> certificates;
  certificates.push_back(MakeGarbageCollected<RTCCertificate>(certificate));
  Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
      scope, ice_transport, certificates, std::move(mock_factory));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
}

// Test that calling connect() creates a P2PQuicTransport with the correct
// P2PQuicTransportConfig. The config should have:
// 1. The P2PQuicPacketTransport returned by the MockIceTransportAdapter.
// 2. Client mode configured.
TEST_F(RTCQuicTransportTest, P2PQuicTransportConstructedByConnect) {
  V8TestingScope scope;

  auto quic_packet_transport = std::make_unique<MockP2PQuicPacketTransport>();
  auto* quic_packet_transport_ptr = quic_packet_transport.get();
  auto ice_transport_adapter_mock = std::make_unique<MockIceTransportAdapter>(
      std::move(quic_packet_transport));
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(ice_transport_adapter_mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>();
  EXPECT_CALL(*mock_factory, CreateQuicTransport(_, _, _))
      .WillOnce(Invoke(
          [quic_packet_transport_ptr](P2PQuicTransport::Delegate* delegate,
                                      P2PQuicPacketTransport* packet_transport,
                                      const P2PQuicTransportConfig& config) {
            EXPECT_EQ(quic_packet_transport_ptr, packet_transport);
            EXPECT_EQ(quic::Perspective::IS_CLIENT, config.perspective);
            return std::make_unique<MockP2PQuicTransport>();
          }));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_factory));
  quic_transport->connect(ASSERT_NO_EXCEPTION);
}

// Test that calling listen() creates a P2PQuicTransport with the correct
// P2PQuicTransportConfig. The config should have:
// 1. The P2PQuicPacketTransport returned by the MockIceTransportAdapter.
// 2. Server mode configured.
TEST_F(RTCQuicTransportTest, P2PQuicTransportConstructedByListen) {
  V8TestingScope scope;

  auto quic_packet_transport = std::make_unique<MockP2PQuicPacketTransport>();
  auto* quic_packet_transport_ptr = quic_packet_transport.get();
  auto ice_transport_adapter_mock = std::make_unique<MockIceTransportAdapter>(
      std::move(quic_packet_transport));
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(ice_transport_adapter_mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>();
  EXPECT_CALL(*mock_factory, CreateQuicTransport(_, _, _))
      .WillOnce(Invoke(
          [quic_packet_transport_ptr](P2PQuicTransport::Delegate* delegate,
                                      P2PQuicPacketTransport* packet_transport,
                                      const P2PQuicTransportConfig& config) {
            EXPECT_EQ(quic_packet_transport_ptr, packet_transport);
            EXPECT_EQ(quic::Perspective::IS_SERVER, config.perspective);
            return std::make_unique<MockP2PQuicTransport>();
          }));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_factory));
  quic_transport->listen(DOMArrayBuffer::Create(kKey, kKeyLength),
                         ASSERT_NO_EXCEPTION);
}

// Test that calling start() creates a P2PQuicTransport with client perspective
// if the RTCIceTransport role is 'controlled'.
TEST_F(RTCQuicTransportTest, P2PQuicTransportConstructedByStartClient) {
  V8TestingScope scope;

  auto ice_transport_adapter_mock = std::make_unique<MockIceTransportAdapter>(
      std::make_unique<MockP2PQuicPacketTransport>());
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(ice_transport_adapter_mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlled",
                       ASSERT_NO_EXCEPTION);

  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>(
      std::make_unique<MockP2PQuicTransport>());
  EXPECT_CALL(*mock_factory, CreateQuicTransport(_, _, _))
      .WillOnce(Invoke([](P2PQuicTransport::Delegate* delegate,
                          P2PQuicPacketTransport* packet_transport,
                          const P2PQuicTransportConfig& config) {
        EXPECT_EQ(quic::Perspective::IS_CLIENT, config.perspective);
        return std::make_unique<MockP2PQuicTransport>();
      }));
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_factory));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
}

// Test that calling start() calls Start() on the P2PQuicTransport with the
// correct remote fingerprints.
TEST_F(RTCQuicTransportTest, StartPassesRemoteFingerprints) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, MockStart(_))
      .WillOnce(Invoke([](const P2PQuicTransport::StartConfig& config) {
        ASSERT_EQ(1u, config.remote_fingerprints.size());
        EXPECT_EQ(kRemoteFingerprintAlgorithm1,
                  config.remote_fingerprints[0]->algorithm);
        EXPECT_EQ(kRemoteFingerprintValue1,
                  config.remote_fingerprints[0]->GetRfc4572Fingerprint());
      }));
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
}

// Test that calling start() with a started RTCIceTransport changes its state to
// connecting.
TEST_F(RTCQuicTransportTest, StartWithConnectedTransportChangesToConnecting) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ("connecting", quic_transport->state());
}

// Test that calling start() changes its state to connecting once
// RTCIceTransport starts.
TEST_F(RTCQuicTransportTest, StartChangesToConnectingWhenIceStarts) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ("new", quic_transport->state());

  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  EXPECT_EQ("connecting", quic_transport->state());
}

// Test that calling start() twice throws a kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartTwiceThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);

  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling start() after connect() throws a kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartAfterConnectThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->connect(ASSERT_NO_EXCEPTION);

  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling start() after listen() throws a kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartAfterListenThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->listen(DOMArrayBuffer::Create(kKey, kKeyLength),
                         ASSERT_NO_EXCEPTION);

  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling start() after stop() throws a kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartAfterStopThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));

  quic_transport->stop();
  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling start() after RTCIceTransport::stop() throws a
// kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartAfterIceStopsThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));

  ice_transport->stop();
  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling connect() calls Start() on the P2PQuicTransport with the
// generated pre shared key from the local side.
TEST_F(RTCQuicTransportTest, ConnectPassesPreSharedKey) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  auto* mock_transport_ptr = mock_transport.get();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_transport));
  DOMArrayBuffer* key = quic_transport->getKey();
  std::string pre_shared_key(static_cast<const char*>(key->Data()),
                             key->ByteLength());

  EXPECT_CALL(*mock_transport_ptr, MockStart(_))
      .WillOnce(
          Invoke([pre_shared_key](const P2PQuicTransport::StartConfig& config) {
            EXPECT_EQ(pre_shared_key, config.pre_shared_key);
          }));
  quic_transport->connect(ASSERT_NO_EXCEPTION);
}

// Test that calling listen() calls Start() on the P2PQuicTransport with the
// correct given pre shared key from the remote side.
TEST_F(RTCQuicTransportTest, ListenPassesPreSharedKey) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  auto* mock_transport_ptr = mock_transport.get();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_transport));

  std::string pre_shared_key = "foobar";
  EXPECT_CALL(*mock_transport_ptr, MockStart(_))
      .WillOnce(
          Invoke([pre_shared_key](const P2PQuicTransport::StartConfig& config) {
            EXPECT_EQ(pre_shared_key, config.pre_shared_key);
          }));

  quic_transport->listen(
      DOMArrayBuffer::Create(pre_shared_key.c_str(), pre_shared_key.length()),
      ASSERT_NO_EXCEPTION);
}

// Test that calling stop() deletes the underlying P2PQuicTransport.
TEST_F(RTCQuicTransportTest, StopCallsStopThenDeletesQuicTransportAdapter) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  bool mock_deleted = false;
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, Stop()).Times(1);
  EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);

  quic_transport->stop();
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that calling stop() on the underlying RTCIceTransport deletes the
// underlying P2PQuicTransport.
TEST_F(RTCQuicTransportTest, RTCIceTransportStopDeletesP2PQuicTransport) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  bool mock_deleted = false;
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);

  ice_transport->stop();
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that the P2PQuicTransport is deleted and the RTCQuicTransport goes to
// the "failed" state when the QUIC connection fails.
TEST_F(RTCQuicTransportTest,
       ConnectionFailedBecomesClosedAndDeletesP2PQuicTransport) {
  V8TestingScope scope;

  bool mock_deleted = false;
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(mock_transport), &delegate);
  DCHECK(delegate);
  delegate->OnConnectionFailed("test_failure", /*from_remote=*/false);
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
  EXPECT_EQ("failed", quic_transport->state());
}

// Test that after the connection fails, stop() will change the state
// of the transport to "closed".
TEST_F(RTCQuicTransportTest, StopAfterConnectionFailed) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport = CreateConnectedQuicTransport(
      scope, std::make_unique<MockP2PQuicTransport>(), &delegate);
  DCHECK(delegate);
  delegate->OnConnectionFailed("test_failure", /*from_remote=*/false);
  RunUntilIdle();

  EXPECT_EQ("failed", quic_transport->state());

  quic_transport->stop();
  EXPECT_EQ("closed", quic_transport->state());
}

// Test that the P2PQuicTransport is deleted when the underlying RTCIceTransport
// is ContextDestroyed.
TEST_F(RTCQuicTransportTest,
       RTCIceTransportContextDestroyedDeletesP2PQuicTransport) {
  bool mock_deleted = false;
  {
    V8TestingScope scope;

    Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
    ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                         ASSERT_NO_EXCEPTION);

    auto mock_transport = std::make_unique<MockP2PQuicTransport>();
    EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

    Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
        scope, ice_transport, GenerateLocalRTCCertificates(),
        std::move(mock_transport));
    quic_transport->start(CreateRemoteRTCQuicParameters1(),
                          ASSERT_NO_EXCEPTION);
  }  // ContextDestroyed when V8TestingScope goes out of scope.

  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that the certificate passed to RTCQuicTransport is the same
// returned by getCertificates().
TEST_F(RTCQuicTransportTest, GetCertificatesReturnsGivenCertificates) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  auto certificates = GenerateLocalRTCCertificates();
  Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
      scope, ice_transport, certificates, std::move(mock_transport));
  auto returned_certificates = quic_transport->getCertificates();

  EXPECT_EQ(certificates[0], returned_certificates[0]);
}

// Test that the fingerprint returned by getLocalParameters() is
// the fingerprint of the certificate passed to the RTCQuicTransport.
TEST_F(RTCQuicTransportTest,
       GetLocalParametersReturnsGivenCertificatesFingerprints) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  auto certificates = GenerateLocalRTCCertificates();
  auto fingerprints = certificates[0]->getFingerprints();
  Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
      scope, ice_transport, certificates, std::move(mock_transport));
  auto returned_fingerprints =
      quic_transport->getLocalParameters()->fingerprints();

  EXPECT_EQ(1u, returned_fingerprints.size());
  EXPECT_EQ(fingerprints.size(), returned_fingerprints.size());
  EXPECT_EQ(fingerprints[0]->value(), returned_fingerprints[0]->value());
  EXPECT_EQ(fingerprints[0]->algorithm(),
            returned_fingerprints[0]->algorithm());
}

TEST_F(RTCQuicTransportTest, ExpiredCertificateThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>();
  HeapVector<Member<RTCCertificate>> certificates;
  certificates.push_back(MakeGarbageCollected<RTCCertificate>(
      rtc::RTCCertificateGenerator::GenerateCertificate(rtc::KeyParams::ECDSA(),
                                                        /*expires_ms=*/0)));
  RTCQuicTransport::Create(scope.GetExecutionContext(), ice_transport,
                           certificates, scope.GetExceptionState(),
                           std::move(mock_factory));
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that the key returned has at least 128 bits of entropy as required by
// QUIC.
TEST_F(RTCQuicTransportTest, GetKeyReturnsValidKey) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_transport));
  auto* key = quic_transport->getKey();

  EXPECT_GE(key->ByteLength(), 16u);
}

}  // namespace blink
