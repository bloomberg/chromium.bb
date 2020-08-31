// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_ATTESTATION_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_ATTESTATION_DBUS_CONSTANTS_H_

namespace attestation {

constexpr char kAttestationInterface[] = "org.chromium.Attestation";
constexpr char kAttestationServicePath[] = "/org/chromium/Attestation";
constexpr char kAttestationServiceName[] = "org.chromium.Attestation";

// Methods exported by attestation.
constexpr char kCreateGoogleAttestedKey[] = "CreateGoogleAttestedKey";
constexpr char kGetKeyInfo[] = "GetKeyInfo";
constexpr char kGetEndorsementInfo[] = "GetEndorsementInfo";
constexpr char kGetAttestationKeyInfo[] = "GetAttestationKeyInfo";
constexpr char kActivateAttestationKey[] = "ActivateAttestationKey";
constexpr char kCreateCertifiableKey[] = "CreateCertifiableKey";
constexpr char kDecrypt[] = "Decrypt";
constexpr char kSign[] = "Sign";
constexpr char kRegisterKeyWithChapsToken[] = "RegisterKeyWithChapsToken";
constexpr char kGetEnrollmentPreparations[] = "GetEnrollmentPreparations";
constexpr char kGetStatus[] = "GetStatus";
constexpr char kVerify[] = "Verify";
constexpr char kCreateEnrollRequest[] = "CreateEnrollRequest";
constexpr char kFinishEnroll[] = "FinishEnroll";
constexpr char kEnroll[] = "Enroll";
constexpr char kCreateCertificateRequest[] = "CreateCertificateRequest";
constexpr char kFinishCertificateRequest[] = "FinishCertificateRequest";
constexpr char kGetCertificate[] = "GetCertificate";
constexpr char kSignEnterpriseChallenge[] = "SignEnterpriseChallenge";
constexpr char kSignSimpleChallenge[] = "SignSimpleChallenge";
constexpr char kSetKeyPayload[] = "SetKeyPayload";
constexpr char kDeleteKeys[] = "DeleteKeys";
constexpr char kResetIdentity[] = "ResetIdentity";
constexpr char kGetEnrollmentId[] = "GetEnrollmentId";
constexpr char kGetCertifiedNvIndex[] = "GetCertifiedNvIndex";

namespace pca_agent {

constexpr char kPcaAgentServiceInterface[] = "org.chromium.PcaAgent";
constexpr char kPcaAgentServicePath[] = "/org/chromium/PcaAgent";
constexpr char kPcaAgentServiceName[] = "org.chromium.PcaAgent";

// Methods exported by |pca_agentd|.
constexpr char kEnroll[] = "Enroll";
constexpr char kGetCertificate[] = "GetCertificate";

}  // namespace pca_agent

}  // namespace attestation

#endif  // SYSTEM_API_DBUS_ATTESTATION_DBUS_CONSTANTS_H_
