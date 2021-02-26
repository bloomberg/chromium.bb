// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/fake_test_cert_verifier_params_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

FakeTestCertVerifierParamsFactory::FakeTestCertVerifierParamsFactory() =
    default;
FakeTestCertVerifierParamsFactory::~FakeTestCertVerifierParamsFactory() =
    default;

// static
mojom::CertVerifierParamsPtr
FakeTestCertVerifierParamsFactory::GetCertVerifierParams() {
  if (!base::FeatureList::IsEnabled(network::features::kCertVerifierService)) {
    return mojom::CertVerifierParams::NewCreationParams(
        mojom::CertVerifierCreationParams::New());
  }

  auto remote_params = mojom::CertVerifierServiceRemoteParams::New();
  mojo::PendingRemote<cert_verifier::mojom::CertVerifierService> cv_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeTestCertVerifierParamsFactory>(),
      cv_remote.InitWithNewPipeAndPassReceiver());
  remote_params->cert_verifier_service = std::move(cv_remote);
  return mojom::CertVerifierParams::NewRemoteParams(std::move(remote_params));
}

void FakeTestCertVerifierParamsFactory::Verify(
    const ::net::CertVerifier::RequestParams& params,
    mojo::PendingRemote<cert_verifier::mojom::CertVerifierRequest>
        cert_verifier_request) {
  mojo::Remote<cert_verifier::mojom::CertVerifierRequest> request(
      std::move(cert_verifier_request));
  net::CertVerifyResult result;
  result.verified_cert = params.certificate();
  request->Complete(std::move(result), net::OK);
}
}  // namespace network
